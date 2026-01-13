#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <QDebug>

#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <algorithm>
#include <cmath>

#include "core/telemetry/telemetry_bus.h"
#include "core/telemetry/telemetry_snapshot.h"

#include "core/audio/sndfile_replay_source.h"
#include "core/audio/i_audio_source.h"

#include "core/dsp/pcen_extractor.h"
#include "core/dsp/pcen_ring_buffer.h"

#include "qt_bridge/telemetry_provider.h"
#include "qt_bridge/pcen_image_provider.h"

static std::int64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    auto bus = std::make_shared<core::telemetry::TelemetryBus>();

    // Ring buffer под heatmap: 64 mel, ~15 секунд при hop=10ms => 1500 фреймов
    auto pcen_rb = std::make_shared<core::dsp::PcenRingBuffer>(64, 1500);

    QQmlApplicationEngine engine;

    auto* pcenProvider = new qt_bridge::PcenImageProvider();
    engine.addImageProvider("pcen", pcenProvider);

    auto* telemetry = new qt_bridge::TelemetryProvider(bus, pcen_rb, pcenProvider);
    telemetry->Start(12);
    engine.rootContext()->setContextProperty("telemetry", telemetry);

    std::atomic<bool> running{ true };

    std::thread audio_thread([&] {
       // std::string path = "C:/Users/Owner/_pish/II_Project/II_Project/datasets/uav_audio/raw/Mavik_3T/flight_026.flac"; 
        std::string path = "C:/Users/Owner/_pish/II_Project/II_Project/datasets/uav_audio/raw/Baba_yaga/flight_022.flac"; 

        core::audio::AudioSourceConfig acfg;
        acfg.sample_rate = 0;   // игнорируем, возьмём SR из файла
        acfg.channels = 0;      // игнорируем, возьмём CH из файла
        acfg.chunk_ms = 20;
        acfg.realtime = true;
        acfg.loop = true;

        core::audio::SndfileReplaySource src(path);
        if (!src.Open(acfg)) {
            qWarning() << "[audio] Failed to open:" << QString::fromStdString(path);
            while (running.load()) std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return;
        }

        qInfo() << "[audio] Opened"
            << "sr=" << src.file_sample_rate()
            << "ch=" << src.file_channels();

        // Создаём PCEN под фактический SR файла
        core::dsp::PcenConfig pcfg;
        pcfg.sample_rate = src.file_sample_rate();
        pcfg.n_fft = 512;
        pcfg.win_length = 400;
        pcfg.hop_length = 160;
        pcfg.n_mels = 64;

        core::dsp::PcenExtractor pcen(pcfg);

        std::vector<float> pcen_frames;
        pcen_frames.reserve(64 * 16);

        // Mock publisher vars (p_detect + FSM) — пока оставляем
        double t = 0.0;
        core::telemetry::FsmState state = core::telemetry::FsmState::IDLE;

        while (running.load()) {
            auto chunk = src.Read();
            if (!chunk) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            const int frames = chunk->frames;
            const int ch = chunk->channels;

            // Downmix to mono (для PCEN берём mono)
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

            // PCEN
            pcen_frames.clear();
            const int produced = pcen.Process(mono.data(), frames, &pcen_frames);
            if (produced > 0) {
                const int mels = pcen.n_mels();
                for (int i = 0; i < produced; ++i) {
                    pcen_rb->PushFrame(&pcen_frames[static_cast<std::size_t>(i * mels)]);
                }
            }

            // Mock p_detect/FSM publish
            t += 0.05;
            float p = static_cast<float>(0.5 + 0.45 * std::sin(t));
            if (static_cast<int>(t) % 7 == 0) p = std::min(1.0f, p + 0.25f);

            if (p > 0.65f) state = core::telemetry::FsmState::ACTIVE;
            else if (p > 0.5f) state = core::telemetry::FsmState::CANDIDATE;
            else state = core::telemetry::FsmState::IDLE;

            auto s = std::make_shared<core::telemetry::TelemetrySnapshot>();
            s->t_ns = now_ns();
            s->p_detect_latest = p;
            s->fsm_state = state;

            s->timeline_n = 64;
            const std::int64_t base = s->t_ns - 64 * 250'000'000LL;
            for (int i = 0; i < s->timeline_n; ++i) {
                s->timeline[static_cast<std::size_t>(i)].t_ns = base + i * 250'000'000LL;
                s->timeline[static_cast<std::size_t>(i)].p_detect =
                    static_cast<float>(0.5 + 0.45 * std::sin(t - 0.08 * (64 - i)));
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
