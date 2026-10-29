#include "tabs/HomingDialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

static QDoubleSpinBox *makeSpin(double min, double max, int decimals) {
    auto *s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setDecimals(decimals);
    return s;
}

// Put two radio buttons side by side in their own exclusive group.
static QHBoxLayout *radioPair(QWidget *owner, QRadioButton *a, QRadioButton *b) {
    auto *g = new QButtonGroup(owner);
    g->addButton(a);
    g->addButton(b);
    auto *lay = new QHBoxLayout;
    lay->addWidget(a);
    lay->addWidget(b);
    lay->addStretch();
    return lay;
}

HomingDialog::HomingDialog(MotorWorker *worker, QWidget *parent) : QDialog(parent), m_worker(worker) {
    setWindowTitle("Homing Setup");

    auto *dirBox = new QGroupBox("Homing direction");
    m_ccw = new QRadioButton("CCW (+)");
    m_cw = new QRadioButton("CW (-)");
    dirBox->setLayout(radioPair(this, m_ccw, m_cw));

    auto *moveBox = new QGroupBox("Homing move parameters");
    auto *moveForm = new QFormLayout(moveBox);
    m_vel = makeSpin(0.1, 10000, 1);
    m_acc = makeSpin(1, 1e6, 0);
    moveForm->addRow("Max velocity during home moves (RPM):", m_vel);
    moveForm->addRow("Max accel during home moves (RPM/s):", m_acc);

    auto *targetBox = new QGroupBox("Homing target");
    auto *targetLay = new QVBoxLayout(targetBox);
    m_toInput = new QRadioButton("Change of input");
    m_toHardStop = new QRadioButton("Hard stop");
    auto *targetGroup = new QButtonGroup(this);
    targetGroup->addButton(m_toInput);
    targetGroup->addButton(m_toHardStop);
    m_inAOn = new QRadioButton("A input turns on");
    m_inAOff = new QRadioButton("A input turns off");
    m_inBOn = new QRadioButton("B input turns on");
    m_inBOff = new QRadioButton("B input turns off");
    auto *inputGroup = new QButtonGroup(this);
    auto *inputGrid = new QGridLayout;
    inputGrid->setContentsMargins(20, 0, 0, 0);
    int i = 0;
    for (QRadioButton *r : {m_inAOn, m_inBOn, m_inAOff, m_inBOff}) {
        inputGroup->addButton(r);
        inputGrid->addWidget(r, i / 2, i % 2);
        i++;
    }
    m_trq = makeSpin(0, 100, 1);
    auto *trqForm = new QFormLayout;
    trqForm->setContentsMargins(20, 0, 0, 0);
    trqForm->addRow("Homing torque limit (% of max):", m_trq);
    targetLay->addWidget(m_toInput);
    targetLay->addLayout(inputGrid);
    targetLay->addWidget(m_toHardStop);
    targetLay->addLayout(trqForm);

    auto *offsBox = new QGroupBox("Offset move");
    auto *offsLay = new QVBoxLayout(offsBox);
    m_offset = makeSpin(0, 2e9, 0);
    m_offsDirLabel = new QLabel;
    auto *offsRow = new QHBoxLayout;
    offsRow->addWidget(new QLabel("Distance from homing target (cnts):"));
    offsRow->addWidget(m_offset);
    offsRow->addWidget(m_offsDirLabel);
    offsRow->addStretch();
    m_offsReverse = new QRadioButton("Reverse of home");
    m_offsSame = new QRadioButton("Same as home");
    offsLay->addLayout(offsRow);
    offsLay->addWidget(new QLabel("When homing is complete (after the offset move), the motor position will be 0."));
    offsLay->addLayout(radioPair(this, m_offsReverse, m_offsSame));

    m_note = new QLabel;
    m_note->setWordWrap(true);
    m_note->setStyleSheet("color: #c62828;");

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *lay = new QVBoxLayout(this);
    for (QWidget *wd : std::initializer_list<QWidget *>{dirBox, moveBox, targetBox, offsBox, m_note})
        lay->addWidget(wd);
    lay->addWidget(buttons);

    for (QRadioButton *r : {m_ccw, m_cw, m_toInput, m_toHardStop, m_offsReverse, m_offsSame,
                            m_inAOn, m_inAOff, m_inBOn, m_inBOff})
        connect(r, &QRadioButton::toggled, this, &HomingDialog::updateUi);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        HomingSettings h = m_h;
        h.cw = m_cw->isChecked();
        h.velRpm = m_vel->value();
        h.accRpmS = m_acc->value();
        h.hardStop = m_toHardStop->isChecked();
        h.inputBits = m_inAOn->isChecked() ? HOME_IN_A_ON : m_inAOff->isChecked() ? HOME_IN_A_OFF
                    : m_inBOn->isChecked() ? HOME_IN_B_ON : m_inBOff->isChecked() ? HOME_IN_B_OFF : 0;
        if (!h.hardStop && !h.inputBits) return;  // updateUi shows why
        h.hardStopTrqPct = m_trq->value();
        h.offsetCnts = m_offset->value();
        h.offsetSameDir = m_offsSame->isChecked();
        onWorker(m_worker, [w = m_worker, h] { w->writeHoming(h); });
        accept();
    });
    connect(worker, &MotorWorker::homingRead, this, &HomingDialog::showSettings);

    setEnabled(false);  // until the node's settings arrive
    onWorker(worker, [w = worker] { w->readHoming(); });
}

void HomingDialog::showSettings(const HomingSettings &h) {
    m_h = h;
    (h.cw ? m_cw : m_ccw)->setChecked(true);
    m_vel->setValue(h.velRpm);
    m_acc->setValue(h.accRpmS);
    (h.hardStop ? m_toHardStop : m_toInput)->setChecked(true);
    m_trq->setValue(h.hardStopTrqPct);
    for (auto [r, bits] : {std::pair{m_inAOn, HOME_IN_A_ON}, {m_inAOff, HOME_IN_A_OFF},
                           {m_inBOn, HOME_IN_B_ON}, {m_inBOff, HOME_IN_B_OFF}})
        r->setChecked(h.inputBits == bits);
    m_offset->setValue(h.offsetCnts);
    (h.offsetSameDir ? m_offsSame : m_offsReverse)->setChecked(true);
    setEnabled(true);
    updateUi();
}

void HomingDialog::updateUi() {
    m_trq->setEnabled(m_toHardStop->isChecked());
    for (QRadioButton *r : {m_inAOn, m_inAOff, m_inBOn, m_inBOff}) r->setEnabled(m_toInput->isChecked());
    bool offsetCw = m_cw->isChecked() == m_offsSame->isChecked();
    m_offsDirLabel->setText(offsetCw ? "CW (-)" : "CCW (+)");

    QStringList notes;
    bool inputChosen = m_inAOn->isChecked() || m_inAOff->isChecked() || m_inBOn->isChecked() || m_inBOff->isChecked();
    if (m_toInput->isChecked() && !inputChosen)
        notes << QString(m_h.inputBits ? "The node's home input setting (0x%1) is not one of these; choose one."
                                       : "Choose the input that ends homing.").arg(m_h.inputBits, 4, 16, QChar('0'));
    if (m_h.extended)
        notes << "This node uses an extended homing mode (e.g. home to shaft position) that this dialog does not show.";
    m_note->setText(notes.join("\n"));
    m_note->setVisible(!notes.isEmpty());
}
