#include "utf8utils.h"

#include <QStringDecoder>

#include <QDebug>

bool Utf8Utils::isValidUtf8(const QByteArray &data) {
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString result = decoder(data);
    return !result.contains(QChar::ReplacementCharacter);
}

QString Utf8Utils::decode(const QByteArray &data) {
    if (isValidUtf8(data)) {
        return QString::fromUtf8(data);
    } else {
        // return "<hidden>";
        qDebug() << "Not UTF8: "  << data.toHex(' ');
        return QString::fromLatin1(data);  // or fallback to hex if needed
    }
}
