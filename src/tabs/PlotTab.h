#pragma once

#include <QVector>
#include <QWidget>
#include <deque>
#include <vector>

#include "MotorWorker.h"

class QCheckBox;
class QChart;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineSeries;
class QValueAxis;

// Rolling plot of polled data, with ClearView-style software triggering.
class PlotTab : public QWidget {
    Q_OBJECT
public:
    explicit PlotTab(MotorWorker *worker, QWidget *parent = nullptr);

private:
    enum Mode { Auto, Normal, Single, Stop };
    enum Source {
        StartPos, StartNeg, StartAny, EndPos, EndNeg, EndAny,
        SettledPos, SettledNeg, SettledAny, AlertRaised, Rising, Falling
    };

    void addSnapshot(const Snapshot &s);
    double triggerTime(const Snapshot &prev, const Snapshot &s, bool started, bool ended) const;
    void rearm();
    void setMode(Mode m);
    void clear();
    void updateStatus();
    void redraw();
    template <typename It> void plot(It begin, It end, double t0, double xMin, double xMax);
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
    QLineSeries *m_trigLine;
    QDoubleSpinBox *m_window;
    QComboBox *m_mode, *m_source, *m_levelTrace, *m_position;
    QDoubleSpinBox *m_level;
    QLabel *m_status;
    qint64 m_lastRedrawMs = 0;

    // Trigger state
    double m_trigMs = -1;           // time of a pending trigger, or -1 while armed
    bool m_forced = false;          // the pending trigger came from Force
    bool m_wasMoving = false;       // commanded motion in the previous sample
    int m_moveDir = 0;              // direction of the latest commanded motion
    std::vector<Snapshot> m_capture;  // last completed capture
    double m_captureTrigMs = 0;
    bool m_showCapture = false;     // display shows m_capture rather than the rolling view
    double m_stopMs = 0;            // end of the rolling view when stopped from Auto
};
