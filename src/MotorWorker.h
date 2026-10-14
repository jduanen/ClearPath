#pragma once

#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

class QTimer;
namespace sFnd { class SysManager; class INode; }

struct NodeInfo {
    int port = -1;
    int node = -1;
    QString model, serial, firmware, hardware, userId, userDesc, type;
    unsigned resolution = 0;
};

// Motion limits of the selected node, in the node's current units.
struct MotorLimits {
    double velLimit = 0, accLimit = 0, trqGlobal = 0, dwellMs = 0;
    double jrkLimit = 0;
    double softLimit1 = 0, softLimit2 = 0, posnTrackingLimit = 0;
};

// One poll of the selected node.
struct Snapshot {
    double tMs = 0;
    double posMeas = 0, posCmd = 0, posTrk = 0;
    double velMeas = 0, velCmd = 0;
    double trqMeas = 0, trqCmd = 0;
    double rms = 0, tempC = 0, busV = 0;
    bool ready = false, enabled = false, moveDone = false, inMotion = false,
         alertPresent = false, warning = false, disabled = false, homing = false,
         wasHomed = false, inPosLimit = false, inNegLimit = false,
         motionBlocked = false, atTargetVel = false, inA = false, inB = false;
    QString statusStr, alertStr, warnStr;
};

Q_DECLARE_METATYPE(NodeInfo)
Q_DECLARE_METATYPE(MotorLimits)
Q_DECLARE_METATYPE(Snapshot)

// Owns all sFoundation access. Lives on its own QThread; the GUI talks to it
// only through queued signal/slot connections.
class MotorWorker : public QObject {
    Q_OBJECT
public:
    explicit MotorWorker(QObject *parent = nullptr);
    ~MotorWorker() override;

public slots:
    void scanPorts();
    void openPorts(const QStringList &paths);
    void closePorts();
    void selectNode(int port, int node);

    void enable(bool on);
    void clearAlerts();
    void eStop();
    void stopMotion();

    void moveVel(double vel);
    void movePosn(int target, bool absolute, bool dwell, bool repeat, bool reciprocate);
    void startHoming();
    void setLimits(const MotorLimits &lim);
    void setUnits(int velUnit, int accUnit, int trqUnit);

    void readParams(const QList<int> &ids);
    void writeParam(int id, double value);
    void configLoad(const QString &path);
    void configSave(const QString &path);
    void factoryDefaults();
    void setUserInfo(const QString &userId, const QString &userDesc);

signals:
    void log(const QString &msg);
    void error(const QString &msg);
    void portsFound(const QStringList &paths);
    void connected(const QList<NodeInfo> &nodes);
    void disconnected();
    void nodeSelected(const NodeInfo &info);
    void limitsRead(const MotorLimits &lim);
    void snapshot(const Snapshot &s);
    void paramValue(int id, double value, bool ok);

private slots:
    void poll();

private:
    NodeInfo readInfo(int port, int node);
    void emitLimits();
    template <typename F> bool guarded(const char *what, F &&fn);

    sFnd::SysManager *m_mgr = nullptr;
    sFnd::INode *m_node = nullptr;
    NodeInfo m_info;
    int m_portCount = 0;
    int m_pollFailures = 0;
    QString m_lastAlerts;  // alert names from the previous poll, to log changes
    QTimer *m_timer = nullptr;
};

// Run f (typically a lambda calling a MotorWorker slot) on the worker's thread.
template <typename F> void onWorker(MotorWorker *w, F &&f) {
    QMetaObject::invokeMethod(w, std::forward<F>(f), Qt::QueuedConnection);
}
