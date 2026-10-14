#include "tabs/MotionTab.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

static QDoubleSpinBox *makeSpin(double min, double max, double val, int decimals = 1) {
    auto *s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setDecimals(decimals);
    s->setValue(val);
    s->setKeyboardTracking(false);
    return s;
}

MotionTab::MotionTab(MotorWorker *worker, QWidget *parent) : QWidget(parent), m_worker(worker) {
    auto call = [this](auto f) { onWorker(m_worker, f); };
    MotorWorker *w = worker;

    // Units (combo index == sFoundation enum value)
    auto *unitBox = new QGroupBox("Units");
    auto *unitLay = new QHBoxLayout(unitBox);
    m_velUnit = new QComboBox;
    m_velUnit->addItems({"RPM", "counts/s"});
    m_accUnit = new QComboBox;
    m_accUnit->addItems({"RPM/s", "counts/s²"});
    m_trqUnit = new QComboBox;
    m_trqUnit->addItems({"% of max", "Amps"});
    unitLay->addWidget(new QLabel("Velocity:"));
    unitLay->addWidget(m_velUnit);
    unitLay->addWidget(new QLabel("Accel:"));
    unitLay->addWidget(m_accUnit);
    unitLay->addWidget(new QLabel("Torque:"));
    unitLay->addWidget(m_trqUnit);
    unitLay->addStretch();
    for (QComboBox *c : {m_velUnit, m_accUnit, m_trqUnit})
        connect(c, &QComboBox::activated, this, &MotionTab::sendUnits);

    // Jog: velocity while held, ramp stop on release
    auto *jogBox = new QGroupBox("Jog (hold button)");
    auto *jogLay = new QHBoxLayout(jogBox);
    m_jogVel = makeSpin(0, 10000, 60);
    auto *jogNeg = new QPushButton("◀ Jog -");
    auto *jogPos = new QPushButton("Jog + ▶");
    jogLay->addWidget(new QLabel("Speed:"));
    jogLay->addWidget(m_jogVel);
    jogLay->addWidget(jogNeg);
    jogLay->addWidget(jogPos);
    jogLay->addStretch();
    connect(jogNeg, &QPushButton::pressed, this, [=] { double v = -m_jogVel->value(); call([w, v] { w->moveVel(v); }); });
    connect(jogPos, &QPushButton::pressed, this, [=] { double v = m_jogVel->value(); call([w, v] { w->moveVel(v); }); });
    for (QPushButton *b : {jogNeg, jogPos})
        connect(b, &QPushButton::released, this, [=] { call([w] { w->stopMotion(); }); });

    // Velocity move
    auto *velBox = new QGroupBox("Velocity move");
    auto *velLay = new QHBoxLayout(velBox);
    m_vel = makeSpin(-10000, 10000, 100);
    auto *velGo = new QPushButton("Start");
    auto *velStop = new QPushButton("Stop");
    velLay->addWidget(new QLabel("Target:"));
    velLay->addWidget(m_vel);
    velLay->addWidget(velGo);
    velLay->addWidget(velStop);
    velLay->addStretch();
    connect(velGo, &QPushButton::clicked, this, [=] { double v = m_vel->value(); call([w, v] { w->moveVel(v); }); });
    connect(velStop, &QPushButton::clicked, this, [=] { call([w] { w->stopMotion(); }); });

    // Position move
    auto *posBox = new QGroupBox("Position move");
    auto *posLay = new QGridLayout(posBox);
    m_target = new QSpinBox;
    m_target->setRange(-2000000000, 2000000000);
    m_target->setValue(800);  // one rev at standard resolution
    m_target->setSuffix(" cnts");
    m_absolute = new QCheckBox("Absolute");
    m_dwell = new QCheckBox("Dwell after move");
    m_repeat = new QCheckBox("Repeat");
    m_reciprocate = new QCheckBox("Back and forth");
    auto *posGo = new QPushButton("Go");
    auto *posStop = new QPushButton("Stop");
    posLay->addWidget(new QLabel("Target:"), 0, 0);
    posLay->addWidget(m_target, 0, 1);
    posLay->addWidget(posGo, 0, 2);
    posLay->addWidget(posStop, 0, 3);
    posLay->addWidget(m_absolute, 1, 0);
    posLay->addWidget(m_dwell, 1, 1);
    posLay->addWidget(m_repeat, 1, 2);
    posLay->addWidget(m_reciprocate, 1, 3);
    connect(posGo, &QPushButton::clicked, this, [=] {
        int t = m_target->value();
        bool abs = m_absolute->isChecked(), dw = m_dwell->isChecked(),
             rep = m_repeat->isChecked(), rec = m_reciprocate->isChecked();
        call([w, t, abs, dw, rep, rec] { w->movePosn(t, abs, dw, rep, rec); });
    });
    connect(posStop, &QPushButton::clicked, this, [=] { call([w] { w->stopMotion(); }); });

    // Homing
    auto *homeBox = new QGroupBox("Homing");
    auto *homeLay = new QHBoxLayout(homeBox);
    auto *homeGo = new QPushButton("Start Homing");
    m_homingState = new QLabel("-");
    homeLay->addWidget(homeGo);
    homeLay->addWidget(m_homingState);
    homeLay->addStretch();
    connect(homeGo, &QPushButton::clicked, this, [=] { call([w] { w->startHoming(); }); });

    // Motion limits
    auto *limBox = new QGroupBox("Motion limits");
    auto *limForm = new QFormLayout(limBox);
    m_velLimit = makeSpin(0, 1e7, 0);
    m_accLimit = makeSpin(0, 1e9, 0);
    m_jrkLimit = makeSpin(0, 1e6, 0, 0);
    m_dwellMs = makeSpin(0, 1e6, 0, 0);
    auto *limApply = new QPushButton("Apply");
    limForm->addRow("Velocity limit:", m_velLimit);
    limForm->addRow("Acceleration limit:", m_accLimit);
    limForm->addRow("Jerk limit (RAS):", m_jrkLimit);
    limForm->addRow("Dwell (ms):", m_dwellMs);
    limForm->addRow(limApply);
    connect(limApply, &QPushButton::clicked, this, [=] {
        MotorLimits l = m_limits;
        l.velLimit = m_velLimit->value();
        l.accLimit = m_accLimit->value();
        l.jrkLimit = m_jrkLimit->value();
        l.dwellMs = m_dwellMs->value();
        call([w, l] { w->setLimits(l); });
    });

    auto *left = new QVBoxLayout;
    left->addWidget(jogBox);
    left->addWidget(velBox);
    left->addWidget(posBox);
    left->addWidget(homeBox);
    left->addStretch();
    auto *right = new QVBoxLayout;
    right->addWidget(limBox);
    right->addStretch();
    auto *cols = new QHBoxLayout;
    cols->addLayout(left, 3);
    cols->addLayout(right, 2);
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(unitBox);
    lay->addLayout(cols, 1);

    connect(worker, &MotorWorker::limitsRead, this, &MotionTab::showLimits);
    connect(worker, &MotorWorker::nodeSelected, this, [this] {
        // The worker resets units to RPM, RPM/s, % max on node selection
        m_velUnit->setCurrentIndex(0);
        m_accUnit->setCurrentIndex(0);
        m_trqUnit->setCurrentIndex(0);
    });
    connect(worker, &MotorWorker::snapshot, this, [this](const Snapshot &s) {
        m_homingState->setText(s.homing ? "Homing..." : s.wasHomed ? "Homed" : "Not homed");
    });
}

void MotionTab::showLimits(const MotorLimits &l) {
    m_limits = l;
    m_velLimit->setValue(l.velLimit);
    m_accLimit->setValue(l.accLimit);
    m_jrkLimit->setValue(l.jrkLimit);
    m_dwellMs->setValue(l.dwellMs);
}

void MotionTab::sendUnits() {
    int v = m_velUnit->currentIndex(), a = m_accUnit->currentIndex(), t = m_trqUnit->currentIndex();
    onWorker(m_worker, [w = m_worker, v, a, t] { w->setUnits(v, a, t); });
}
