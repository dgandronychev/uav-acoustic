#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QTimer>
#include <QPixmap>
#include <QImage>

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
class PlotWidget final : public QWidget {
public:
    explicit PlotWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(160);
    }

    void SetData(const QVector<double>& values, double threshold) {
        values_ = values;
        threshold_ = threshold;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#0B0F14"));

        const QRect area = rect().adjusted(12, 12, -12, -12);
        if (area.width() <= 0 || area.height() <= 0) return;

        p.setPen(QPen(QColor("#BFD3EE"), 1));
        p.drawRect(area);

        auto yFromProb = [&](double prob) {
            const double clamped = std::clamp(prob, 0.0, 1.0);
            return area.bottom() - static_cast<int>(clamped * area.height());
            };

        p.setPen(QPen(QColor("#646F7F"), 1, Qt::DashLine));
        p.drawLine(area.left(), yFromProb(threshold_), area.right(), yFromProb(threshold_));

        if (values_.size() < 2) return;

        QPainterPath path;
        const int n = values_.size();
        for (int i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(n - 1);
            const int x = area.left() + static_cast<int>(t * area.width());
            const int y = yFromProb(values_[i]);
            if (i == 0) path.moveTo(x, y);
            else path.lineTo(x, y);
        }

        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor("#FF5A5A"), 2));
        p.drawPath(path);
    }

private:
    QVector<double> values_;
    double threshold_ = 0.65;
};

