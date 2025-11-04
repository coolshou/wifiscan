#ifndef QMLBEACONDETAIL_H
#define QMLBEACONDETAIL_H

#include <QObject>
#include "beacondetail.h"


// Auxiliary class to wrap BeaconDetail for QML
class QmlBeaconDetail : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString bssid READ bssid CONSTANT)
    Q_PROPERTY(QString ssid READ ssid CONSTANT)
    Q_PROPERTY(int frequency READ frequency CONSTANT)
    Q_PROPERTY(int signal READ signal CONSTANT)
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

#endif // QMLBEACONDETAIL_H
