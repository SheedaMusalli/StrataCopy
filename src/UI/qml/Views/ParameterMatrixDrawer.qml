import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Drawer {
    id: drawer
    width: parent.width; height: parent.height
    edge: Qt.RightEdge; modal: true; dim: false
    interactive: false
    signal moveWindowRequested()
    property string integrationFeedback: ""
    property var currentGroup: appState.parameterGroups[categories.currentIndex]
    function openPresetPopup() {presets.popup.open()}
    onOpened: appState.refreshIntegration()
    background: Rectangle {color: "#171c24";border.color: "#364151";radius: 8}
    ColumnLayout {
        anchors.fill: parent;anchors.margins: 18;spacing: 12
        RowLayout {
            Layout.fillWidth: true;Layout.maximumHeight: 30
            Text {
                text: "Transfer settings";font.pixelSize: 18;font.weight: Font.DemiBold;color: "#e8eef8";Layout.fillWidth: true
                MouseArea {objectName: "settingsWindowDrag";anchors.fill: parent;onPressed: drawer.moveWindowRequested()}
            }
            GlassButton {text: "Done";implicitHeight: 30;onClicked: drawer.close()}
        }
        RowLayout {
            Layout.fillWidth: true;Layout.maximumHeight: 34
            ModernComboBox {id: presets;objectName: "presetSelector";Layout.fillWidth: true;model: appState.builtinPresets;onActivated: appState.applyPreset(currentIndex);enabled: !appState.busy}
            GlassButton {text: "Reset";enabled: !appState.busy;onClicked: {appState.applyPreset(0);presets.currentIndex=0}}
        }
        LiquidCheckBox {
            Layout.fillWidth: true
            text: "Set StrataCopy as default copy handler"
            checked: appState.defaultCopyHandler
            enabled: !appState.busy
            onClicked: {appState.setKeyboardPasteIntegration(checked);drawer.integrationFeedback=appState.message}
        }
        Text {
            Layout.fillWidth: true;wrapMode: Text.WordWrap
            text: "Use StrataCopy for Explorer paste and drag-and-drop. Uncheck to use the Windows copier."
            color: "#8b9bb2";font.pixelSize: 10
        }
        Text {
            visible: drawer.integrationFeedback.length > 0
            Layout.fillWidth: true;wrapMode: Text.WordWrap
            text: drawer.integrationFeedback;color: "#a9cfff";font.pixelSize: 10
        }
        ModernComboBox {
            id: categories;Layout.fillWidth: true
            model: appState.parameterGroups.map(group => group.title.replace(/^[A-Z] \/ /,"" )).concat(["Saved presets"])
        }
        Rectangle {Layout.fillWidth: true;height: 1;color: "#303947"}
        ScrollView {
            id: settingsScroll
            Layout.fillWidth: true;Layout.fillHeight: true;clip: true;contentWidth: availableWidth
            ScrollBar.vertical: SlimScrollBar {}
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: settingsScroll.availableWidth-10;spacing: 2
                Text {
                    visible: !!drawer.currentGroup && !!drawer.currentGroup.help
                    text: drawer.currentGroup ? drawer.currentGroup.help || "" : ""
                    Layout.fillWidth: true;wrapMode: Text.WordWrap
                    color: "#8b9bb2";font.pixelSize: 11;Layout.bottomMargin: 5
                }
                ButtonGroup {id: choices}
                Repeater {
                    model: drawer.currentGroup ? drawer.currentGroup.options : []
                    delegate: Loader {
                        id: optionLoader
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        enabled: !appState.busy
                        sourceComponent: drawer.currentGroup.kind==="radio" ? radioOption : modelData.type==="bool" ? boolOption : inputOption
                        Component {
                            id: radioOption
                            LiquidRadioDot {
                                width: optionLoader.width
                                text: optionLoader.modelData.label;badge: optionLoader.modelData.flag
                                ButtonGroup.group: choices
                                checked: Number(appState.profile[drawer.currentGroup.key])===optionLoader.index
                                onClicked: appState.setOption(drawer.currentGroup.key,optionLoader.index)
                            }
                        }
                        Component {
                            id: boolOption
                            LiquidCheckBox {
                                width: optionLoader.width
                                text: optionLoader.modelData.label;badge: optionLoader.modelData.flag
                                checked: !!appState.profile[optionLoader.modelData.key]
                                onClicked: appState.setOption(optionLoader.modelData.key,checked)
                                ToolTip.visible: hovered && !!optionLoader.modelData.help
                                ToolTip.text: optionLoader.modelData.help || "";ToolTip.delay: 500
                            }
                        }
                        Component {
                            id: inputOption
                            RowLayout {
                                width: optionLoader.width;spacing: 10
                                Text {text: optionLoader.modelData.label;color: "#b2bfd2";font.pixelSize: 11;Layout.fillWidth: true;wrapMode: Text.WordWrap}
                                GlassField {
                                    Layout.preferredWidth: 185
                                    text: String(appState.profile[optionLoader.modelData.key] ?? "")
                                    placeholderText: optionLoader.modelData.flag
                                    onEditingFinished: appState.setOption(optionLoader.modelData.key,optionLoader.modelData.type==="int" ? Number(text) : text)
                                }
                            }
                        }
                    }
                }
                ModernComboBox {
                    visible: categories.currentIndex===appState.parameterGroups.length-1
                    Layout.fillWidth: true;enabled: !appState.busy
                    model: ["XXH3-64 · fast integrity check","BLAKE3 · cryptographic","SHA-256 · cryptographic"]
                    currentIndex: Number(appState.profile.hashAlgorithm)
                    onActivated: appState.setOption("hashAlgorithm",currentIndex)
                }
                ColumnLayout {
                    visible: !drawer.currentGroup
                    Layout.fillWidth: true;spacing: 10;enabled: !appState.busy
                    Text {text: "Save these settings for another transfer.";color: "#8b9bb2";font.pixelSize: 12}
                    RowLayout {
                        Layout.fillWidth: true
                        GlassField {id: presetName;placeholderText: "Preset name";Layout.fillWidth: true}
                        GlassButton {text: "Save";onClicked: {appState.savePreset(presetName.text);presetName.clear()}}
                    }
                    Repeater {model: appState.savedPresets;delegate: GlassButton {required property var modelData;required property int index;text: modelData.name;Layout.fillWidth: true;onClicked: appState.loadPreset(index)}}
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text {Layout.fillWidth: true;text: "StrataCopy v" + Qt.application.version;color: "#7e8da3";font.pixelSize: 10}
            GlassButton {
                objectName: "checkForUpdates"
                text: appState.checkingForUpdates ? "Checking…" : "Check for Updates";implicitHeight: 29
                enabled: !appState.checkingForUpdates
                onClicked: appState.checkForUpdates()
                ToolTip.visible: hovered
                ToolTip.text: "Compare your version with the latest GitHub release"
            }
        }
    }
}
