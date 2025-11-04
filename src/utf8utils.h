#ifndef UTF8UTILS_H
#define UTF8UTILS_H

#include <QString>
#include <QByteArray>

class Utf8Utils {
public:
    // Check if the QByteArray is valid UTF-8
    static bool isValidUtf8(const QByteArray &data);

    // Decode with fallback: UTF-8 if valid, else Latin1
    static QString decode(const QByteArray &data);
};

#endif // UTF8UTILS_H
