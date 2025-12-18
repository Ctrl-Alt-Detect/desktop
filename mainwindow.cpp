#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QCamera *camera = nullptr;
    QCameraViewfinder *viewfinder = new QCameraViewfinder(this);
    viewfinder->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Maximum);
    ui->groupBox->layout()->addWidget(viewfinder);

    ui->chooseCameraList->addItem("Выберите камеру", QVariant());

    int i = 1;
    const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    for (const QCameraInfo &cameraInfo : cameras)
    {
        ui->chooseCameraList->addItem(cameraInfo.description(), QVariant::fromValue(cameraInfo));
    }

    connect(ui->chooseCameraList, QOverload<int>::of(&QComboBox::currentIndexChanged), [=, &camera](int index)
    {
        if (index == 0)
        {
            if (camera == nullptr) return;

            camera->stop();
            camera->deleteLater();
            camera = nullptr;
            return;
        }

        QCameraInfo selectedInfo = ui->chooseCameraList->itemData(index).value<QCameraInfo>();
        // Now you can create a QCamera using selectedInfo
        camera = new QCamera(selectedInfo, this);
        camera->setViewfinder(viewfinder);
        camera->start();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

