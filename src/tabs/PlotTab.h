#pragma once

#include <QVector>
#include <QWidget>
#include <deque>

#include "MotorWorker.h"

class QCheckBox;
class QChart;
class QLineSeries;
class QSpinBox;
class QValueAxis;

class PlotTab : public QWidget {
    Q_OBJECT
public:
    explicit PlotTab(MotorWorker *worker, QWidget *parent = nullptr);

private:
    void addSnapshot(const Snapshot &s);
    void redraw();
    void exportCsv();

    struct Trace {
        const char *name;
        double Snapshot::*field;
        QCheckBox *check = nullptr;
        QLineSeries *series = nullptr;
    };
    QVector<Trace> m_traces;
    std::deque<Snapshot> m_history;
    QChart *m_chart;
    QValueAxis *m_xAxis, *m_yAxis;
    QSpinBox *m_window;
    QCheckBox *m_pause;
    qint64 m_lastRedrawMs = 0;
};
