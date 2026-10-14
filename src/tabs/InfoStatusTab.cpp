#include "tabs/InfoStatusTab.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

static const char *INFO_FIELDS[] = {"Model", "Type", "Serial", "Firmware", "Hardware",
                                    "Resolution", "User ID", "Description"};
static const char *LED_FIELDS[] = {"Ready", "Enabled", "Disabled", "Move Done", "In Motion",
                                   "At Target Vel", "Homing", "Was Homed", "Alert", "Warning",
                                   "Motion Blocked", "+Limit", "-Limit", "Input A", "Input B"};
static const char *VALUE_FIELDS[] = {"Position (cnts)", "Commanded Pos", "Tracking Err",
                                     "Velocity", "Commanded Vel", "Torque", "RMS Level",
                                     "Bus Voltage (V)", "Temperature (C)"};

static void setLed(QLabel *l, bool on, const char *onColor = "#2e7d32") {
    l->setStyleSheet(QString("QLabel { border-radius: 4px; padding: 3px 8px; color: %1; background: %2; }")
                         .arg(on ? "white" : "palette(text)", on ? onColor : "palette(midlight)"));
}

static QPlainTextEdit *makeText() {
    auto *t = new QPlainTextEdit;
    t->setReadOnly(true);
    t->setMaximumHeight(90);
    return t;
}

InfoStatusTab::InfoStatusTab(MotorWorker *worker, QWidget *parent) : QWidget(parent), m_worker(worker) {
    auto *infoBox = new QGroupBox("Node");
    auto *infoForm = new QFormLayout(infoBox);
    for (const char *f : INFO_FIELDS) {
        auto *v = new QLabel("-");
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        infoForm->addRow(QString(f) + ":", v);
        m_info[f] = v;
    }

    auto *valBox = new QGroupBox("Live values");
    auto *valForm = new QFormLayout(valBox);
    for (const char *f : VALUE_FIELDS) {
        auto *v = new QLabel("-");
        v->setMinimumWidth(100);
        valForm->addRow(QString(f) + ":", v);
        m_values[f] = v;
    }

    auto *ledBox = new QGroupBox("Status");
    auto *ledGrid = new QGridLayout(ledBox);
    int i = 0;
    for (const char *f : LED_FIELDS) {
        auto *l = new QLabel(f);
        l->setAlignment(Qt::AlignCenter);
        setLed(l, false);
        ledGrid->addWidget(l, i / 5, i % 5);
        m_leds[f] = l;
        i++;
    }

    m_alerts = makeText();
    m_warnings = makeText();
    m_status = makeText();
    auto *clearBtn = new QPushButton("Clear Alerts / Node Stops");
    connect(clearBtn, &QPushButton::clicked, this, [this] { onWorker(m_worker, [w = m_worker] { w->clearAlerts(); }); });

    auto *alertBox = new QGroupBox("Alerts and warnings");
    auto *alertLay = new QVBoxLayout(alertBox);
    alertLay->addWidget(new QLabel("Alerts (shutdowns):"));
    alertLay->addWidget(m_alerts);
    alertLay->addWidget(new QLabel("Warnings:"));
    alertLay->addWidget(m_warnings);
    alertLay->addWidget(new QLabel("Status register (set bits):"));
    alertLay->addWidget(m_status);
    alertLay->addWidget(clearBtn, 0, Qt::AlignLeft);

    auto *top = new QHBoxLayout;
    top->addWidget(infoBox, 1);
    top->addWidget(valBox, 1);
    auto *lay = new QVBoxLayout(this);
    lay->addLayout(top);
    lay->addWidget(ledBox);
    lay->addWidget(alertBox, 1);

    connect(worker, &MotorWorker::nodeSelected, this, &InfoStatusTab::showInfo);
    connect(worker, &MotorWorker::snapshot, this, &InfoStatusTab::showSnapshot);
}

void InfoStatusTab::showInfo(const NodeInfo &n) {
    m_info["Model"]->setText(n.model);
    m_info["Type"]->setText(n.type);
    m_info["Serial"]->setText(n.serial);
    m_info["Firmware"]->setText(n.firmware);
    m_info["Hardware"]->setText(n.hardware);
    m_info["Resolution"]->setText(QString("%1 cnts/rev").arg(n.resolution));
    m_info["User ID"]->setText(n.userId);
    m_info["Description"]->setText(n.userDesc);
}

void InfoStatusTab::showSnapshot(const Snapshot &s) {
    setLed(m_leds["Ready"], s.ready);
    setLed(m_leds["Enabled"], s.enabled);
    setLed(m_leds["Disabled"], s.disabled, "#757575");
    setLed(m_leds["Move Done"], s.moveDone);
    setLed(m_leds["In Motion"], s.inMotion, "#1565c0");
    setLed(m_leds["At Target Vel"], s.atTargetVel);
    setLed(m_leds["Homing"], s.homing, "#1565c0");
    setLed(m_leds["Was Homed"], s.wasHomed);
    setLed(m_leds["Alert"], s.alertPresent, "#c62828");
    setLed(m_leds["Warning"], s.warning, "#ef6c00");
    setLed(m_leds["Motion Blocked"], s.motionBlocked, "#ef6c00");
    setLed(m_leds["+Limit"], s.inPosLimit, "#ef6c00");
    setLed(m_leds["-Limit"], s.inNegLimit, "#ef6c00");
    setLed(m_leds["Input A"], s.inA, "#1565c0");
    setLed(m_leds["Input B"], s.inB, "#1565c0");

    m_values["Position (cnts)"]->setText(QString::number(s.posMeas, 'f', 0));
    m_values["Commanded Pos"]->setText(QString::number(s.posCmd, 'f', 0));
    m_values["Tracking Err"]->setText(QString::number(s.posTrk, 'f', 0));
    m_values["Velocity"]->setText(QString::number(s.velMeas, 'f', 1));
    m_values["Commanded Vel"]->setText(QString::number(s.velCmd, 'f', 1));
    m_values["Torque"]->setText(QString::number(s.trqMeas, 'f', 1));
    m_values["RMS Level"]->setText(QString::number(s.rms, 'f', 1));
    m_values["Bus Voltage (V)"]->setText(QString::number(s.busV, 'f', 1));
    m_values["Temperature (C)"]->setText(QString::number(s.tempC, 'f', 1));

    auto update = [](QPlainTextEdit *t, const QString &txt) {
        QString v = txt.trimmed().isEmpty() ? "(none)" : txt.trimmed();
        if (t->toPlainText() != v) t->setPlainText(v);
    };
    update(m_alerts, s.alertStr);
    update(m_warnings, s.warnStr);
    update(m_status, s.statusStr);
}
