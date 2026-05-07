#include "yoloassist.h"

#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace {
cv::Size inferYoloInputSizeFromModelPath(const QString& modelPath) {
    const QRegularExpression pairPattern("(\\d{3,4})[xX](\\d{3,4})");
    const QRegularExpressionMatch pairMatch = pairPattern.match(modelPath);
    if (pairMatch.hasMatch()) {
        const int width = pairMatch.captured(1).toInt();
        const int height = pairMatch.captured(2).toInt();
        if (width > 0 && height > 0) {
            return cv::Size(width, height);
        }
    }

    const QRegularExpression squarePattern("_(\\d{3,4})(?=\\.[^.]+$)");
    const QRegularExpressionMatch squareMatch = squarePattern.match(modelPath);
    if (squareMatch.hasMatch()) {
        const int size = squareMatch.captured(1).toInt();
        if (size > 0) {
            return cv::Size(size, size);
        }
    }

    return cv::Size(640, 640);
}

bool isLikelyNormalizedBox(float cx, float cy, float width, float height) {
    return cx >= 0.0f && cy >= 0.0f && width > 0.0f && height > 0.0f && cx <= 1.5f && cy <= 1.5f && width <= 1.5f && height <= 1.5f;
}

double rectDistance(const cv::Rect2d& lhs, const cv::Rect2d& rhs) {
    const double lhsLeft = lhs.x;
    const double lhsTop = lhs.y;
    const double lhsRight = lhs.x + lhs.width;
    const double lhsBottom = lhs.y + lhs.height;
    const double rhsLeft = rhs.x;
    const double rhsTop = rhs.y;
    const double rhsRight = rhs.x + rhs.width;
    const double rhsBottom = rhs.y + rhs.height;

    const double horizontalGap = std::max(0.0, std::max(lhsLeft - rhsRight, rhsLeft - lhsRight));
    const double verticalGap = std::max(0.0, std::max(lhsTop - rhsBottom, rhsTop - lhsBottom));
    return std::sqrt(horizontalGap * horizontalGap + verticalGap * verticalGap);
}
}

void YoloAssist::setModel(const QString& modelPath, const QStringList& classNames) {
    const bool changed = (m_modelPath != modelPath) || (m_classNames != classNames);
    m_modelPath = modelPath;
    m_classNames = classNames;
    if (changed) {
        m_netLoaded = false;
        m_netDirty = true;
        m_outputInfoLogged = false;
    }
}

bool YoloAssist::isModelConfigured() const {
    return !m_modelPath.isEmpty();
}

QString YoloAssist::modelPath() const {
    return m_modelPath;
}

cv::Size YoloAssist::lastInputSize() const {
    return m_lastInputSize;
}

QStringList YoloAssist::lastOutputShapes() const {
    return m_lastOutputShapes;
}

int YoloAssist::lastRawCandidateCount() const {
    return m_lastRawCandidateCount;
}

int YoloAssist::lastNmsCount() const {
    return m_lastNmsCount;
}

bool YoloAssist::ensureNetLoaded(QString* errorMessage) {
    if (m_netLoaded && !m_netDirty) {
        return true;
    }

    if (m_modelPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "Model path is empty";
        }
        m_netLoaded = false;
        m_netDirty = false;
        return false;
    }

    if (!QFileInfo::exists(m_modelPath)) {
        if (errorMessage) {
            *errorMessage = QString("Model file does not exist: %1").arg(m_modelPath);
        }
        m_netLoaded = false;
        m_netDirty = false;
        return false;
    }

    try {
        m_net = cv::dnn::readNetFromONNX(m_modelPath.toStdString());
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        m_netLoaded = true;
        m_netDirty = false;
        return true;
    } catch (const cv::Exception& ex) {
        if (errorMessage) {
            *errorMessage = ex.what();
        }
        m_netLoaded = false;
        m_netDirty = false;
        return false;
    }
}

