#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <videodev.h>
#include <QTimer>

//#include "decoder.h"
//#include "decoder_mjpeg.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateFramesLeft();
    void updateFramesRight();


private:
    Ui::MainWindow *ui;
    VideoDev *cam1, *cam2;
    //Decoder *decoder = nullptr;

    QTimer *timerleft, *timerright;
    QImage leftImg, rightImg;
    QPixmap leftPix, rightPix;
};

#endif // MAINWINDOW_H
