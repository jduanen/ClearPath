#include "MainWindow.h"

#include <QAction>
#include <QDateTime>
#include <QDockWidget>
#include <QEvent>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "tabs/ConfigTab.h"
#include "tabs/InfoStatusTab.h"
#include "tabs/MotionTab.h"
#include "tabs/PlotTab.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_worker(new MotorWorker) {
    setWindowTitle("cpview - ClearPath-SC setup");
    resize(1200, 800);

    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();

    // Toolbar
    QToolBar *tb = addToolBar("Main");
    tb->setMovable(false);
    m_scanAct = tb->addAction("Scan");
    m_connectAct = tb->addAction("Connect");
    m_disconnectAct = tb->addAction("Disconnect");
    tb->addSeparator();
    m_enableAct = tb->addAction("Enable");
    m_enableAct->setCheckable(true);
    tb->addSeparator();
    m_estopAct = tb->addAction("E-STOP");
    m_estopAct->setShortcut(Qt::Key_Escape);
    m_estopAct->setToolTip("Abrupt E-stop of all nodes (Esc)");
    if (QWidget *w = tb->widgetForAction(m_estopAct))
        w->setStyleSheet("QToolButton { background: #c62828; color: white; font-weight: bold; padding: 4px 12px; }");
    tb->addSeparator();
    m_plotWinAct = tb->addAction("Plot Window");
    m_plotWinAct->setCheckable(true);
    m_plotWinAct->setToolTip("Show the plot in its own window");

    // Node tree + tabs
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabel("Nodes");
    m_tabs = new QTabWidget;
    m_infoTab = new InfoStatusTab(m_worker);
    m_motionTab = new MotionTab(m_worker);
    m_configTab = new ConfigTab(m_worker);
    m_plotTab = new PlotTab(m_worker);
    m_tabs->addTab(m_infoTab, "Status");
    m_tabs->addTab(m_motionTab, "Motion");
    m_tabs->addTab(m_configTab, "Config");
    m_tabs->addTab(m_plotTab, "Plot");

    auto *split = new QSplitter;
    split->addWidget(m_tree);
    split->addWidget(m_tabs);
    split->setStretchFactor(1, 1);
    split->setSizes({240, 960});
    setCentralWidget(split);

    // Separate plot window; the PlotTab is moved into it while detached
    m_plotWin = new QWidget(this, Qt::Window);
    m_plotWin->setWindowTitle("cpview - Plot");
    m_plotWin->resize(1000, 650);
    new QVBoxLayout(m_plotWin);
    m_plotWin->addAction(m_estopAct);  // Esc works from the plot window too
    m_plotWin->installEventFilter(this);

    // Log dock
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    auto *dock = new QDockWidget("Log");
    dock->setWidget(m_log);
    addDockWidget(Qt::BottomDockWidgetArea, dock);

    // Actions -> worker
    connect(m_scanAct, &QAction::triggered, this, [this] { onWorker(m_worker, [w = m_worker] { w->scanPorts(); }); });
    connect(m_connectAct, &QAction::triggered, this, [this] {
        QStringList ports = m_ports;
        onWorker(m_worker, [w = m_worker, ports] { w->openPorts(ports); });
    });
    connect(m_disconnectAct, &QAction::triggered, this, [this] { onWorker(m_worker, [w = m_worker] { w->closePorts(); }); });
    connect(m_enableAct, &QAction::triggered, this, [this](bool on) {
        onWorker(m_worker, [w = m_worker, on] { w->enable(on); });
    });
    connect(m_estopAct, &QAction::triggered, this, [this] { onWorker(m_worker, [w = m_worker] { w->eStop(); }); });
    connect(m_plotWinAct, &QAction::toggled, this, &MainWindow::setPlotDetached);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (!item || !item->data(0, Qt::UserRole + 1).isValid()) return;
        int port = item->data(0, Qt::UserRole).toInt();
        int node = item->data(0, Qt::UserRole + 1).toInt();
        onWorker(m_worker, [w = m_worker, port, node] { w->selectNode(port, node); });
    });

    // Worker -> UI
    connect(m_worker, &MotorWorker::log, this, [this](const QString &m) { appendLog(m); });
    connect(m_worker, &MotorWorker::error, this, [this](const QString &m) { appendLog(m, true); });
    connect(m_worker, &MotorWorker::portsFound, this, [this](const QStringList &p) {
        m_ports = p;
        m_connectAct->setEnabled(!p.isEmpty());
    });
    connect(m_worker, &MotorWorker::connected, this, [this](const QList<NodeInfo> &nodes) {
        m_tree->clear();
        QTreeWidgetItem *first = nullptr;
        for (int p = 0; p < m_ports.size(); p++) {
            auto *portItem = new QTreeWidgetItem(m_tree, {QString("Port %1: %2").arg(p).arg(m_ports[p])});
            portItem->setExpanded(true);
            for (const NodeInfo &n : nodes) {
                if (n.port != p) continue;
                QString label = QString("Node %1: %2 (SN %3)").arg(n.node).arg(n.model, n.serial);
                if (!n.userId.isEmpty()) label += " - " + n.userId;
                auto *it = new QTreeWidgetItem(portItem, {label});
                it->setData(0, Qt::UserRole, n.port);
                it->setData(0, Qt::UserRole + 1, n.node);
                if (!first) first = it;
            }
        }
        setConnectedUi(true);
        if (first) m_tree->setCurrentItem(first);
        else appendLog("Hub opened but no nodes found", true);
    });
    connect(m_worker, &MotorWorker::disconnected, this, [this] {
        m_tree->clear();
        setConnectedUi(false);
    });
    connect(m_worker, &MotorWorker::nodeSelected, this, [this] {
        m_tabs->setEnabled(true);
        m_plotTab->setEnabled(true);
    });
    connect(m_worker, &MotorWorker::snapshot, this, [this](const Snapshot &s) {
        m_enableAct->setChecked(s.enabled);
        m_enableAct->setText(s.enabled ? "Disable" : "Enable");
    });

    setConnectedUi(false);
    m_connectAct->setEnabled(false);
    appendLog("Press Scan to look for SC hubs.");
    onWorker(m_worker, [w = m_worker] { w->scanPorts(); });
}

