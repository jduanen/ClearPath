#pragma once

#include <QMainWindow>
#include <QStringList>
#include <QThread>

#include "MotorWorker.h"

class QAction;
class QPlainTextEdit;
class QTabWidget;
class QTreeWidget;
class InfoStatusTab;
class MotionTab;
class ConfigTab;
class PlotTab;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void appendLog(const QString &msg, bool isError = false);
    void setConnectedUi(bool connected);

    QThread m_thread;
    MotorWorker *m_worker;
    QStringList m_ports;

    QAction *m_scanAct, *m_connectAct, *m_disconnectAct, *m_enableAct, *m_estopAct;
    QTreeWidget *m_tree;
    QTabWidget *m_tabs;
    QPlainTextEdit *m_log;
    InfoStatusTab *m_infoTab;
    MotionTab *m_motionTab;
    ConfigTab *m_configTab;
    PlotTab *m_plotTab;
};
