// #include "mainwindow.h"

// #include <QApplication>

// int main(int argc, char *argv[])
// {
//     QApplication a(argc, argv);
//     MainWindow w;
//     w.show();
//     return a.exec();
// }

#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <iostream>

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Cannot open camera\n";
        return -1;
    }

    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) return -1;

    cv::Rect roi = cv::selectROI("Select Object", frame, true, true);
    cv::destroyWindow("Select Object");
    if (roi.width == 0 || roi.height == 0) return 0;

    cv::Ptr<cv::Tracker> tracker = cv::TrackerCSRT::create();
    cv::Rect2i bbox(roi); // convert
    tracker->init(frame, bbox);

    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        bool success = tracker->update(frame, bbox);
        if (success) {
            cv::rectangle(frame,
                cv::Point(static_cast<int>(bbox.x), static_cast<int>(bbox.y)),
                cv::Point(static_cast<int>(bbox.x + bbox.width), static_cast<int>(bbox.y + bbox.height)),
                cv::Scalar(0, 255, 0), 2);
        }

        cv::imshow("Tracking", frame);
        if (cv::waitKey(1) == 'q') break;
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
