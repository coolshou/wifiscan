#include "wifiscanner.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>

#if defined(Q_OS_WIN)
// Helper structures & converters
struct SecurityDetails {
    QString auth = "Open";
    QString cipher = "";
};

static QString getAuthName(DOT11_AUTH_ALGORITHM authAlgo) {
    switch (authAlgo) {
    case DOT11_AUTH_ALGO_80211_OPEN: return "Open";
    case DOT11_AUTH_ALGO_80211_SHARED_KEY: return "WEP";
    case DOT11_AUTH_ALGO_WPA: return "WPA-Enterprise";
    case DOT11_AUTH_ALGO_WPA_PSK: return "WPA-Personal";
    case DOT11_AUTH_ALGO_RSNA: return "WPA2-Enterprise";
    case DOT11_AUTH_ALGO_RSNA_PSK: return "WPA2-Personal";
    case DOT11_AUTH_ALGO_WPA3: return "WPA3-Enterprise";
    case DOT11_AUTH_ALGO_WPA3_SAE: return "WPA3-Personal";
    case DOT11_AUTH_ALGO_OWE: return "Enhanced-Open";
    default: return "Unknown";
    }
}

static QString getCipherName(DOT11_CIPHER_ALGORITHM cipherAlgo) {
    switch (cipherAlgo) {
    case DOT11_CIPHER_ALGO_NONE: return "None";
    case DOT11_CIPHER_ALGO_WEP40: return "WEP40";
    case DOT11_CIPHER_ALGO_TKIP: return "TKIP";
    case DOT11_CIPHER_ALGO_CCMP: return "AES (CCMP)";
    case DOT11_CIPHER_ALGO_WEP104: return "WEP104";
    case DOT11_CIPHER_ALGO_BIP: return "BIP";
    case DOT11_CIPHER_ALGO_GCMP: return "GCMP";
    default: return "Unknown";
    }
}
// Helper: Parse raw 802.11 Information Elements (IEs)
SecurityDetails parseBssIEs(const WLAN_BSS_ENTRY &bssEntry)
{
    SecurityDetails sec;

    if (bssEntry.ulIeSize == 0) {
        return sec;
    }

    // Pointer to start of Information Elements
    const BYTE *pIEs = (const BYTE *)&bssEntry + bssEntry.ulIeOffset;
    DWORD ieSize = bssEntry.ulIeSize;
    DWORD offset = 0;

    bool hasRsn = false;  // RSN (WPA2/WPA3) - Tag 0x30 (48)
    bool hasWpa = false;  // WPA1 - Tag 0xDD (221) with OUI 00:50:F2:01

    while (offset + 2 <= ieSize) {
        BYTE elementId = pIEs[offset];
        BYTE length = pIEs[offset + 1];

        if (offset + 2 + length > ieSize) break; // Out of bounds safety check

        const BYTE *pBody = &pIEs[offset + 2];

        // -------------------------------------------------------------
        // 1. Check for RSN IE (WPA2 / WPA3) -> Tag 0x30 (48)
        // -------------------------------------------------------------
        if (elementId == 0x30) {
            hasRsn = true;

            // Basic RSN parsing
            sec.auth = "WPA2-Personal";
            sec.cipher = "AES (CCMP)";

            // Check if RSN body contains WPA3/SAE flags or Suite selectors
            if (length >= 8) {
                // If the Auth Suite list contains AKM type 8 (SAE / WPA3)
                // You can inspect AKM suites inside pBody if deep parsing is needed
            }
        }
        // -------------------------------------------------------------
        // 2. Check for Vendor Specific IE (WPA1) -> Tag 0xDD (221)
        // -------------------------------------------------------------
        else if (elementId == 0xDD && length >= 6) {
            // Check for Microsoft WPA OUI: 00:50:F2:01
            if (pBody[0] == 0x00 && pBody[1] == 0x50 && pBody[2] == 0xF2 && pBody[3] == 0x01) {
                hasWpa = true;
                if (!hasRsn) {
                    sec.auth = "WPA-Personal";
                    sec.cipher = "TKIP";
                }
            }
        }

        offset += 2 + length;
    }

    // -------------------------------------------------------------
    // 3. Fallback: Check Capability Information for WEP / Open
    // -------------------------------------------------------------
    if (!hasRsn && !hasWpa) {
        // Bit 4 of Capability Information indicates Privacy (WEP)
        if (bssEntry.usCapabilityInformation & 0x0010) {
            sec.auth = "WEP";
            sec.cipher = "WEP";
        } else {
            sec.auth = "Open";
            sec.cipher = "None";
        }
    }

    return sec;
}
#endif
// #if defined(__linux__) && !defined(__ANDROID__)
    // #define IS_DESKTOP_LINUX
#ifdef IS_DESKTOP_LINUX
    // C-style includes for Netlink (ensure these are in your PKGCONFIG)
extern "C" {
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>
#include <errno.h>
}
#include <net/if.h> //‘if_nametoindex’
// This is required to pass data from the C callback back to the C++ class
static QList<BeaconDetail> *g_scan_results = nullptr;
// Netlink callback function (defined later)
static int handle_scan_result(struct nl_msg *msg, void *arg);
#elif defined(Q_OS_ANDROID)
// Android: JNI includes
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    #include <QJniObject>
    #include <QJniEnvironment>
    // #include <QNativeInterface/QAndroidApplication>
