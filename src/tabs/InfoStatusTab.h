#pragma once

#include <QMap>
#include <QWidget>

#include "MotorWorker.h"

class QLabel;
class QPlainTextEdit;

class InfoStatusTab : public QWidget {
    Q_OBJECT
public:
    explicit InfoStatusTab(MotorWorker *worker, QWidget *parent = nullptr);

private:
    void showInfo(const NodeInfo &info);
    void showSnapshot(const Snapshot &s);

    MotorWorker *m_worker;
    QMap<QString, QLabel *> m_info;   // info field -> value label
    QMap<QString, QLabel *> m_leds;   // status bit -> LED label
    QMap<QString, QLabel *> m_values; // live readouts
    QPlainTextEdit *m_alerts, *m_warnings, *m_status;
};