MainWindow::~MainWindow() {
    QMetaObject::invokeMethod(m_worker, [w = m_worker] { w->closePorts(); }, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void MainWindow::appendLog(const QString &msg, bool isError) {
    QString line = QDateTime::currentDateTime().toString("hh:mm:ss.zzz ") + (isError ? "ERROR: " : "") + msg;
    m_log->appendPlainText(line);
    if (isError) statusBar()->showMessage(msg, 10000);
}

void MainWindow::setConnectedUi(bool connected) {
    m_scanAct->setEnabled(!connected);
    m_connectAct->setEnabled(!connected && !m_ports.isEmpty());
    m_disconnectAct->setEnabled(connected);
    m_enableAct->setEnabled(connected);
    m_estopAct->setEnabled(connected);
    if (!connected) {
        m_enableAct->setChecked(false);
        m_enableAct->setText("Enable");
        m_tabs->setEnabled(false);
        m_plotTab->setEnabled(false);
    }
}

void MainWindow::setPlotDetached(bool detached) {
    if (detached) {
        m_tabs->removeTab(m_tabs->indexOf(m_plotTab));
        m_plotWin->layout()->addWidget(m_plotTab);
        m_plotTab->show();
        m_plotWin->show();
        m_plotWin->raise();
        m_plotWin->activateWindow();
    } else {
        m_plotWin->hide();
        m_tabs->addTab(m_plotTab, "Plot");
    }
}

// Closing the plot window puts the plot back in its tab.
bool MainWindow::eventFilter(QObject *obj, QEvent *ev) {
    if (obj == m_plotWin && ev->type() == QEvent::Close)
        m_plotWinAct->setChecked(false);
    return QMainWindow::eventFilter(obj, ev);
}
