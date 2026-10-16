#include "tabs/PlotTab.h"

#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLegendMarker>
#include <QLineSeries>
#include <QMessageBox>
#include <QPushButton>
#include <QTextStream>
#include <QValueAxis>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <iterator>

static const size_t MAX_HISTORY = 200000;  // ~2.8 h at 20 Hz
static const int REDRAW_MS = 100;

PlotTab::PlotTab(MotorWorker *worker, QWidget *parent) : QWidget(parent) {
    m_traces = {
        {"Measured position", &Snapshot::posMeas},
        {"Commanded position", &Snapshot::posCmd},
        {"Tracking error", &Snapshot::posTrk},
        {"Measured velocity", &Snapshot::velMeas},
        {"Commanded velocity", &Snapshot::velCmd},
        {"Measured torque", &Snapshot::trqMeas},
        {"Commanded torque", &Snapshot::trqCmd},
    };

    m_chart = new QChart;
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_xAxis = new QValueAxis;
    m_xAxis->setTitleText("Time (s)");
    m_yAxis = new QValueAxis;
    m_chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_chart->addAxis(m_yAxis, Qt::AlignLeft);

    auto *controls = new QHBoxLayout;
    for (Trace &t : m_traces) {
        t.series = new QLineSeries;
        t.series->setName(t.name);
        m_chart->addSeries(t.series);
        t.series->attachAxis(m_xAxis);
        t.series->attachAxis(m_yAxis);
        t.check = new QCheckBox(t.name);
        controls->addWidget(t.check);
    }
    m_traces[3].check->setChecked(true);  // measured + commanded velocity by default
    m_traces[4].check->setChecked(true);
    for (Trace &t : m_traces) {
        t.series->setVisible(t.check->isChecked());
        // Connected after the defaults are set: redraw() needs widgets created below
        connect(t.check, &QCheckBox::toggled, this, [this, &t](bool on) {
            t.series->setVisible(on);
            redraw();
        });
    }
    controls->addStretch();

    // Vertical line at the trigger point (t = 0), hidden from the legend
    m_trigLine = new QLineSeries;
    m_trigLine->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    m_chart->addSeries(m_trigLine);
    m_trigLine->attachAxis(m_xAxis);
    m_trigLine->attachAxis(m_yAxis);
    m_chart->legend()->markers(m_trigLine).first()->setVisible(false);

    auto *trigger = new QHBoxLayout;
    m_mode = new QComboBox;
    m_mode->addItems({"Auto", "Normal", "Single", "Stop"});
    auto *forceBtn = new QPushButton("Force");
    forceBtn->setToolTip("Trigger now, capture once, then stop");
    m_source = new QComboBox;
    m_source->addItems({"Start of positive command", "Start of negative command",
                        "Start of any command", "End of positive command",
                        "End of negative command", "End of any command",
                        "End of positive settled move", "End of negative settled move",
                        "End of any settled move", "Alert (shutdown or exception)",
                        "Rising slope", "Falling slope"});
    m_source->setCurrentIndex(StartAny);
    m_levelTrace = new QComboBox;
    for (const Trace &t : m_traces) m_levelTrace->addItem(t.name);
    m_levelTrace->setCurrentIndex(3);
    m_level = new QDoubleSpinBox;
    m_level->setRange(-1e9, 1e9);
    m_level->setDecimals(2);
    m_position = new QComboBox;
    for (int pct : {10, 50, 90}) m_position->addItem(QString("%1%").arg(pct), pct / 100.0);
    m_status = new QLabel;
    m_status->setMinimumWidth(80);
    trigger->addWidget(new QLabel("Mode:"));
    trigger->addWidget(m_mode);
    trigger->addWidget(forceBtn);
    trigger->addWidget(new QLabel("Trigger on:"));
    trigger->addWidget(m_source);
    trigger->addWidget(m_levelTrace);
    trigger->addWidget(new QLabel("Level:"));
    trigger->addWidget(m_level);
    trigger->addWidget(new QLabel("Position:"));
    trigger->addWidget(m_position);
    trigger->addWidget(m_status);
    trigger->addStretch();

    auto slopeControls = [this] {
        bool slope = m_source->currentIndex() >= Rising;
        m_levelTrace->setEnabled(slope);
        m_level->setEnabled(slope);
    };
    slopeControls();
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this] {
        Mode m = Mode(m_mode->currentIndex());
        if (m == Stop) {
            if (!m_history.empty()) m_stopMs = m_history.back().tMs;
        } else {
            m_showCapture = m != Auto;
            if (m != Auto) m_capture.clear();
        }
        rearm();
    });
    connect(forceBtn, &QPushButton::clicked, this, [this] {
        if (m_history.empty()) return;
        m_trigMs = m_history.back().tMs;
        m_forced = true;
        updateStatus();
    });
    connect(m_source, &QComboBox::currentIndexChanged, this, [this, slopeControls] {
        slopeControls();
        rearm();
    });
    connect(m_levelTrace, &QComboBox::currentIndexChanged, this, &PlotTab::rearm);
    connect(m_level, &QDoubleSpinBox::valueChanged, this, &PlotTab::rearm);
    connect(m_position, &QComboBox::currentIndexChanged, this, &PlotTab::rearm);

    auto *bottom = new QHBoxLayout;
    m_window = new QDoubleSpinBox;
    m_window->setRange(0.5, 600);
    m_window->setDecimals(1);
    m_window->setValue(10);
    m_window->setSuffix(" s");
    auto *clearBtn = new QPushButton("Clear");
    auto *csvBtn = new QPushButton("Export CSV...");
    bottom->addWidget(new QLabel("Window:"));
    bottom->addWidget(m_window);
    bottom->addWidget(clearBtn);
    bottom->addStretch();
    bottom->addWidget(new QLabel("Polled at 20 Hz; not a high-speed scope."));
    bottom->addWidget(csvBtn);
    connect(m_window, &QDoubleSpinBox::valueChanged, this, &PlotTab::rearm);
    connect(clearBtn, &QPushButton::clicked, this, &PlotTab::clear);
    connect(csvBtn, &QPushButton::clicked, this, &PlotTab::exportCsv);

    auto *view = new QChartView(m_chart);
    view->setRenderHint(QPainter::Antialiasing);
    auto *lay = new QVBoxLayout(this);
    lay->addLayout(controls);
    lay->addLayout(trigger);
    lay->addWidget(view, 1);
    lay->addLayout(bottom);

    connect(worker, &MotorWorker::snapshot, this, &PlotTab::addSnapshot);
    connect(worker, &MotorWorker::nodeSelected, this, &PlotTab::clear);
    updateStatus();
}

