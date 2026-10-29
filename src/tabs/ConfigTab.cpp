#include "tabs/ConfigTab.h"

#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "tabs/HomingDialog.h"

namespace {
struct ParamDef { int id; const char *name; const char *desc; };
const ParamDef PARAMS[] = {
#include "CpmParams.inc"
};
enum { COL_ID, COL_NAME, COL_DESC, COL_VALUE };
}

static QDoubleSpinBox *makeSpin(double min, double max, int decimals) {
    auto *s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setDecimals(decimals);
    return s;
}

ConfigTab::ConfigTab(MotorWorker *worker, QWidget *parent) : QWidget(parent), m_worker(worker) {
    MotorWorker *w = worker;

    // Config file
    auto *fileBox = new QGroupBox("Configuration file (.mtr)");
    auto *fileLay = new QHBoxLayout(fileBox);
    auto *loadBtn = new QPushButton("Load config...");
    auto *saveBtn = new QPushButton("Save config...");
    auto *factBtn = new QPushButton("Restore factory defaults");
    fileLay->addWidget(loadBtn);
    fileLay->addWidget(saveBtn);
    fileLay->addStretch();
    fileLay->addWidget(factBtn);
    connect(loadBtn, &QPushButton::clicked, this, [=] {
        QString p = QFileDialog::getOpenFileName(this, "Load motor config", QString(), "Motor config (*.mtr);;All files (*)");
        if (p.isEmpty()) return;
        if (QMessageBox::question(this, "Load config", "The node will be disabled and its configuration "
                                  "replaced from:\n" + p + "\n\nContinue?") != QMessageBox::Yes)
            return;
        onWorker(w, [w, p] { w->configLoad(p); });
    });
    connect(saveBtn, &QPushButton::clicked, this, [=] {
        QString p = QFileDialog::getSaveFileName(this, "Save motor config", "motor.mtr", "Motor config (*.mtr)");
        if (p.isEmpty()) return;
        if (!p.endsWith(".mtr")) p += ".mtr";
        onWorker(w, [w, p] { w->configSave(p); });
    });
    connect(factBtn, &QPushButton::clicked, this, [=] {
        if (QMessageBox::warning(this, "Factory defaults",
                                 "Restore ALL node parameters to factory defaults?\n"
                                 "Save a config file first if you may want the current settings back.",
                                 QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
            return;
        onWorker(w, [w] { w->factoryDefaults(); });
    });

    // User info
    auto *userBox = new QGroupBox("Identification");
    auto *userForm = new QFormLayout(userBox);
    m_userId = new QLineEdit;
    m_userDesc = new QLineEdit;
    auto *userApply = new QPushButton("Apply");
    userForm->addRow("User ID:", m_userId);
    userForm->addRow("Description:", m_userDesc);
    userForm->addRow(userApply);
    connect(userApply, &QPushButton::clicked, this, [=] {
        QString id = m_userId->text(), desc = m_userDesc->text();
        onWorker(w, [w, id, desc] { w->setUserInfo(id, desc); });
    });

    // Safety limits
    auto *limBox = new QGroupBox("Limits");
    auto *limForm = new QFormLayout(limBox);
    m_trqGlobal = makeSpin(0, 1000, 1);
    m_soft1 = makeSpin(-2e9, 2e9, 0);
    m_soft2 = makeSpin(-2e9, 2e9, 0);
    m_trkLimit = makeSpin(0, 2e9, 0);
    auto *limApply = new QPushButton("Apply");
    limForm->addRow("Global torque limit:", m_trqGlobal);
    limForm->addRow("Software limit 1 (cnts):", m_soft1);
    limForm->addRow("Software limit 2 (cnts):", m_soft2);
    limForm->addRow("Position tracking limit (cnts):", m_trkLimit);
    limForm->addRow(limApply);
    connect(limApply, &QPushButton::clicked, this, [=] {
        MotorLimits l = m_limits;
        l.trqGlobal = m_trqGlobal->value();
        l.softLimit1 = m_soft1->value();
        l.softLimit2 = m_soft2->value();
        l.posnTrackingLimit = m_trkLimit->value();
        onWorker(w, [w, l] { w->setLimits(l); });
    });

    auto *homingBtn = new QPushButton("Homing setup...");
    limForm->addRow(homingBtn);
    connect(homingBtn, &QPushButton::clicked, this, [=] {
        auto *dlg = new HomingDialog(w, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->open();
    });

    // Raw parameter table
    auto *paramBox = new QGroupBox("Parameters (expert: values are written to the node immediately)");
    auto *paramLay = new QVBoxLayout(paramBox);
    auto *readBtn = new QPushButton("Read all");
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({"ID", "Name", "Description", "Value"});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(COL_DESC, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_updating = true;
    for (const ParamDef &p : PARAMS) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        auto *id = new QTableWidgetItem(QString::number(p.id));
        auto *name = new QTableWidgetItem(p.name);
        auto *desc = new QTableWidgetItem(p.desc);
        for (auto *it : {id, name, desc}) it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(r, COL_ID, id);
        m_table->setItem(r, COL_NAME, name);
        m_table->setItem(r, COL_DESC, desc);
        m_table->setItem(r, COL_VALUE, new QTableWidgetItem);
        m_rowForId[p.id] = r;
    }
    m_updating = false;
    m_table->resizeColumnToContents(COL_NAME);
    paramLay->addWidget(readBtn, 0, Qt::AlignLeft);
    paramLay->addWidget(m_table);
    connect(readBtn, &QPushButton::clicked, this, &ConfigTab::readAllParams);
    connect(m_table, &QTableWidget::itemChanged, this, &ConfigTab::paramEdited);

    auto *top = new QHBoxLayout;
    top->addWidget(userBox, 1);
    top->addWidget(limBox, 1);
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(fileBox);
    lay->addLayout(top);
    lay->addWidget(paramBox, 1);

    connect(worker, &MotorWorker::nodeSelected, this, [this](const NodeInfo &n) {
        m_userId->setText(n.userId);
        m_userDesc->setText(n.userDesc);
    });
    connect(worker, &MotorWorker::limitsRead, this, &ConfigTab::showLimits);
    connect(worker, &MotorWorker::paramValue, this, &ConfigTab::showParam);
}

void ConfigTab::readAllParams() {
    QList<int> ids;
    for (const ParamDef &p : PARAMS) ids << p.id;
    onWorker(m_worker, [w = m_worker, ids] { w->readParams(ids); });
}

void ConfigTab::showLimits(const MotorLimits &l) {
    m_limits = l;
    m_trqGlobal->setValue(l.trqGlobal);
    m_soft1->setValue(l.softLimit1);
    m_soft2->setValue(l.softLimit2);
    m_trkLimit->setValue(l.posnTrackingLimit);
}

void ConfigTab::showParam(int id, double value, bool ok) {
    auto it = m_rowForId.find(id);
    if (it == m_rowForId.end()) return;
    m_updating = true;
    QTableWidgetItem *item = m_table->item(*it, COL_VALUE);
    item->setText(ok ? QString::number(value, 'g', 10) : "n/a");
    item->setData(Qt::UserRole, ok ? QVariant(value) : QVariant());
    m_updating = false;
}

void ConfigTab::paramEdited(QTableWidgetItem *item) {
    if (m_updating || item->column() != COL_VALUE) return;
    int row = item->row();
    int id = m_table->item(row, COL_ID)->text().toInt();
    QString name = m_table->item(row, COL_NAME)->text();
    QVariant old = item->data(Qt::UserRole);
    bool numOk = false;
    double v = item->text().toDouble(&numOk);
    auto revert = [&] {
        m_updating = true;
        item->setText(old.isValid() ? QString::number(old.toDouble(), 'g', 10) : QString());
        m_updating = false;
    };
    if (!numOk) {
        revert();
        return;
    }
    if (QMessageBox::question(this, "Write parameter",
                              QString("Write %1 (%2) = %3 to the node?").arg(name).arg(id).arg(v)) != QMessageBox::Yes) {
        revert();
        return;
    }
    onWorker(m_worker, [w = m_worker, id, v] { w->writeParam(id, v); });
}
