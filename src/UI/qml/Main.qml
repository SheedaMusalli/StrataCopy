import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
ApplicationWindow {
    id: root
    width: 640; height: 510
    minimumWidth: 600; minimumHeight: detailsExpanded ? 650 : 480
    property bool forceTransferWindow: false
    property bool openedFromTray: false
    visible: forceTransferWindow || !transferOnly; title: "StrataCopy"; color: "#2b2b2b"
    flags: Qt.Window | Qt.FramelessWindowHint | (pinned ? Qt.WindowStaysOnTopHint : 0)
    property bool detailsExpanded: uiPreferences.detailed
    Settings {id: uiPreferences;category: "Interface";property bool detailed: false;property bool pinned: false;property int afterCopy: 1;property bool transferOnly: true;property bool completionNotifications: true;property bool notificationAnimations: true}
    property bool pinned: uiPreferences.pinned
    property alias completionNotifications: uiPreferences.completionNotifications
    property alias notificationAnimations: uiPreferences.notificationAnimations
    property alias transferOnly: uiPreferences.transferOnly
    property bool allowClose: false
    property string confirmationText: ""
    property string updateMessage: ""
    property var currentConflict: ({})
    property int pendingCompletion: 0
    font.family: LiquidGlassTheme.font
    palette.window: "#2b2b2b"; palette.windowText: "#f2f2f2"; palette.base: "#252525"
    palette.text: "#f2f2f2"; palette.button: "#383838"; palette.buttonText: "#f2f2f2"
    palette.highlight: "#1683c7"; palette.highlightedText: "#ffffff"
    onDetailsExpandedChanged: height=detailsExpanded ? 670 : 510
    onVisibleChanged: if (!visible) openedFromTray = false
    function closeAfterTransfer(force) {if(force || !openedFromTray){if(transferOnly&&!forceTransferWindow)hide();else {allowClose=true;close()}}}
    onClosing: function(event) {if(appState.busy&&!allowClose){event.accepted=false;closeDialog.open()}else if(root.transferOnly&&!root.forceTransferWindow){event.accepted=false;root.hide()}}
    function setUiTestState(mode) {
        parameters.close();runHistory.close()
        detailsExpanded=mode==="details"
        if(mode==="parameters" || mode==="presets") {
            parameters.open()
            if(mode==="presets") Qt.callLater(parameters.openPresetPopup)
        }
        if(mode==="history")runHistory.open()
    }
    Rectangle {anchors.fill: parent;color: "transparent";border.color: "#4a4a4a";border.width: 1}
    ColumnLayout {
        objectName: "windowLayout"
        anchors.fill: parent;anchors.margins: 12;anchors.topMargin: 7;spacing: 9
        RowLayout {
            Layout.fillWidth: true;Layout.preferredHeight: 24;Layout.maximumHeight: 24;spacing: 7
            Image {source: "qrc:/resources/icons/stratacopy.svg";sourceSize: Qt.size(48,48);Layout.preferredWidth: 23;Layout.preferredHeight: 23}
            Text {text: "StrataCopy";font.pixelSize: 12;font.weight: Font.Medium;color: "#f2f2f2"}
            Item {Layout.fillWidth: true;Layout.fillHeight: true;MouseArea {anchors.fill: parent;onPressed: root.startSystemMove()}}
            GlassButton {text: root.pinned ? "Pinned" : "Pin";accent: root.pinned;implicitWidth: 42;implicitHeight: 23;onClicked: root.pinned=!root.pinned;ToolTip.visible: hovered;ToolTip.text: "Keep on top"}
            GlassButton {text: "−";implicitWidth: 27;implicitHeight: 23;onClicked: root.showMinimized()}
            GlassButton {text: "×";implicitWidth: 27;implicitHeight: 23;onClicked: root.close()}
        }
        RowLayout {
            Layout.fillWidth: true
            visible: appState.canRetryElevated
            Text {text: "This operation may need administrator access."; color: "#e6b15a"; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight}
            GlassButton {text: "Retry as administrator"; accent: true; implicitHeight: 28; onClicked: appState.retryAsAdministrator()}
        }
        CopyWindowView {
            id: copyView
            objectName: "copyView"
            Layout.fillWidth: true;Layout.fillHeight: true
            detailsExpanded: root.detailsExpanded
            completionDefault: uiPreferences.afterCopy
            transferOnly: root.transferOnly
            onToggleDetails: root.detailsExpanded=!root.detailsExpanded
            onAddFiles: sourceFiles.open()
            onAddFolder: sourceFolder.open()
            onChooseDestination: destinationFolder.open()
            onShowParameters: parameters.open()
            onSaveScript: scriptFile.open()
            onShowHistory: runHistory.open()
        }
    }
    DropArea {
        anchors.fill: parent
        onDropped: function(drop){if(drop.hasUrls){appState.addSources(drop.urls);drop.acceptProposedAction()}}
        Rectangle {anchors.fill: parent;visible: parent.containsDrag;color: "#e6282828";border.color: "#65ade8";border.width: 2;Text {anchors.centerIn: parent;text: appState.busy ? "Drop to queue the next transfer" : "Drop to add files";font.pixelSize: 18;color: "#f2f2f2"}}
    }
    ParameterMatrixDrawer {id: parameters;preferences: uiPreferences;onMoveWindowRequested: root.startSystemMove()}
    FileDialog {id: sourceFiles;title: "Add source files";fileMode: FileDialog.OpenFiles;onAccepted: appState.addSources(selectedFiles)}
    FolderDialog {id: sourceFolder;title: "Add source folder";onAccepted: appState.addSources([selectedFolder])}
    FolderDialog {id: destinationFolder;title: "Choose destination";onAccepted: appState.destination=appState.pathFromUrl(selectedFolder)}
    FileDialog {id: scriptFile;title: "Export Robocopy script";fileMode: FileDialog.SaveFile;nameFilters: ["PowerShell script (*.ps1)","Batch script (*.bat)"];defaultSuffix: selectedNameFilter.index===0 ? "ps1" : "bat";onAccepted: appState.exportScript(selectedFile)}
    ModernDialog {
        id: destructiveDialog;title: "Confirm operation";width: root.width-36;height: Math.min(370,root.height-36)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ScrollView {
            contentWidth: availableWidth
            ScrollBar.vertical: SlimScrollBar {}
            TextArea {text: root.confirmationText;readOnly: true;wrapMode: TextEdit.WrapAnywhere;color: "#eeeeee";font.pixelSize: 12;background: null}
        }
        onAccepted: appState.start(false,true)
        onRejected: appState.rejectExplorerRequest()
    }
    ModernDialog {
        id: closeDialog;title: "Cancel this transfer?";width: 390;standardButtons: Dialog.Yes | Dialog.No
        contentItem: Text {text: "Completed files will remain at the destination.";color: "#dddddd";wrapMode: Text.WordWrap;font.pixelSize: 12}
        onAccepted: {appState.cancel();root.allowClose=true;root.close()}
    }
    ModernDialog {
        id: completionDialog;title: root.pendingCompletion===2 ? "Put the PC to sleep?" : "Eject the drive?";width: 400;standardButtons: Dialog.Yes | Dialog.No
        contentItem: Text {text: root.pendingCompletion===2 ? "The transfer is complete. Sleep now?" : "The transfer is complete. Eject the removable destination drive?";color: "#dddddd";wrapMode: Text.WordWrap;font.pixelSize: 12}
        onAccepted: {if(appState.completionAction(root.pendingCompletion))root.closeAfterTransfer()}
        onRejected: root.closeAfterTransfer()
    }
    ModernDialog {
        id: runHistory;title: "Recent transfers";width: root.width-36;height: Math.min(380,root.height-36);standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            RowLayout {Layout.fillWidth: true;Text {text: "Restore a queue to review and retry.";font.pixelSize: 11;color: "#c1c1c1";Layout.fillWidth: true}GlassButton {text: "Explorer menu";implicitHeight: 28;onClicked: shellDialog.open()}}
            ListView {
                Layout.fillWidth: true;Layout.fillHeight: true;clip: true;spacing: 5;model: appState.recentRuns
                ScrollBar.vertical: SlimScrollBar {}
                delegate: Rectangle {
                    required property var modelData;required property int index
                    width: ListView.view.width;height: 66;color: index%2 ? "#303030" : "#292929";border.color: "#454545"
                    RowLayout {anchors.fill: parent;anchors.margins: 8;ColumnLayout {Layout.fillWidth: true;spacing: 3;Text {text: modelData.started+" UTC · "+modelData.state;color: "#eeeeee";font.pixelSize: 11}Text {text: modelData.message || "In progress";Layout.fillWidth: true;elide: Text.ElideRight;color: "#bcbcbc";font.pixelSize: 10}Text {text: modelData.payload.destination || "";Layout.fillWidth: true;elide: Text.ElideMiddle;color: "#8fc6ef";font.pixelSize: 10}}GlassButton {text: "Restore";implicitHeight: 28;enabled: !appState.busy;onClicked: {appState.restoreRun(index);runHistory.close()}}}
                }
                Text {anchors.centerIn: parent;visible: parent.count===0;text: "No recent transfers.";color: "#bdbdbd";font.pixelSize: 12}
            }
        }
    }
    ModernDialog {
        id: shellDialog;title: "Explorer integration";width: 460;standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 12
            Text {text: "Right-click Copy / Paste with StrataCopy (Show more options on Windows 11).";color: "#dddddd";wrapMode: Text.WordWrap;Layout.fillWidth: true;font.pixelSize: 12}
            RowLayout {GlassButton {text: "Add menu actions";onClicked: appState.setShellIntegration(true)}GlassButton {text: "Remove actions";onClicked: appState.setShellIntegration(false)}}
            Text {text: "Use StrataCopy for Explorer file copy/move: keyboard paste, native Paste and drag-and-drop. Loads a native hook into Explorer. Virtual items and operations Robocopy cannot represent use Windows. Disabling takes effect immediately; the DLL unloads when Explorer exits.";color: "#dddddd";wrapMode: Text.WordWrap;Layout.fillWidth: true;font.pixelSize: 12}
            RowLayout {GlassButton {text: "Use as Explorer copier";onClicked: appState.setKeyboardPasteIntegration(true)}GlassButton {text: "Disable";onClicked: appState.setKeyboardPasteIntegration(false)}}
        }
    }
    ModernDialog {
        id: updateDialog;title: "Check for Updates";width: Math.min(450,root.width-36)
        standardButtons: Dialog.Ok
        contentItem: Text {text: root.updateMessage;color: "#dddddd";font.pixelSize: 12;wrapMode: Text.WordWrap}
    }
    ModernDialog {
        id: fileConflictDialog;title: "File already exists";width: root.width-36
        standardButtons: Dialog.Cancel
        enterAction: function() {appState.resolveFileConflict(root.currentConflict.token,0,allConflicts.checked);fileConflictDialog.close()}
        contentItem: ColumnLayout {
            spacing: 12
            Text {text: root.currentConflict.name || "";color: "#f2f2f2";font.pixelSize: 16;font.weight: Font.Medium;Layout.fillWidth: true;elide: Text.ElideMiddle}
            Text {text: "Choose what to do with this file.";color: "#bcbcbc";font.pixelSize: 12}
            Text {text: "Incoming: " + (root.currentConflict.source || "") + "\n" + (root.currentConflict.sourceSize || "") + " · " + (root.currentConflict.sourceDate || "");color: "#dddddd";font.pixelSize: 11;wrapMode: Text.WrapAnywhere;Layout.fillWidth: true}
            Text {text: "Existing: " + (root.currentConflict.destination || "") + "\n" + (root.currentConflict.destinationSize || "") + " · " + (root.currentConflict.destinationDate || "");color: "#dddddd";font.pixelSize: 11;wrapMode: Text.WrapAnywhere;Layout.fillWidth: true}
            LiquidCheckBox {id: allConflicts;text: "Do this for all conflicts"}
            RowLayout {
                Layout.fillWidth: true;spacing: 8
                GlassButton {text: "Replace Existing File";accent: true;Layout.fillWidth: true;onClicked: {appState.resolveFileConflict(root.currentConflict.token,0,allConflicts.checked);fileConflictDialog.close()}}
                GlassButton {text: "Skip This File";Layout.fillWidth: true;onClicked: {appState.resolveFileConflict(root.currentConflict.token,1,allConflicts.checked);fileConflictDialog.close()}}
                GlassButton {text: "Make Duplicate";Layout.fillWidth: true;onClicked: {appState.resolveFileConflict(root.currentConflict.token,2,allConflicts.checked);fileConflictDialog.close()}}
            }
        }
        onRejected: appState.resolveFileConflict(root.currentConflict.token,3)
    }
    DryRunDiffModal {id: preview}
    Connections {
        target: appState
        function onUpdateCheckFinished(message){root.updateMessage=message;updateDialog.open()}
        function onFileConflictRequired(conflict){root.currentConflict=conflict;allConflicts.checked=false;fileConflictDialog.open()}
        function onDestructiveConfirmationRequired(message){root.confirmationText=message;destructiveDialog.open()}
        function onCloseRequested(){root.close()}
        function onPreviewReady(){root.detailsExpanded=true}
        function onCompletedSuccessfully(){if(copyView.completionChoice>1){root.pendingCompletion=copyView.completionChoice;completionDialog.open()}else if(copyView.completionChoice===1)root.closeAfterTransfer(copyView.completionChanged)}
    }
    MouseArea {anchors.right: parent.right;anchors.bottom: parent.bottom;width: 9;height: 9;cursorShape: Qt.SizeFDiagCursor;onPressed: root.startSystemResize(Qt.RightEdge|Qt.BottomEdge)}
}
