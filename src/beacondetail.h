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
    Q_PROPERTY(bool isHtSupported MEMBER isHtSupported)
    Q_PROPERTY(bool isVhtSupported MEMBER isVhtSupported)
    Q_PROPERTY(bool isHeSupported MEMBER isHeSupported)
    Q_PROPERTY(bool isEhtSupported MEMBER isEhtSupported)
    Q_PROPERTY(bool isUhrSupported MEMBER isUhrSupported)
    Q_PROPERTY(int bss_color MEMBER bss_color)
    Q_PROPERTY(bool bss_color_disable MEMBER bss_color_disable)
    Q_PROPERTY(QList<double> supportedRates MEMBER supportedRates)
    Q_PROPERTY(QStringList authList MEMBER authList)
    Q_PROPERTY(QStringList encryptionList MEMBER encryptionList)
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
    bool hasPrivacyBit = false;
    QList<double> supportedRates; // ID 1: 支援速率列表 (Mbps)
    int8_t txPower = 0;           // ID 35: 發射功率 (dBm)
    int8_t linkMargin = 0;        // ID 35: Link Margin
    bool isHtSupported = false;   // ID 45: 是否支援 802.11n (HT)
    bool supports40MHz = false;   // ID 45: 是否支援 40MHz 頻道
    bool isVhtSupported = false;  // ID 191: 是否支援 802.11ac (VHT)
    bool supports160MHz = false;  // 由 ID 191 (VHT), Ext 35 (HE), Ext 107 (EHT) 觸發
    bool supports320MHz = false; // 由 Ext 107 (EHT) 觸發
    QStringList authList;       // e.g., ["WPA2-PSK", "WPA3-SAE"]
    QStringList encryptionList; // e.g., ["CCMP (AES)"]
    int transmitpower = 0;
    bool isHeSupported = false; // 802.11ax (wifi-6) HE - ID 255 / Ext 35
    bool isEhtSupported = false; // 802.11be (Wi-Fi 7) EHT - ID 255 / Ext 107
    bool isUhrSupported = false; // 802.11bn (Wi-Fi 8) UHR
    int bss_color=0;
    bool bss_color_partial=false;
    bool bss_color_disable=false;
    QMap<int,QByteArray> elmIDs;
    QMap<int,QByteArray> elmExtIDs;
};

Q_DECLARE_METATYPE(BeaconDetail)

#endif // BEACONDETAIL_H
