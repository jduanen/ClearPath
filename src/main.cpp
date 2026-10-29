#include <QApplication>

#include "MainWindow.h"
#include "MotorWorker.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("cpview");
    qRegisterMetaType<NodeInfo>();
    qRegisterMetaType<QList<NodeInfo>>();
    qRegisterMetaType<MotorLimits>();
    qRegisterMetaType<HomingSettings>();
    qRegisterMetaType<Snapshot>();

    MainWindow w;
    w.show();
    return app.exec();
}
