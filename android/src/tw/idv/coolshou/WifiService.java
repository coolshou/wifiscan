package tw.idv.coolshou;
//package org.qtproject.qt.android;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.util.Log;
// import org.qtproject.qt.android.QtActivity;
import org.qtproject.qt.android.QtNative;
import org.json.JSONArray;
import org.json.JSONObject;

import java.util.List;

public class WifiService {
    private static final String TAG = "QtWifiService";
    private static WifiManager wifiManager;
    private static WifiScanReceiver wifiReceiver;
    //  NEW FIELD: Store the Application Context
    private static Context appContext;
    public static native void sendScanResultsToCpp(String jsonResult);
    // 這是 JNI 回呼的目標 C++ 類別和方法名稱
    private static String callbackClassName = "WifiScanner";
    private static String callbackMethodName = "handleScanResults";
    // ----------------------------------------------------
    // JNI Call: New static method to receive Context
    // ----------------------------------------------------
    public static void setContext(Context context) {
        appContext = context;
        Log.d(TAG, "Application Context set successfully.");
    }
    // ----------------------------------------------------
    // JNI 呼叫：從 C++ 啟動掃描
    // ----------------------------------------------------
    public static boolean startWifiScan() {
        Log.d(TAG, "startWifiScan called from C++.");
        if (appContext == null) {
            Log.e(TAG, "Application Context is null. Initialization failed.");
            return false;
        }
        // Activity activity = QtNative.activity();
        // // Activity activity = QtActivity.instance();
        // if (activity == null) {
        //     Log.e(TAG, "Activity is null. Cannot start scan.");
        //     return false;
        // }

        // 懶惰初始化 WifiManager 和 BroadcastReceiver
        if (wifiManager == null) {
            // wifiManager = (WifiManager) activity.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            // Use the stored context
            wifiManager = (WifiManager) appContext.getSystemService(Context.WIFI_SERVICE);
        }
        if (wifiReceiver == null) {
            wifiReceiver = new WifiScanReceiver();
            IntentFilter intentFilter = new IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION);
            // activity.registerReceiver(wifiReceiver, intentFilter);
            // Use appContext to register the receiver
            appContext.registerReceiver(wifiReceiver, intentFilter);
        }

        if (wifiManager == null) {
            Log.e(TAG, "WifiManager is null.");
            return false;
        }

        // 檢查 Wi-Fi 是否開啟 (非必要，但推薦)
        if (!wifiManager.isWifiEnabled()) {
             Log.w(TAG, "Wi-Fi is not enabled. Attempting to enable it.");
             // 在較新的 Android 版本中，此呼叫會失敗，需要使用者手動開啟
             // wifiManager.setWifiEnabled(true);
             // 實際應用中應該提示使用者開啟 Wi-Fi
        }

        //  這是核心 API 呼叫
        boolean success = wifiManager.startScan();
        Log.i(TAG, "Wifi scan initiated: " + success);

        // 如果成功，結果將透過 BroadcastReceiver 回傳
        return success;
    }

    // ----------------------------------------------------
    // JNI 呼叫：設定回呼目標
    // ----------------------------------------------------
    public static void setCallbackReceiver(String className, String methodName) {
        callbackClassName = className;
        callbackMethodName = methodName;
        Log.d(TAG, "Callback set to: " + className + "." + methodName);
    }

    // ----------------------------------------------------
    // BroadcastReceiver：接收掃描結果
    // ----------------------------------------------------
    private static class WifiScanReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            boolean success = intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false);
            if (success) {
                if (wifiManager == null) return;

                List<ScanResult> results = wifiManager.getScanResults();
                Log.d(TAG, "Scan results received: " + results.size());

                String jsonResults = formatResultsToJson(results);

                // 呼叫 C++ 方法
                WifiService.sendScanResultsToCpp(jsonResults);
                // if (callbackClassName != null && callbackMethodName != null) {
                //     QtNative.invokeMethod(
                //         callbackClassName,
                //         callbackMethodName,
                //         "(Ljava/lang/String;)V", // 簽名：一個 String 參數，無返回值
                //         jsonResults
                //     );

                // }
            } else {
                Log.e(TAG, "Wi-Fi scan failed or results not updated.");
            }
        }
    }

    // ----------------------------------------------------
    // 輔助方法：將結果格式化為 JSON
    // ----------------------------------------------------
    private static String formatResultsToJson(List<ScanResult> results) {
        JSONArray jsonArray = new JSONArray();
        try {
            for (ScanResult result : results) {
                JSONObject jsonObject = new JSONObject();
                // 核心數據
                jsonObject.put("ssid", result.SSID);
                jsonObject.put("bssid", result.BSSID);
                jsonObject.put("level", result.level);         // dBm
                jsonObject.put("frequency", result.frequency); // MHz
                jsonObject.put("capabilities", result.capabilities); // WPA/WPA2/ESS

                // 轉換為 JSON 陣列
                jsonArray.put(jsonObject);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error formatting JSON: " + e.getMessage());
        }
        return jsonArray.toString();
    }

    // ----------------------------------------------------
    // JNI Call: Get the status of the primary Wi-Fi interface
    // ----------------------------------------------------
    public static boolean isWifiInterfaceAvailable() {
        // Activity activity = QtNative.activity();
        // if (activity == null) {
        //     Log.e(TAG, "Activity is null.");
        //     return false;
        // }
        if (appContext == null) {
            Log.e(TAG, "Application Context is null. Initialization failed.");
            return false;
        }

        if (wifiManager == null) {
            // wifiManager = (WifiManager) activity.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            wifiManager = (WifiManager) appContext.getSystemService(Context.WIFI_SERVICE);
        }

        // Return true if the WifiManager object is successfully retrieved,
        // indicating the main Wi-Fi adapter hardware is present and managed.
        return wifiManager != null;
    }
}
