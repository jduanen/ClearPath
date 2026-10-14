#pragma once

#include <QWidget>

#include "MotorWorker.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

class MotionTab : public QWidget {
    Q_OBJECT
public:
    explicit MotionTab(MotorWorker *worker, QWidget *parent = nullptr);

private:
    void showLimits(const MotorLimits &l);
    void sendUnits();

    MotorWorker *m_worker;
    MotorLimits m_limits;

    QComboBox *m_velUnit, *m_accUnit, *m_trqUnit;
    QDoubleSpinBox *m_jogVel, *m_vel;
    QSpinBox *m_target;
    QCheckBox *m_absolute, *m_dwell, *m_repeat, *m_reciprocate;
    QDoubleSpinBox *m_velLimit, *m_accLimit, *m_jrkLimit, *m_dwellMs;
    QLabel *m_homingState;
};
