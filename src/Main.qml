import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

ApplicationWindow {
    id: mainWindow
    width: 1024
    height: 480
    visible: true
    minimumWidth: 800
    title: qsTr("WiFi scan")
    property var columnWidths: [150, -1, 100, 120, 30, 30]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            spacing: 12
            width: parent.width
            height: 40

            ComboBox {
                id: ifaceSelector
                model: interfaceModel
                textRole: "display"
                Layout.preferredWidth: parent.width * 0.5
            }
            BusyIndicator {
                running: wifiScanner.busy
                visible: wifiScanner.busy
                Layout.preferredHeight: parent.height * 0.9
                Layout.preferredWidth: Layout.preferredHeight
                Layout.alignment: Qt.AlignVCenter
            }
            Button {
                text: "Scan"
                Layout.preferredWidth: 80
                enabled: !wifiScanner.busy
                onClicked: {
                    debugPopup.text = "Scanning on: " + ifaceSelector.currentText
                    debugPopup.open()
                    wifiScanner.startScan(ifaceSelector.currentText)
                }
            }
            TextEdit {
                id: sitesurveys
                Layout.preferredWidth: 30
                // text: beaconModel.count
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.NoWrap
                font.pointSize: 12
                //anchors.fill: parent
                //anchors.margins: 4
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "..."
                width: 30
                onClicked: {
                    debugPopup.text = "TODO: options"
                }
            }
        }
        // 🔹 Header Row
        RowLayout {
            id: headerRow
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            height: 40
            spacing: 4
            // Helper function for sorting: Assumes beaconModel has a sortByRole(string roleName)
            function sortTable(roleName) {
                if (beaconModel && beaconModel.sortByRole) {
                    beaconModel.sortByRole(roleName);
                }
            }
            Rectangle {
                Layout.preferredWidth: columnWidths[0]
                Layout.preferredHeight: headerRow.Layout.preferredHeight
                color: "#ddd"
                Text {
                    anchors.centerIn: parent
                    text: "BSSID"
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: headerRow.sortTable("bssid") // 'bssid' is assumed role name
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: headerRow.Layout.preferredHeight
                color: "#ddd"
                Text {
                    anchors.centerIn: parent
                    text: "SSID"
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: headerRow.sortTable("ssid") // 'ssid' is assumed role name
                }
            }
            Rectangle {
                Layout.preferredWidth: columnWidths[2]
                Layout.preferredHeight: headerRow.Layout.preferredHeight
                color: "#ddd"
                Text {
                    anchors.centerIn: parent
                    text: "Freq\nSignal"
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    // Sorting by two fields is complex. You might choose signal or freq as primary.
                    onClicked: headerRow.sortTable("signal") // 'signal' is assumed role name
                }
            }
            Rectangle {
                Layout.preferredWidth: columnWidths[3]
                Layout.preferredHeight: headerRow.Layout.preferredHeight
                color: "#ddd"
                Text {
                    anchors.centerIn: parent
                    text: "Generation"
                    font.bold: true
                }

            }
            Rectangle {
                Layout.preferredWidth: columnWidths[4]
                Layout.preferredHeight: headerRow.Layout.preferredHeight
                color: "#ddd"
                Text {
                    anchors.centerIn: parent
                    text: "BSS"
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: headerRow.sortTable("bsscolor") // role name
                }
            }
            Rectangle {
                Layout.preferredWidth: columnWidths[5]
                Layout.preferredHeight: headerRow.Layout.preferredHeight
                color: "#ddd"
                Text {
                    anchors.centerIn: parent
                    text: "..."
                    font.bold: true
                }
            }
        }
        // 🔹 TableView with delegate
        TableView {
            id: beaconTableView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: beaconModel
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                // implicitHeight: contentRow.implicitHeight + 8
                implicitHeight: 50
                implicitWidth: beaconTableView.width
                color: index % 2 === 0 ? "#fcf6c6" : "#ffffff"
                border.color: "#dddddd"
                border.width: 1
                radius: 4

                RowLayout {
                    anchors.fill: parent
                    spacing: 4

                    Rectangle {
                        Layout.preferredWidth: columnWidths[0]
                        Layout.fillHeight: true
                        color: "transparent"
                        // color: "#f0f0f0"
                        radius: 4
                        TextEdit {
                            text: model.bssid
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.NoWrap
                            font.bold: true
                            font.pointSize: 12
                            anchors.fill: parent
                            anchors.margins: 4
                        }
                    }
                    Rectangle {
                        color: "transparent"
                        // Layout.preferredWidth: ssidColumnWidth
                        Layout.fillWidth:  true
                        Layout.fillHeight: true
                        TextEdit {
                            text: model.ssid
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.NoWrap
                            font.pointSize: 12
                            anchors.fill: parent
                        }
                    }
                    Rectangle {
                        color: "transparent"
                        Layout.preferredWidth: columnWidths[2]
                        Layout.fillHeight: true
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 2
                            TextEdit {
                                text: model.frequency + " MHz("+model.channel+")"
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.NoWrap
                                font.pointSize: 10
                            }
                            TextEdit {
                                text: model.signal + " dBm"
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.NoWrap
                                font.pointSize: 10
                            }
                        }
                    }
                    Rectangle {
                        color: "transparent"
                        Layout.preferredWidth: columnWidths[3]
                        Layout.fillHeight: true
                        // color: "red"
                        // 使用 RowLayout 輕鬆排列多個圖示
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 2 // 給圖示一些邊距
                            spacing: 1
                            Layout.alignment: Qt.AlignVCenter // 垂直居中對齊

                            Image {
                                visible: model.is11n
                                source: "qrc:///image/wifi4"
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                fillMode: Image.PreserveAspectFit
                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    ToolTip.text: "802.11n"
                                    ToolTip.visible: containsMouse
                                }
                            }
                            Image {
                                visible: model.is11ac
                                source: "qrc:///image/wifi5"
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                fillMode: Image.PreserveAspectFit
                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    ToolTip.text: "802.11ac"
                                    ToolTip.visible: containsMouse
                                }
                            }
                            Image {
                                visible: model.is11ax
                                source: "qrc:///image/wifi6"
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                fillMode: Image.PreserveAspectFit
                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    ToolTip.text: "802.11ax"
                                    ToolTip.visible: containsMouse
                                }
                            }
                            Image {
                                visible: model.is11be
                                source: "qrc:///image/wifi7"
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                fillMode: Image.PreserveAspectFit
                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    ToolTip.text: "802.11be"
                                    ToolTip.visible: containsMouse
                                }

                            }
                            Item { Layout.fillWidth: true } // 填滿剩餘空間，將圖示靠左對齊
                        }
                    }
                    Rectangle {
                        // color: model.bsscolordisable?"gray":"transparent"
                        color:"transparent"
                        Layout.preferredWidth: columnWidths[4]
                        Layout.fillHeight: true
                        TextEdit {
                            // text: model.bsscolordisable?"":model.bsscolor?model.bsscolor:""
                            text:model.bsscolor?model.bsscolor:""
                            color: model.bsscolordisable?"gray":"black"
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.NoWrap
                            font.pointSize: 12
                            anchors.fill: parent
                        }
                    }
                    Rectangle {
                        color: "transparent"
                        Layout.preferredWidth: columnWidths[5]
                        Layout.fillHeight: true
                        Button {
                            anchors.centerIn: parent
                            text: "..."
                            width: parent.width - 2
                            onClicked: {
                                //TODO: current row's detail info
                                // console.log("elmIDs:", model.elementIDs);
                                // myDetailDialog.beaconModel = beaconModel
                                let rowData = {ssid: model.ssid,
                                    bssid: model.bssid,
                                    signal: model.signal,
                                    capabilities: model.capabilities,
                                    transmitpower: model.transmitpower,
                                    elmIds: beaconModel.getElementIDsRoleMap(index),
                                    elmExtIDs: model.elementExtIDs,
                                                    // ... 傳遞所有需要的 roles
                                    };
                                myDetailDialog.currentBeacon = rowData
                                myDetailDialog.dialogWidth = mainWindow.width
                                myDetailDialog.open()
                            }
                        }
                    }
                }
            }
        }
    }
    Detail {
        id: myDetailDialog
        // 可以透過屬性覆寫寬高
        // dialogWidth: 450
        // dialogHeight: 250
        width: mainWindow.width
        height: mainWindow.height
        implicitWidth: mainWindow.width
        implicitHeight: mainWindow.height
        anchors.centerIn: parent;
        // 處理 Dialog 的結果
        onAccepted: {
            console.log("Detail onAccepted")
            // statusText.text = "設定已儲存 (Accepted)"
        }
        onRejected: {
            console.log("Detail onRejected")
            // statusText.text = "設定操作已取消 (Rejected)"
        }
    }

    Popup {
        id: debugPopup
        x: 20
        y: parent.height - 100
        width: parent.width - 40
        height: 60
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape

        Rectangle {
            anchors.fill: parent
            color: "#333"
            radius: 8
            opacity: 0.9

            Text {
                id: debugText
                anchors.centerIn: parent
                text: "Debug message"
                color: "white"
                font.pointSize: 12
            }
        }

        property string text: ""
        onTextChanged: {
            debugText.text = text
            if (!visible) open()
            hideTimer.restart()
        }

        Timer {
            id: hideTimer
            interval: 5000
            running: false
            repeat: false
            onTriggered: debugPopup.close()
        }
    }

    Connections {
        target: wifiScanner
        function onScanFinished() {
            debugPopup.close()
            //TODO: can not call c++
            console.debug("sitesurveys", beaconTableView.model.count())
            // sitesurveys.text = beaconTableView.model.count()
        }
        function onError(msg) {
            debugPopup.text = "ERROR: " + msg
        }
    }
}
