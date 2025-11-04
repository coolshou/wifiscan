#ifndef WIFISCANNERJNI_H
#define WIFISCANNERJNI_H

#include <QObject>
#include <QList>
#include <QString>

struct BeaconDetail {
    QString bssid;
    QString ssid;
    int frequency; // Mhz
    int signal;    // dBm
    QString capabilities;
};

class WifiScannerJni : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<QObject*> scanResults READ scanResults NOTIFY scanResultsChanged)

public:
    explicit WifiScannerJni(QObject *parent = nullptr);

    QList<QObject*> scanResults() const;

public slots:
    // 呼叫 Java 方法來開始掃描
    void startScan();

signals:
    void scanResultsChanged();
    void scanFinished(int count);
    void error(const QString &message);

private slots:
    // 接收來自 Java 的掃描結果 (由 QAndroidJniObject::callMethod 調用)
    void handleScanResults(const QString &jsonResult);

private:
    // 儲存掃描結果
    QList<BeaconDetail> m_results;
    QList<QObject*> m_qobjectResults;
};

// 輔助類：將 BeaconDetail 包裝成 QObject 以便在 QML 中使用
class QmlBeaconDetail : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString bssid READ bssid CONSTANT)
    Q_PROPERTY(QString ssid READ ssid CONSTANT)
    Q_PROPERTY(int frequency READ frequency CONSTANT)
    Q_PROPERTY(int signal READ signal CONSTIDANT)
    Q_PROPERTY(QString capabilities READ capabilities CONSTANT)

public:
    QmlBeaconDetail(const BeaconDetail& detail, QObject* parent = nullptr);
    QString bssid() const { return m_detail.bssid; }
    QString ssid() const { return m_detail.ssid; }
    int frequency() const { return m_detail.frequency; }
    int signal() const { return m_detail.signal; }
    QString capabilities() const { return m_detail.capabilities; }

private:
    BeaconDetail m_detail;
};

#endif // WIFISCANNERJNI_H
