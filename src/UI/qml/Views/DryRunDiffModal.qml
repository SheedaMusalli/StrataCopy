import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: dialog
    title: "Dry-run preview"
    modal: true;width: Math.min(900,parent.width-70);height: Math.min(560,parent.height-70)
    anchors.centerIn: parent
    standardButtons: Dialog.Close
    background: Rectangle {color: "#111a28";radius: 18;border.color: "#35465d"}
    ColumnLayout {anchors.fill: parent;Text {Layout.fillWidth: true;text: "Planned operations from Robocopy /L. No files are copied or deleted.";color: "#a2b6ce";font.pixelSize: 12;wrapMode: Text.WordWrap} FileTable {Layout.fillWidth: true;Layout.fillHeight: true}}
}
