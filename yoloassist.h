#pragma once

#include <QString>
#include <QStringList>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>
#include <vector>

class YoloAssist {
public:
    struct Detection {
        int classId{-1};
        float confidence{0.0f};
        cv::Rect box;
    };

    void setModel(const QString& modelPath, const QStringList& classNames);
    bool isModelConfigured() const;

    bool detect(const cv::Mat& frame, std::vector<Detection>& detections, QString* errorMessage = nullptr);

    static double rectIntersectionOverUnion(const cv::Rect2d& lhs, const cv::Rect2d& rhs);
    static bool chooseDetectionForSelection(const std::vector<Detection>& detections, const cv::Rect2d& referenceBox, Detection& selectedDetection);
    static bool chooseDetectionForCorrection(const std::vector<Detection>& detections, const cv::Rect2d& referenceBox, Detection& selectedDetection);

private:
    bool ensureNetLoaded(QString* errorMessage);

    QString m_modelPath;
    QStringList m_classNames;
    cv::dnn::Net m_net;
    bool m_netLoaded{false};
    bool m_netDirty{false};
    bool m_outputInfoLogged{false};
};
