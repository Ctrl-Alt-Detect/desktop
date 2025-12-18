#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QCamera *camera = new QCamera(this);
    QCameraViewfinder *viewfinder = new QCameraViewfinder(this);
    viewfinder->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Maximum);
    camera->setViewfinder(viewfinder);

    ui->groupBox->layout()->addWidget(viewfinder);

    camera->start(); // to start the viewfinder


    int i = 0;
    const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    for (const QCameraInfo &cameraInfo : cameras)
    {
        ui->chooseCameraList->addItem(cameraInfo.description(), i++);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

