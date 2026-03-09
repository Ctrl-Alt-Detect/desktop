#include "aicore.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

AICore::AICore() 
    : m_orb(cv::ORB::create()),
      m_matcher(cv::makePtr<cv::BFMatcher>(cv::NORM_HAMMING, true))
{
}

AICore::~AICore() = default;

void AICore::setupYOLO(const QString &modelPath, const QString &className, int classId)
{
    m_target.useYOLO = true;
    m_target.yoloModelPath = modelPath;
    m_target.yoloClassName = className;
    m_target.yoloClassId = classId;
    m_target.isValid = true;
}

bool AICore::learnObject(const cv::Mat &frame, const cv::Rect &rect, const QString &name)
{
    if (frame.empty() || rect.area() <= 0)
        return false;

    // Извлекаем ROI объекта
    cv::Mat roi = extractROI(frame, rect);
    if (roi.empty())
        return false;

    // Сохраняем шаблон для template matching
    m_target.templateImage = roi.clone();

    // Извлекаем ORB признаки
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    m_orb->detectAndCompute(roi, cv::noArray(), keypoints, descriptors);

    if (descriptors.empty())
    {
        // Если не удалось извлечь признаки, пробуем увеличить изображение
        cv::Mat resized;
        cv::resize(roi, resized, cv::Size(), 2.0, 2.0, cv::INTER_CUBIC);
        m_orb->detectAndCompute(resized, cv::noArray(), keypoints, descriptors);
    }

    m_target.keypoints = keypoints;
    m_target.featureDescriptors = descriptors.clone();
    m_target.originalRect = convertCvRectToQRect(rect);
    m_target.originalSize = QSize(frame.cols, frame.rows);
    m_target.name = name;
    m_target.isValid = true;

    return true;
}

AIDetectionResult AICore::findObject(const cv::Mat &frame)
{
    if (!m_target.isValid || frame.empty())
        return AIDetectionResult();

    // Если настроен YOLO, используем его
    if (m_target.useYOLO && !m_target.yoloModelPath.isEmpty())
    {
        AIDetectionResult yoloResult = findWithYOLO(frame);
        if (yoloResult.detected)
            return yoloResult;
    }

    // Сначала пытаемся найти через template matching (быстрее)
    AIDetectionResult templateResult = findWithTemplate(frame);
    if (templateResult.detected && templateResult.confidence >= m_templateThreshold)
        return templateResult;

    // Если template matching не дал результата, пробуем ORB
    AIDetectionResult orbResult = findWithORB(frame);
    if (orbResult.detected)
        return orbResult;

    // Если ничего не найдено
    AIDetectionResult result;
    result.methodName = "none";
    return result;
}

