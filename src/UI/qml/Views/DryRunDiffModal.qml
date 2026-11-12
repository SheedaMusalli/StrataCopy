import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: dialog
    title: "Dry-run preview"
    modal: true;width: Math.min(900,parent.width-70);height: Math.min(560,parent.height-70)
    anchors.centerIn: parent
    standardButtons: Dialog.Close
    background: Rectangle {color: "#2c2c2c";border.color: "#555555"}
    ColumnLayout {anchors.fill: parent;Text {Layout.fillWidth: true;text: "Planned operations from Robocopy /L. No files are copied or deleted.";color: "#dedede";font.pixelSize: 12;wrapMode: Text.WordWrap} FileTable {Layout.fillWidth: true;Layout.fillHeight: true}}
}