void PlotTab::addSnapshot(const Snapshot &s) {
    m_history.push_back(s);
    if (m_history.size() > MAX_HISTORY) m_history.pop_front();
    if (m_history.size() < 2) return;
    const Snapshot &prev = m_history[m_history.size() - 2];

    // Commanded motion: a nonzero commanded velocity or a change in commanded position
    double dPos = s.posCmd - prev.posCmd;
    bool moving = s.velCmd != 0 || dPos != 0;
    bool started = moving && !m_wasMoving, ended = !moving && m_wasMoving;
    if (moving) {
        double d = s.velCmd != 0 ? s.velCmd : dPos;
        if (d != 0) m_moveDir = d > 0 ? 1 : -1;
    }
    m_wasMoving = moving;

    Mode mode = Mode(m_mode->currentIndex());
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (mode == Auto && !m_forced) {
        if (isVisible() && now - m_lastRedrawMs >= REDRAW_MS) {
            m_lastRedrawMs = now;
            redraw();
        }
        return;
    }
    if (!m_forced && mode == Stop) return;

    if (m_trigMs < 0) {
        m_trigMs = triggerTime(prev, s, started, ended);
        if (m_trigMs >= 0) updateStatus();
    }
    if (m_trigMs < 0) return;

    double pos = m_position->currentData().toDouble();
    double windowMs = m_window->value() * 1000.0;
    double from = m_trigMs - pos * windowMs, to = m_trigMs + (1 - pos) * windowMs;
    if (s.tMs < to) return;

    // Capture complete
    m_capture.clear();
    for (const Snapshot &h : m_history)
        if (h.tMs >= from && h.tMs <= to) m_capture.push_back(h);
    m_captureTrigMs = m_trigMs;
    m_showCapture = true;
    m_trigMs = -1;
    bool stop = m_forced || mode == Single;
    m_forced = false;
    if (stop) setMode(Stop);
    updateStatus();
    redraw();
}