AIDetectionResult AICore::findWithYOLO(const cv::Mat &frame, float confidenceThreshold)
{
    AIDetectionResult result;
    result.methodName = "yolo";

    if (!m_target.useYOLO || m_target.yoloModelPath.isEmpty())
        return result;

    QFile modelFile(m_target.yoloModelPath);
    if (!modelFile.exists())
    {
        qWarning() << "YOLO model not found:" << m_target.yoloModelPath;
        return result;
    }

    try
    {
        // Загружаем YOLO модель
        cv::dnn::Net net = cv::dnn::readNetFromONNX(m_target.yoloModelPath.toStdString());
        if (net.empty())
            return result;

        // Создаём blob из изображения
        cv::Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1.0/255.0, cv::Size(640, 640),
                               cv::Scalar(0, 0, 0), true, false);

        net.setInput(blob);

        // Получаем выводы сети
        std::vector<cv::Mat> outputs;
        net.forward(outputs, net.getUnconnectedOutLayersNames());

        // Проверка выходов
        if (outputs.empty() || outputs[0].empty())
            return result;

        // Обработка результатов YOLOv8/v5
        int numClasses = 80; // COCO classes
        float *data = nullptr;

        if (outputs[0].dims >= 3)
        {
            // YOLOv8 format: [1, 84, 8400] -> [batch, coords+classes, anchors]
            data = outputs[0].ptr<float>(0);
            int numAnchors = outputs[0].size[2];
            int numValues = outputs[0].size[1]; // 84 = 4 (bbox) + 80 (classes)

            // Проверка на разумные значения
            if (numAnchors <= 0 || numValues <= 4 || numAnchors > 100000)
                return result;

            float bestConfidence = 0;
            cv::Rect bestRect;

            for (int i = 0; i < numAnchors; i++)
            {
                // Получаем confidence для нужного класса
                float confidence = 0;
                int classId = -1;

                if (m_target.yoloClassId >= 0 && m_target.yoloClassId < numClasses)
                {
                    // Ищем конкретный класс
                    classId = m_target.yoloClassId;
                    int idx = numValues * i + 4 + classId;
                    if (idx >= 0 && idx < numValues * numAnchors)
                        confidence = data[idx];
                }
                else if (!m_target.yoloClassName.isEmpty())
                {
                    // Ищем по имени класса (упрощённо - используем ID)
                    classId = getCOCOClassId(m_target.yoloClassName);
                    if (classId >= 0 && classId < numClasses)
                    {
                        int idx = numValues * i + 4 + classId;
                        if (idx >= 0 && idx < numValues * numAnchors)
                            confidence = data[idx];
                    }
                }
                else
                {
                    // Ищем любой класс с максимальной уверенностью
                    for (int c = 0; c < numClasses; c++)
                    {
                        int idx = numValues * i + 4 + c;
                        if (idx >= 0 && idx < numValues * numAnchors)
                        {
                            float conf = data[idx];
                            if (conf > confidence)
                            {
                                confidence = conf;
                                classId = c;
                            }
                        }
                    }
                }

                if (confidence < confidenceThreshold || !std::isfinite(confidence))
                    continue;

                // Получаем координаты bbox
                float x = data[numValues * i + 0];
                float y = data[numValues * i + 1];
                float w = data[numValues * i + 2];
                float h = data[numValues * i + 3];

                // Проверка на валидность координат
                if (!std::isfinite(x) || !std::isfinite(y) || 
                    !std::isfinite(w) || !std::isfinite(h))
                    continue;

                // Конвертируем из формата YOLO (center-x, center-y, w, h) в (x1, y1, w, h)
                int x1 = static_cast<int>((x - w/2) * frame.cols / 640);
                int y1 = static_cast<int>((y - h/2) * frame.rows / 640);
                int x2 = static_cast<int>((x + w/2) * frame.cols / 640);
                int y2 = static_cast<int>((y + h/2) * frame.rows / 640);

                // Ограничиваем координаты размерами кадра
                x1 = std::max(0, std::min(x1, frame.cols - 1));
                y1 = std::max(0, std::min(y1, frame.rows - 1));
                x2 = std::max(0, std::min(x2, frame.cols));
                y2 = std::max(0, std::min(y2, frame.rows));

                cv::Rect rect(x1, y1, x2 - x1, y2 - y1);

                // Пропускаем слишком маленькие или невалидные прямоугольники
                if (rect.width < 10 || rect.height < 10 || rect.area() <= 0)
                    continue;

                // Оставляем только если уверенность выше предыдущей
                if (confidence > bestConfidence)
                {
                    bestConfidence = confidence;
                    bestRect = rect;
                }
            }

            if (bestConfidence >= confidenceThreshold && bestRect.area() > 0)
            {
                result.detected = true;
                result.confidence = bestConfidence;
                result.boundingRect = QRect(bestRect.x, bestRect.y, bestRect.width, bestRect.height);
            }
        }
    }
    catch (const cv::Exception &e)
    {
        qWarning() << "YOLO detection error:" << e.what();
    }
    catch (...)
    {
        qWarning() << "YOLO detection: unknown error";
    }

    return result;
}

int AICore::getCOCOClassId(const QString &className)
{
    // COCO class names
    static const QStringList classes = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat",
        "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat",
        "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack",
        "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball",
        "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket",
        "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake",
        "chair", "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop",
        "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
        "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
    };

    int idx = classes.indexOf(className.toLower());
    return idx;
}

AIDetectionResult AICore::findWithTemplate(const cv::Mat &frame)
{
    AIDetectionResult result;
    result.methodName = "template";

    if (m_target.templateImage.empty())
        return result;

    // Конвертируем в оттенки серого для template matching
    cv::Mat frameGray, templateGray;
    if (frame.channels() > 1)
        cv::cvtColor(frame, frameGray, cv::COLOR_BGR2GRAY);
    else
        frameGray = frame;

    if (m_target.templateImage.channels() > 1)
        cv::cvtColor(m_target.templateImage, templateGray, cv::COLOR_BGR2GRAY);
    else
        templateGray = m_target.templateImage;

    // Выполняем template matching
    cv::Mat matchResult;
    cv::matchTemplate(frameGray, templateGray, matchResult, cv::TM_CCOEFF_NORMED);

    // Находим лучшее совпадение
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);

    if (maxVal >= m_templateThreshold)
    {
        result.detected = true;
        result.confidence = static_cast<float>(maxVal);
        result.boundingRect = QRect(
            maxLoc.x, maxLoc.y,
            m_target.templateImage.cols,
            m_target.templateImage.rows
        );
    }

    return result;
}

