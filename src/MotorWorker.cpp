#include "MotorWorker.h"

#include <QElapsedTimer>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "pubSysCls.h"
#include "pubCpmAdvAPI.h"

using namespace sFnd;

static const int POLL_MS = 50;  // poll period; one poll takes ~25 ms on the wire
static const int MAX_POLL_FAILURES = 3;

MotorWorker::MotorWorker(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);  // child: moves to the worker thread with us
    m_timer->setSingleShot(true);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &MotorWorker::poll);
}

MotorWorker::~MotorWorker() {
    if (m_portCount > 0) {
        try { m_mgr->PortsClose(); } catch (mnErr &) {}
    }
}

// Run fn, converting sFoundation exceptions into error() signals.
template <typename F> bool MotorWorker::guarded(const char *what, F &&fn) {
    try {
        fn();
        return true;
    } catch (mnErr &e) {
        emit error(QString("%1 failed: %2 (err 0x%3, addr %4)")
                       .arg(what)
                       .arg(QString::fromLatin1(e.ErrorMsg).trimmed())
                       .arg(unsigned(e.ErrorCode), 8, 16, QChar('0'))
                       .arg(e.TheAddr));
        return false;
    }
}

void MotorWorker::scanPorts() {
    guarded("Scan", [&] {
        std::vector<std::string> hubs;
        SysManager::FindComHubPorts(hubs);
        QStringList paths;
        for (auto &h : hubs) paths << QString::fromStdString(h);
        emit log(paths.isEmpty() ? "No SC hubs found"
                                 : QString("Found SC hub(s): %1").arg(paths.join(", ")));
        emit portsFound(paths);
    });
}

void MotorWorker::openPorts(const QStringList &paths) {
    if (m_portCount > 0) closePorts();
    if (paths.isEmpty()) {
        emit error("No ports to open");
        return;
    }
    QList<NodeInfo> nodes;
    bool ok = guarded("Open ports", [&] {
        m_mgr = SysManager::Instance();
        int n = 0;
        for (; n < paths.size() && n < NET_CONTROLLER_MAX; n++)
            m_mgr->ComHubPort(n, paths[n].toLocal8Bit().constData());
        m_mgr->PortsOpen(n);
        m_portCount = n;
        for (int p = 0; p < n; p++) {
            IPort &port = m_mgr->Ports(p);
            emit log(QString("Port %1 (%2): state=%3, %4 node(s)")
                         .arg(p).arg(paths[p]).arg(int(port.OpenState())).arg(port.NodeCount()));
            for (unsigned i = 0; i < port.NodeCount(); i++)
                nodes << readInfo(p, int(i));
        }
    });
    if (!ok) {
        // Shut down the library's port threads even when PortsOpen failed;
        // otherwise they crash during static teardown at exit.
        try { m_mgr->PortsClose(); } catch (mnErr &) {}
        m_portCount = 0;
        closePorts();
        emit log("Is a motor powered and connected to the hub, with the hub jumpers set? "
                 "Is another program (ClearView, an SDK example) using the port? "
                 "Is your user in the dialout group?");
        return;
    }
    emit connected(nodes);
}

void MotorWorker::closePorts() {
    m_timer->stop();
    m_node = nullptr;
    if (m_portCount > 0) {
        guarded("Close ports", [&] { m_mgr->PortsClose(); });
        m_portCount = 0;
        emit log("Ports closed");
    }
    emit disconnected();
}

NodeInfo MotorWorker::readInfo(int port, int node) {
    INode &n = m_mgr->Ports(port).Nodes(node);
    NodeInfo info;
    info.port = port;
    info.node = node;
    info.model = n.Info.Model.Value();
    info.serial = QString::number(n.Info.SerialNumber.Value());
    info.firmware = n.Info.FirmwareVersion.Value();
    info.hardware = n.Info.HardwareVersion.Value();
    info.userId = n.Info.UserID.Value();
    info.userDesc = n.Info.UserDesc.Value();
    info.resolution = n.Info.PositioningResolution.Value();
    switch (n.Info.NodeType()) {
    case IInfo::CLEARPATH_SC: info.type = "ClearPath-SC"; break;
    case IInfo::CLEARPATH_SC_ADV: info.type = "ClearPath-SC Advanced"; break;
    default: info.type = "Unknown"; break;
    }
    return info;
}

void MotorWorker::selectNode(int port, int node) {
    m_timer->stop();
    m_node = nullptr;
    bool ok = guarded("Select node", [&] {
        INode &n = m_mgr->Ports(port).Nodes(node);
        if (!n.Setup.AccessLevelIsFull())
            emit log("Warning: no full access to node (ClearView connected to its USB port?)");
        n.VelUnit(INode::RPM);
        n.AccUnit(INode::RPM_PER_SEC);
        n.TrqUnit(INode::PCT_MAX);
        m_info = readInfo(port, node);
        m_node = &n;
    });
    if (!ok) return;
    emit nodeSelected(m_info);
    emitLimits();
    m_pollFailures = 0;
    m_lastAlerts.clear();
    m_timer->start(0);
}

