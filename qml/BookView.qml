import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Stories
import Stories.Native

Rectangle {
    id: root
    required property string bookId
    signal back()
    color: Theme.bg

    readonly property var book: Core.book(bookId, Core.revision)
    readonly property bool isCurrent: Core.current.id === bookId
    readonly property real at: isCurrent ? Core.position : (book.positionMs ?? 0)
    readonly property string status: book.status ?? "new"

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        model: root.book.chapters ?? []
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        bottomMargin: 20

        header: ColumnLayout {
            width: list.width
            spacing: 0

            IconButton {
                Layout.margins: 12
                icon_: "arrowBack"
                tip: "Back"
                onClicked: root.back()
            }
            RowLayout {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                spacing: 28
                Cover {
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: 220
                    Layout.alignment: Qt.AlignTop
                    source: root.book.cover ?? ""
                    title: root.book.title ?? ""
                    author: root.book.author ?? ""
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 5
                    Text {
                        visible: text.length > 0
                        text: (root.book.series ?? "").toUpperCase()
                        color: Theme.accent
                        font.pixelSize: 12
                        font.weight: Font.Bold
                        font.letterSpacing: 1
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.book.title ?? ""
                        color: Theme.text
                        font.pixelSize: 32
                        font.weight: Font.Bold
                        wrapMode: Text.Wrap
                    }
                    Text { text: root.book.author ?? ""; color: Theme.secondary; font.pixelSize: 16 }
                    Text {
                        visible: (root.book.narrator ?? "").length > 0 && root.book.narrator !== root.book.author
                        text: "Read by " + root.book.narrator
                        color: Theme.muted
                        font.pixelSize: 14
                    }
                    Text {
                        text: [root.book.year, Theme.span(root.book.durationMs ?? 0),
                               (root.book.chapters?.length ?? 0) > 1 ? root.book.chapters.length + " chapters" : ""]
                            .filter(x => x).join("  ·  ")
                        color: Theme.muted
                        font.pixelSize: 13
                    }
                    ColumnLayout {
                        visible: root.status === "listening" || root.isCurrent
                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        spacing: 5
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.maximumWidth: 520
                            height: 4; radius: 2
                            color: Theme.raised
                            Rectangle {
                                width: parent.width * Math.min(1, root.at / Math.max(1, root.book.durationMs ?? 1))
                                height: 4; radius: 2
                                color: Theme.accent
                            }
                        }
                        Text {
                            text: `${Math.floor(root.at * 100 / Math.max(1, root.book.durationMs ?? 1))}% heard  ·  ${Theme.span((root.book.durationMs ?? 0) - root.at)} left`
                            color: Theme.secondary
                            font.pixelSize: 13
                        }
                    }
                    RowLayout {
                        Layout.topMargin: 12
                        spacing: 10
                        Pill {
                            primary: true
                            readonly property bool playingThis: root.isCurrent && Core.playing
                            icon_: playingThis ? "pause" : "play"
                            text: playingThis ? "Pause" : root.status === "listening" ? "Continue"
                                : root.status === "finished" ? "Listen again" : "Play"
                            onClicked: playingThis ? Core.toggle() : Core.play(root.bookId)
                        }
                        Pill {
                            icon_: root.status === "finished" ? "close" : "check"
                            text: root.status === "finished" ? "Mark as new" : "Mark finished"
                            onClicked: Core.markFinished(root.bookId, root.status !== "finished")
                        }
                    }
                }
            }

            Text {
                visible: (root.book.bookmarks?.length ?? 0) > 0
                Layout.leftMargin: 32
                Layout.topMargin: 26
                Layout.bottomMargin: 6
                text: "Bookmarks"
                color: Theme.secondary
                font.pixelSize: 14
                font.weight: Font.Bold
            }
            Repeater {
                model: root.book.bookmarks ?? []
                Rectangle {
                    id: mark
                    required property var modelData
                    property bool editing: false
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    implicitHeight: 48
                    radius: 8
                    color: markHover.hovered ? Theme.raised : "transparent"
                    HoverHandler { id: markHover }
                    TapHandler { enabled: !mark.editing; onTapped: Core.play(root.bookId, mark.modelData.bookMs) }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 4
                        spacing: 12
                        Icon { name: "bookmark"; size: 17; color: Theme.accent }
                        Text {
                            visible: !mark.editing
                            Layout.fillWidth: true
                            text: mark.modelData.note || mark.modelData.chapter
                            color: Theme.text
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                        TextField {
                            id: noteField
                            visible: mark.editing
                            Layout.fillWidth: true
                            text: mark.modelData.note
                            placeholderText: "A note"
                            color: Theme.text
                            placeholderTextColor: Theme.muted
                            background: Rectangle { radius: 6; color: Theme.surface; border.color: Theme.accent }
                            onAccepted: { Core.noteBookmark(mark.modelData.id, text); mark.editing = false }
                            Keys.onEscapePressed: mark.editing = false
                        }
                        Text { text: Theme.clock(mark.modelData.bookMs); color: Theme.muted; font.pixelSize: 13 }
                        IconButton {
                            icon_: "edit"; iconSize: 16; tint: Theme.muted; tip: "Note"
                            onClicked: { mark.editing = !mark.editing; if (mark.editing) noteField.forceActiveFocus() }
                        }
                        IconButton {
                            icon_: "close"; iconSize: 16; tint: Theme.muted; tip: "Remove"
                            onClicked: Core.removeBookmark(mark.modelData.id)
                        }
                    }
                }
            }

            Text {
                Layout.leftMargin: 32
                Layout.topMargin: 26
                Layout.bottomMargin: 6
                text: (root.book.chapters?.length ?? 0) > 1 ? "Chapters" : "File"
                color: Theme.secondary
                font.pixelSize: 14
                font.weight: Font.Bold
            }
        }

        delegate: Item {
            id: row
            required property var modelData
            required property int index
            readonly property bool here: root.isCurrent ? index === Core.chapter : modelData.current
            width: list.width
            height: 44
            HoverHandler { id: rowHover }
            TapHandler { onTapped: Core.play(root.bookId, row.modelData.start) }
            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                radius: 8
                color: row.here ? Qt.alpha(Theme.accent, 0.14) : rowHover.hovered ? Theme.raised : "transparent"
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 32
                anchors.rightMargin: 32
                spacing: 12
                Item {
                    implicitWidth: 28
                    implicitHeight: 20
                    Rectangle {
                        visible: row.here
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8; height: 8; radius: 4
                        color: Theme.accent
                    }
                    Text {
                        visible: !row.here
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.index + 1
                        color: Theme.muted
                        font.pixelSize: 13
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: row.modelData.title
                    color: row.here ? Theme.accent : row.modelData.heard ? Theme.muted : Theme.text
                    font.pixelSize: 14
                    font.weight: row.here ? Font.Bold : Font.Medium
                    elide: Text.ElideRight
                }
                Text { text: Theme.clock(row.modelData.durationMs); color: Theme.muted; font.pixelSize: 13 }
            }
        }
    }
}
