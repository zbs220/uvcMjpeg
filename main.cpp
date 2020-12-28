#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    char dev[100] = {0};
    sprintf(dev, "/dev/video%s", argv[1]);

    MainWindow w;
    w.show();

    return a.exec();
}