#else
    //(Qt5)
    #include <QtAndroidExtras/QAndroidJniObject>
    #include <QtAndroidExtras/QAndroidJniEnvironment>
    #include <QtAndroidExtras/QtAndroid>
#endif
#endif

#include "utf8utils.h"
#include "wifiutils.h"

// --- Global variables for the callback and policy ---
struct ScanContext {
    WifiScanner *scanner;
    bool completed = false;
};


#ifdef IS_DESKTOP_LINUX
// A basic policy to help parse the BSS attributes
static struct nla_policy bss_policy[NL80211_BSS_MAX + 1];

static void init_bss_policy() {
    bss_policy[NL80211_BSS_BSSID].type = NLA_UNSPEC;
    bss_policy[NL80211_BSS_FREQUENCY].type = NLA_U32;
    bss_policy[NL80211_BSS_SIGNAL_MBM].type = NLA_U32;
    bss_policy[NL80211_BSS_INFORMATION_ELEMENTS].type = NLA_UNSPEC;
}
#endif

void parse_rsn_or_wpa_ie(const unsigned char *data, int len, BeaconDetail *detail, bool is_wpa1)
{
    // Minimal IE length check
    if (len < 8) return;

    // Index 0-1: Version (2 bytes)
    // Index 2-5: Group Data Cipher Suite (4 bytes)
    // Index 6-7: Pairwise Cipher Suite Count (2 bytes)
    uint16_t pairwise_count = data[6] | (data[7] << 8);
    int offset = 8;

    // 1. Extract Pairwise Ciphers
    for (int i = 0; i < pairwise_count && (offset + 4) <= len; ++i) {
        uint32_t cipher = ((uint32_t)data[offset] << 24) |
                          ((uint32_t)data[offset+1] << 16) |
                          ((uint32_t)data[offset+2] << 8) |
                          (uint32_t)data[offset+3];

        uint32_t oui = cipher & 0xFFFFFF00;
        uint8_t type = cipher & 0xFF;

        if (oui == 0x000FAC00 || oui == 0x0050F200) {
            if (type == 2 && !detail->encryptionList.contains("TKIP"))
                detail->encryptionList.append("TKIP");
            else if (type == 4 && !detail->encryptionList.contains("CCMP (AES)"))
                detail->encryptionList.append("CCMP (AES)");
            else if ((type == 8 || type == 9) && !detail->encryptionList.contains("GCMP"))
                detail->encryptionList.append("GCMP");
        }
        offset += 4;
    }

    // 2. Extract AKM Suites (Authentication)
    if (offset + 2 <= len) {
        uint16_t akm_count = data[offset] | (data[offset+1] << 8);
        offset += 2;

        for (int i = 0; i < akm_count && (offset + 4) <= len; ++i) {
            uint32_t akm = ((uint32_t)data[offset] << 24) |
                           ((uint32_t)data[offset+1] << 16) |
                           ((uint32_t)data[offset+2] << 8) |
                           (uint32_t)data[offset+3];

            uint32_t oui = akm & 0xFFFFFF00;
            uint8_t type = akm & 0xFF;

            if (is_wpa1) {
                // WPA1 專用 OUI (00:50:F2)
                if (oui == 0x0050F200 || oui == 0x000FAC00) {
                    if (type == 1 && !detail->authList.contains("WPA-Enterprise"))
                        detail->authList.append("WPA-Enterprise");
                    else if (type == 2 && !detail->authList.contains("WPA-PSK"))
                        detail->authList.append("WPA-PSK");
                }
            } else {
                // RSN (WPA2 / WPA3) (OUI 00:0F:AC)
                if (type == 1 && !detail->authList.contains("WPA2-Enterprise"))
                    detail->authList.append("WPA2-Enterprise");
                else if (type == 2 && !detail->authList.contains("WPA2-PSK"))
                    detail->authList.append("WPA2-PSK");
                else if (type == 8 && !detail->authList.contains("WPA3-SAE"))
                    detail->authList.append("WPA3-SAE");
                else if (type == 18 && !detail->authList.contains("OWE"))
                    detail->authList.append("OWE");
                else if (type == 24 && !detail->authList.contains("WPA3-Enterprise"))
                    detail->authList.append("WPA3-Enterprise");
            }
            offset += 4;
        }
    }
}

