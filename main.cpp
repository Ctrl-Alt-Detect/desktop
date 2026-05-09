#include <QApplication>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=1");

    QApplication app(argc, argv);

    MainWindow w;
    w.show();
    return app.exec();
}