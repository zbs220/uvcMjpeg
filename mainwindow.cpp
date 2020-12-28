#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPixmap>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //
    cam1 = nullptr;
    cam2 = nullptr;
    leftImg = QImage(PIC_WIDTH, PIC_HEIGHT, QImage::Format_RGB888);
    rightImg = QImage(PIC_WIDTH, PIC_HEIGHT, QImage::Format_RGB888);


    if(QApplication::arguments().size()==2){
        QString arg1=QApplication::arguments()[1];
        if(arg1=="0"){
            cam1 = new VideoDev("/dev/video0");
            timerleft = new QTimer(this);
            connect(timerleft, SIGNAL(timeout()), this, SLOT(updateFramesLeft()));
            timerleft->start(100);
        }else if(arg1=="1"){
            cam2 = new VideoDev("/dev/video1");
            timerright = new QTimer(this);
            connect(timerright, SIGNAL(timeout()), this, SLOT(updateFramesRight()));
            timerright->start(100);
        }
    }else if(QApplication::arguments().size()==3){
        QString arg1=QApplication::arguments()[1];
        QString arg2=QApplication::arguments()[2];

        cam1 = new VideoDev("/dev/video" + arg1);
        cam2 = new VideoDev("/dev/video" + arg2);

        timerleft = new QTimer(this);
        timerright = new QTimer(this);
        connect(timerleft, SIGNAL(timeout()), this, SLOT(updateFramesLeft()));
        connect(timerright, SIGNAL(timeout()), this, SLOT(updateFramesRight()));

        timerleft->start(100);
        timerright->start(100);
    }
}

MainWindow::~MainWindow()
{

    if(cam1 != nullptr)
        delete cam1;
    if(cam2 != nullptr)
        delete cam2;

    delete ui;
}

void MainWindow::updateFramesLeft()
{
    if(cam1->deviceAvilable()){
        cam1->getPix(leftPix);
        ui->label->setPixmap(leftPix);
    }
}

void MainWindow::updateFramesRight()
{
    if(cam2->deviceAvilable()){
        cam2->getPix(rightPix);
        ui->label_2->setPixmap(rightPix);
    }
}
