import QtQuick
import Stories

Rectangle {
    id: root
    property string source
    property string title
    property string author

    // Without a picture: a colour of its own, from its name.
    readonly property real hue: {
        let h = 0
        for (let i = 0; i < title.length; ++i) h = (h * 31 + title.charCodeAt(i)) % 360
        return h / 360
    }
    gradient: Gradient {
        GradientStop { position: 0; color: Qt.hsla(root.hue, 0.35, 0.34, 1) }
        GradientStop { position: 1; color: Qt.hsla((root.hue + 0.08) % 1, 0.45, 0.2, 1) }
    }
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 0.08)
    clip: true

    Column {
        visible: image.status !== Image.Ready
        anchors.fill: parent
        anchors.margins: root.width * 0.09
        spacing: 4
        Text {
            width: parent.width
            text: root.title
            color: "#fff8e8"
            wrapMode: Text.Wrap
            maximumLineCount: 4
            elide: Text.ElideRight
            font.pixelSize: Math.max(11, root.width * 0.115)
            font.weight: Font.Bold
        }
        Text {
            width: parent.width
            text: root.author
            color: "#ddd3bd"
            elide: Text.ElideRight
            font.pixelSize: Math.max(9, root.width * 0.07)
        }
    }
    Image {
        id: image
        anchors.fill: parent
        source: root.source
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(480, 480)
    }
}
