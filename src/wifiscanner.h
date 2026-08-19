#ifndef WIFISCANNER_H
#define WIFISCANNER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QElapsedTimer>

#include "beacondetail.h"
// #include "src/qmlbeacondetail.h"

// 802.11 Information Element IDs
#define WLAN_EID_SSID               0
#define WLAN_EID_SUPP_RATES         1   // ID 1: Supported Rates
#define WLAN_EID_EXT_SUPP_RATES     50  // ID 50
#define WLAN_EID_TPC_REPORT         35  // ID 35: TPC Report
#define WLAN_EID_HT_CAP             45  // ID 45: HT Capabilities (802.11n / Wi-Fi 4)
// Tag Number: HT Information (802.11n D1.10) (61)
#define WLAN_EID_RSN                48  // ID 48: RSN (WPA2/WPA3)
#define WLAN_EID_VHT_CAP            191 // ID 191: VHT Capabilities (802.11ac / Wi-Fi 5)
// Tag Number: VHT Operation (192)
#define WLAN_EID_VENDOR_SPECIFIC    221 // ID 221: Vendor Specific
#define WLAN_EID_EXTENSION          255 // ID 255: Extension Element (Wi-Fi 6/7 等)

// Extended Element IDs (for ID 255)
#define WLAN_EID_EXT_HE_CAP         35  // Ext ID 35: HE Capabilities (802.11ax / Wi-Fi 6)
#define WLAN_EID_EXT_HE_OPERATION   36  // Ext ID 35: HE Operation
#define WLAN_EID_EXT_EHT_CAP        107 // Ext ID 107: EHT Capabilities (802.11be / Wi-Fi 7)
// OUI Types for WPA1
static const unsigned char WPA_OUI[] = { 0x00, 0x50, 0xf2, 0x01 };
static const unsigned char WMM_OUI[] = { 0x00, 0x50, 0xf2, 0x02 };
static const unsigned char ATHEROS_ADV_CAPABILITY_OUI[] = { 0x00, 0x03, 0x7f, 0x01 };

void parse_rsn_or_wpa_ie(const unsigned char *data, int len, BeaconDetail *detail, bool is_wpa1);
void parse_beacon_ies(const unsigned char *data, int data_len, BeaconDetail *detail);


class WifiScanner : public QObject
{
    Q_OBJECT
    friend class TestWifiScanner;
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
    void scanFinished(const QList<BeaconDetail> &results);
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
#ifdef IS_DESKTOP_LINUX
    bool checkPrivacyBit(uint16_t capability_info);
    void finalize_security_type(uint16_t capability_info, BeaconDetail *detail);
#endif
#ifdef Q_OS_ANDROID
    void setupAndroidJniCallback();
    void processJsonResults(const QString &jsonResult);
#endif
#ifdef Q_OS_WIN
    // 1. Declare a static C-style callback function
    static VOID WINAPI WlanNotificationCallback(PWLAN_NOTIFICATION_DATA pNotifData, PVOID pContext);
    void fetchWindowsScanResults();
#endif
    void setBusy(bool value);
    QString decodeSsid(const QByteArray &raw);
    bool m_busy = false;
    QElapsedTimer *scanTimer;
    const int scanCooldownMs = 8000;
    QList<QObject*> m_qobjectResults;
};

#endif // WIFISCANNER_H
