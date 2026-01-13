#pragma once

#include <QObject>
#include <QVariantList>
#include <QTimer>

#include <memory>

#include "core/telemetry/telemetry_bus.h"
#include "core/dsp/pcen_ring_buffer.h"

namespace qt_bridge {

    class PcenImageProvider;

    class TelemetryProvider : public QObject {
        Q_OBJECT

            Q_PROPERTY(double pDetectLatest READ pDetectLatest NOTIFY updated)
            Q_PROPERTY(QString fsmState READ fsmState NOTIFY updated)
            Q_PROPERTY(int frameId READ frameId NOTIFY updated)
            Q_PROPERTY(QVariantList timeline READ timeline NOTIFY updated)

            // New: event markers for “now”
            Q_PROPERTY(bool eventStarted READ eventStarted NOTIFY updated)
            Q_PROPERTY(bool eventEnded READ eventEnded NOTIFY updated)

    public:
        TelemetryProvider(std::shared_ptr<core::telemetry::TelemetryBus> bus,
            std::shared_ptr<core::dsp::PcenRingBuffer> pcen_rb,
            PcenImageProvider* pcen_provider,
            QObject* parent = nullptr);

        void Start(int fps);

        double pDetectLatest() const { return p_detect_latest_; }
        QString fsmState() const { return fsm_state_str_; }
        int frameId() const { return frame_id_; }
        QVariantList timeline() const { return timeline_; }

        bool eventStarted() const { return event_started_; }
        bool eventEnded() const { return event_ended_; }

    signals:
        void updated();

    private slots:
        void OnTick();

    private:
        static QString FsmToString(core::telemetry::FsmState s);

    private:
        std::shared_ptr<core::telemetry::TelemetryBus> bus_;
        std::shared_ptr<core::dsp::PcenRingBuffer> pcen_rb_;
        PcenImageProvider* pcen_provider_ = nullptr;

        QTimer timer_;

        double p_detect_latest_ = 0.0;
        QString fsm_state_str_ = "IDLE";
        int frame_id_ = 0;

        QVariantList timeline_;

        bool event_started_ = false;
        bool event_ended_ = false;
    };

}  // namespace qt_bridge