bool YoloAssist::detect(const cv::Mat& frame, std::vector<Detection>& detections, QString* errorMessage) {
    detections.clear();
    m_lastRawCandidateCount = 0;
    m_lastNmsCount = 0;
    m_lastOutputShapes.clear();
    if (frame.empty()) {
        if (errorMessage) {
            *errorMessage = "Input frame is empty";
        }
        return false;
    }

    try {
        if (!ensureNetLoaded(errorMessage)) {
            return false;
        }

        const cv::Size modelInputSize = inferYoloInputSizeFromModelPath(m_modelPath);
        m_lastInputSize = modelInputSize;
        const cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, modelInputSize, cv::Scalar(), true, false);
        m_net.setInput(blob);

        std::vector<cv::Mat> outputs;
        m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());

        if (!m_outputInfoLogged) {
            QStringList shapes;
            for (const cv::Mat& output : outputs) {
                QStringList dims;
                for (int dimIndex = 0; dimIndex < output.dims; ++dimIndex) {
                    dims << QString::number(output.size[dimIndex]);
                }
                shapes << QString("[%1]").arg(dims.join('x'));
            }
            m_lastOutputShapes = shapes;
            qInfo() << "YOLO outputs:" << shapes.join(", ") << "input" << modelInputSize.width << "x" << modelInputSize.height;
            m_outputInfoLogged = true;
        } else {
            QStringList shapes;
            for (const cv::Mat& output : outputs) {
                QStringList dims;
                for (int dimIndex = 0; dimIndex < output.dims; ++dimIndex) {
                    dims << QString::number(output.size[dimIndex]);
                }
                shapes << QString("[%1]").arg(dims.join('x'));
            }
            m_lastOutputShapes = shapes;
        }

        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;
        std::vector<int> classIds;

        auto appendDetectionsFrom2D = [&](const cv::Mat& predictions) {
            if (predictions.empty() || predictions.cols < 6) {
                return;
            }

            for (int rowIndex = 0; rowIndex < predictions.rows; ++rowIndex) {
                const float* row = predictions.ptr<float>(rowIndex);
                const int attributes = predictions.cols;

                int classStartIndex = 5;
                float objectness = row[4];

                if (!m_classNames.isEmpty()) {
                    if (attributes == m_classNames.size() + 4) {
                        classStartIndex = 4;
                        objectness = 1.0f;
                    } else if (attributes == m_classNames.size() + 5) {
                        classStartIndex = 5;
                    }
                } else if (attributes >= 8 && attributes != 85) {
                    classStartIndex = 4;
                    objectness = 1.0f;
                }

                if (attributes <= classStartIndex) {
                    continue;
                }

                float bestClassScore = 0.0f;
                int bestClassId = -1;
                for (int classIndex = classStartIndex; classIndex < attributes; ++classIndex) {
                    if (row[classIndex] > bestClassScore) {
                        bestClassScore = row[classIndex];
                        bestClassId = classIndex - classStartIndex;
                    }
                }

                const float confidence = objectness * bestClassScore;
                if (confidence < 0.25f || bestClassId < 0) {
                    continue;
                }

                float centerX = row[0];
                float centerY = row[1];
                float width = row[2];
                float height = row[3];

                if (isLikelyNormalizedBox(centerX, centerY, width, height)) {
                    centerX *= static_cast<float>(frame.cols);
                    centerY *= static_cast<float>(frame.rows);
                    width *= static_cast<float>(frame.cols);
                    height *= static_cast<float>(frame.rows);
                } else {
                    const float xScale = static_cast<float>(frame.cols) / static_cast<float>(std::max(1, modelInputSize.width));
                    const float yScale = static_cast<float>(frame.rows) / static_cast<float>(std::max(1, modelInputSize.height));
                    centerX *= xScale;
                    centerY *= yScale;
                    width *= xScale;
                    height *= yScale;
                }

                int left = static_cast<int>(centerX - width * 0.5f);
                int top = static_cast<int>(centerY - height * 0.5f);
                int boxWidth = static_cast<int>(width);
                int boxHeight = static_cast<int>(height);

                left = std::max(0, std::min(left, std::max(0, frame.cols - 1)));
                top = std::max(0, std::min(top, std::max(0, frame.rows - 1)));
                boxWidth = std::max(1, std::min(boxWidth, std::max(1, frame.cols - left)));
                boxHeight = std::max(1, std::min(boxHeight, std::max(1, frame.rows - top)));

                boxes.emplace_back(left, top, boxWidth, boxHeight);
                confidences.push_back(confidence);
                classIds.push_back(bestClassId);
                ++m_lastRawCandidateCount;
            }
        };

        for (const cv::Mat& output : outputs) {
            if (output.empty()) {
                continue;
            }

            if (output.dims == 2) {
                appendDetectionsFrom2D(output);
                continue;
            }

            if (output.dims == 3 && output.size[0] == 1) {
                const int dim1 = output.size[1];
                const int dim2 = output.size[2];

                if (dim1 > 0 && dim2 > 0) {
                    cv::Mat view;
                    if (dim1 >= dim2) {
                        view = cv::Mat(dim1, dim2, CV_32F, const_cast<float*>(output.ptr<float>(0)));
                    } else {
                        const cv::Mat attrsByPred = cv::Mat(dim1, dim2, CV_32F, const_cast<float*>(output.ptr<float>(0)));
                        view = attrsByPred.t();
                    }
                    appendDetectionsFrom2D(view);
                }
            }
        }

        if (boxes.empty()) {
            return false;
        }

        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, 0.25f, 0.45f, indices);
        m_lastNmsCount = static_cast<int>(indices.size());

        for (int index : indices) {
            Detection detection;
            detection.classId = classIds[index];
            detection.confidence = confidences[index];
            detection.box = boxes[index];
            detections.push_back(detection);
        }

        return !detections.empty();
    } catch (const cv::Exception& ex) {
        if (errorMessage) {
            *errorMessage = ex.what();
        }
        return false;
    }
}

