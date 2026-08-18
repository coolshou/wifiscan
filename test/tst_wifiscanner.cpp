#include <QtTest>
#include "wifiscanner.h"

class TestWifiScanner : public QObject
{
    Q_OBJECT

private slots:
    // --- parse_beacon_ies 數據驅動測試 ---
    void testParseBeaconIes_data();
    void testParseBeaconIes();

    // --- 特殊邊界測試 ---
    void testMalformedIePreventOverflow();
};

void TestWifiScanner::testParseBeaconIes_data()
{
    QTest::addColumn<QByteArray>("rawIeData");
    QTest::addColumn<QString>("expectedSsid");
    QTest::addColumn<int>("expectedSsidLen");
    QTest::addColumn<QStringList>("expectedAuth");
    QTest::addColumn<QStringList>("expectedEnc");
    QTest::addColumn<bool>("expectedHt");
    QTest::addColumn<bool>("expectedVht");
    QTest::addColumn<bool>("expectedHe");
    QTest::addColumn<bool>("expectedEht");
    QTest::addColumn<bool>("expected40MHz");
    QTest::addColumn<bool>("expected160MHz");
    QTest::addColumn<bool>("expected320MHz");

    // --------------------------------------------------------------------------
    // Test Case 1: 一般 WPA2-PSK (AES) + Wi-Fi 4 (HT) 40MHz
    // --------------------------------------------------------------------------
    // IE 0 (SSID): "TestNet" (len 7) -> 00 07 54 65 73 74 4e 65 74
    // IE 45 (HT Cap): len 2, HT Info 0x0002 (bit 1 = 40MHz) -> 2d 02 02 00
    // IE 48 (RSN/WPA2-PSK AES): len 20 -> 30 14 01 00 00 0f ac 04 01 00 00 0f ac 04 01 00 00 0f ac 02 00 00
    QByteArray case1;
    case1.append("\x00\x07TestNet", 9);
    case1.append("\x2d\x02\x02\x00", 4);
    case1.append("\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x02\x00\x00", 22);

    QTest::newRow("WPA2-PSK HT 40MHz")
        << case1
        << "TestNet" << 7
        << QStringList{"WPA2-PSK"} << QStringList{"CCMP (AES)"}
        << true  /* HT */  << false /* VHT */ << false /* HE */ << false /* EHT */
        << true  /* 40M */ << false /* 160M */<< false /* 320M */;

    // --------------------------------------------------------------------------
    // Test Case 2: 隱藏 SSID (Null Bytes) + WPA3-SAE + Wi-Fi 6 (HE)
    // --------------------------------------------------------------------------
    // IE 0 (SSID): 4 個 \0 -> 00 04 00 00 00 00
    // IE 48 (RSN/WPA3-SAE): AKM 8 -> 30 14 01 00 00 0f ac 04 01 00 00 0f ac 04 01 00 00 0f ac 08 00 00
    // IE 255 Ext 35 (HE Cap): Ext ID 35, PHY Cap Byte 0 bit 1 set -> ff 08 23 00 00 00 00 00 02
    QByteArray case2;
    case2.append("\x00\x04\x00\x00\x00\x00", 6);
    case2.append("\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\x00\x00", 22);
    case2.append("\xff\x08\x23\x00\x00\x00\x00\x00\x02", 10);

    QTest::newRow("Hidden SSID WPA3 HE")
        << case2
        << "[hidden]" << 4
        << QStringList{"WPA3-SAE"} << QStringList{"CCMP (AES)"}
        << false << false << true << false
        << false << true  << false;

    // --------------------------------------------------------------------------
    // Test Case 3: Wi-Fi 7 (EHT) 320MHz
    // --------------------------------------------------------------------------
    // IE 0 (SSID): "WiFi7_AP"
    // IE 255 Ext 107 (EHT Cap): len 4 (ExtID 107 + 3 bytes, PHY Cap0 bit 1 set: 0x02) -> ff 04 6b 00 02 00
    QByteArray case3;
    case3.append("\x00\x08WiFi7_AP", 10);
    case3.append("\xff\x04\x6b\x00\x02\x00", 6);

    QTest::newRow("WiFi 7 EHT 320MHz")
        << case3
        << "WiFi7_AP" << 8
        << QStringList{"Open"} << QStringList{}
        << false << false << false << true
        << false << true  << true;
}

void TestWifiScanner::testParseBeaconIes()
{
    QFETCH(QByteArray, rawIeData);
    QFETCH(QString, expectedSsid);
    QFETCH(int, expectedSsidLen);
    QFETCH(QStringList, expectedAuth);
    QFETCH(QStringList, expectedEnc);
    QFETCH(bool, expectedHt);
    QFETCH(bool, expectedVht);
    QFETCH(bool, expectedHe);
    QFETCH(bool, expectedEht);
    QFETCH(bool, expected40MHz);
    QFETCH(bool, expected160MHz);
    QFETCH(bool, expected320MHz);

    BeaconDetail detail;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(rawIeData.constData());

    // 執行要測試的函式
    parse_beacon_ies(data, rawIeData.size(), &detail);

    // 驗證解析結果
    QCOMPARE(detail.ssid, expectedSsid);
    QCOMPARE(detail.ssid_len, expectedSsidLen);
    QCOMPARE(detail.authList, expectedAuth);
    QCOMPARE(detail.encryptionList, expectedEnc);
    QCOMPARE(detail.isHtSupported, expectedHt);
    QCOMPARE(detail.isVhtSupported, expectedVht);
    QCOMPARE(detail.isHeSupported, expectedHe);
    QCOMPARE(detail.isEhtSupported, expectedEht);
    QCOMPARE(detail.supports40MHz, expected40MHz);
    QCOMPARE(detail.supports160MHz, expected160MHz);
    QCOMPARE(detail.supports320MHz, expected320MHz);
}

// 測試防禦性程式碼：畸形 IE (Malformed IE) 導致的 Buffer 溢位風險
void TestWifiScanner::testMalformedIePreventOverflow()
{
    // 宣告 IE ID=0，但宣告長度 255 (實際資料只有 3 bytes)
    QByteArray malformedData;
    malformedData.append("\x00\xFF\x41\x42", 4);

    BeaconDetail detail;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(malformedData.constData());

    // 不應該 Crash 或進入死迴圈
    parse_beacon_ies(data, malformedData.size(), &detail);

    // 確保因為資料不完整，沒有填入無效 SSID
    QVERIFY(detail.ssid.isEmpty() || detail.ssid == "[hidden]");
}

QTEST_MAIN(TestWifiScanner)
#include "tst_wifiscanner.moc"