void MotorWorker::poll() {
    if (!m_node) return;
    QElapsedTimer pollTime;
    pollTime.start();
    Snapshot s;
    bool ok = guarded("Poll", [&] {
        INode &n = *m_node;
        s.tMs = m_mgr->TimeStampMsec();
        n.Status.RT.Refresh();
        n.Status.Alerts.Refresh();
        n.Status.Warnings.Refresh();
        n.Motion.PosnMeasured.Refresh();
        n.Motion.PosnCommanded.Refresh();
        n.Motion.PosnTracking.Refresh();
        n.Motion.VelMeasured.Refresh();
        n.Motion.VelCommanded.Refresh();
        n.Motion.TrqMeasured.Refresh();
        n.Motion.TrqCommanded.Refresh();
        n.Status.RMSlevel.Refresh();
        n.Status.Temperature.Refresh();

        mnStatusReg rt = n.Status.RT.Value();
        const auto &c = rt.cpm;
        s.ready = c.Ready;
        s.enabled = c.Enabled;
        s.moveDone = c.MoveDone;
        s.inMotion = c.InMotion != 0;
        s.alertPresent = c.AlertPresent;
        s.warning = c.Warning;
        s.disabled = c.Disabled;
        s.homing = c.Homing;
        s.wasHomed = c.WasHomed;
        s.inPosLimit = c.InPosLimit;
        s.inNegLimit = c.InNegLimit;
        s.motionBlocked = c.MotionBlocked;
        s.atTargetVel = c.AtTargetVel;
        s.inA = c.InA;
        s.inB = c.InB;

        char buf[1024];
        s.statusStr = rt.StateStr(buf, sizeof(buf));
        s.alertStr = n.Status.Alerts.Value().StateStr(buf, sizeof(buf));
        s.warnStr = n.Status.Warnings.Value().StateStr(buf, sizeof(buf));

        s.posMeas = n.Motion.PosnMeasured;
        s.posCmd = n.Motion.PosnCommanded;
        s.posTrk = n.Motion.PosnTracking;
        s.velMeas = n.Motion.VelMeasured;
        s.velCmd = n.Motion.VelCommanded;
        s.trqMeas = n.Motion.TrqMeasured;
        s.trqCmd = n.Motion.TrqCommanded;
        s.rms = n.Status.RMSlevel;
        s.tempC = n.Status.Temperature;
        s.busV = n.Info.Ex.Parameter(CPM_P_DRV_BUS_VOLTS);
    });
    if (ok) {
        m_pollFailures = 0;
        // StateStr is a hex line followed by one "[bit] Name" line per set bit
        QStringList names;
        for (const QString &line : s.alertStr.split('\n'))
            if (line.startsWith('[')) names << line.section(' ', 1).trimmed();
        QString alerts = names.join(", ");
        if (alerts != m_lastAlerts && !alerts.isEmpty())
            emit error(QString("Node alert: %1").arg(alerts));
        m_lastAlerts = alerts;
        emit snapshot(s);
    } else if (++m_pollFailures >= MAX_POLL_FAILURES) {
        emit error("Lost communication with node; closing ports");
        closePorts();
        return;
    }
    // Fixed period: subtract the time this poll spent on the wire.
    m_timer->start(std::max<qint64>(0, POLL_MS - pollTime.elapsed()));
}

void MotorWorker::enable(bool on) {
    if (!m_node) return;
    guarded(on ? "Enable" : "Disable", [&] {
        if (on) {
            m_node->Status.AlertsClear();
            m_node->Motion.NodeStopClear();
        }
        m_node->EnableReq(on);
        emit log(on ? "Enable requested" : "Disable requested");
    });
}

void MotorWorker::clearAlerts() {
    if (!m_node) return;
    guarded("Clear alerts", [&] {
        m_node->Status.AlertsClear();
        m_node->Motion.NodeStopClear();
        emit log("Alerts and node stops cleared");
    });
}

void MotorWorker::eStop() {
    if (m_portCount == 0) return;
    guarded("E-Stop", [&] {
        for (int p = 0; p < m_portCount; p++)
            m_mgr->Ports(p).NodeStop(STOP_TYPE_ESTOP_ABRUPT);
        emit log("E-STOP issued to all nodes. Use Clear Alerts to resume.");
    });
}