class MainWidget final : public QWidget {
public:
    MainWidget(qt_bridge::TelemetryProvider* telemetry, qt_bridge::PcenImageProvider* pcen_provider, QWidget* parent = nullptr)
        : QWidget(parent), telemetry_(telemetry), pcen_provider_(pcen_provider) {
        setWindowTitle("UAV Acoustic (QWidget)");
        resize(1365, 768);
        setStyleSheet("background-color:#123A6E; color:#EAF2FF;");

        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(18, 18, 18, 18);
        root->setSpacing(0);

        auto* leftPanel = new QFrame(this);
        leftPanel->setFixedWidth(260);
        leftPanel->setStyleSheet("QFrame{background:#1B4F8F;border:2px solid #9DB7D7;}");
        auto* leftLayout = new QVBoxLayout(leftPanel);
        auto* leftHeader = new QLabel("КОМАНДНЫЙ ПУНКТ", leftPanel);
        leftHeader->setStyleSheet("background:#2C77D1; font-weight:700; font-size:16px; padding:8px; border:1px solid #9DB7D7;");
        auto* leftText = new QLabel("(левая панель — резерв)", leftPanel);
        leftText->setStyleSheet("color:#BFD3EE; padding:12px;");
        leftLayout->addWidget(leftHeader);
        leftLayout->addWidget(leftText);
        leftLayout->addStretch();

        auto* center = new QFrame(this);
        center->setStyleSheet("QFrame{background:#0F2F5C;border:2px solid #9DB7D7;}");
        auto* centerLayout = new QVBoxLayout(center);
        centerLayout->setSpacing(0);
        centerLayout->setContentsMargins(0, 0, 0, 0);

        auto* bannerZone = new QFrame(center);
        bannerZone->setFixedHeight(90);
        bannerZone->setStyleSheet("QFrame{background:#1B4F8F;border:2px solid #9DB7D7;}");
        auto* bannerLayout = new QVBoxLayout(bannerZone);
        bannerLayout->setContentsMargins(0, 0, 0, 0);
        bannerLabel_ = new QLabel("ОБНАРУЖЕН БПЛА", bannerZone);
        bannerLabel_->setAlignment(Qt::AlignCenter);
        bannerLabel_->setFixedHeight(44);
        bannerLabel_->setStyleSheet("background:#FFE600; color:black; font-weight:700; font-size:16px;");
        bannerLabel_->hide();
        bannerLayout->addStretch();
        bannerLayout->addWidget(bannerLabel_, 0, Qt::AlignCenter);
        bannerLayout->addStretch();

        auto* pcenZone = new QFrame(center);
        pcenZone->setStyleSheet("QFrame{background:#0F2F5C;border:2px solid #9DB7D7;}");
        auto* pcenLayout = new QVBoxLayout(pcenZone);
        pcenLayout->setContentsMargins(10, 10, 10, 10);
        pcenImageLabel_ = new QLabel("PCEN MEL", pcenZone);
        pcenImageLabel_->setAlignment(Qt::AlignCenter);
        pcenImageLabel_->setMinimumHeight(260);
        pcenImageLabel_->setStyleSheet("color:#BFD3EE;");
        pcenLayout->addWidget(pcenImageLabel_);

        auto* plotZone = new QFrame(center);
        plotZone->setFixedHeight(190);
        plotZone->setStyleSheet("QFrame{background:#0F2F5C;border:2px solid #9DB7D7;}");
        auto* plotLayout = new QVBoxLayout(plotZone);
        plotLayout->setContentsMargins(10, 10, 10, 10);
        plotWidget_ = new PlotWidget(plotZone);
        plotLayout->addWidget(plotWidget_);

        centerLayout->addWidget(bannerZone);
        centerLayout->addWidget(pcenZone, 1);
        centerLayout->addWidget(plotZone);

        auto* rightPanel = new QFrame(this);
        rightPanel->setFixedWidth(320);
        rightPanel->setStyleSheet("QFrame{background:#1B4F8F;border:2px solid #9DB7D7;}");
        auto* rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(10, 10, 10, 10);
        auto* title = new QLabel("ПАНЕЛЬ ДАННЫХ", rightPanel);
        title->setStyleSheet("font-size:16px; font-weight:700; background:#2C77D1; padding:8px;");
        rightLayout->addWidget(title);

        pDetectLabel_ = new QLabel("P_detect: 0.000", rightPanel);
        fsmLabel_ = new QLabel("FSM: IDLE", rightPanel);
        frameLabel_ = new QLabel("Frame ID: 0", rightPanel);
        for (QLabel* lbl : { pDetectLabel_, fsmLabel_, frameLabel_ }) {
            lbl->setStyleSheet("font-size:14px; padding:4px;");
            rightLayout->addWidget(lbl);
        }
        rightLayout->addStretch();

        root->addWidget(leftPanel);
        root->addWidget(center, 1);
        root->addWidget(rightPanel);

        connect(telemetry_, &qt_bridge::TelemetryProvider::updated, this, [this] { Refresh(); });
        Refresh();
    }

private:
    void Refresh() {
        const double p = telemetry_->pDetectLatest();
        const QString fsm = telemetry_->fsmState();
        const int frame = telemetry_->frameId();
        const double threshold = 0.65;

        pDetectLabel_->setText(QString("P_detect: %1").arg(p, 0, 'f', 3));
        fsmLabel_->setText(QString("FSM: %1").arg(fsm));
        frameLabel_->setText(QString("Frame ID: %1").arg(frame));
        bannerLabel_->setVisible(p >= threshold);

        QImage img = pcen_provider_->GetLatestImage();
        if (!img.isNull()) {
            pcenImageLabel_->setPixmap(QPixmap::fromImage(img).scaled(pcenImageLabel_->size(), Qt::IgnoreAspectRatio, Qt::FastTransformation));
        }

        QVector<double> values;
        const QVariantList tl = telemetry_->timeline();
        values.reserve(tl.size());
        for (const QVariant& point : tl) {
            const QVariantMap m = point.toMap();
            values.push_back(m.value("p").toDouble());
        }
        plotWidget_->SetData(values, threshold);
    }

private:
    qt_bridge::TelemetryProvider* telemetry_;
    qt_bridge::PcenImageProvider* pcen_provider_;

