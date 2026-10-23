import QtQuick
import QtQuick.Controls
TextField {
    id: field
    implicitHeight: 34
    color: LiquidGlassTheme.text
    placeholderTextColor: "#65748a"
    selectionColor: "#255a6b"
    font.family: LiquidGlassTheme.font
    font.pixelSize: 12
    leftPadding: 12
    rightPadding: 12
    background: Rectangle { color: "#13171e"; radius: 6; border.color: field.activeFocus ? "#7bb4ff" : "#343c49" }
}