void MotorWorker::stopMotion() {
    if (!m_node) return;
    guarded("Stop", [&] { m_node->Motion.NodeStop(STOP_TYPE_RAMP_AT_DECEL); });
}

void MotorWorker::moveVel(double vel) {
    if (!m_node) return;
    guarded("Velocity move", [&] { m_node->Motion.MoveVelStart(vel); });
}

void MotorWorker::movePosn(int target, bool absolute, bool dwell, bool repeat,
                           bool reciprocate) {
    if (!m_node) return;
    guarded("Position move", [&] {
        m_node->Motion.MovePosnStart(target, absolute, dwell, repeat, reciprocate);
        emit log(QString("Position move to %1 (%2)").arg(target).arg(absolute ? "abs" : "rel"));
    });
}

void MotorWorker::startHoming() {
    if (!m_node) return;
    guarded("Homing", [&] {
        if (!m_node->Motion.Homing.HomingValid()) {
            emit error("Homing is not configured for this node (set it up in the .mtr config)");
            return;
        }
        m_node->Motion.Homing.Initiate();
        emit log("Homing started");
    });
}

// Velocity/accel parameters are in counts/s and counts/s^2; the hard-stop
// torque limit is in amps (scaled to % of CPM_P_DRV_I_MAX, as ClearView does).
void MotorWorker::readHoming() {
    if (!m_node || m_info.resolution == 0) return;
    HomingSettings h;
    bool ok = guarded("Read homing setup", [&] {
        INode &n = *m_node;
        IInfoEx &ex = n.Info.Ex;
        double rpmPerCps = 60.0 / m_info.resolution;
        double iMax = ex.Parameter(nodeparam(CPM_P_DRV_I_MAX));
        mnAppConfigReg app = n.Setup.Ex.App.Value();
        mnHwConfigReg hw = n.Setup.Ex.HW.Value();
        h.cw = app.cpm.HomingDirection;
        h.offsetSameDir = app.cpm.OffsetMoveInHomeDir;
        h.hardStop = hw.cpm.HomingStyle == _cpmHwConfigFlds::HOME_TO_HARDSTOP;
        h.extended = hw.cpm.HomingExtended != _cpmHwConfigFlds::USE_LEGACY;
        h.velRpm = std::abs(ex.Parameter(nodeparam(CPM_P_APP_HOMING_VEL))) * rpmPerCps;
        h.accRpmS = ex.Parameter(nodeparam(CPM_P_APP_HOMING_ACCEL)) * rpmPerCps;
        h.offsetCnts = ex.Parameter(nodeparam(CPM_P_APP_HOMING_OFFSET));
        h.hardStopTrqPct = iMax > 0 ? 100.0 * ex.Parameter(nodeparam(CPM_P_DRV_HARDSTOP_TRQ_LIM)) / iMax : 0;
        h.inputBits = unsigned(ex.Parameter(nodeparam(CPM_P_XPS_FEAT_AT_HOME))) & 0xffff;
    });
    if (ok) emit homingRead(h);
}

void MotorWorker::writeHoming(const HomingSettings &h) {
    if (!m_node || m_info.resolution == 0) return;
    guarded("Write homing setup", [&] {
        INode &n = *m_node;
        IInfoEx &ex = n.Info.Ex;
        double cpsPerRpm = m_info.resolution / 60.0;
        double iMax = ex.Parameter(nodeparam(CPM_P_DRV_I_MAX));
        ex.Parameter(nodeparam(CPM_P_APP_HOMING_VEL), h.velRpm * cpsPerRpm);
        ex.Parameter(nodeparam(CPM_P_APP_HOMING_ACCEL), h.accRpmS * cpsPerRpm);
        ex.Parameter(nodeparam(CPM_P_APP_HOMING_OFFSET), std::round(h.offsetCnts));
        if (iMax > 0)
            ex.Parameter(nodeparam(CPM_P_DRV_HARDSTOP_TRQ_LIM), h.hardStopTrqPct / 100.0 * iMax);
        // No home input when homing to a hard stop
        ex.Parameter(nodeparam(CPM_P_XPS_FEAT_AT_HOME), h.hardStop ? 0 : h.inputBits);

        mnAppConfigReg dir{}, offs{};
        dir.cpm.HomingDirection = 1;
        offs.cpm.OffsetMoveInHomeDir = 1;
        h.cw ? n.Setup.Ex.App.Set(dir) : n.Setup.Ex.App.Clear(dir);
        h.offsetSameDir ? n.Setup.Ex.App.Set(offs) : n.Setup.Ex.App.Clear(offs);

        mnHwConfigReg style{};
        style.cpm.HomingStyle = _cpmHwConfigFlds::HOME_TO_HARDSTOP;
        h.hardStop ? n.Setup.Ex.HW.Set(style) : n.Setup.Ex.HW.Clear(style);
        emit log("Homing setup written");
    });
    readHoming();
}

