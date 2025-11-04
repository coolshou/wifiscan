#include "wifiutils.h"

#include <QDebug>

// --- 2.4 GHz Band ---
// C = (f - 2407) / 5
const int FREQ_2G_START = 2412;
const int FREQ_2G_END = 2484;
const int FORMULA_2G_OFFSET = 2407;

// --- 5 GHz Band ---
// C = (f - 5000) / 5
const int FREQ_5G_START = 5170; // 包含 5180 及附近範圍
const int FREQ_5G_END = 5885;
const int FORMULA_5G_OFFSET = 5000;

// --- 6 GHz Band (Wi-Fi 6E) ---
// C = (f - 5950) / 5
const int FREQ_6G_START = 5955;
const int FREQ_6G_END = 7125;
const int FORMULA_6G_OFFSET = 5950;


/**
 * @brief 將中心頻率 (MHz) 轉換為 Wi-Fi 頻道編號。
 */
int WifiUtils::frequencyToChannel(int frequencyMHz)
{
    // 1. 處理 2.4 GHz 頻帶
    if (frequencyMHz >= FREQ_2G_START && frequencyMHz <= FREQ_2G_END) {
        // 例外情況：Channel 14 是 2484 MHz (僅日本，間隔 12 MHz)
        if (frequencyMHz == 2484) {
            return 14;
        }
        // 標準 2.4G 轉換公式
        return (frequencyMHz - FORMULA_2G_OFFSET) / 5;
    }

    // 2. 處理 5 GHz 頻帶 (U-NII)
    else if (frequencyMHz >= FREQ_5G_START && frequencyMHz <= FREQ_5G_END) {
        // 5G 轉換公式，頻道編號通常為 36, 40, 44...
        int channel = (frequencyMHz - FORMULA_5G_OFFSET) / 5;
        // 5G 頻道號碼通常以 4 遞增，這是正確的計算方法。
        // 例如：(5180 - 5000) / 5 = 36
        // 例如：(5825 - 5000) / 5 = 165
        return channel;
    }

    // 3. 處理 6 GHz 頻帶 (Wi-Fi 6E)
    else if (frequencyMHz >= FREQ_6G_START && frequencyMHz <= FREQ_6G_END) {
        // 6G 轉換公式，頻道編號通常為奇數 1, 5, 9...
        int channel = (frequencyMHz - FORMULA_6G_OFFSET) / 5;
        // 6G 頻道號碼範圍為 1 到 233
        return channel;
    }

    // 4. 無效頻率
    else {
        qWarning() << "Invalid Wi-Fi frequency detected:" << frequencyMHz << "MHz";
        return 0; // 返回 0 或 -1 表示無效頻道
    }
}

/**
 * @brief 取得指定頻率的 Wi-Fi 頻帶名稱。
 */
QString WifiUtils::getBandName(int frequencyMHz)
{
    if (frequencyMHz >= FREQ_2G_START && frequencyMHz <= FREQ_2G_END) {
        return "2.4 GHz";
    } else if (frequencyMHz >= FREQ_5G_START && frequencyMHz <= FREQ_5G_END) {
        return "5 GHz";
    } else if (frequencyMHz >= FREQ_6G_START && frequencyMHz <= FREQ_6G_END) {
        return "6 GHz";
    }
    return "Unknown";
}
