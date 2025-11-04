#include "wifi_scanner_jni.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>

#ifdef Q_OS_ANDROID
// Qt6 does not have QtAndroidExtras
// #include <QtAndroidExtras/QAndroidJniObject>
// #include <QtAndroidExtras/QAndroidJniEnvironment>
#endif

// --- QmlBeaconDetail 實現 ---
QmlBeaconDetail::QmlBeaconDetail(const BeaconDetail& detail, QObject* parent)
    : QObject(parent), m_detail(detail) {}

// --- WifiScannerJni 實現 ---
WifiScannerJni::WifiScannerJni(QObject *parent) : QObject(parent)
{
    // 將 C++ 槽連接到一個 Android Java 方法，以便 Java 可以回呼 C++
    #ifdef Q_OS_ANDROID
    QAndroidJniObject::callStaticMethod<void>(
        "idv/coolshou/WifiService",
        "setCallbackReceiver",
        "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString("WifiScannerJni").object(),
        QJniObject::fromString("handleScanResults").object());
    #endif
}

QList<QObject*> WifiScannerJni::scanResults() const
{
    return m_qobjectResults;
}

void WifiScannerJni::startScan()
{
#ifdef Q_OS_ANDROID
    // 檢查權限 (Android Rulings)
    // 實際應用中需要先呼叫 QAndroidJniObject::requestPermissions

    // 呼叫 Java 靜態方法開始掃描
    bool success = QAndroidJniObject::callStaticMethod<jboolean>(
        "idv/coolshou/WifiService",
        "startWifiScan",
        "()Z"); // 簽名為 ()Z (無參數，返回 boolean)

    if (!success) {
        emit error("Failed to start Wi-Fi scan. Check permissions and location service.");
    } else {
        qDebug() << "Android Wi-Fi scan requested.";
    }
#else
    emit error("startScan is only implemented for Android.");
#endif
}

void WifiScannerJni::handleScanResults(const QString &jsonResult)
{
    qDeleteAll(m_qobjectResults);
    m_qobjectResults.clear();

    QJsonDocument doc = QJsonDocument::fromJson(jsonResult.toUtf8());
    QJsonArray array = doc.array();

    for (const QJsonValue &value : array) {
        QJsonObject obj = value.toObject();

        BeaconDetail detail;
        detail.bssid = obj["bssid"].toString();
        detail.ssid = obj["ssid"].toString();
        detail.frequency = obj["frequency"].toInt();
        detail.signal = obj["signal"].toInt(); // dBm
        detail.capabilities = obj["capabilities"].toString();

        // 將 C++ 結構包裝成 QObject，並加入到 QList<QObject*>
        m_qobjectResults.append(new QmlBeaconDetail(detail));
    }

    emit scanResultsChanged();
    emit scanFinished(m_qobjectResults.size());
    qDebug() << "Scan results received. Count:" << m_qobjectResults.size();
}
