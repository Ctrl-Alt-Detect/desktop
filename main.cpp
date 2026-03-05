#include "customwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    CustomWindow w;
    w.show();
    return a.exec();
}
