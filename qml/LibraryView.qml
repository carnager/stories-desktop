import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Stories
import Stories.Native

Item {
    id: root
    signal openBook(string id)
    signal openSettings()

    function focusSearch() { grid.headerItem.focusSearch() }

    // The header grows and shrinks with this row; near the top, the view stays at the top.
    onShowContinueChanged: if (grid.pinned) pin.restart()
    Timer {
        id: pin
        interval: 0
        onTriggered: grid.contentY = grid.originY
    }

    readonly property int columns: Math.max(2, Math.floor((width - 40) / 186))
    readonly property real cell: (width - 40) / columns
    readonly property bool showContinue: Core.listening.count > 0 && Core.filter === "all" && Core.query.length === 0

    GridView {
        id: grid
        property bool pinned: true
        onMovementEnded: pinned = contentY - originY < 12
        onOriginYChanged: if (pinned) pin.restart()
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        cellWidth: root.cell
        cellHeight: root.cell + 84
        model: Core.books
        reuseItems: true
        cacheBuffer: 600
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        header: Column {
            width: grid.width
            topPadding: 20
            spacing: 16
            function focusSearch() { search.forceActiveFocus(); search.selectAll() }

            RowLayout {
                width: parent.width
                spacing: 8
                Text {
                    text: "Library"
                    color: Theme.text
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    Layout.rightMargin: 12
                }
                Repeater {
                    model: [["all", "All"], ["listening", "Listening"], ["new", "New"], ["finished", "Finished"]]
                    Pill {
                        required property var modelData
                        implicitHeight: 32
                        text: modelData[1]
                        selected: Core.filter === modelData[0]
                        onClicked: Core.filter = modelData[0]
                    }
                }
                Item { Layout.fillWidth: true }
                TextField {
                    id: search
                    Layout.preferredWidth: 230
                    implicitHeight: 36
                    text: Core.query
                    onTextEdited: Core.query = text
                    Keys.onEscapePressed: { Core.query = ""; text = ""; focus = false }
                    placeholderText: "Search"
                    placeholderTextColor: Theme.muted
                    color: Theme.text
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.onAccent
                    font.pixelSize: 14
                    leftPadding: 36
                    background: Rectangle {
                        radius: 18
                        color: Theme.surface
                        border.width: search.activeFocus ? 2 : 1
                        border.color: search.activeFocus ? Theme.accent : Theme.line
                        Icon { x: 11; anchors.verticalCenter: parent.verticalCenter; name: "search"; size: 18; color: Theme.muted }
                    }
                }
                IconButton { icon_: "settings"; tint: Theme.secondary; tip: "Settings"; onClicked: root.openSettings() }
            }

            Column {
                visible: root.showContinue
                width: parent.width
                spacing: 8
                Text {
                    leftPadding: 8
                    text: "Continue listening"
                    color: Theme.secondary
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }
                ListView {
                    width: parent.width
                    height: 112
                    orientation: ListView.Horizontal
                    spacing: 10
                    clip: true
                    model: root.showContinue ? Core.listening : null
                    boundsBehavior: Flickable.StopAtBounds
                    delegate: Rectangle {
                        id: card
                        required property string bookId
                        required property string title
                        required property string author
                        required property string cover
                        required property real fraction
                        required property string line
                        width: 320
                        height: 112
                        radius: 10
                        color: cardHover.hovered ? Theme.raised : Theme.surface
                        HoverHandler { id: cardHover }
                        TapHandler { onTapped: root.openBook(card.bookId) }
                        Cover {
                            id: art
                            x: 10; y: 10
                            width: 92; height: 92
                            source: card.cover; title: card.title; author: card.author
                        }
                        Column {
                            anchors { left: art.right; right: resume.left; verticalCenter: parent.verticalCenter; leftMargin: 12; rightMargin: 6 }
                            spacing: 3
                            Text { width: parent.width; text: card.title; color: Theme.text; font.pixelSize: 14; font.weight: Font.Bold; elide: Text.ElideRight }
                            Text { width: parent.width; text: card.author; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
                            Item { width: 1; height: 5 }
                            Rectangle {
                                width: parent.width; height: 4; radius: 2
                                color: Theme.raised
                                Rectangle { width: parent.width * card.fraction; height: 4; radius: 2; color: Theme.accent }
                            }
                            Text { width: parent.width; text: card.line; color: Theme.secondary; font.pixelSize: 12; elide: Text.ElideRight }
                        }
                        IconButton {
                            id: resume
                            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                            icon_: Core.current.id === card.bookId && Core.playing ? "pause" : "play"
                            filled: true
                            onClicked: Core.current.id === card.bookId ? Core.toggle() : Core.play(card.bookId)
                        }
                    }
                }
            }
            Item { width: 1; height: 1 }
        }

        delegate: BookTile {
            width: grid.cellWidth
            height: grid.cellHeight
            onOpen: id => root.openBook(id)
        }
    }

    Text {
        anchors.centerIn: parent
        visible: Core.books.count === 0
        text: Core.query.length > 0 || Core.filter !== "all" ? "Nothing here." : "No books yet. Is the server's library folder right?"
        color: Theme.muted
        font.pixelSize: 15
    }
}