    QLabel* bannerLabel_ = nullptr;
    QLabel* pcenImageLabel_ = nullptr;
    PlotWidget* plotWidget_ = nullptr;

    QLabel* pDetectLabel_ = nullptr;
    QLabel* fsmLabel_ = nullptr;
    QLabel* frameLabel_ = nullptr;
};


int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    auto bus = std::make_shared<core::telemetry::TelemetryBus>();

    auto pcen_rb = std::make_shared<core::dsp::PcenRingBuffer>(
        /*n_mels=*/128,
        /*capacity_frames=*/1500
    );

    core::dsp::PcenConfig pcfg;
    pcfg.sample_rate = 22050;
    pcfg.n_fft = 1024;
    pcfg.win_length = 1024;
    pcfg.hop_length = 256;
    pcfg.n_mels = 128;
    pcfg.alpha = 0.6f;
    pcfg.delta = 2.0f;
    pcfg.r = 0.1f;
    pcfg.eps = 1e-6f;
    {
        const float time_constant = 0.4f;
        const float hop_sec = static_cast<float>(pcfg.hop_length) / static_cast<float>(pcfg.sample_rate);
        pcfg.s = 1.0f - std::exp(-hop_sec / time_constant);
    }
    core::dsp::PcenExtractor pcen(pcfg);

    // --- TCN detector (dynamic float32) ---
    //core::tflite::TcnDetector::Config tcfg;
    //core::tflite::TcnDetector tcn(pcen_rb, tcfg);

    //tcfg.n_mels = 128;
    //tcfg.n_frames = 169;
    //tcfg.step_ms = 250;
    //tcfg.num_threads = 2;
    // Paths: adjust to your layout
    //tcn.LoadModel("model_dynamic.tflite");
    //tcn.LoadClassNames("class_names.txt");

    // --- Mock detector (without TFLite dependency) ---
    core::detect::MockDetector::Config dcfg;
    dcfg.sample_rate = pcfg.sample_rate;
    dcfg.frame_ms = 20;
    core::detect::MockDetector detector(dcfg);

    // --- FSM параметры ---
    const float p_on = 0.65f;
    const float p_off = 0.45f;
    const int t_confirm_ms = 200;
    const int t_release_ms = 300;
    const int cooldown_ms = 800;

    core::telemetry::FsmState fsm = core::telemetry::FsmState::IDLE;
    int above_ms = 0;
    int below_ms = 0;
    int cooldown_left_ms = 0;

    // --- SegmentBuilder  ---
    core::segment::SegmentBuilder::Config scfg;
    scfg.n_mels = 64;
    scfg.hop_ms = 10;          
    scfg.pre_roll_ms = 2000;    // 2s до старта
    scfg.post_roll_ms = 2000;   // 2s после конца
    scfg.max_event_ms = 12000;  // защита
    scfg.out_dir = "segments";

    try {
        std::filesystem::create_directories(scfg.out_dir);
    }
    catch (...) {
    }

    core::segment::SegmentBuilder segment_builder(pcen_rb, scfg);

    auto* pcenProvider = new qt_bridge::PcenImageProvider();

    auto* telemetry = new qt_bridge::TelemetryProvider(bus, pcen_rb, pcenProvider);
    telemetry->Start(12);

    MainWidget window(telemetry, pcenProvider);

    // ---- Audio replay + PCEN + detector thread ----
    std::atomic<bool> running{ true };
    std::thread audio_thread([&] {
        std::string path = "C:/Users/Owner/_pish/test_orig.flac";

        core::audio::AudioSourceConfig acfg;
        acfg.sample_rate = 22050;
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
        pcen_frames.reserve(128 * 32);

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

            // Run TCN inference periodically
            //tcn.Tick(acfg.chunk_ms);
            //const float p = tcn.p_detect();
            const float p = detector.Process(mono.data(), frames);

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
    window.show();
    const int rc = app.exec();

    running.store(false);
    audio_thread.join();
    return rc;
}
