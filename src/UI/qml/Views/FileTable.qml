import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: table
    ColumnLayout {
        anchors.fill: parent;spacing: 0
        RowLayout {
            Layout.fillWidth: true;Layout.preferredHeight: 35;Layout.maximumHeight: 35;spacing: 12
            Text {text: "STATUS";color: "#63758e";font.pixelSize: 9;font.letterSpacing: 1;Layout.preferredWidth: 76}
            Text {text: "FILE / PATH";color: "#63758e";font.pixelSize: 9;font.letterSpacing: 1;Layout.fillWidth: true}
            Text {text: "SIZE";color: "#63758e";font.pixelSize: 9;font.letterSpacing: 1;Layout.preferredWidth: 76;horizontalAlignment: Text.AlignRight}
        }
        Rectangle {Layout.fillWidth: true;height: 1;color: "#243044"}
        ListView {
            id: list
            Layout.fillWidth: true;Layout.fillHeight: true;clip: true
            model: appState.files;reuseItems: true;cacheBuffer: 300
            ScrollBar.vertical: SlimScrollBar { }
            delegate: Rectangle {
                id: row
                required property string filePath
                required property string fileState
                required property string classification
                required property string fileHash
                required property var fileBytes
                required property int index
                width: ListView.view.width;height: 38
                color: index%2 ? "#08101a" : "transparent"
                property color statusColor: fileState==="Failed" ? LiquidGlassTheme.red : fileState==="Verified" || fileState==="Copied" ? LiquidGlassTheme.green : fileState==="Skipped" || fileState==="Extra" ? LiquidGlassTheme.amber : LiquidGlassTheme.cyan
                RowLayout {
                    anchors.fill: parent;spacing: 12
                    Rectangle {Layout.preferredWidth: 76;height: 23;radius: 6;color: Qt.rgba(row.statusColor.r,row.statusColor.g,row.statusColor.b,.08);border.color: Qt.rgba(row.statusColor.r,row.statusColor.g,row.statusColor.b,.24);Text {anchors.centerIn: parent;text: row.fileState;color: row.statusColor;font.pixelSize: 10}}
                    ColumnLayout {Layout.fillWidth: true;spacing: 2;Text {Layout.fillWidth: true;text: row.filePath;color: "#b5c5d9";font.pixelSize: 11;elide: Text.ElideMiddle} Text {Layout.fillWidth: true;visible: row.fileHash.length>0;text: row.fileHash;color: "#5a997f";font.family: LiquidGlassTheme.mono;font.pixelSize: 9;elide: Text.ElideRight}}
                    Text {Layout.preferredWidth: 76;text: appState.formatBytes(row.fileBytes);color: "#8c9db6";font.pixelSize: 10;horizontalAlignment: Text.AlignRight}
                }
                HoverHandler {id: hover}
                ToolTip.visible: hover.hovered
                ToolTip.text: row.filePath+"\n"+row.classification+(row.fileHash ? "\n"+row.fileHash : "")
                ToolTip.delay: 600
            }
            Column {
                anchors.centerIn: parent;spacing: 10;visible: list.count===0
                Text {anchors.horizontalCenter: parent.horizontalCenter;text: appState.busy ? "Waiting for file activity…" : "Files will appear here during the transfer.";font.pixelSize: 11;color: "#899db5"}
            }
        }
    }
}