double YoloAssist::rectIntersectionOverUnion(const cv::Rect2d& lhs, const cv::Rect2d& rhs) {
    const double intersectionLeft = std::max(lhs.x, rhs.x);
    const double intersectionTop = std::max(lhs.y, rhs.y);
    const double intersectionRight = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
    const double intersectionBottom = std::min(lhs.y + lhs.height, rhs.y + rhs.height);

    const double intersectionWidth = std::max(0.0, intersectionRight - intersectionLeft);
    const double intersectionHeight = std::max(0.0, intersectionBottom - intersectionTop);
    const double intersectionArea = intersectionWidth * intersectionHeight;
    const double lhsArea = std::max(0.0, lhs.width) * std::max(0.0, lhs.height);
    const double rhsArea = std::max(0.0, rhs.width) * std::max(0.0, rhs.height);

    const double unionArea = lhsArea + rhsArea - intersectionArea;
    if (unionArea <= 0.0) {
        return 0.0;
    }

    return intersectionArea / unionArea;
}

bool YoloAssist::chooseDetectionForSelection(const std::vector<Detection>& detections, const cv::Rect2d& referenceBox, Detection& selectedDetection) {
    double bestScore = -1.0;
    bool found = false;

    for (const Detection& detection : detections) {
        const double score = -rectDistance(referenceBox, detection.box);
        if (!found || score > bestScore) {
            bestScore = score;
            selectedDetection = detection;
            found = true;
        }
    }

    return found;
}

bool YoloAssist::chooseDetectionForCorrection(const std::vector<Detection>& detections, const cv::Rect2d& referenceBox, Detection& selectedDetection) {
    double bestScore = -1.0;
    bool found = false;

    for (const Detection& detection : detections) {
        const double score = -rectDistance(referenceBox, detection.box);
        if (!found || score > bestScore) {
            bestScore = score;
            selectedDetection = detection;
            found = true;
        }
    }

    return found;
}
