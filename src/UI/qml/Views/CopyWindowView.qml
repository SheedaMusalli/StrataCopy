import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: view
    signal addFiles()
    signal addFolder()
    signal chooseDestination()
    signal showParameters()
    signal saveScript()
    signal showHistory()
    signal toggleDetails()
    property bool detailsExpanded: false
    property bool transferOnly: false
    property int completionDefault: 1
    property int completionChoice: completion.currentIndex
    property bool completionChanged: false
    property string actionName: Number(appState.profile.transferMode)===5 ? "Mirror" : [3,4].indexOf(Number(appState.profile.transferMode))>=0 ? "Move" : "Copy"
    property string activeAction: Number(appState.profile.transferMode)===5 ? "Mirroring items" : [3,4].indexOf(Number(appState.profile.transferMode))>=0 ? "Moving items" : "Copying items"
    function sourceSummary() {
        if (!appState.sources.length) return "Choose files or folders"
        const first = appState.sources[0].split(/[\\/]/).pop()
        return appState.sources.length === 1 ? first : first + " and " + (appState.sources.length-1) + " more"
    }
    ColumnLayout {
        anchors.fill: parent;spacing: 12
        RowLayout {
            Layout.fillWidth: true;Layout.preferredHeight: 24;spacing: 8
            Text {text: appState.busy ? appState.paused ? "Paused" : view.activeAction : appState.outcome === "success" ? "Transfer complete" : appState.outcome === "error" ? "Action needed" : "Ready";font.pixelSize: 15;font.weight: Font.Medium;color: "#f2f2f2";Layout.fillWidth: true;elide: Text.ElideRight}
            Text {text: appState.sourceFileCount ? appState.sourceFileCount + " items · " + appState.totalSize : "";font.pixelSize: 11;color: "#b6b6b6"}
        }
        Rectangle {
            Layout.fillWidth: true;Layout.preferredHeight: 82
            color: "#292929";border.color: "#454545"
            ColumnLayout {
                anchors.fill: parent;anchors.margins: 9;spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    Text {text: "From";color: "#c3c3c3";font.pixelSize: 11;Layout.preferredWidth: 34}
                    Text {text: view.sourceSummary();color: "#f1f1f1";font.pixelSize: 12;elide: Text.ElideMiddle;Layout.fillWidth: true;ToolTip.visible: sourceHover.hovered && appState.sources.length>0;ToolTip.text: appState.sources.join("\n");HoverHandler {id: sourceHover}}
                    GlassButton {visible: !appState.busy;text: "+ Files";implicitHeight: 27;onClicked: view.addFiles()}
                    GlassButton {visible: !appState.busy;text: "+ Folder";implicitHeight: 27;onClicked: view.addFolder()}
                    GlassButton {visible: !appState.busy;text: "…";implicitWidth: 32;implicitHeight: 27;onClicked: queueDialog.open()}
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {text: "To";color: "#c3c3c3";font.pixelSize: 11;Layout.preferredWidth: 34}
                    GlassField {id: destination;visible: !appState.busy;Layout.fillWidth: true;implicitHeight: 28;text: appState.destination;placeholderText: "Choose destination folder";onEditingFinished: appState.destination=text}
                    Text {visible: appState.busy;Layout.fillWidth: true;text: appState.destination;color: "#f1f1f1";font.pixelSize: 12;elide: Text.ElideMiddle}
                    GlassButton {visible: !appState.busy;text: "Browse…";implicitHeight: 28;onClicked: view.chooseDestination()}
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true;spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Text {text: Math.round(appState.progress*100)+"%";font.pixelSize: 21;font.weight: Font.Medium;color: "#f2f2f2"}
                Text {text: appState.busy ? "complete" : appState.progress>0 ? "complete" : "";font.pixelSize: 11;color: "#bdbdbd";Layout.alignment: Qt.AlignBottom;Layout.bottomMargin: 3}
                Item {Layout.fillWidth: true}
                Text {text: appState.paused ? "Paused" : Number(appState.stats.writeMbps || 0).toFixed(1)+" MB/s";font.pixelSize: 12;color: "#e8e8e8"}
            }
            Rectangle {
                Layout.fillWidth: true;height: 5;color: "#464646"
                Rectangle {height: parent.height;width: parent.width*appState.progress;color: appState.outcome==="error" ? "#ed8495" : "#55bce8";Behavior on width {NumberAnimation {duration: 130}}}
            }
            RowLayout {
                Layout.fillWidth: true
                Text {text: appState.busy ? appState.phase : appState.sources.length ? "Ready to start." : "Add items to begin.";color: "#bcbcbc";font.pixelSize: 11;elide: Text.ElideRight;Layout.fillWidth: true}
                Text {text: "Time remaining: " + appState.eta;color: "#bcbcbc";font.pixelSize: 11}
            }
        }
        Rectangle {
            visible: appState.busy;Layout.fillWidth: true;Layout.preferredHeight: 106
            color: "#292929";border.color: "#454545"
            TelemetryGraph {anchors.fill: parent;anchors.margins: 1}
            Text {anchors.right: parent.right;anchors.top: parent.top;anchors.margins: 8;text: "Speed: " + Number(appState.stats.writeMbps || 0).toFixed(1) + " MB/s";color: "#e8e8e8";font.pixelSize: 10}
        }
        RowLayout {
            visible: appState.busy;Layout.fillWidth: true;Layout.preferredHeight: 34
            Text {text: "Status:";color: "#bcbcbc";font.pixelSize: 11}
            Text {text: appState.phase;Layout.fillWidth: true;color: "#f2f2f2";font.pixelSize: 11;elide: Text.ElideMiddle}
            GlassButton {text: appState.paused ? "Resume" : "Pause";accent: true;implicitWidth: 74;implicitHeight: 28;onClicked: appState.togglePause()}
            GlassButton {text: "×";implicitWidth: 30;implicitHeight: 28;onClicked: appState.cancel()}
        }
        RowLayout {
            visible: !appState.busy && view.detailsExpanded;Layout.fillWidth: true;Layout.preferredHeight: 30
            ModernComboBox {Layout.preferredWidth: 135;implicitHeight: 28;model: ["All files", "Copied / copying", "Skipped", "Extra items", "Failed", "Verified", "Planned"];onActivated: appState.setFilter(["","Copied","Skipped","Extra","Failed","Verified","Planned"][currentIndex])}
            GlassField {Layout.fillWidth: true;implicitHeight: 28;placeholderText: "Search files";onTextChanged: appState.setSearch(text)}
            GlassButton {text: "Log";implicitHeight: 28;enabled: appState.logPath.length>0;onClicked: appState.openLog()}
        }
        FileTable {visible: view.detailsExpanded;Layout.fillWidth: true;Layout.fillHeight: true;Layout.minimumHeight: 110}
        ListView {
            visible: !appState.busy && !view.detailsExpanded && appState.recentRuns.length > 0
            Layout.fillWidth: true;Layout.preferredHeight: 48;clip: true
            model: appState.recentRuns.slice(0, 2)
            delegate: Text {required property var modelData;width: ListView.view.width;height: 24;elide: Text.ElideMiddle;text: modelData.state + " · " + (modelData.payload.destination || "");color: "#c1c1c1";font.pixelSize: 11}
        }
        Item {visible: !view.detailsExpanded;Layout.fillHeight: true;Layout.minimumHeight: 0}
        Text {
            Layout.fillWidth: true;Layout.maximumHeight: 30
            text: appState.message || (appState.omittedRows>0 || appState.fileStats.omittedCount>0 ? "Some file activity is omitted from this view. Full output is saved in the log." : "Folders keep their name at the destination.")
            font.pixelSize: 10;color: appState.outcome==="error" ? "#ed8495" : "#a8a8a8";elide: Text.ElideRight;maximumLineCount: 2;wrapMode: Text.WordWrap
        }
        Rectangle {Layout.fillWidth: true;height: 1;color: "#474747"}
        RowLayout {
            Layout.fillWidth: true;Layout.preferredHeight: 34;spacing: 7
            GlassButton {text: view.detailsExpanded ? "Fewer details" : "More details";implicitHeight: 29;onClicked: view.toggleDetails()}
            GlassButton {visible: !appState.busy;text: "Settings";implicitHeight: 29;onClicked: view.showParameters()}
            GlassButton {visible: !appState.busy && appState.sources.length>0;text: "Test";implicitHeight: 29;onClicked: appState.generateChecksums()}
            GlassButton {visible: !appState.busy;text: "History";implicitHeight: 29;onClicked: view.showHistory()}
            Item {Layout.fillWidth: true}
            Text {text: "After completion";font.pixelSize: 10;color: "#bcbcbc"}
            ModernComboBox {id: completion;Layout.preferredWidth: 132;implicitHeight: 29;currentIndex: view.completionDefault;model: ["Keep open","Close window","Sleep PC…","Eject drive…"];onActivated: view.completionChanged=true}
            GlassButton {visible: !appState.busy || (!view.transferOnly && appState.outcome!=="success");text: appState.busy ? "Cancel" : view.actionName;danger: appState.busy;accent: !appState.busy;implicitHeight: 30;Layout.preferredWidth: 82;onClicked: {if(appState.busy)appState.cancel();else {destination.focus=false;appState.destination=destination.text;appState.start()}}}
        }
    }
    Dialog {
        id: queueDialog;title: "Source items";anchors.centerIn: Overlay.overlay;modal: true;width: Math.min(530,view.width-30);height: Math.min(320,view.height-30);standardButtons: Dialog.Close
        background: Rectangle {color: "#2c2c2c";border.color: "#555555"}
        contentItem: ListView {
            model: appState.sources;clip: true;spacing: 2
            delegate: RowLayout {required property string modelData;required property int index;width: ListView.view.width;height: 34;Text {text: modelData;color: "#eeeeee";font.pixelSize: 11;elide: Text.ElideMiddle;Layout.fillWidth: true}GlassButton {text: "Remove";implicitHeight: 26;enabled: !appState.busy;onClicked: appState.removeSource(index)}}
            Text {visible: appState.sources.length===0;anchors.centerIn: parent;text: "No items selected.";font.pixelSize: 12;color: "#bdbdbd"}
        }
    }
}
