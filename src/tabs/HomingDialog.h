#pragma once

#include <QDialog>

#include "MotorWorker.h"

class QDoubleSpinBox;
class QLabel;
class QRadioButton;

// ClearView-style Homing Setup dialog. Reads the node's settings when opened
// and writes them on OK.
class HomingDialog : public QDialog {
    Q_OBJECT
public:
    explicit HomingDialog(MotorWorker *worker, QWidget *parent = nullptr);

private:
    void showSettings(const HomingSettings &h);
    void updateUi();

    MotorWorker *m_worker;
    HomingSettings m_h;
    QRadioButton *m_ccw, *m_cw, *m_toInput, *m_toHardStop, *m_offsReverse, *m_offsSame;
    QDoubleSpinBox *m_vel, *m_acc, *m_trq, *m_offset;
    QRadioButton *m_inAOn, *m_inAOff, *m_inBOn, *m_inBOff;
    QLabel *m_offsDirLabel, *m_note;
};