void MotorWorker::emitLimits() {
    if (!m_node) return;
    MotorLimits l;
    bool ok = guarded("Read limits", [&] {
        INode &n = *m_node;
        l.velLimit = n.Motion.VelLimit;
        l.accLimit = n.Motion.AccLimit;
        l.jrkLimit = n.Motion.JrkLimit;
        l.dwellMs = n.Motion.DwellMs;
        l.trqGlobal = n.Limits.TrqGlobal;
        l.softLimit1 = n.Limits.SoftLimit1;
        l.softLimit2 = n.Limits.SoftLimit2;
        l.posnTrackingLimit = n.Limits.PosnTrackingLimit;
    });
    if (ok) emit limitsRead(l);
}

void MotorWorker::setLimits(const MotorLimits &l) {
    if (!m_node) return;
    guarded("Write limits", [&] {
        INode &n = *m_node;
        n.Motion.VelLimit.Value(l.velLimit);
        n.Motion.AccLimit.Value(l.accLimit);
        n.Motion.JrkLimit.Value(uint32_t(std::lround(l.jrkLimit)));
        n.Motion.DwellMs.Value(uint32_t(std::lround(l.dwellMs)));
        n.Limits.TrqGlobal.Value(l.trqGlobal);
        n.Limits.SoftLimit1.Value(int32_t(std::lround(l.softLimit1)));
        n.Limits.SoftLimit2.Value(int32_t(std::lround(l.softLimit2)));
        n.Limits.PosnTrackingLimit.Value(uint32_t(std::lround(l.posnTrackingLimit)));
        emit log("Limits written");
    });
    emitLimits();
}

void MotorWorker::setUnits(int velUnit, int accUnit, int trqUnit) {
    if (!m_node) return;
    guarded("Set units", [&] {
        m_node->VelUnit(INode::_velUnits(velUnit));
        m_node->AccUnit(INode::_accUnits(accUnit));
        m_node->TrqUnit(INode::_trqUnits(trqUnit));
    });
    emitLimits();
}

void MotorWorker::readParams(const QList<int> &ids) {
    if (!m_node) return;
    for (int id : ids) {
        try {
            emit paramValue(id, m_node->Info.Ex.Parameter(nodeparam(id)), true);
        } catch (mnErr &) {
            emit paramValue(id, 0, false);  // parameter not present on this node
        }
    }
}

void MotorWorker::writeParam(int id, double value) {
    if (!m_node) return;
    guarded("Write parameter", [&] {
        m_node->Info.Ex.Parameter(nodeparam(id), value);
        emit log(QString("Parameter %1 set to %2").arg(id).arg(value));
    });
    readParams({id});
}

void MotorWorker::configLoad(const QString &path) {
    if (!m_node) return;
    bool full = false;
    if (!guarded("Config load", [&] { full = m_node->Setup.AccessLevelIsFull(); })) return;
    if (!full) {
        emit error("Config load needs full access; close ClearView or set it to Monitor mode");
        return;
    }
    bool ok = guarded("Config load", [&] {
        m_node->EnableReq(false);
        m_node->Setup.ConfigLoad(path.toLocal8Bit().constData());
        emit log(QString("Loaded config %1").arg(path));
    });
    if (!ok) return;
    m_info = readInfo(m_info.port, m_info.node);
    emit nodeSelected(m_info);
    emitLimits();
}

void MotorWorker::configSave(const QString &path) {
    if (!m_node) return;
    guarded("Config save", [&] {
        m_node->Setup.ConfigSave(path.toLocal8Bit().constData());
        emit log(QString("Saved config %1").arg(path));
    });
}

void MotorWorker::factoryDefaults() {
    if (!m_node) return;
    guarded("Factory defaults", [&] {
        m_node->EnableReq(false);
        cnErrCode err = cpmFactoryDefaults(m_node->Info.Ex.Addr());
        if (err != MN_OK)
            emit error(QString("Factory defaults failed (err 0x%1)").arg(unsigned(err), 8, 16, QChar('0')));
        else
            emit log("Factory defaults restored");
    });
    emitLimits();
}

void MotorWorker::setUserInfo(const QString &userId, const QString &userDesc) {
    if (!m_node) return;
    guarded("Set user info", [&] {
        m_node->Info.UserID.Value(userId.toLocal8Bit().constData());
        m_node->Info.UserDesc.Value(userDesc.toLocal8Bit().constData());
        m_info.userId = userId;
        m_info.userDesc = userDesc;
        emit log("User ID/description written");
    });
    emit nodeSelected(m_info);
}