AIDetectionResult AICore::findWithORB(const cv::Mat &frame)
{
    AIDetectionResult result;
    result.methodName = "orb";

    if (m_target.featureDescriptors.empty() || m_target.keypoints.empty())
        return result;

    // Конвертируем в оттенки серого
    cv::Mat frameGray;
    if (frame.channels() > 1)
        cv::cvtColor(frame, frameGray, cv::COLOR_BGR2GRAY);
    else
        frameGray = frame;

    // Детектируем keypoints и вычисляем дескрипторы
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    m_orb->detectAndCompute(frameGray, cv::noArray(), keypoints, descriptors);

    if (descriptors.empty())
        return result;

    // Сопоставляем дескрипторы
    std::vector<std::vector<cv::DMatch>> knnMatches;
    m_matcher->knnMatch(m_target.featureDescriptors, descriptors, knnMatches, 2);

    // Фильтруем хорошие совпадения (ratio test)
    std::vector<cv::DMatch> goodMatches;
    for (const auto &matches : knnMatches)
    {
        if (matches.size() == 2)
        {
            if (matches[0].distance < 0.75f * matches[1].distance)
                goodMatches.push_back(matches[0]);
        }
    }

    if (static_cast<int>(goodMatches.size()) < m_orbMinMatches)
        return result;

    // Находим bounding box совпавших точек
    std::vector<cv::Point2f> targetPoints, scenePoints;
    for (const auto &match : goodMatches)
    {
        targetPoints.push_back(m_target.keypoints[match.queryIdx].pt);
        scenePoints.push_back(keypoints[match.trainIdx].pt);
    }

    // Используем гомографию для нахождения объекта
    if (targetPoints.size() >= 4)
    {
        cv::Mat homography = cv::findHomography(targetPoints, scenePoints, cv::RANSAC, 3.0);
        if (!homography.empty())
        {
            // Получаем размеры целевого объекта
            float targetW = static_cast<float>(m_target.templateImage.cols);
            float targetH = static_cast<float>(m_target.templateImage.rows);

            // Углы целевого объекта
            std::vector<cv::Point2f> corners(4);
            corners[0] = cv::Point2f(0, 0);
            corners[1] = cv::Point2f(targetW, 0);
            corners[2] = cv::Point2f(targetW, targetH);
            corners[3] = cv::Point2f(0, targetH);

            // Проецируем углы на сцену
            std::vector<cv::Point2f> projectedCorners(4);
            cv::perspectiveTransform(corners, projectedCorners, homography);

            // Находим bounding box спроецированных точек
            float minX = projectedCorners[0].x, maxX = projectedCorners[0].x;
            float minY = projectedCorners[0].y, maxY = projectedCorners[0].y;

            for (const auto &pt : projectedCorners)
            {
                minX = std::min(minX, pt.x);
                maxX = std::max(maxX, pt.x);
                minY = std::min(minY, pt.y);
                maxY = std::max(maxY, pt.y);
            }

            // Проверяем, что bounding box в пределах кадра
            minX = std::max(0.0f, minX);
            minY = std::max(0.0f, minY);
            maxX = std::min(static_cast<float>(frame.cols), maxX);
            maxY = std::min(static_cast<float>(frame.rows), maxY);

            result.detected = true;
            result.boundingRect = QRect(
                static_cast<int>(minX),
                static_cast<int>(minY),
                static_cast<int>(maxX - minX),
                static_cast<int>(maxY - minY)
            );

            // Вычисляем confidence на основе количества совпадений
            result.confidence = std::min(1.0f, static_cast<float>(goodMatches.size()) / 50.0f);
        }
    }

    return result;
}

AIDetectionResult AICore::findWithDNN(const cv::Mat &frame,
                                       const QString &modelPath,
                                       const QString &configPath,
                                       const QStringList &classNames)
{
    AIDetectionResult result;
    result.methodName = "dnn";

    if (frame.empty() || modelPath.isEmpty())
        return result;

    QFile modelFile(modelPath);
    if (!modelFile.exists())
        return result;

    try
    {
        // Определяем тип модели по расширению
        QString ext = QFileInfo(modelPath).suffix().toLower();
        cv::dnn::Net net;

        if (ext == "onnx")
        {
            net = cv::dnn::readNetFromONNX(modelPath.toStdString());
        }
        else if (ext == "caffemodel")
        {
            net = cv::dnn::readNetFromCaffe(
                configPath.toStdString(),
                modelPath.toStdString()
            );
        }
        else if (ext == "weights" || ext == "bin")
        {
            net = cv::dnn::readNetFromDarknet(
                configPath.toStdString(),
                modelPath.toStdString()
            );
        }
        else
        {
            net = cv::dnn::readNet(modelPath.toStdString());
        }

        if (net.empty())
            return result;

        // Создаём blob из изображения
        cv::Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1.0/255.0, cv::Size(416, 416),
                               cv::Scalar(0, 0, 0), true, false);

        net.setInput(blob);
        cv::Mat detection = net.forward();

        // Обрабатываем результаты детекции (формат зависит от модели)
        // Это общая обработка для YOLO-like моделей
        float *data = (float *)detection.data;
        
        int numDetections = detection.size[2];
        int numClasses = classNames.isEmpty() ? 80 : classNames.size();

        for (int i = 0; i < numDetections; ++i)
        {
            float confidence = data[i * numClasses + 0];
            if (confidence < 0.5f) continue;

            // Координаты bounding box (нормализованные)
            float x1 = data[i * numClasses + 1] * frame.cols;
            float y1 = data[i * numClasses + 2] * frame.rows;
            float x2 = data[i * numClasses + 3] * frame.cols;
            float y2 = data[i * numClasses + 4] * frame.rows;

            result.detected = true;
            result.confidence = confidence;
            result.boundingRect = QRect(
                static_cast<int>(x1),
                static_cast<int>(y1),
                static_cast<int>(x2 - x1),
                static_cast<int>(y2 - y1)
            );
            break; // Берём первое подходящее обнаружение
        }
    }
    catch (const cv::Exception &e)
    {
        qWarning() << "DNN detection error:" << e.what();
        return result;
    }

    return result;
}

