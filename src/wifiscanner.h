#ifndef WIFISCANNER_H
#define WIFISCANNER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QElapsedTimer>

#include "src/qmlbeacondetail.h"

class WifiScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QList<QObject*> scanResults READ scanResults NOTIFY scanResultsChanged)
public:
    explicit WifiScanner(QObject *parent = nullptr);
    // Call this to start the scan thread
    Q_INVOKABLE void startScan(const QString &iface);
    QList<QObject*> scanResults() const;
    QStringList getWirelessInterfaces();
    Q_INVOKABLE bool isBusy() const;
signals:
    // Signal emitted when the scan is complete
#ifdef IS_DESKTOP_LINUX
    void scanFinished(const QList<BeaconDetail> &results);
#else
    void scanFinished(int count);
#endif
    // Signal emitted on error
    void error(const QString &message);
    void busyChanged();
    void scanResultsChanged();
private slots:
#ifdef Q_OS_ANDROID
    // This slot receives results from the Android Java callback
    void handleScanResults(const QString &jsonResult);
#endif
private:
    // The actual blocking Netlink function that runs in a separate thread
    void performNetlinkScan(const QString &iface);
#ifdef Q_OS_ANDROID
    void setupAndroidJniCallback();
    void processJsonResults(const QString &jsonResult);
#endif

    void setBusy(bool value);
    QString decodeSsid(const QByteArray &raw);
    bool m_busy = false;
    QElapsedTimer *scanTimer;
    const int scanCooldownMs = 8000;
    QList<QObject*> m_qobjectResults;
};

#endif // WIFISCANNER_H
