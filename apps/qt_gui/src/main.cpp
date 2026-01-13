#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <algorithm>
#include <cmath>
#include <vector>

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

static float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static float sigmoid(float x) {
    // безопасная сигмоида
    if (x > 20.0f) return 1.0f;
    if (x < -20.0f) return 0.0f;
    return 1.0f / (1.0f + std::exp(-x));
}

// RMS->dBFS (примерно), вход float [-1..1]
static float rms_db(const float* x, int n) {
    if (!x || n <= 0) return -120.0f;
    double acc = 0.0;
    for (int i = 0; i < n; ++i) {
        const double v = static_cast<double>(x[i]);
        acc += v * v;
    }
    const double mean = acc / static_cast<double>(n);
    const double rms = std::sqrt(mean);
    const double eps = 1e-12;
    const double db = 20.0 * std::log10(rms + eps);
    // ограничим «пол» для стабильности
    if (db < -120.0) return -120.0f;
    return static_cast<float>(db);
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    auto bus = std::make_shared<core::telemetry::TelemetryBus>();

    // ---------- PCEN DSP ----------
    auto pcen_rb = std::make_shared<core::dsp::PcenRingBuffer>(
        /*n_mels=*/64,
        /*capacity_frames=*/1500  // ~15s при hop=10ms (100 fps)
    );

    core::dsp::PcenConfig pcfg;
    pcfg.sample_rate = 16000;
    pcfg.n_fft = 512;
    pcfg.win_length = 400;   // 25ms @16k
    pcfg.hop_length = 160;   // 10ms @16k
    pcfg.n_mels = 64;
    // PCEN параметры (можешь тюнить позже):
    // pcfg.s = 0.025f; pcfg.alpha = 0.98f; pcfg.delta = 2.0f; pcfg.r = 0.5f;

    core::dsp::PcenExtractor pcen(pcfg);

    QQmlApplicationEngine engine;

    // Heatmap provider
    auto* pcenProvider = new qt_bridge::PcenImageProvider();
    engine.addImageProvider("pcen", pcenProvider);

    // UI polling provider
    auto* telemetry = new qt_bridge::TelemetryProvider(bus, pcen_rb, pcenProvider);
    // ВАЖНО: в твоём текущем коде метод называется Start (с большой буквы)
    telemetry->Start(12);
    engine.rootContext()->setContextProperty("telemetry", telemetry);

    // ---------- Audio + PCEN + MockDetector thread ----------
    std::atomic<bool> running{ true };

    std::thread audio_thread([&] {
        // 1) путь к flac: можно потом вынести в argv/config
        std::string path =
            "C:/Users/Owner/_pish/test_orig.flac";

        core::audio::AudioSourceConfig acfg;
        acfg.sample_rate = 16000;
        acfg.channels = 1;
        acfg.chunk_ms = 20;    // 20ms
        acfg.realtime = true;
        acfg.loop = true;

        core::audio::SndfileReplaySource src(path);
        if (!src.Open(acfg)) {
            // Если не открылось — p_detect будет 0, но UI живёт
            while (running.load()) {
                auto s = std::make_shared<core::telemetry::TelemetrySnapshot>();
                s->t_ns = now_ns();
                s->p_detect_latest = 0.0f;
                s->fsm_state = core::telemetry::FsmState::IDLE;
                s->timeline_n = 0;
                bus->Publish(std::move(s));
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            return;
        }

        std::vector<float> pcen_frames;  // packed: frames x mels
        pcen_frames.reserve(64 * 64);

        // 2) MockDetector: EMA mean/var по dB и sigmoid->p
        bool stats_inited = false;
        float mu = -60.0f;      // EMA mean dB
        float var = 100.0f;     // EMA var
        const float ema = 0.02f;  // скорость адаптации
        const float k = 2.0f;     // крутизна sigmoid

        // 3) FSM (минимальный гистерезис)
        // Если у тебя enum только IDLE/CANDIDATE/ACTIVE — используем их.
        core::telemetry::FsmState state = core::telemetry::FsmState::IDLE;
        const float p_on = 0.65f;
        const float p_off = 0.45f;
        int on_count = 0;
        int off_count = 0;
        // подтверждение: примерно 200мс (10 чанков по 20мс)
        const int on_confirm = 10;
        const int off_confirm = 10;

        // timeline p_detect
        constexpr int kHist = 64;
        float hist[kHist]{};
        int hist_i = 0;

        while (running.load()) {
            auto chunk = src.Read();
            if (!chunk) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            const int frames = chunk->frames;
            const int ch = chunk->channels;

            // mono float
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

            // ---- PCEN update (подсистема 3 продолжает работать) ----
            pcen_frames.clear();
            const int produced = pcen.Process(mono.data(), frames, &pcen_frames);
            if (produced > 0) {
                const int mels = pcen.n_mels();
                for (int i = 0; i < produced; ++i) {
                    pcen_rb->PushFrame(&pcen_frames[static_cast<std::size_t>(i * mels)]);
                }
            }

            // ---- MockDetector: энергия по аудио (RMS dB) -> p_detect ----
            const float db = rms_db(mono.data(), frames);

            if (!stats_inited) {
                mu = db;
                var = 25.0f;
                stats_inited = true;
            }
            else {
                const float diff = db - mu;
                mu = (1.0f - ema) * mu + ema * db;
                var = (1.0f - ema) * var + ema * (diff * diff);
                if (var < 1e-3f) var = 1e-3f;
            }

            const float sigma = std::sqrt(var);
            const float z = (db - mu) / sigma;         // z-score
            float p = sigmoid(k * z);                  // 0..1
            // дополнительный пол: если абсолютный db совсем низкий — прижмём
            if (db < -85.0f) p *= 0.2f;
            p = clamp01(p);

            // ---- FSM (гистерезис + подтверждение) ----
            if (state == core::telemetry::FsmState::IDLE) {
                if (p >= p_on) {
                    on_count++;
                    if (on_count >= on_confirm) {
                        state = core::telemetry::FsmState::ACTIVE;
                        on_count = 0;
                    }
                }
                else {
                    on_count = 0;
                }
            }
            else { // ACTIVE (и/или CANDIDATE если у тебя есть)
                if (p <= p_off) {
                    off_count++;
                    if (off_count >= off_confirm) {
                        state = core::telemetry::FsmState::IDLE;
                        off_count = 0;
                    }
                }
                else {
                    off_count = 0;
                }
            }

            // ---- timeline ring ----
            hist[hist_i] = p;
            hist_i = (hist_i + 1) % kHist;

            // ---- publish snapshot ----
            auto s = std::make_shared<core::telemetry::TelemetrySnapshot>();
            s->t_ns = now_ns();
            s->p_detect_latest = p;
            s->fsm_state = state;

            s->timeline_n = kHist;
            const std::int64_t base = s->t_ns - static_cast<std::int64_t>(kHist) * 250'000'000LL; // 0.25s шаг на графике
            for (int i = 0; i < kHist; ++i) {
                const int idx = (hist_i + i) % kHist; // oldest -> newest
                s->timeline[static_cast<std::size_t>(i)].t_ns = base + i * 250'000'000LL;
                s->timeline[static_cast<std::size_t>(i)].p_detect = hist[idx];
            }

            bus->Publish(std::move(s));
        }
        });

    // Load QML
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
