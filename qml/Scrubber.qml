import QtQuick
import Stories

Item {
    id: root
    property real position: 0
    property real duration: 1
    property var chapters: []
    signal moved(real ms)

    readonly property real shown: drag.pressed ? drag.at : position
    implicitHeight: 22

    Rectangle {
        id: track
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        height: drag.containsMouse || drag.pressed ? 6 : 4
        radius: height / 2
        color: Theme.raised
        Rectangle {
            width: parent.width * Math.min(1, root.shown / Math.max(1, root.duration))
            height: parent.height
            radius: parent.radius
            color: Theme.accent
        }
        Repeater {
            model: root.chapters.length > 1 && root.chapters.length <= 120 ? root.chapters : []
            Rectangle {
                required property var modelData
                required property int index
                visible: index > 0
                x: track.width * modelData.start / Math.max(1, root.duration)
                width: 1
                height: track.height
                color: Theme.bg
            }
        }
    }
    Rectangle {
        visible: drag.containsMouse || drag.pressed
        x: root.width * Math.min(1, root.shown / Math.max(1, root.duration)) - width / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 14; height: 14; radius: 7
        color: Theme.accent
    }
    MouseArea {
        id: drag
        property real at: 0
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        preventStealing: true
        function follow(x) { at = Math.max(0, Math.min(1, x / width)) * root.duration }
        onPressed: mouse => follow(mouse.x)
        onPositionChanged: mouse => { if (pressed) follow(mouse.x) }
        onReleased: root.moved(at)
    }
}
