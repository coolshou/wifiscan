#include "wifiscanner.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>

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

#include "src/utf8utils.h"
#include "src/wifiutils.h"

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

#ifdef IS_DESKTOP_LINUX
// --- Netlink Callback Function ---
void parse_beacon_ies(const unsigned char *data, int data_len, BeaconDetail *detail)
{
    // Start iterating from the beginning of the IE data buffer
    const unsigned char *pos = data;
    const unsigned char *end = data + data_len;

    // Zero out the SSID details before parsing
    detail->ssid = QString();
    detail->ssid_len = 0;

    // Loop through the buffer while there is enough room for an IE header (ID + Length)
    while (pos + 2 <= end) {

        // --- 1. Read the IE Header ---
        unsigned char element_id = pos[0];
        unsigned char element_len = pos[1]; // Length of the payload only (0-255)

        // --- 2. Check for buffer integrity (Is the full payload inside the buffer?) ---
        if (pos + 2 + element_len > end) {
            // Error: The buffer is truncated or corrupted
            // The declared length of the element goes beyond the end of the data_len
            fprintf(stderr, "IE ID 0x%02x length (%d) exceeds remaining buffer.\n",
                    element_id, element_len);
            break;
        }

        // --- 3. Process the Element ID (The SSID is ID 0x00) ---
        if (element_id == 0x00) {
            // Check for a non-hidden SSID
            if (element_len > 0) {
                // 1. Get a pointer to the start of the raw SSID bytes
                const char *ssid_raw_ptr = reinterpret_cast<const char *>(pos + 2);

                // 2. Create a QByteArray from the raw data and the known length
                // QByteArray is used as a safe container for the raw bytes
                QByteArray raw_ssid(ssid_raw_ptr, element_len);

                // 3. Decode the raw bytes into the QString, assuming UTF-8
                detail->ssid = QString::fromUtf8(raw_ssid);
                // break;

            } else {
                // Length is 0, indicating a Hidden SSID
                // You can set a special flag or string here
                detail->ssid = "[hidden]";
                // break;
            }
        }
        // Tag Number: Supported Rates (1)
        if (element_id == 1) {
            // The length can be up to 8 bytes
            if (element_len > 0 && element_len <= 8) {
                // Start pointer at the rates payload
                const unsigned char *rates_ptr = pos + 2;
                for (int i = 0; i < element_len; ++i) {
                    unsigned char rate_byte = rates_ptr[i];

                    // Check if it's a Basic Rate (Bit 7 is set)
                    bool is_basic = (rate_byte & 0x80) != 0;

                    // Get the rate value (Bits 6-0)
                    // The value is in units of 0.5 Mbps (500 kbps)
                    unsigned char rate_value = rate_byte & 0x7F;

                    // Calculate the actual rate in Mbps (as a float or int*10 for precision)
                    double actual_rate_mbps = rate_value * 0.5;

                    // qDebug() << "Rate:" << actual_rate_mbps << "Mbps, Basic:" << is_basic;

                    // TODO: You would store these values in a list within your detail struct
                    // detail->supported_rates.append({actual_rate_mbps, is_basic});
                }
            } else {
                qWarning() << "Supported Rates IE has invalid length:" << element_len;
            }
        }
        if (element_id == 35) {
            //Tag: TPC Report Transmit Power (35)
            if (element_len == 2) {
                // Transmitted Power is the first data byte (at pos + 2)
                // It is a signed 8-bit value (char/qint8) in dBm.

                // 1. Get the raw signed byte
                const char *tx_power_raw = reinterpret_cast<const char *>(pos + 2);

                // 2. Convert the signed char byte to a standard integer type
                // We use qint8 to ensure it's treated as a signed byte (like -5, 10, etc.)
                qint8 transmitted_power_dbm = *tx_power_raw;

                // qDebug() << "[" << detail->bssid << "]txpower:" << transmitted_power_dbm;
                detail->transmitpower = transmitted_power_dbm;

            }
        }
        if (element_id == 45) {
            // Tag Number: HT Capabilities (802.11n D1.10) (45)
            if (element_len > 0) {
                detail->is11n = true;
            }
        }
        // Tag Number: HT Information (802.11n D1.10) (61)

        if (element_id == 191) {
            // Tag Number: VHT Capabilities (191)
            if (element_len > 0) {
                detail->is11ac = true;
            }
        }
        // Tag Number: VHT Operation (192)

        //     // 2. Capability Information (Element ID 10) - Very complex, just a placeholder
        //     if (data[0] == 10 && data[1] == 2) {
        //         // Capability info is usually part of the fixed fields, not an IE, but
        //         // a proper beacon frame parser would handle this.
        //         // For simplicity, let's just mark the capabilities data is present.
        //         detail.capabilities = "IEs Found";
        //     }


        // qDebug() << "element_id:" << element_id;
        if (element_id < 255){
            detail->elmIDs[element_id] = QByteArray(reinterpret_cast<const char *>(pos + 2),
                                                    element_len);
            // qDebug() << "detail.elmIDs:" + QString::number(detail->elmIDs.keys().length());
        }else{
            // Check for minimum length: 1 byte for Ext. ID + 1 byte payload
            if (element_len < 1) {
                qWarning() << "Extended Tag IE has minimum length error.";
                // Move to next IE: 1 byte ID + 1 byte Length + element_len payload
                pos += 2 + element_len;
                continue;
            }
            // Get the Extended Element ID (Type)
            unsigned char extended_id = pos[2];

            // Check if this Extended Tag contains the HE Capabilities (Type 35)
            if (extended_id == 35) {
                // Ext Tag Number: Tag 255, HE Capabilities (35)
                detail->is11ax = true;

                // The HE Capabilities payload starts at offset 3 of the main IE
                const unsigned char *he_data_ptr = pos + 3;

                // The length of the HE Capabilities payload is element_len - 1
                int he_data_len = element_len - 1;

                // --- Process HE Capabilities Payload ---
                // The first two bytes (he_data_ptr[0] and he_data_ptr[1]) are
                // typically the HE MAC Capabilities Info field.

                if (he_data_len >= 2) {
                    // Example: Extract the first two bytes (MAC Capabilities)
                    quint16 mac_cap_info = (quint16)he_data_ptr[0] | (quint16)he_data_ptr[1] << 8;

                    // qDebug() << "Found HE Capabilities (Wi-Fi 6). MAC Cap Info:"
                    //          << QString::number(mac_cap_info, 16);

                    // You would continue to parse the remaining bytes
                    // (PHY capabilities, TX/RX L-SIG, MCS/NSS Set, etc.) here.
                    // Store the raw HE capabilities data in your struct for later parsing.
                    // detail->he_capabilities_raw = QByteArray::fromRawData(
                    //    reinterpret_cast<const char*>(he_data_ptr), he_data_len);
                } else {
                    qWarning() << "HE Capabilities element too short.";
                }
            }
            if (extended_id == 36) {
                // Ext Tag Number: Tag 255, HE Operation (36)
                // extended_id(1)+HE Operation Information(3)+BSS color(1)+HE-MCS(2)

                // The HE Operation Information starts at pos + 3 (len 3)
                // Byte 0 of HE Op Info (pos[3]) contains the BSS Color
                unsigned char he_op_info_byte_0 = pos[6];
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
                    qDebug() << "[" << detail->ssid << "]"
                             <<" BSS Color Found:" << bss_color
                             << " (Disabled:" << (is_disabled ? "Yes)" : "No)");
                } else {
                    // BSS Color value 0 means BSS Coloring is not used.
                    qDebug() << "BSS Coloring is not used (Value is 0)";
                }
                // Basic HE-MCS and NSS Set: 0xfffc
            }

            if (extended_id == 108) {
                // EHT Capabilities (108)
                detail->is11be = true;

            }
            if (extended_id == 106) {
                // EHT Operation (106)
            }

            // UHR Operation
            // UHR Capabilities
            //         int iLen = static_cast<unsigned char>(data[1]);
            //         // const char *extidPtr = reinterpret_cast<const char *>(data[2]);
            //         detail.elmExtIDs[data[2]] = QByteArray(reinterpret_cast<const char *>(&data[3]),
            //                                                iLen - 1);
            //         // qDebug() << "detail.elmExtIDs:" +  QString::number(detail.elmExtIDs.keys().length());

        }

        //     // Tag Number: Traffic Indication Map (TIM) (5)
        //     // Tag Number: RSN Information (48)
        //     // Tag Number: Extended Capabilities (127)
        //     // Tag Number: Vendor Specific (221)
        //     //      OUI: 00:50:f2 (Microsoft Corp.)
        //     //      Vendor Specific OUI Type: 1
        //     // TODO: many other Element

        // --- 4. Advance the Pointer (The most crucial step!) ---
        // Move the pointer past the current element:
        // 1 byte (ID) + 1 byte (Length) + element_len (Payload)
        pos += 2 + element_len;

        // Optionally add logic here to parse other IEs (e.g., Rates 0x01, HT Capabilities 0x2D)
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
        int mbm_signal = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
        detail.signal = mbm_signal / 100; // Convert mBm to dBm
    }

    // INFORMATION ELEMENTS: Raw data including SSID, Capabilities, etc.
    if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
        struct nlattr *ie = bss[NL80211_BSS_INFORMATION_ELEMENTS];
        const unsigned char *data = (unsigned char *)nla_data(ie);
        int data_len = nla_len(ie);

        parse_beacon_ies(data , data_len, &detail);
    }

    // 4. Add the parsed entry to the list
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
#ifdef IS_DESKTOP_LINUX
    if (scanTimer->isValid() && scanTimer->elapsed() < scanCooldownMs) {
        emit error("Scan blocked: cooldown active");
        return;
    }
    scanTimer->restart();

    qDebug() << "--- Starting WiFi Scan on interface:" << iface << "---";
    qDebug() << "--- (This may require root privileges) ---";

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
    emit scanFinished(0);
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
    emit scanFinished(m_qobjectResults.size());
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
    emit scanFinished(m_qobjectResults.size());
}
#endif

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
