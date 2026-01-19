#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <algorithm>
#include <vector>
#include <deque>
#include <iostream>
#include <filesystem>

#include "core/telemetry/telemetry_bus.h"
#include "core/telemetry/telemetry_snapshot.h"

#include "core/audio/sndfile_replay_source.h"
#include "core/audio/i_audio_source.h"

#include "core/dsp/pcen_extractor.h"
#include "core/dsp/pcen_ring_buffer.h"

#include "core/detect/mock_detector.h"
#include "core/segment/segment_builder.h"

#include "qt_bridge/telemetry_provider.h"
#include "qt_bridge/pcen_image_provider.h"

static std::int64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    auto bus = std::make_shared<core::telemetry::TelemetryBus>();

    // --- PCEN ring buffer (for UI heatmap) ---
    auto pcen_rb = std::make_shared<core::dsp::PcenRingBuffer>(
        /*n_mels=*/64,
        /*capacity_frames=*/1500  // ~15s if hop ~10ms
    );

    core::dsp::PcenConfig pcfg;
    pcfg.sample_rate = 16000;
    pcfg.n_fft = 512;
    pcfg.win_length = 400;
    pcfg.hop_length = 160;  // 10ms at 16k
    pcfg.n_mels = 64;
    core::dsp::PcenExtractor pcen(pcfg);

    // --- Mock VAD detector ---
    core::detect::MockDetector::Config dcfg;
    dcfg.sample_rate = 16000;
    dcfg.frame_ms = 20;
    dcfg.norm_alpha = 0.01f;
    dcfg.p_ema_alpha = 0.20f;
    dcfg.sigmoid_k = 1.2f;
    dcfg.sigmoid_bias = 0.0f;
    dcfg.p_on = 0.65f;
    dcfg.p_off = 0.45f;
    dcfg.t_confirm_ms = 200;
    dcfg.t_release_ms = 300;
    core::detect::MockDetector det(dcfg);

    // --- FSM (подсистема 5) параметры ---
    const float p_on = 0.65f;
    const float p_off = 0.45f;
    const int t_confirm_ms = 200;
    const int t_release_ms = 300;
    const int cooldown_ms = 800;

    core::telemetry::FsmState fsm = core::telemetry::FsmState::IDLE;
    int above_ms = 0;
    int below_ms = 0;
    int cooldown_left_ms = 0;

    // --- SegmentBuilder (подсистема 6) ---
    core::segment::SegmentBuilder::Config scfg;
    scfg.n_mels = 64;
    scfg.hop_ms = 10;           // соответствует pcfg.hop_length=160 @ 16k
    scfg.pre_roll_ms = 2000;    // 2s до старта
    scfg.post_roll_ms = 2000;   // 2s после конца
    scfg.max_event_ms = 12000;  // защита
    scfg.out_dir = "segments";

    // На всякий случай создадим директорию
    try {
        std::filesystem::create_directories(scfg.out_dir);
    }
    catch (...) {
        // не фатально
    }

    core::segment::SegmentBuilder segment_builder(pcen_rb, scfg);

    QQmlApplicationEngine engine;

    auto* pcenProvider = new qt_bridge::PcenImageProvider();
    engine.addImageProvider("pcen", pcenProvider);

    auto* telemetry = new qt_bridge::TelemetryProvider(bus, pcen_rb, pcenProvider);
    telemetry->Start(12);
    engine.rootContext()->setContextProperty("telemetry", telemetry);

    std::atomic<bool> running{ true };

    // ---- Audio replay + PCEN + detector thread ----
    std::thread audio_thread([&] {
        std::string path =
            "C:/Users/Owner/_pish/test_orig.flac";

        core::audio::AudioSourceConfig acfg;
        acfg.sample_rate = 16000;
        acfg.channels = 1;
        acfg.chunk_ms = 20;
        acfg.realtime = true;
        acfg.loop = true;

        core::audio::SndfileReplaySource src(path);
        if (!src.Open(acfg)) {
            while (running.load()) std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return;
        }

        std::vector<float> pcen_frames;
        pcen_frames.reserve(64 * 32);

        std::deque<float> p_hist;
        const int kHistN = 64;

        const std::int64_t hop_ns = static_cast<std::int64_t>(pcfg.hop_length) * 1'000'000'000LL / pcfg.sample_rate;
        const int dt_ms = acfg.chunk_ms;

        while (running.load()) {
            auto chunk = src.Read();
            if (!chunk) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            const int frames = chunk->frames;
            const int ch = chunk->channels;

            // Interleaved float -> mono
            std::vector<float> mono(static_cast<std::size_t>(frames), 0.0f);
            const float* inter = chunk->interleaved.data();

            if (ch == 1) {
                std::copy(inter, inter + frames, mono.begin());
            }
            else {
                for (int i = 0; i < frames; ++i) {
                    float s = 0.0f;
                    for (int c = 0; c < ch; ++c) s += inter[i * ch + c];
                    mono[static_cast<std::size_t>(i)] = s / static_cast<float>(ch);
                }
            }

            // --- PCEN -> ring buffer ---
            pcen_frames.clear();
            const int produced = pcen.Process(mono.data(), frames, &pcen_frames);
            if (produced > 0) {
                const int mels = pcen.n_mels();
                // База времени для PCEN-фреймов: берем t0 чанка
                // (AudioChunk::t0_ns выставляется в источнике)
                const std::int64_t base_ns = chunk->t0_ns;

                for (int i = 0; i < produced; ++i) {
                    const float* frame_ptr = &pcen_frames[static_cast<std::size_t>(i * mels)];
                    pcen_rb->PushFrame(frame_ptr);

                    const std::int64_t frame_t_ns = base_ns + hop_ns * i;
                    segment_builder.OnFramePushed(frame_t_ns);
                }
            }

            // --- p_detect ---
            const float p = det.Process(mono.data(), frames);

            // --- FSM update ---
            bool event_started = false;
            bool event_ended = false;

            if (cooldown_left_ms > 0) {
                cooldown_left_ms = std::max(0, cooldown_left_ms - dt_ms);
                fsm = core::telemetry::FsmState::COOLDOWN;
            }
            else {
                switch (fsm) {
                case core::telemetry::FsmState::IDLE:
                    if (p >= p_on) {
                        above_ms += dt_ms;
                        if (above_ms >= t_confirm_ms) {
                            fsm = core::telemetry::FsmState::ACTIVE;
                            event_started = true;
                            above_ms = 0;
                            below_ms = 0;
                        }
                        else {
                            fsm = core::telemetry::FsmState::CANDIDATE;
                        }
                    }
                    else {
                        above_ms = 0;
                        below_ms = 0;
                    }
                    break;

                case core::telemetry::FsmState::CANDIDATE:
                    if (p >= p_on) {
                        above_ms += dt_ms;
                        if (above_ms >= t_confirm_ms) {
                            fsm = core::telemetry::FsmState::ACTIVE;
                            event_started = true;
                            above_ms = 0;
                            below_ms = 0;
                        }
                    }
                    else {
                        // сорвалось
                        fsm = core::telemetry::FsmState::IDLE;
                        above_ms = 0;
                    }
                    break;

                case core::telemetry::FsmState::ACTIVE:
                    if (p <= p_off) {
                        below_ms += dt_ms;
                        if (below_ms >= t_release_ms) {
                            fsm = core::telemetry::FsmState::COOLDOWN;
                            cooldown_left_ms = cooldown_ms;
                            event_ended = true;
                            below_ms = 0;
                        }
                    }
                    else {
                        below_ms = 0;
                    }
                    break;

                case core::telemetry::FsmState::COOLDOWN:
                default:
                    break;
                }
            }

            const std::int64_t t_ns = now_ns();

            // Segment builder hooks
            if (event_started) segment_builder.OnEventStart(t_ns);
            if (event_ended) segment_builder.OnEventEnd(t_ns);

            if (segment_builder.HasReadySegment()) {
                auto info = segment_builder.PopReadySegment();
                std::cout << "[SEGMENT] saved: " << info.path
                    << " frames=" << info.frames
                    << " n_mels=" << info.n_mels
                    << std::endl;
            }

            // history for plot
            p_hist.push_back(p);
            while (static_cast<int>(p_hist.size()) > kHistN) p_hist.pop_front();

            // publish telemetry snapshot
            auto s = std::make_shared<core::telemetry::TelemetrySnapshot>();
            s->t_ns = t_ns;
            s->p_detect_latest = p;
            s->fsm_state = fsm;

            // Эти поля должны быть добавлены в TelemetrySnapshot
            s->event_started = event_started;
            s->event_ended = event_ended;

            s->timeline_n = static_cast<int>(p_hist.size());
            const std::int64_t step_ns = static_cast<std::int64_t>(acfg.chunk_ms) * 1'000'000LL;
            const std::int64_t base_plot = s->t_ns - step_ns * static_cast<std::int64_t>(s->timeline_n);

            for (int i = 0; i < s->timeline_n; ++i) {
                s->timeline[static_cast<std::size_t>(i)].t_ns = base_plot + step_ns * i;
                s->timeline[static_cast<std::size_t>(i)].p_detect = p_hist[static_cast<std::size_t>(i)];
            }

            bus->Publish(std::move(s));
        }
        });

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        running.store(false);
        audio_thread.join();
        return -1;
    }

    const int rc = app.exec();

    running.store(false);
    audio_thread.join();
    return rc;
}
