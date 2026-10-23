import QtQuick
import QtQuick.Controls
ScrollBar {
    id: bar
    padding: 1
    implicitWidth: 7
    implicitHeight: 7
    minimumSize: 0.08
    policy: ScrollBar.AsNeeded
    background: Rectangle { color: "transparent" }
    contentItem: Rectangle {
        implicitWidth: 4; implicitHeight: 4; radius: 2
        color: bar.pressed ? "#8491a3" : "#4b5360"
        opacity: bar.size >= 1 ? 0 : (bar.active || bar.hovered) ? 1 : 0.35
    }
}
