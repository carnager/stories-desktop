import QtQuick
import QtQuick.Controls.Basic
import Stories

AbstractButton {
    id: root
    property string icon_
    property int iconSize: 22
    property color tint: Theme.text
    property bool filled: false
    property string tip

    implicitWidth: iconSize + 18
    implicitHeight: iconSize + 18
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    opacity: enabled ? 1 : 0.35
    scale: pressed ? 0.92 : 1
    Behavior on scale { NumberAnimation { duration: 80 } }

    background: Rectangle {
        radius: width / 2
        color: root.filled ? Theme.accent : root.hovered ? Theme.raised : "transparent"
    }
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: root.icon_
            size: root.iconSize
            color: root.filled ? Theme.onAccent : root.tint
        }
    }
    ToolTip.visible: hovered && tip.length > 0
    ToolTip.text: tip
    ToolTip.delay: 600
}
