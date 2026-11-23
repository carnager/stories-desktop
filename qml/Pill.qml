import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Stories

AbstractButton {
    id: root
    property string icon_
    property bool primary: false
    property bool selected: false
    readonly property color ink: primary || selected ? Theme.onAccent : Theme.text

    implicitHeight: 38
    implicitWidth: row.implicitWidth + 32
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    opacity: enabled ? (hovered && !primary && !selected ? 0.85 : 1) : 0.45

    background: Rectangle {
        radius: height / 2
        color: root.primary || root.selected ? Theme.accent : Theme.raised
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.text
    }
    contentItem: Item {
        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 7
            Icon {
                visible: root.icon_.length > 0
                name: root.icon_
                size: 18
                color: root.ink
            }
            Text {
                text: root.text
                color: root.ink
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
        }
    }
}
