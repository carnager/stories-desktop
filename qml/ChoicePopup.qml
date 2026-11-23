import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Stories

// A small list of choices, opened beside the button that asked for it.
Popup {
    id: root
    property string heading
    property var choices: []  // [{ label, value, selected }]
    signal chosen(var value)

    function openAt(item) {
        parent = Overlay.overlay
        const p = item.mapToItem(parent, item.width / 2, 0)
        x = Math.max(8, Math.min(parent.width - width - 8, p.x - width / 2))
        y = Math.max(8, p.y - implicitHeight - 8)
        open()
    }

    width: 230
    padding: 8
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle {
        radius: 12
        color: Theme.surface
        border.width: 1
        border.color: Theme.line
    }
    contentItem: ColumnLayout {
        spacing: 1
        Text {
            Layout.margins: 8
            text: root.heading
            color: Theme.secondary
            font.pixelSize: 12
            font.weight: Font.Bold
        }
        Repeater {
            model: root.choices
            AbstractButton {
                id: item
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 34
                hoverEnabled: true
                background: Rectangle { radius: 7; color: item.hovered ? Theme.raised : "transparent" }
                contentItem: RowLayout {
                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: 9
                        text: item.modelData.label
                        color: item.modelData.selected ? Theme.accent : Theme.text
                        font.pixelSize: 14
                        font.weight: item.modelData.selected ? Font.Bold : Font.Medium
                    }
                    Icon {
                        visible: item.modelData.selected === true
                        Layout.rightMargin: 8
                        name: "check"
                        size: 16
                        color: Theme.accent
                    }
                }
                onClicked: { root.chosen(modelData.value); root.close() }
            }
        }
    }
}
