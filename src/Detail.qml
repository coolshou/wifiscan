import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Dialog {
    id: root
    // 定義 Dialog 的標題
    // property var beaconModel: null
    property var currentBeacon: null
    property real transmitpower: 0.0
    // 讓外部可以設定 Dialog 的寬高
    property int dialogWidth: 300
    property int dialogHeight: 200
    // width: dialogWidth
    // height: dialogHeight
    title: "Detail:"
    // 定義標準按鈕 (例如 確定/取消)
    standardButtons: Dialog.Ok // | Dialog.Cancel
    // 透過 JavaScript 函數將 Map 轉換為 Array of {key, value} 結構
    function mapToArray(dataMap) {
        let array = [];
        for (let key in dataMap) {
            array.push({
                           roleKey: key,         // 鍵: "1", "2", "3"
                           roleValue: dataMap[key] // 值: "Name", "RSSI", "UUID"
                       });
        }
        return array;
    }
    property var elementIDList: []
    property var elementExtIDList: []

    onOpened: {
        title = "Detail: " + currentBeacon.ssid
        transmitpower = currentBeacon.transmitpower
        // console.log("currentBeacon ssid:", currentBeacon.ssid)
        elementIDList = mapToArray(currentBeacon.elmIds);
        console.log("elementIDList:", elementIDList);
        // elementExtIDList = mapToArray(currentBeacon.elmExtIDs);
    }
    // 定義 Dialog 的內容
    contentItem: ColumnLayout {
        Layout.fillWidth: true
        anchors.margins: 12
        spacing: 8

        TextEdit {
            // 安全檢查：確保 currentBeacon 不是 null
            text: currentBeacon ? "["+currentBeacon.bssid + "]名稱: " + currentBeacon.ssid : "請選擇一列數據"
            font.bold: true
            readOnly: true
            selectByMouse: true
        }
        TextEdit {
            text: currentBeacon ? "訊號強度 (RSSI): " + currentBeacon.signal : ""
            readOnly: true
            selectByMouse: true
        }
        TextEdit {
            text: currentBeacon ? "capabilities: " + currentBeacon.capabilities : ""
            readOnly: true
            selectByMouse: true
        }
        TextEdit {
            text: transmitpower ? "transmit power: " + transmitpower +" dBm": ""
            readOnly: true
            selectByMouse: true
        }
        // ==========================================================
        // 替換為 TableView 來顯示 Key/Value 列表
        // ==========================================================
        Label {
            text: "Element IDs:"
            font.pointSize: 10
            font.bold: true
        }

        // 使用 ColumnLayout 來更方便地管理 TableView 及其標頭的寬度
        ColumnLayout {
            width: parent.width // 繼承外層 Column 的寬度

            // 1. 顯示標頭 (Header)
            // 必須手動加入，並連結到 TableView
            // HorizontalHeaderView {
            //     id: headerView
            //     Layout.fillWidth: true // 讓標頭填滿寬度
            //     tableView: elmDataTable    // 連結到下面的 TableView 實例
            // }

            // 2. TableView 實例
            TableView {
                id: elmDataTable
                Layout.fillWidth: true // 填滿父級 ColumnLayout 的寬度
                Layout.preferredHeight: 250 // 給 TableView 固定的高度

                // 使用轉換後的 Array 作為 Model
                model: elementIDList

                // 指定每欄寬度
                columnWidthProvider: function(column) {
                    if (column === 0)
                        return elmDataTable.width * 0.4
                    else if (column === 1)
                        return elmDataTable.width * 0.6
                    return 100
                }
                // --------------------------------------------------
                // 1. 定義 TableColumn 結構
                // --------------------------------------------------
                // TableColumn 標題會自動顯示在 HorizontalHeaderView 中


                // --------------------------------------------------
                // 2. 定義 Delegate (邏輯保持不變，因為是正確的)
                // --------------------------------------------------
                delegate: Rectangle {
                    height: 30
                    width: elmDataTable.columnWidthProvider(column)
                    implicitHeight: height
                    implicitWidth: width
                    Text {
                        id: cellText
                        anchors.verticalCenter: parent.verticalCenter
                        // 注意：這裡的 width: parent.width 應避免，因為 TableView Delegate 會自動處理寬度
                        // 只要 Text 的內容寬度不大於 TableColumn 的寬度即可
                        anchors.left: parent.left; anchors.leftMargin: 5

                        text: {
                            // 關鍵：在 TableView Delegate 中使用 model.roleName 存取數據
                            if (column === 0) {
                                return model.roleKey ? model.roleKey:""
                            } else if (column === 1) {
                                return model.roleValue ? model.roleValue:""
                            }
                            return ""
                        }
                    }

                    // 視覺效果：分割線
                    Rectangle {
                        height: 1
                        width: parent.width
                        color: "#e0e0e0"
                        anchors.bottom: parent.bottom
                    }
                }
            }
        }

        // ListView {
        //     width: 300;
        //     height: 300
        //     model: elementExtIDList // 使用轉換後的 Array 作為 Model
        //     delegate: Rectangle {
        //         width: parent.width;
        //         height: 40
        //         Text {
        //             // 這裡的 roleKey 和 roleValue 是 mapToArray 函數中定義的
        //             text: "Key: " + roleKey + ", Value: " + roleValue
        //             anchors.centerIn: parent
        //         }
        //     }
        // }
    }

}