void parse_beacon_ies(const unsigned char *data, int data_len, BeaconDetail *detail)
{
    // Start iterating from the beginning of the IE data buffer
    // Zero out the SSID details before parsing
    detail->ssid = QString();
    detail->ssid_len = 0;
    detail->supportedRates.clear();
    detail->encryptionList.clear();
    detail->authList.clear();
    int offset = 0;
    // bool has_privacy = false; // Set this if Capability Info bit 4 is set, or track via IEs

    // Loop through the buffer while there is enough room for an IE header (ID + Length)
    while (offset + 2 <= data_len) {
        uint8_t id = data[offset];
        uint8_t ie_len = data[offset + 1];

        // 避免長度溢位 (Malformed IE)
        if (offset + 2 + ie_len > data_len) {
            break;
        }

        const unsigned char *ie_data = &data[offset + 2];

        switch (id) {
        case WLAN_EID_SSID: {// ID 0
            detail->ssid_len = ie_len;
            bool is_hidden = (ie_len == 0);

            if (!is_hidden) {
                // 檢查是否全為 NULL Byte (\0)
                is_hidden = true;
                for (int i = 0; i < ie_len; ++i) {
                    if (ie_data[i] != 0x00) {
                        is_hidden = false;
                        break;
                    }
                }
            }
            if (is_hidden) {
                detail->ssid = "[hidden]";
            }else {
                detail->ssid = QString::fromUtf8((const char *)ie_data, ie_len);
            }
            break;
        }
        // case WLAN_EID_SUPP_RATES:    // ID 1: Supported Rates
        case WLAN_EID_EXT_SUPP_RATES:{
            for (int i = 0; i < ie_len; ++i) {
                double rate_mbps = (ie_data[i] & 0x7F) * 0.5;
                detail->supportedRates.append(rate_mbps);
            }
            break;
        }

        case WLAN_EID_TPC_REPORT: { // ID 35: TPC Report Transmit Power
            if (ie_len >= 2) {
                // Transmitted Power is the first data byte (at pos + 2)
                // It is a signed 8-bit value (char/qint8) in dBm.
                detail->txPower = (int8_t)ie_data[0];
                detail->linkMargin = (int8_t)ie_data[1];
            }
            break;
        }

        case WLAN_EID_HT_CAP: { // ID 45: 11n HT Capabilities (Wi-Fi 4)
            detail->isHtSupported = true;
            if (ie_len >= 2) {
                uint16_t ht_cap_info = ie_data[0] | (ie_data[1] << 8);
                detail->supports40MHz = (ht_cap_info & (1 << 1)) != 0;
            }
            break;
        }
            // Tag Number: HT Information (802.11n D1.10) (61)

        case WLAN_EID_RSN: { // ID 48
            parse_rsn_or_wpa_ie(ie_data, ie_len, detail, false);
            break;
        }
        case WLAN_EID_VHT_CAP: { // ID 191: 11ac VHT Capabilities (Wi-Fi 5)
            detail->isVhtSupported = true;
            if (ie_len >= 4) {
                uint32_t vht_cap_info = ie_data[0] | (ie_data[1] << 8) |
                                        (ie_data[2] << 16) | (ie_data[3] << 24);
                uint8_t chan_width = (vht_cap_info >> 2) & 0x03;
                // detail->supports160MHz = (chan_width > 0);
            }
            break;
        }
        // Tag Number: VHT Operation (192)
        case WLAN_EID_VENDOR_SPECIFIC: {// ID 221
            if (ie_len >= 4 && memcmp(ie_data, WPA_OUI, 4) == 0) {
                parse_rsn_or_wpa_ie(ie_data + 4, ie_len - 4, detail, true);
            }else {
                printf("[%s]Vendor Specific IE Data (Len: %d): ", detail->ssid.toUtf8().constData(),ie_len);
                for (int i = 0; i < ie_len; i++) {
                    printf("%02X ", ie_data[i]);
                }
                printf("\n");
            }
            break;
        }
        case WLAN_EID_EXTENSION: { // ID 255: Extended Elements
            if (ie_len < 1) break; // 長度不足以包含 Ext ID

            uint8_t ext_id = ie_data[0]; // 第一個 byte 是 Extended Element ID
            const unsigned char *ext_data = &ie_data[1];
            uint8_t ext_len = ie_len - 1;

            switch (ext_id) {
            case WLAN_EID_EXT_HE_CAP: {// Ext ID 35: HE Capabilities (Wi-Fi 6)
                detail->isHeSupported = true; // 支援 802.11ax
                // HE PHY Capabilities Info 通常位於 HE Capabilities 的第 6 個 byte 開始
                // (前 6 bytes 為 HE MAC Capabilities Info)
                if (ext_len >= 7) {
                    const unsigned char *he_phy_cap = &ext_data[6]; // HE PHY Cap Byte 0

                    // HE PHY Cap Byte 0, Bit 1: Channel Width Set
                    // (0 = 80MHz only, 1 = 160MHz or 80+80MHz)
                    if (he_phy_cap[0] & (1 << 1)) {
                        detail->supports160MHz = true;
                    }
                }
                break;
            }

            case WLAN_EID_EXT_HE_OPERATION: {
                // Ext Tag Number: Tag 255, HE Operation (36)
                // extended_id(1)+HE Operation Information(3)+BSS color(1)+HE-MCS(2)

                // The HE Operation Information starts at pos + 3 (len 3)
                // Byte 0 of HE Op Info (pos[3]) contains the BSS Color
                unsigned char he_op_info_byte_0 = ext_data[6];
                // qDebug("[%s]he_op_info_byte_0: 0x%02hhx",
                //        detail->ssid.toUtf8().constData(),
                //        he_op_info_byte_0);

                // The BSS Color is the lower 6 bits (Bits 0-5)
                // We use a bitwise AND with 0x3F (binary 0011 1111) to mask the value.
                int bss_color = (int)(he_op_info_byte_0 & 0x3F);
                detail->bss_color = bss_color;
                // qDebug() << "bss_color:" << bss_color;
                // .0.. .... = Partial BSS Color: (Bit 6)
                bool is_Partial = (he_op_info_byte_0 & 0x40) != 0;
                detail->bss_color_partial = is_Partial;
                // Optional: Check the BSS Color Disabled flag (Bit 7)
                bool is_disabled = (he_op_info_byte_0 & 0x80) != 0;
                detail->bss_color_disable = is_disabled;
                if (bss_color > 0) {
                    // qDebug() << "[" << detail->ssid << "]"
                    //          <<" BSS Color Found:" << bss_color
                    //          << " (Disabled:" << (is_disabled ? "Yes)" : "No)");
                } else {
                    // BSS Color value 0 means BSS Coloring is not used.
                    qDebug() << "BSS Coloring is not used (Value is 0)";
                }
                // Basic HE-MCS and NSS Set: 0xfffc
            }

            case WLAN_EID_EXT_EHT_CAP: {// Ext ID 107: EHT Capabilities (Wi-Fi 7)
                detail->isEhtSupported = true; // 支援 802.11be
                // EHT Capabilities 包含 MAC 與 PHY Cap Info
                // EHT PHY Capabilities Info 位於第 2 個 Byte (或 offset 根據 MAC cap 長度而定)
                if (ext_len >= 3) {
                    // Byte 1 或 Byte 2 包含 EHT Channel Width 資訊
                    // Bit 1: 320 MHz 支援標誌
                    uint8_t eht_phy_cap_0 = ext_data[1];

                    if (eht_phy_cap_0 & (1 << 1)) {
                        detail->supports320MHz = true;
                    }
                    // Wi-Fi 7 預設也會向下支援 160MHz
                    detail->supports160MHz = true;
                }
                break;
            }
            default:
                break;
            }
            break;
        }

        default:
            break;
        }

        offset += 2 + ie_len;
    }

    // Fallback security checks
    if (detail->authList.isEmpty()) {
        detail->authList.append("Open");
    }

}



