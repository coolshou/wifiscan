#ifndef WIFIUTIL_H
#define WIFIUTIL_H

#include <QtGlobal>
#include <QString>

class WifiUtils
{
public:
    /**
     * @brief 將中心頻率 (MHz) 轉換為 Wi-Fi 頻道編號。
     * @param frequencyMHz 網路的中心頻率 (MHz)。
     * @return 頻道編號的整數，如果頻率無效則返回 0。
     */
    static int frequencyToChannel(int frequencyMHz);

    /**
     * @brief 取得指定頻率的 Wi-Fi 頻帶名稱。
     * @param frequencyMHz 網路的中心頻率 (MHz)。
     * @return 頻帶名稱 (例如: "2.4 GHz", "5 GHz", "6 GHz")。
     */
    static QString getBandName(int frequencyMHz);
};

#endif //WIFIUTIL_H
