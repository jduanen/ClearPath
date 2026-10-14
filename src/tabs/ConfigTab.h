#pragma once

#include <QMap>
#include <QWidget>

#include "MotorWorker.h"

class QDoubleSpinBox;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;

class ConfigTab : public QWidget {
    Q_OBJECT
public:
    explicit ConfigTab(MotorWorker *worker, QWidget *parent = nullptr);

private:
    void showLimits(const MotorLimits &l);
    void showParam(int id, double value, bool ok);
    void paramEdited(QTableWidgetItem *item);
    void readAllParams();

    MotorWorker *m_worker;
    MotorLimits m_limits;
    QLineEdit *m_userId, *m_userDesc;
    QDoubleSpinBox *m_trqGlobal, *m_soft1, *m_soft2, *m_trkLimit;
    QTableWidget *m_table;
    QMap<int, int> m_rowForId;
    bool m_updating = false;  // suppresses itemChanged while we fill the table
};
