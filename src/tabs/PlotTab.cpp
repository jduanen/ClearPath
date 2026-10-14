#include "tabs/PlotTab.h"

#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextStream>
#include <QValueAxis>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

static const size_t MAX_HISTORY = 100000;  // ~80 min at 20 Hz
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
        connect(t.check, &QCheckBox::toggled, this, [this, &t](bool on) {
            t.series->setVisible(on);
            redraw();
        });
    }
    m_traces[3].check->setChecked(true);  // measured + commanded velocity by default
    m_traces[4].check->setChecked(true);
    for (Trace &t : m_traces) t.series->setVisible(t.check->isChecked());
    controls->addStretch();

    auto *bottom = new QHBoxLayout;
    m_window = new QSpinBox;
    m_window->setRange(1, 600);
    m_window->setValue(10);
    m_window->setSuffix(" s");
    m_pause = new QCheckBox("Pause");
    auto *clearBtn = new QPushButton("Clear");
    auto *csvBtn = new QPushButton("Export CSV...");
    bottom->addWidget(new QLabel("Window:"));
    bottom->addWidget(m_window);
    bottom->addWidget(m_pause);
    bottom->addWidget(clearBtn);
    bottom->addStretch();
    bottom->addWidget(new QLabel("Polled at ~20 Hz; not a high-speed scope."));
    bottom->addWidget(csvBtn);
    connect(m_window, &QSpinBox::valueChanged, this, &PlotTab::redraw);
    connect(m_pause, &QCheckBox::toggled, this, &PlotTab::redraw);
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        m_history.clear();
        redraw();
    });
    connect(csvBtn, &QPushButton::clicked, this, &PlotTab::exportCsv);

    auto *view = new QChartView(m_chart);
    view->setRenderHint(QPainter::Antialiasing);
    auto *lay = new QVBoxLayout(this);
    lay->addLayout(controls);
    lay->addWidget(view, 1);
    lay->addLayout(bottom);

    connect(worker, &MotorWorker::snapshot, this, &PlotTab::addSnapshot);
    connect(worker, &MotorWorker::nodeSelected, this, [this] {
        m_history.clear();
        redraw();
    });
}

void PlotTab::addSnapshot(const Snapshot &s) {
    m_history.push_back(s);
    if (m_history.size() > MAX_HISTORY) m_history.pop_front();
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_pause->isChecked() && isVisible() && now - m_lastRedrawMs >= REDRAW_MS) {
        m_lastRedrawMs = now;
        redraw();
    }
}

void PlotTab::redraw() {
    if (m_history.empty()) {
        for (Trace &t : m_traces) t.series->clear();
        return;
    }
    const double t0 = m_history.front().tMs;
    const double tEnd = (m_history.back().tMs - t0) / 1000.0;
    const double tStart = std::max(0.0, tEnd - m_window->value());
    double yMin = INFINITY, yMax = -INFINITY;
    for (Trace &t : m_traces) {
        QList<QPointF> pts;
        if (t.check->isChecked()) {
            for (auto it = m_history.rbegin(); it != m_history.rend(); ++it) {
                double x = (it->tMs - t0) / 1000.0;
                if (x < tStart) break;
                double y = (*it).*t.field;
                pts.prepend({x, y});
                yMin = std::min(yMin, y);
                yMax = std::max(yMax, y);
            }
        }
        t.series->replace(pts);
    }
    m_xAxis->setRange(tStart, std::max(tStart + m_window->value(), tEnd));
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
