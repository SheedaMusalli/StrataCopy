import QtQuick
import QtQuick.Controls
TextField {
    id: field
    implicitHeight: 32
    color: "#f2f2f2"
    placeholderTextColor: "#a0a0a0"
    selectionColor: "#255a7d"
    selectedTextColor: "#ffffff"
    font.family: LiquidGlassTheme.font
    font.pixelSize: 12
    leftPadding: 9
    rightPadding: 9
    background: Rectangle {color: field.enabled ? "#252525" : "#303030";radius: 2;border.color: field.activeFocus ? "#65ade8" : "#555555";border.width: field.activeFocus ? 2 : 1}
}
