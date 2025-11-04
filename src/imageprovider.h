#ifndef IMAGEPROVIDER_H
#define IMAGEPROVIDER_H

#include <QObject>
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QDebug>

class ImageProvider : public QObject
{
    Q_OBJECT
public:
    explicit ImageProvider(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QString getWifiIconSource(int version) {
        QString resourcePath = QString(":/image/wifi%1").arg(version);
        QImage image(resourcePath);

        if (image.isNull()) {
            qWarning() << "Failed to load image from resource:" << resourcePath;
            return QString(); // 返回空字串表示失敗
        }

        // 將 QImage 轉換為 Base64 字串
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG"); // 以 PNG 格式保存到緩衝區

        // 返回 Base64 字串的 data URI 格式
        return QString("data:image/png;base64,") + ba.toBase64();
    }
};

#endif //IMAGEPROVIDER_H
