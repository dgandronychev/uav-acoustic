#include "qt_bridge/telemetry_provider.h"

#include <algorithm>

#include "core/telemetry/telemetry_snapshot.h"

namespace qt_bridge {

    static QString ToQString(core::telemetry::FsmState s) {
        using core::telemetry::FsmState;
        switch (s) {
        case FsmState::IDLE: return "IDLE";
        case FsmState::CANDIDATE: return "CANDIDATE";
        case FsmState::ACTIVE: return "ACTIVE";
        case FsmState::COOLDOWN: return "COOLDOWN";
        default: return "UNKNOWN";
        }
    }

    TelemetryProvider::TelemetryProvider(std::shared_ptr<core::telemetry::TelemetryBus> bus,
        PcenImageProvider* pcen_provider,
        QObject* parent)
        : QObject(parent),
        bus_(std::move(bus)),
        pcen_provider_(pcen_provider) {
        connect(&timer_, &QTimer::timeout, this, &TelemetryProvider::onTick);
    }

    void TelemetryProvider::start(int fps) {
        fps = std::clamp(fps, 1, 60);
        timer_.start(1000 / fps);
    }

    void TelemetryProvider::onTick() {
        // Pull latest snapshot
        auto s = bus_ ? bus_->Latest() : nullptr;
        if (s) {
            p_detect_latest_ = static_cast<double>(s->p_detect_latest);
            fsm_state_ = ToQString(s->fsm_state);

            // timeline -> QVariantList of {t, p}
            QVariantList tl;
            tl.reserve(s->timeline_n);
            for (int i = 0; i < s->timeline_n; ++i) {
                const auto& pt = s->timeline[static_cast<std::size_t>(i)];
                QVariantMap m;
                m["t"] = static_cast<qlonglong>(pt.t_ns);
                m["p"] = static_cast<double>(pt.p_detect);
                tl.push_back(m);
            }
            timeline_ = std::move(tl);
        }

        // Update mock heatmap
        if (pcen_provider_) {
            pcen_provider_->UpdateMockHeatmap();
        }

        ++frame_id_;
        emit updated();
    }

}  // namespace qt_bridge
