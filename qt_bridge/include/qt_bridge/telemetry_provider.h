#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <memory>

#include "core/telemetry/telemetry_bus.h"
#include "qt_bridge/pcen_image_provider.h"

namespace qt_bridge {

    class TelemetryProvider final : public QObject {
        Q_OBJECT

            Q_PROPERTY(double pDetectLatest READ pDetectLatest NOTIFY updated)
            Q_PROPERTY(QString fsmState READ fsmState NOTIFY updated)
            Q_PROPERTY(QVariantList timeline READ timeline NOTIFY updated)
            Q_PROPERTY(int frameId READ frameId NOTIFY updated)

    public:
        TelemetryProvider(std::shared_ptr<core::telemetry::TelemetryBus> bus,
            PcenImageProvider* pcen_provider,
            QObject* parent = nullptr);

        // Called from main.cpp
        Q_INVOKABLE void start(int fps = 12);

        double pDetectLatest() const { return p_detect_latest_; }
        QString fsmState() const { return fsm_state_; }
        QVariantList timeline() const { return timeline_; }
        int frameId() const { return frame_id_; }

    signals:
        void updated();

    private slots:
        void onTick();

    private:
        std::shared_ptr<core::telemetry::TelemetryBus> bus_;
        PcenImageProvider* pcen_provider_ = nullptr;
        QTimer timer_;

        double p_detect_latest_ = 0.0;
        QString fsm_state_ = "IDLE";
        QVariantList timeline_;
        int frame_id_ = 0;
    };

}  // namespace qt_bridge