#ifdef IS_DESKTOP_LINUX
// --- Netlink Callback Function ---

bool checkPrivacyBit(uint16_t capability_info)
{
    // Bit 4 (0x0010) 為 Privacy 標誌
    return (capability_info & (1 << 4)) != 0;
}
void finalize_security_type(uint16_t capability_info, BeaconDetail *detail)
{
    bool has_privacy = checkPrivacyBit(capability_info);
    detail->hasPrivacyBit = has_privacy;

    // 若既沒有 WPA 也不存在 RSN/WPA2/WPA3
    if (detail->authList.isEmpty() && detail->encryptionList.isEmpty()) {
        if (has_privacy) {
            // 有啟動 Privacy 但沒有任何 RSN/WPA IE -> WEP
            detail->authList.append("WEP");
            detail->encryptionList.append("WEP");
        } else {
            // 沒有 Privacy 標誌 -> 完全開放的 Open 網路
            detail->authList.append("Open");
            detail->encryptionList.append("None");
        }
    }
}

// This function is called by nl_recvmsgs_default() for each BSS entry.
static int handle_scan_result(struct nl_msg *msg, void *arg)
{
    Q_UNUSED(arg);

    struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct nlattr *bss[NL80211_BSS_MAX + 1];
    int rem;

    // 1. Parse main attributes from the nl80211 message
    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

    // Check for the BSS attribute (where the beacon/scan result is)
    if (!tb[NL80211_ATTR_BSS]) {
        return NL_SKIP; // Skip this message if no BSS data
    }

    // 2. Parse nested BSS attributes (using the defined policy)
    nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy);

    // 3. Extract Beacon Details
    BeaconDetail detail;
    uint16_t capability_info = 0;

    // BSSID (MAC Address)
    if (bss[NL80211_BSS_BSSID]) {
        const unsigned char *mac = (unsigned char *)nla_data(bss[NL80211_BSS_BSSID]);
        detail.bssid = QStringLiteral("%1:%2:%3:%4:%5:%6")
                           .arg(mac[0], 2, 16, QChar('0'))
                           .arg(mac[1], 2, 16, QChar('0'))
                           .arg(mac[2], 2, 16, QChar('0'))
                           .arg(mac[3], 2, 16, QChar('0'))
                           .arg(mac[4], 2, 16, QChar('0'))
                           .arg(mac[5], 2, 16, QChar('0')).toUpper();
        // qDebug() << "bssid: " << detail.bssid;
    }

    // Frequency
    if (bss[NL80211_BSS_FREQUENCY]) {
        detail.frequency = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
        //TODO: frequency to channel
        detail.channel = WifiUtils::frequencyToChannel(detail.frequency);
    }

    // Signal (nl80211 returns signal in mBm, need to convert to dBm)
    if (bss[NL80211_BSS_SIGNAL_MBM]) {
        // NL80211_BSS_SIGNAL is typically a nested attribute containing a 33-bit signal value.
        // For simplicity, we assume the raw value is in mBm (0.001 dBm)
        int mbm_signal = nla_get_s32(bss[NL80211_BSS_SIGNAL_MBM]);
        detail.signal = mbm_signal / 100; // Convert mBm to dBm
    }
    // Capability Information (用於 WEP / Privacy Bit 判斷)
    if (bss[NL80211_BSS_CAPABILITY]) {
        capability_info = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
    }
    // INFORMATION ELEMENTS: Raw data including SSID, Capabilities, etc.
    // INFORMATION ELEMENTS (優先使用 IES，不存在時退回使用 BEACON_IES)
    struct nlattr *ie_attr = bss[NL80211_BSS_INFORMATION_ELEMENTS];
    if (!ie_attr) {
        ie_attr = bss[NL80211_BSS_BEACON_IES];
    }
    if (ie_attr) {
        // struct nlattr *ie = bss[NL80211_BSS_INFORMATION_ELEMENTS];
        const unsigned char *data = (unsigned char *)nla_data(ie_attr);
        int data_len = nla_len(ie_attr);

        parse_beacon_ies(data , data_len, &detail);
    }
    // 判定網路加密狀態 (解決 WEP 與 Open 的判定)
    finalize_security_type(capability_info, &detail);
    // Add the parsed entry to the list
    if (g_scan_results) {
        g_scan_results->append(detail);
    }

    return NL_OK;
}
#endif

