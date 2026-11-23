import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Stories
import Stories.Native

ApplicationWindow {
    id: window
    width: 1240
    height: 820
    minimumWidth: 820
    minimumHeight: 560
    visible: true
    title: Core.current.title ? `${Core.current.title} – Stories` : "Stories"
    color: Theme.bg

    property string openBook: ""
    property bool expanded: false

    function speedChoices() {
        return [0.8, 1, 1.1, 1.2, 1.3, 1.5, 1.75, 2, 2.5].map(s => ({
            label: Theme.speedLabel(s), value: s, selected: Math.abs(Core.speed - s) < 0.01
        }))
    }
    function sleepChoices() {
        const list = [5, 10, 15, 20, 30, 45, 60, 90].map(m => ({ label: `${m} minutes`, value: m }))
        list.push({ label: "End of chapter", value: -1, selected: Core.sleepAtChapter })
        if (Core.sleepLeft > 0 || Core.sleepAtChapter)
            list.push({ label: "Turn off", value: 0 })
        return list
    }
    function askSpeed(item) { speedPopup.choices = speedChoices(); speedPopup.openAt(item) }
    function askSleep(item) { sleepPopup.choices = sleepChoices(); sleepPopup.openAt(item) }

    Shortcut { sequence: "Space"; enabled: Core.signedIn; onActivated: Core.toggle() }
    Shortcut { sequence: "Left"; enabled: Core.signedIn; onActivated: Core.skip(-15000) }
    Shortcut { sequence: "Right"; enabled: Core.signedIn; onActivated: Core.skip(30000) }
    Shortcut { sequence: "Ctrl+Left"; enabled: Core.signedIn; onActivated: Core.previousChapter() }
    Shortcut { sequence: "Ctrl+Right"; enabled: Core.signedIn; onActivated: Core.nextChapter() }
    Shortcut { sequence: "B"; enabled: Core.signedIn && Core.current.id !== undefined; onActivated: Core.addBookmark() }
    Shortcut { sequence: "Ctrl+F"; enabled: Core.signedIn; onActivated: { window.expanded = false; window.openBook = ""; library.focusSearch() } }
    Shortcut {
        sequence: "Escape"
        enabled: window.expanded || window.openBook.length > 0
        onActivated: window.expanded ? window.expanded = false : window.openBook = ""
    }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }

    Connections {
        target: Core
        function onRaiseRequested() { window.show(); window.raise(); window.requestActivate() }
        function onSignedInChanged() {
            if (!Core.signedIn) { window.expanded = false; window.openBook = "" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: Core.ready && Core.signedIn

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            LibraryView {
                id: library
                anchors.fill: parent
                visible: window.openBook.length === 0
                onOpenBook: id => window.openBook = id
                onOpenSettings: settings.open()
            }
            Loader {
                anchors.fill: parent
                active: window.openBook.length > 0
                sourceComponent: BookView {
                    bookId: window.openBook
                    onBack: window.openBook = ""
                }
            }
        }
        PlayerBar {
            Layout.fillWidth: true
            onExpand: window.expanded = true
            onChooseSpeed: item => window.askSpeed(item)
            onChooseSleep: item => window.askSleep(item)
        }
    }

    Loader {
        anchors.fill: parent
        active: window.expanded && Core.current.id !== undefined
        sourceComponent: NowPlaying {
            onClose: window.expanded = false
            onChooseSpeed: item => window.askSpeed(item)
            onChooseSleep: item => window.askSleep(item)
        }
    }

    LoginView {
        anchors.fill: parent
        visible: Core.ready && !Core.signedIn
    }

    Rectangle {
        visible: Core.signedIn && Core.error.length > 0
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 14 }
        width: Math.min(toast.implicitWidth + 62, parent.width - 40)
        height: 44
        radius: 22
        color: Theme.raised
        border.width: 1
        border.color: Theme.danger
        z: 10
        Text {
            id: toast
            anchors { left: parent.left; right: dismiss.left; verticalCenter: parent.verticalCenter; leftMargin: 18 }
            text: Core.error
            color: Theme.text
            font.pixelSize: 13
            elide: Text.ElideRight
        }
        IconButton {
            id: dismiss
            anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
            icon_: "close"
            iconSize: 16
            onClicked: Core.clearError()
        }
    }

    ChoicePopup {
        id: speedPopup
        heading: "Speed, for this book"
        onChosen: value => Core.speed = value
    }
    ChoicePopup {
        id: sleepPopup
        heading: "Sleep timer"
        onChosen: value => value > 0 ? Core.sleepIn(value) : value < 0 ? Core.sleepAtEndOfChapter() : Core.cancelSleep()
    }
    SettingsDialog { id: settings; objectName: "settings"; onAddPhone: addPhone.open() }
    AddPhoneDialog { id: addPhone; objectName: "addPhone" }
    NewerDialog {}
}
