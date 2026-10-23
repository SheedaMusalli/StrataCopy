import QtQuick
import QtQuick.Controls
Dialog {
    id: dialog
    property var enterAction: null
    modal: true
    anchors.centerIn: parent
    padding: 18
    function confirmFromEnter() {
        if (!opened || !activeFocus)
            return
        if (enterAction) {
            enterAction()
            return
        }
        if (standardButtons & (Dialog.Ok | Dialog.Yes | Dialog.Save | Dialog.Open | Dialog.Apply | Dialog.Retry | Dialog.Close))
            accept()
    }
    Shortcut {
        sequence: "Return"
        context: Qt.WindowShortcut
        enabled: dialog.opened && dialog.activeFocus
        onActivated: dialog.confirmFromEnter()
    }
    Shortcut {
        sequence: "Enter"
        context: Qt.WindowShortcut
        enabled: dialog.opened && dialog.activeFocus
        onActivated: dialog.confirmFromEnter()
    }
    background: Rectangle {color: "#191d25";radius: 10;border.color: "#3a424f"}
    header: Text {
        text: dialog.title
        leftPadding: 18; rightPadding: 18; topPadding: 18; bottomPadding: 12
        color: "#ecf0f7"; font.family: LiquidGlassTheme.font
        font.pixelSize: 16; font.weight: Font.DemiBold
    }
    footer: DialogButtonBox {
        visible: dialog.standardButtons !== Dialog.NoButton
        standardButtons: dialog.standardButtons
        padding: 12; spacing: 8
        background: Rectangle {color: "#161a21";radius: 8}
        delegate: GlassButton {}
        onAccepted: dialog.accept()
        onRejected: dialog.reject()
    }
}