#ifdef Q_OS_ANDROID
// ------------------------------------------------------------------
//  NEW: JNI EXPORT FUNCTION
// This function must have a specific signature required by JNI and
// should be static. This is the entry point from the Java side.
// ------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_idv_coolshou_WifiService_sendScanResultsToCpp(JNIEnv *env, jclass clazz, jstring results)
{
    Q_UNUSED(clazz);

    // Convert jstring to QString
    QString jsonResult = QJniObject(results).toString();

    // Find the running C++ WifiScanner instance and call the slot on the main thread
    WifiScanner *scanner = QCoreApplication::instance()->findChild<WifiScanner*>("wifiScannerInstance");

    if (scanner) {
        // Use QMetaObject::invokeMethod to safely call the slot on the main thread
        QMetaObject::invokeMethod(scanner,
                                  "handleScanResults",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, jsonResult));
    } else {
        qWarning() << "WifiScanner instance not found for JNI callback.";
    }
}
#endif

WifiScanner::WifiScanner(QObject *parent)
    : QObject{parent}
{
#ifdef IS_DESKTOP_LINUX
    init_bss_policy();
#endif
#ifdef Q_OS_ANDROID
    // 1. Get the native Android Context jobject
    auto contextWrapper = QNativeInterface::QAndroidApplication::context();
    jobject contextObject = contextWrapper.object<jobject>();

    if (contextObject) {
        // 2. Wrap the jobject in a QAndroidJniObject (for calling Java methods)
        QJniObject context = QJniObject(contextObject);

        // 3. Call the static Java method to set the context
        QJniObject::callStaticMethod<void>(
            "tw/idv/coolshou/WifiService", // Your Java class package/name
            "setContext",
            "(Landroid/content/Context;)V", // Signature
            context.object<jobject>());
    } else {
        qCritical() << "Failed to retrieve native Android context!";
    }
    setupAndroidJniCallback();
#endif
    scanTimer = new QElapsedTimer();
}

void WifiScanner::startScan(const QString &iface)
{
    if (iface.isEmpty()){
        emit error("--- Not set interface ---");
        return;
    }
    if (scanTimer->isValid() && scanTimer->elapsed() < scanCooldownMs) {
        emit error("Scan blocked: cooldown active");
        return;
    }
    scanTimer->restart();

#if defined(Q_OS_WIN)
    // --- Windows Native Wifi API ---
    // HANDLE hClient = NULL;
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;

    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &m_hClient);
    if (dwResult != ERROR_SUCCESS) {
        emit error("Windows WLAN API initialization failed.");
        return;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    dwResult = WlanEnumInterfaces(m_hClient, NULL, &pIfList);
    if (dwResult != ERROR_SUCCESS || pIfList == NULL) {
        WlanCloseHandle(m_hClient, NULL);
        emit error("Failed to enumerate Windows wireless interfaces.");
        return;
    }

    bool found = false;

    // Match selected interface name or GUID with enumerated interfaces
    for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
        PWLAN_INTERFACE_INFO pIfInfo = (WLAN_INTERFACE_INFO *)&pIfList->InterfaceInfo[i];
        QString desc = QString::fromWCharArray(pIfInfo->strInterfaceDescription);

        if (desc == iface) {
            m_currentIfaceGuid = pIfInfo->InterfaceGuid;
            found = true;
            break;
        }
    }

    if (!found && pIfList->dwNumberOfItems > 0) {
        // Fallback: If no match found by description, pick the first interface
        m_currentIfaceGuid = pIfList->InterfaceInfo[0].InterfaceGuid;
        found = true;
    }

    if (pIfList != NULL) {
        WlanFreeMemory(pIfList);
    }

    if (!found) {
        WlanCloseHandle(m_hClient, NULL);
        emit error("Target wireless interface not found on Windows.");
        return;
    }
    dwResult = WlanRegisterNotification(
        m_hClient,
        WLAN_NOTIFICATION_SOURCE_ACM,
        TRUE,
        (WLAN_NOTIFICATION_CALLBACK)WlanNotificationCallback,
        this, // Context pointer to your class instance
        NULL,
        NULL
        );
    if (dwResult != ERROR_SUCCESS) {
        WlanCloseHandle(m_hClient, NULL);
        emit error("Failed WlanRegisterNotification.");
        return;
    }
    // Trigger async scan
    dwResult = WlanScan(m_hClient, &m_currentIfaceGuid, NULL, NULL, NULL);
    if (dwResult != ERROR_SUCCESS) {
        emit error(QString("Windows WlanScan failed with error code: %1").arg(dwResult));
        return;
    }
    this->setBusy(true);

    // Clean up handle
    // WlanCloseHandle(m_hClient, NULL);

    qDebug() << "--- Windows Wi-Fi scan requested successfully on interface:" << iface << "---";
#elif defined(IS_DESKTOP_LINUX)
    qDebug() << "--- Starting WiFi Scan on interface:" << iface << "---";
    // qDebug() << "--- (This may require root privileges) ---";

    // Run the blocking Netlink code in a separate thread
    QThread *thread = QThread::create([this, iface]() {
        this->setBusy(true);
        this->performNetlinkScan(iface);
    });

    // Clean up the thread when finished
    QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
