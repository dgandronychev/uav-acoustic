#include "qt_bridge/telemetry_provider.h"

#include "qt_bridge/pcen_image_provider.h"
#include "core/telemetry/telemetry_snapshot.h"

#include <QVariantMap>
#include <algorithm>

namespace qt_bridge {

    TelemetryProvider::TelemetryProvider(std::shared_ptr<core::telemetry::TelemetryBus> bus,
        std::shared_ptr<core::dsp::PcenRingBuffer> pcen_rb,
        PcenImageProvider* pcen_provider,
        QObject* parent)
        : QObject(parent),
        bus_(std::move(bus)),
        pcen_rb_(std::move(pcen_rb)),
        pcen_provider_(pcen_provider) {
    }

    void TelemetryProvider::Start(int fps) {
        const int ms = (fps > 0) ? std::max(1, 1000 / fps) : 100;
        connect(&timer_, &QTimer::timeout, this, &TelemetryProvider::OnTick);
        timer_.start(ms);
    }

    QString TelemetryProvider::FsmToString(core::telemetry::FsmState s) {
        using core::telemetry::FsmState;
        switch (s) {
        case FsmState::IDLE: return "IDLE";
        case FsmState::CANDIDATE: return "CANDIDATE";
        case FsmState::ACTIVE: return "ACTIVE";
        case FsmState::COOLDOWN: return "COOLDOWN";
        default: return "UNKNOWN";
        }
    }

    void TelemetryProvider::OnTick() {
        // 1) Consume latest telemetry snapshot for plot + state.
        auto snap = bus_ ? bus_->Latest() : nullptr;
        if (snap) {
            p_detect_latest_ = snap->p_detect_latest;
            fsm_state_str_ = FsmToString(snap->fsm_state);

            // Event flags only for current tick (so QML draws marker “now”)
            event_started_ = snap->event_started;
            event_ended_ = snap->event_ended;

            // Build timeline list for QML Canvas
            QVariantList list;
            const int n = std::clamp(snap->timeline_n, 0, static_cast<int>(core::telemetry::TelemetrySnapshot::kMaxTimeline));
            list.reserve(n);
            for (int i = 0; i < n; ++i) {
                QVariantMap m;
                m["t"] = static_cast<qlonglong>(snap->timeline[static_cast<std::size_t>(i)].t_ns);
                m["p"] = snap->timeline[static_cast<std::size_t>(i)].p_detect;
                list.push_back(m);
            }
            timeline_ = std::move(list);
        }
        else {
            // no snapshot yet
            event_started_ = false;
            event_ended_ = false;
        }

        // 2) Update PCEN image matrix (last ~15s) for image provider.
        if (pcen_provider_ && pcen_rb_) {
            int got_frames = 0;
            // Prefer stable UI: request fixed width (128 frames visible).
            const int want_frames = 128;
            auto frames = pcen_rb_->SnapshotLast(want_frames, &got_frames);  // packed [got_frames][n_mels]
            if (got_frames > 0) {
                pcen_provider_->SetPcenMatrix(std::move(frames), pcen_rb_->n_mels(), got_frames);
                frame_id_++;
            }
        }

        emit updated();
    }

}  // namespace qt_bridge