// Time of the trigger between prev and s, or -1 if none.
double PlotTab::triggerTime(const Snapshot &prev, const Snapshot &s, bool started,
                            bool ended) const {
    bool settled = s.moveDone && !prev.moveDone;
    switch (Source(m_source->currentIndex())) {
    case StartPos: return started && m_moveDir > 0 ? s.tMs : -1;
    case StartNeg: return started && m_moveDir < 0 ? s.tMs : -1;
    case StartAny: return started ? s.tMs : -1;
    case EndPos: return ended && m_moveDir > 0 ? s.tMs : -1;
    case EndNeg: return ended && m_moveDir < 0 ? s.tMs : -1;
    case EndAny: return ended ? s.tMs : -1;
    case SettledPos: return settled && m_moveDir > 0 ? s.tMs : -1;
    case SettledNeg: return settled && m_moveDir < 0 ? s.tMs : -1;
    case SettledAny: return settled ? s.tMs : -1;
    case AlertRaised: return s.alertPresent && !prev.alertPresent ? s.tMs : -1;
    case Rising:
    case Falling: {
        auto field = m_traces[m_levelTrace->currentIndex()].field;
        double a = prev.*field, b = s.*field, lvl = m_level->value();
        bool cross = m_source->currentIndex() == Rising ? (a < lvl && b >= lvl)
                                                        : (a > lvl && b <= lvl);
        // Interpolate the crossing time between the two samples
        return cross ? prev.tMs + (lvl - a) / (b - a) * (s.tMs - prev.tMs) : -1;
    }
    }
    return -1;
}

// Drop any pending trigger and wait for a new one.
void PlotTab::rearm() {
    m_trigMs = -1;
    m_forced = false;
    updateStatus();
    redraw();
}

void PlotTab::setMode(Mode m) {
    m_mode->setCurrentIndex(m);  // its signal re-arms
}

void PlotTab::clear() {
    m_history.clear();
    m_capture.clear();
    m_wasMoving = false;
    rearm();
}

void PlotTab::updateStatus() {
    QString text;
    if (m_trigMs >= 0) text = "Triggered";
    else switch (Mode(m_mode->currentIndex())) {
    case Auto: text = "Rolling"; break;
    case Stop: text = "Stopped"; break;
    default: text = "Armed"; break;
    }
    m_status->setText(text);
}

void PlotTab::redraw() {
    m_trigLine->clear();
    if (m_showCapture) {
        double pos = m_position->currentData().toDouble();
        double w = m_window->value();
        plot(m_capture.begin(), m_capture.end(), m_captureTrigMs, -pos * w, (1 - pos) * w);
        m_trigLine->replace({{0, m_yAxis->min()}, {0, m_yAxis->max()}});
        return;
    }
    if (m_history.empty()) {
        plot(m_history.begin(), m_history.end(), 0, 0, m_window->value());
        return;
    }
    // Rolling view of the last window, ending at the newest sample (or where Stop froze it)
    auto end = m_history.end();
    if (m_mode->currentIndex() == Stop)
        end = std::upper_bound(m_history.begin(), m_history.end(), m_stopMs,
                               [](double t, const Snapshot &h) { return t < h.tMs; });
    if (end == m_history.begin()) end = m_history.begin() + 1;
    const double t0 = m_history.front().tMs;
    const double tEnd = (std::prev(end)->tMs - t0) / 1000.0;
    const double tStart = std::max(0.0, tEnd - m_window->value());
    auto begin = std::lower_bound(m_history.begin(), end, t0 + tStart * 1000.0,
                                  [](const Snapshot &h, double t) { return h.tMs < t; });
    plot(begin, end, t0, tStart, std::max(tStart + m_window->value(), tEnd));
}

// Plot samples [begin, end) at x = (tMs - t0) seconds over an x range of [xMin, xMax].
template <typename It>
void PlotTab::plot(It begin, It end, double t0, double xMin, double xMax) {
    double yMin = INFINITY, yMax = -INFINITY;
    for (Trace &t : m_traces) {
        QList<QPointF> pts;
        if (t.check->isChecked()) {
            for (It it = begin; it != end; ++it) {
                double y = (*it).*t.field;
                pts.append({(it->tMs - t0) / 1000.0, y});
                yMin = std::min(yMin, y);
                yMax = std::max(yMax, y);
            }
        }
        t.series->replace(pts);
    }
    m_xAxis->setRange(xMin, xMax);
    if (yMin > yMax) yMin = -1, yMax = 1;
    double pad = std::max((yMax - yMin) * 0.05, 1.0);
    m_yAxis->setRange(yMin - pad, yMax + pad);
}

void PlotTab::exportCsv() {
    if (m_history.empty()) {
        QMessageBox::information(this, "Export", "No data recorded yet.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, "Export CSV", "cpview-trace.csv", "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export", "Cannot write " + path);
        return;
    }
    QTextStream out(&f);
    out << "time_ms";
    for (const Trace &t : m_traces) out << ',' << QString(t.name).replace(' ', '_');
    out << ",bus_v,temp_c,rms\n";
    for (const Snapshot &s : m_history) {
        out << QString::number(s.tMs, 'f', 1);
        for (const Trace &t : m_traces) out << ',' << s.*t.field;
        out << ',' << s.busV << ',' << s.tempC << ',' << s.rms << '\n';
    }
}