#elif defined(Q_OS_ANDROID)
    // --- Android JNI Logic ---
    bool success;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    success = QJniObject::callStaticMethod<jboolean>(
#else
    success = QAndroidJniObject::callStaticMethod<jboolean>(
#endif
        "tw/idv/coolshou/WifiService",
        "startWifiScan",
        "()Z"); // Signature ()Z

    if (!success) {
        emit error("Failed to request Wi-Fi scan from Java. Check permissions/location.");
    }

#else
    // --- Other Platforms (Placeholder) ---
    emit error("Wi-Fi scanning is not implemented for this platform.");
    // emit scanFinished(0);
#endif
}

QList<QObject *> WifiScanner::scanResults() const
{
    return m_qobjectResults;
}
#ifdef IS_DESKTOP_LINUX
void WifiScanner::performNetlinkScan(const QString &iface)
{
    struct nl_sock *socket = nullptr;
    struct nl_msg *msg = nullptr;
    struct nl_cb *cb = nullptr;
    int family_id, if_index;

    // bool scanComplete = false;
    ScanContext ctx;
    int timeoutMs = 5000;  // Adjust as needed
    nl_recvmsg_msg_cb_t msg_cb = nullptr;
    int mcid;
    QElapsedTimer timer;
    int ret = 0;

    // --- Step 1: Initialization ---
    QList<BeaconDetail> results;
    g_scan_results = &results; // Set the global pointer for the C callback

    socket = nl_socket_alloc();
    if (!socket) {
        emit error("Failed to allocate Netlink socket.");
        goto cleanup;
    }

    // Disable sequence checking for simplicity in a dump request
    nl_socket_disable_seq_check(socket);

    if (genl_connect(socket) < 0) {
        emit error("Failed to connect to Generic Netlink.");
        goto cleanup;
    }

    family_id = genl_ctrl_resolve(socket, "nl80211");
    if (family_id < 0) {
        emit error("nl80211 family not found (Is it a modern Linux system?).");
        goto cleanup;
    }
    // nl_socket_add_membership(socket, NL80211_MULTICAST_GROUP_SCAN);
    mcid = genl_ctrl_resolve_grp(socket, "nl80211", "scan");
    if (mcid < 0) {
        emit error("Failed to resolve nl80211 scan multicast group.");
        goto cleanup;
    }
    if (nl_socket_add_membership(socket, mcid) < 0) {
        emit error("Failed to join nl80211 scan multicast group.");
        goto cleanup;
    }

    if_index = if_nametoindex(iface.toLocal8Bit().constData());
    if (if_index == 0) {
        emit error(QString("Wireless interface '%1' not found.").arg(iface));
        goto cleanup;
    }

    // --- Step 2: Trigger Scan (Optional, but often necessary for fresh data) ---
    // If you need *fresh* beacon data, uncomment this block.
    // However, it often requires root permissions and time to complete.

    msg = nlmsg_alloc();
    if (!msg) {
        emit error("Failed to allocate Netlink message for TRIGGER_SCAN.");
        goto cleanup;
    }

    genlmsg_put(msg, 0, 0, family_id, 0, 0, NL80211_CMD_TRIGGER_SCAN, 0);
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);
    // nla_put_flag(msg, NL80211_ATTR_SCAN_IE_ALLOW_CHANNEL_SWITCH); // Optional flags

    ret = nl_send_auto(socket, msg);
    nlmsg_free(msg);
    msg = nullptr;
    if (ret < 0) {
        emit error(QString("Failed to send TRIGGER_SCAN command: %1").arg(ret));
        goto cleanup;
    }
    // Optional: wait for scan to complete (omitted for minimal example)

    msg_cb = [](struct nl_msg *msg, void *arg) -> int {
        auto *ctx = static_cast<ScanContext *>(arg);
        struct nlmsghdr *nlh = nlmsg_hdr(msg);
        struct genlmsghdr *ghdr = static_cast<genlmsghdr *>(nlmsg_data(nlh));

        if (ghdr->cmd == NL80211_CMD_NEW_SCAN_RESULTS) {
            qDebug() << "Scan completed.";
            // *static_cast<bool *>(arg) = true;
            ctx->completed = true;
            ctx->scanner->setBusy(false);
        } else if (ghdr->cmd == NL80211_CMD_SCAN_ABORTED) {
            qDebug() << "Scan aborted.";
            // *static_cast<bool *>(arg) = true;
            ctx->completed = true;
            ctx->scanner->setBusy(false);
        }

        return NL_OK;
    };

    ctx.scanner = this;
    nl_socket_modify_cb(socket, NL_CB_VALID, NL_CB_CUSTOM, msg_cb, &ctx);


    timer.start();
    while (!ctx.completed && timer.elapsed() < timeoutMs) {
        nl_recvmsgs_default(socket);  // Blocks until message or timeout
    }


    // --- Step 3: NL80211_CMD_GET_SCAN (Dump/Retrieve Beacon Data) ---
    msg = nlmsg_alloc();
    if (!msg) {
        emit error("Failed to allocate Netlink message for GET_SCAN.");
        goto cleanup;
    }

    // Set up the message header: family_id, NLM_F_DUMP (for a list of results), NL80211_CMD_GET_SCAN
    genlmsg_put(msg, 0, 0, family_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);
    // Add NL80211_ATTR_SCAN_FLAGS if needed

    // Send the message
    ret = nl_send_auto(socket, msg);
    nlmsg_free(msg);
    msg = nullptr;
    if (ret < 0) {
        emit error(QString("Failed to send GET_SCAN command: %1").arg(ret));
        goto cleanup;
    }

    // --- Step 4: Set Callback and Receive Messages ---
    // Use the default callback handler
    cb = nl_cb_alloc(NL_CB_DEFAULT);
    nl_socket_set_cb(socket, cb);
    // Set our custom function to handle valid BSS/scan result messages
    nl_cb_set(nl_socket_get_cb(socket), NL_CB_VALID, NL_CB_CUSTOM, handle_scan_result, nullptr);

    // This blocking call processes all messages from the kernel
    ret = nl_recvmsgs_default(socket);
    if (ret < 0) {
        emit error(QString("Failed to receive Netlink messages: %1").arg(ret));
        goto cleanup;
    }

    // --- Step 5: Success ---
