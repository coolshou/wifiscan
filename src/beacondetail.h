#ifndef BEACONDETAIL_H
#define BEACONDETAIL_H

// beacondetail.h
#pragma once
#include <QObject>
#include <QMap>
#include <QByteArray>

class BeaconDetail {
    Q_GADGET
    Q_PROPERTY(QString ssid MEMBER ssid)
    Q_PROPERTY(QString bssid MEMBER bssid)
    Q_PROPERTY(int frequency MEMBER frequency)
    Q_PROPERTY(int channel MEMBER channel)
    Q_PROPERTY(int signal MEMBER signal)
    Q_PROPERTY(int transmitpower MEMBER transmitpower)
    Q_PROPERTY(bool is11n MEMBER is11n)
    Q_PROPERTY(bool is11ac MEMBER is11ac)
    Q_PROPERTY(bool is11ax MEMBER is11ax)
    Q_PROPERTY(bool is11be MEMBER is11be)
    Q_PROPERTY(bool is11bn MEMBER is11bn)
    Q_PROPERTY(int bss_color MEMBER bss_color)
    Q_PROPERTY(bool bss_color_disable MEMBER bss_color_disable)
    Q_PROPERTY(QString capabilities MEMBER capabilities)
    Q_PROPERTY(QMap<int,QByteArray> elmids MEMBER elmIDs)
    Q_PROPERTY(QMap<int,QByteArray> elmextids MEMBER elmExtIDs)

public:
    QString ssid;
    int ssid_len;
    QString bssid;
    int frequency = 0;
    int channel=0;
    int signal = 0;
    QString capabilities;
    int transmitpower = 0;
    bool is11n=false; // wifi-4 HT
    bool is11ac=false; // wifi-5 VHT
    bool is11ax=false; // wifi-6 HE
    // wifi-6e
    bool is11be=false; // wifi-7 EHT
    bool is11bn=false; // wifi-8 UHR
    int bss_color=0;
    bool bss_color_partial=false;
    bool bss_color_disable=false;
    QMap<int,QByteArray> elmIDs;
    QMap<int,QByteArray> elmExtIDs;
};

#endif // BEACONDETAIL_H