void AICore::clearTarget()
{
    m_target = AITarget();
}

cv::Mat AICore::extractROI(const cv::Mat &frame, const cv::Rect &rect)
{
    // Проверяем границы
    cv::Rect safeRect = rect & cv::Rect(0, 0, frame.cols, frame.rows);
    if (safeRect.area() <= 0)
        return cv::Mat();

    return frame(safeRect).clone();
}

cv::Rect AICore::convertQRectToCvRect(const QRect &qrect)
{
    return cv::Rect(qrect.x(), qrect.y(), qrect.width(), qrect.height());
}

QRect AICore::convertCvRectToQRect(const cv::Rect &cvrect)
{
    return QRect(cvrect.x, cvrect.y, cvrect.width, cvrect.height);
}

bool AICore::saveTarget(const QString &filePath)
{
    if (!m_target.isValid)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    // Сохраняем метаданные
    QDataStream out(&file);
    out << m_target.name
        << m_target.originalRect
        << m_target.originalSize
        << static_cast<quint32>(m_target.templateImage.cols)
        << static_cast<quint32>(m_target.templateImage.rows)
        << static_cast<int>(m_target.templateImage.type());

    // Сохраняем изображение шаблона
    if (!m_target.templateImage.empty())
    {
        std::vector<uchar> buf;
        cv::imencode(".png", m_target.templateImage, buf);
        QByteArray data(reinterpret_cast<const char*>(buf.data()), buf.size());
        out << data;
    }

    // Сохраняем keypoints
    out << static_cast<quint32>(m_target.keypoints.size());
    for (const auto &kp : m_target.keypoints)
    {
        out << kp.pt.x << kp.pt.y << kp.size << kp.angle 
            << kp.response << kp.octave << kp.class_id;
    }

    // Сохраняем дескрипторы
    if (!m_target.featureDescriptors.empty())
    {
        QByteArray descData(
            reinterpret_cast<const char*>(m_target.featureDescriptors.data),
            m_target.featureDescriptors.total() * m_target.featureDescriptors.elemSize()
        );
        out << m_target.featureDescriptors.rows 
            << m_target.featureDescriptors.cols 
            << static_cast<int>(m_target.featureDescriptors.type())
            << descData;
    }

    return true;
}

bool AICore::loadTarget(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    
    QString name;
    QRect originalRect;
    QSize originalSize;
    quint32 width, height;
    int imageType;

    in >> name >> originalRect >> originalSize >> width >> height >> imageType;

    m_target.name = name;
    m_target.originalRect = originalRect;
    m_target.originalSize = originalSize;

    // Загружаем изображение шаблона
    QByteArray imageData;
    in >> imageData;
    if (!imageData.isEmpty())
    {
        std::vector<uchar> buf(imageData.begin(), imageData.end());
        m_target.templateImage = cv::imdecode(buf, cv::IMREAD_COLOR);
    }

    // Загружаем keypoints
    quint32 keypointCount;
    in >> keypointCount;
    m_target.keypoints.clear();
    for (quint32 i = 0; i < keypointCount; ++i)
    {
        cv::KeyPoint kp;
        in >> kp.pt.x >> kp.pt.y >> kp.size >> kp.angle 
           >> kp.response >> kp.octave >> kp.class_id;
        m_target.keypoints.push_back(kp);
    }

    // Загружаем дескрипторы
    int rows, cols, type;
    QByteArray descData;
    in >> rows >> cols >> type >> descData;
    if (!descData.isEmpty())
    {
        m_target.featureDescriptors = cv::Mat(rows, cols, type);
        std::memcpy(m_target.featureDescriptors.data, 
                    descData.data(), 
                    descData.size());
    }

    m_target.isValid = true;
    return true;
}