#ifdef Q_OS_ANDROID
    // Convert results to QmlBeaconDetail objects for QML
    for (const auto &detail : results) {
        m_qobjectResults.append(new QmlBeaconDetail(detail));
    }
    // emit scanFinished(m_qobjectResults.size());
#endif
    // g_scan_results = nullptr;
    emit scanResultsChanged();
    qDebug() << "sites results:" << results.count();
    emit scanFinished(results);

cleanup:
    nl_cb_put(cb); // after nl_recvmsgs_default
    // Clean up resources
    if (socket) {
        nl_socket_free(socket);
    }
    g_scan_results = nullptr; // Clear the global pointer
    qDebug() << "Netlink operation thread finished.";
    setBusy(false);
}
#else
void WifiScanner::performNetlinkScan(const QString &iface)
{
    // 在 Android/Windows 上發出錯誤信號或呼叫 JNI
    emit error("performNetlinkScan not implemented for this platform yet.");
}
// ----------------------------------------------------
//          ANDROID SPECIFIC IMPLEMENTATION
// ----------------------------------------------------

#ifdef Q_OS_ANDROID
void WifiScanner::setupAndroidJniCallback()
{
    // Configure the Java side to call the C++ slot "handleScanResults"
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QJniObject::callStaticMethod<void>(
#else
    QAndroidJniObject::callStaticMethod<void>(
#endif
        "tw/idv/coolshou/WifiService",
        "setCallbackReceiver",
        "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString("WifiScanner").object(), // Class name used in JNI call
        QJniObject::fromString("handleScanResults").object());

    qDebug() << "Android JNI callback setup complete.";
}
void WifiScanner::handleScanResults(const QString &jsonResult)
{
    qDebug() << "Received scan results from Java/Android.";
    processJsonResults(jsonResult);
}

void WifiScanner::processJsonResults(const QString &jsonResult)
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
        detail.signal = obj["level"].toInt(); // Android uses 'level' for dBm
        detail.capabilities = obj["capabilities"].toString();

        m_qobjectResults.append(new QmlBeaconDetail(detail));
    }

    emit scanResultsChanged();
    // emit scanFinished(m_qobjectResults.size());
}
#endif

#endif

#ifdef Q_OS_WIN
// Callback function signature for Windows API
VOID WINAPI WifiScanner::WlanNotificationCallback(PWLAN_NOTIFICATION_DATA pNotifData, PVOID pContext)
{
    if (pNotifData && pNotifData->NotificationSource == WLAN_NOTIFICATION_SOURCE_ACM) {
        if (pNotifData->NotificationCode == wlan_notification_acm_scan_complete) {
            // Retrieve interface GUID from context or notification data
            GUID* pIfaceGuid = (GUID*)pNotifData->pData;

            // Re-route back to Qt thread to execute safely
            WifiScanner* scanner = static_cast<WifiScanner*>(pContext);
            QMetaObject::invokeMethod(scanner, "fetchWindowsScanResults", Qt::QueuedConnection);
        }
    }
}
void WifiScanner::fetchWindowsScanResults()
{
    QList<BeaconDetail> results;

    PWLAN_BSS_LIST pBssList = NULL;

    // Fetch detailed BSS list (Access Points with BSSID and Frequency)
    DWORD dwResult = WlanGetNetworkBssList(
        m_hClient,
        &m_currentIfaceGuid,
        NULL,                   // pDot11Ssid: NULL retrieves all SSIDs
        dot11_BSS_type_any,     // BSS type
        FALSE,                  // bSecurityEnabled
        NULL,                   // Reserved
        &pBssList
        );

    if (dwResult != ERROR_SUCCESS || pBssList == NULL) {
        emit error("Failed to retrieve Windows Wi-Fi BSS list.");
        return;
    }

    for (DWORD i = 0; i < pBssList->dwNumberOfItems; i++) {
        WLAN_BSS_ENTRY bssEntry = pBssList->wlanBssEntries[i];

        // 1. BSSID (MAC Address)
        QString bssid = QString("%1:%2:%3:%4:%5:%6")
                            .arg(bssEntry.dot11Bssid[0], 2, 16, QChar('0'))
                            .arg(bssEntry.dot11Bssid[1], 2, 16, QChar('0'))
                            .arg(bssEntry.dot11Bssid[2], 2, 16, QChar('0'))
                            .arg(bssEntry.dot11Bssid[3], 2, 16, QChar('0'))
                            .arg(bssEntry.dot11Bssid[4], 2, 16, QChar('0'))
                            .arg(bssEntry.dot11Bssid[5], 2, 16, QChar('0'))
                            .toUpper();

        // 2. SSID
        QString ssid = QString::fromUtf8((char *)bssEntry.dot11Ssid.ucSSID,
                                         bssEntry.dot11Ssid.uSSIDLength);
        if (ssid.isEmpty()) {
            ssid = "<Hidden>";
        }

        // 3. Frequency (kHz to MHz)
        // ulChCenterFrequency is stored in kHz (e.g., 2412000 kHz = 2412 MHz)
        int frequency = bssEntry.ulChCenterFrequency / 1000;

        // 4. Signal (Percentage 0-100% converted to dBm estimate)
        // bssEntry.lRssi gives exact dBm signal strength directly!
        int signalDbm = bssEntry.lRssi; // e.g., -65 dBm
        ULONG signalQuality = bssEntry.uLinkQuality; // 0 to 100%

        // 5. Auth & Encryption
        SecurityDetails sec = parseBssIEs(bssEntry);

        BeaconDetail beaconDetail;
        beaconDetail.bssid = bssid;
        beaconDetail.ssid = ssid;
        beaconDetail.frequency = frequency;
        beaconDetail.channel = WifiUtils::frequencyToChannel(frequency);
        beaconDetail.signal = signalDbm;
        beaconDetail.authList = {sec.auth};
        beaconDetail.encryptionList = {sec.cipher};
        results.append(beaconDetail);
    }
    if (pBssList != NULL) {
        WlanFreeMemory(pBssList);
    }
    emit scanFinished(results);
    this->setBusy(false);
}
#endif

