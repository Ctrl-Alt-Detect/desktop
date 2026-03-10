#ifndef AICORE_H
#define AICORE_H

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <QString>
#include <QRect>
#include <QVector>
#include <QSize>

/**
 * @brief Структура, описывающая сохранённый объект для AI-поиска
 */
struct AITarget {
    cv::Mat templateImage;      // Изображение объекта для template matching
    cv::Mat featureDescriptors; // Дескрипторы признаков (ORB)
    std::vector<cv::KeyPoint> keypoints; // Ключевые точки
    QRect originalRect;         // Исходный прямоугольник выделения
    QSize originalSize;         // Размер изображения при выделении
    QString name;               // Имя объекта
    bool isValid = false;       // Флаг валидности данных
    
    // Для YOLO-детекции
    bool useYOLO = false;       // Флаг использования YOLO
    QString yoloModelPath;      // Путь к YOLO модели
    int yoloClassId = -1;       // ID класса для поиска (-1 = любой)
    QString yoloClassName;      // Имя класса для поиска
};

/**
 * @brief Результаты AI-детекции
 */
struct AIDetectionResult {
    bool detected = false;
    QRect boundingRect;
    float confidence = 0.0f;
    QString methodName; // Метод, который обнаружил объект
};

/**
 * @brief Основной класс для AI-обработки изображений
 * 
 * Поддерживает:
 * - Template matching для быстрого поиска
 * - ORB features для надёжного сопоставления
 * - YOLO/SSD через OpenCV DNN (опционально)
 */
class AICore {
public:
    explicit AICore();
    ~AICore();

    /**
     * @brief Сохранить выделенный объект для последующего поиска
     * @param frame Кадр, из которого выделяется объект
     * @param rect Прямоугольник выделения в координатах кадра
     * @param name Имя объекта (опционально)
     * @return true если объект успешно сохранён
     */
    bool learnObject(const cv::Mat &frame, const cv::Rect &rect, const QString &name = "Target");

    /**
     * @brief Настроить YOLO для поиска конкретного класса
     * @param modelPath Путь к модели (.onnx)
     * @param className Имя класса для поиска (например, "person", "car")
     * @param classId ID класса в модели COCO (например, 0=person, 2=car)
     */
    void setupYOLO(const QString &modelPath, const QString &className = QString(), int classId = -1);

    /**
     * @brief Найти сохранённый объект в кадре
     * @param frame Кадр для поиска
     * @return Результат детекции
     */
    AIDetectionResult findObject(const cv::Mat &frame);

    /**
     * @brief Найти объект с использованием YOLO (детекция всех объектов)
     * @param frame Кадр для поиска
     * @param confidenceThreshold Порог уверенности (0-1)
     * @return Результат детекции первого найденного объекта
     */
    AIDetectionResult findWithYOLO(const cv::Mat &frame, float confidenceThreshold = 0.5f);

    /**
     * @brief Найти объект с использованием template matching
     */
    AIDetectionResult findWithTemplate(const cv::Mat &frame);

    /**
     * @brief Найти объект с использованием ORB признаков
     */
    AIDetectionResult findWithORB(const cv::Mat &frame);

    /**
     * @brief Найти объект с использованием YOLO/SSD (если модель загружена)
     * @param frame Кадр для поиска
     * @param modelPath Путь к модели (.onnx, .weights)
     * @param configPath Путь к конфигу (.cfg) - для Darknet моделей
     * @param classNames Список имён классов
     */
    AIDetectionResult findWithDNN(const cv::Mat &frame, 
                                   const QString &modelPath,
                                   const QString &configPath = QString(),
                                   const QStringList &classNames = QStringList());

    /**
     * @brief Проверить, есть ли сохранённый объект
     */
    bool hasTarget() const { return m_target.isValid; }

    /**
     * @brief Очистить сохранённый объект
     */
    void clearTarget();

    /**
     * @brief Получить имя сохранённого объекта
     */
    QString targetName() const { return m_target.name; }

    /**
     * @brief Установить порог уверенности для template matching (0-1)
     */
    void setTemplateThreshold(float threshold) { m_templateThreshold = threshold; }

    /**
     * @brief Установить порог уверенности для ORB matching (минимум совпадений)
     */
    void setORBMinMatches(int minMatches) { m_orbMinMatches = minMatches; }

    /**
     * @brief Сохранить данные объекта в файл
     */
    bool saveTarget(const QString &filePath);

    /**
     * @brief Загрузить данные объекта из файла
     */
    bool loadTarget(const QString &filePath);

    /**
     * @brief Получить ID класса COCO по имени
     */
    static int getCOCOClassId(const QString &className);

private:
    AITarget m_target;
    cv::Ptr<cv::ORB> m_orb;
    cv::Ptr<cv::BFMatcher> m_matcher;
    
    float m_templateThreshold = 0.6f;
    int m_orbMinMatches = 10;

    /**
     * @brief Извлечь ROI из кадра с обработкой краёв
     */
    cv::Mat extractROI(const cv::Mat &frame, const cv::Rect &rect);

    /**
     * @brief Конвертировать QRect в cv::Rect
     */
    static cv::Rect convertQRectToCvRect(const QRect &qrect);

    /**
     * @brief Конвертировать cv::Rect в QRect
     */
    static QRect convertCvRectToQRect(const cv::Rect &cvrect);
};

#endif // AICORE_H
