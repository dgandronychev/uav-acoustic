#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <cmath>
#include <algorithm>

#include "core/telemetry/telemetry_bus.h"
#include "qt_bridge/telemetry_provider.h"
#include "qt_bridge/pcen_image_provider.h"

static std::int64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    auto bus = std::make_shared<core::telemetry::TelemetryBus>();
    QQmlApplicationEngine engine;

    auto* pcenProvider = new qt_bridge::PcenImageProvider();
    engine.addImageProvider("pcen", pcenProvider);

    auto* telemetry = new qt_bridge::TelemetryProvider(bus, pcenProvider);
    telemetry->start(12);

    engine.rootContext()->setContextProperty("telemetry", telemetry);

    std::atomic<bool> running{ true };
    std::thread pub([&] {
        double t = 0.0;
        core::telemetry::FsmState state = core::telemetry::FsmState::IDLE;

        while (running.load()) {
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
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        });

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        running.store(false);
        pub.join();
        return -1;
    }

    const int rc = app.exec();
    running.store(false);
    pub.join();
    return rc;
}