void WifiScanner::setBusy(bool value)
{
    if (m_busy == value)
        return;
    m_busy = value;
    emit busyChanged();
}

QStringList WifiScanner::getWirelessInterfaces()
{

#ifdef Q_OS_ANDROID
    // --- Android JNI Logic ---
    bool isAvailable;
    // Call the Java method to check if the Wi-Fi service is present
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    isAvailable = QJniObject::callStaticMethod<jboolean>(
#else
    isAvailable = QAndroidJniObject::callStaticMethod<jboolean>(
#endif
        "tw/idv/coolshou/WifiService",
        "isWifiInterfaceAvailable",
        "()Z"); // Signature ()Z (No args, returns boolean)

    if (isAvailable) {
        // On Android, we typically only deal with the main interface.
        // We use a conventional name like "wlan0" or "AndroidWi-Fi"
        // as a functional identifier for the UI.
        return QStringList() << "AndroidWi-Fi";
    } else {
        return QStringList();
    }
#elif defined(Q_OS_WIN)
    // --- Windows Native Wifi API ---
    QStringList interfaces;
    HANDLE hClient = NULL;
    DWORD dwMaxClient = 2; // Client version for Windows Vista / 7 and higher
    DWORD dwCurVersion = 0;

    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS) {
        return interfaces;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
    if (dwResult == ERROR_SUCCESS && pIfList != NULL) {
        for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
            PWLAN_INTERFACE_INFO pIfInfo = (WLAN_INTERFACE_INFO *)&pIfList->InterfaceInfo[i];

            // Convert WCHAR description (e.g., "Intel(R) Wi-Fi 6 AX201 160MHz") to QString
            QString interfaceName = QString::fromWCharArray(pIfInfo->strInterfaceDescription);
            interfaces.append(interfaceName);
        }
    }

    // Free resources
    if (pIfList != NULL) {
        WlanFreeMemory(pIfList);
    }
    WlanCloseHandle(hClient, NULL);

    return interfaces;
#elif defined(IS_DESKTOP_LINUX)
    QStringList interfaces;

    struct nl_sock *sock = nl_socket_alloc();
    if (!sock) return interfaces;

    if (genl_connect(sock) < 0) {
        nl_socket_free(sock);
        return interfaces;
    }

    int family_id = genl_ctrl_resolve(sock, "nl80211");
    if (family_id < 0) {
        nl_socket_free(sock);
        return interfaces;
    }

    struct nl_msg *msg = nlmsg_alloc();
    if (!msg) {
        nl_socket_free(sock);
        return interfaces;
    }

    genlmsg_put(msg, 0, 0, family_id, 0, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE, 0);

    QList<QString> result;
    nl_cb *cb = nl_cb_alloc(NL_CB_DEFAULT);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
              [](struct nl_msg *msg, void *arg) -> int {
                  QList<QString> *list = static_cast<QList<QString> *>(arg);
                  struct genlmsghdr *gnlh = static_cast<genlmsghdr *>(nlmsg_data(nlmsg_hdr(msg)));
                  struct nlattr *tb[NL80211_ATTR_MAX + 1];
                  nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
                            genlmsg_attrlen(gnlh, 0), nullptr);

                  if (tb[NL80211_ATTR_IFNAME] && tb[NL80211_ATTR_WIPHY]) {
                      QString name = QString::fromUtf8((char *)nla_data(tb[NL80211_ATTR_IFNAME]));
                      list->append(name);
                  }
                  return NL_OK;
              },
              &result);

    nl_send_auto(sock, msg);
    nl_recvmsgs(sock, cb);

    nl_cb_put(cb);
    nlmsg_free(msg);
    nl_socket_free(sock);
    return result;
#else
    // --- Other Platforms ---
    return QStringList();
#endif

}

bool WifiScanner::isBusy() const
{
    return m_busy;
}
