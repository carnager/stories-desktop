import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Stories
import Stories.Native

// First start: run a server here, or use one that already runs.
Rectangle {
    id: root
    color: Theme.bg
    property string mode: Core.serverUrl.length > 0 ? "remote" : ""
    readonly property bool working: Core.busy || LocalServer.busy
    readonly property string problem: mode === "local" ? (LocalServer.error || Core.error) : Core.error
    readonly property bool needsFolder: mode === "local" && !LocalServer.hasConfig && !LocalServer.running
    readonly property bool choosingPassword: mode === "local" && !LocalServer.hasPassword

    component Field: TextField {
        Layout.fillWidth: true
        implicitHeight: 42
        color: Theme.text
        placeholderTextColor: Theme.muted
        selectionColor: Theme.accent
        selectedTextColor: Theme.onAccent
        font.pixelSize: 15
        leftPadding: 13
        background: Rectangle {
            radius: 8
            color: Theme.surface
            border.width: parent.activeFocus ? 2 : 1
            border.color: parent.activeFocus ? Theme.accent : Theme.line
        }
    }
    component Label_: Text {
        color: Theme.secondary
        font.pixelSize: 13
        font.weight: Font.DemiBold
    }
    component Choice: AbstractButton {
        id: choice
        property string icon_
        property string detail
        Layout.fillWidth: true
        implicitHeight: 92
        hoverEnabled: true
        opacity: enabled ? 1 : 0.5
        background: Rectangle {
            radius: 12
            color: choice.hovered ? Theme.raised : Theme.surface
            border.width: 1
            border.color: choice.hovered ? Theme.accent : Theme.line
        }
        contentItem: RowLayout {
            spacing: 16
            Rectangle {
                Layout.leftMargin: 16
                width: 46; height: 46; radius: 23
                color: Theme.raised
                Icon { anchors.centerIn: parent; name: choice.icon_; size: 24; color: Theme.accent }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.rightMargin: 16
                spacing: 3
                Text { text: choice.text; color: Theme.text; font.pixelSize: 16; font.weight: Font.Bold }
                Text {
                    Layout.fillWidth: true
                    text: choice.detail
                    color: Theme.muted
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    FolderDialog {
        id: folders
        title: "Your audiobooks"
        onAccepted: localFolder.text = LocalServer.localPath(selectedFolder)
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 48)
        spacing: 12

        Text {
            text: "Stories"
            color: Theme.text
            font.pixelSize: 40
            font.weight: Font.Bold
        }
        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 14
            text: root.mode === "local" ? (LocalServer.running
                    ? "storiesd is already running on this computer. Enter its password to connect."
                    : root.choosingPassword
                    ? "storiesd will serve your audiobooks as a service of your user. Your phone can connect to it too."
                    : LocalServer.hasConfig
                    ? "storiesd is already set up on this computer. Enter its password to start it and connect."
                    : "storiesd needs its folder again. Your positions are kept, so enter the password you set before.")
                : root.mode === "remote" ? "Connect to storiesd on your home server or another computer."
                : "Where do your audiobooks live?"
            color: Theme.secondary
            font.pixelSize: 15
            wrapMode: Text.Wrap
        }

        Choice {
            visible: root.mode === ""
            enabled: LocalServer.available
            icon_: "computer"
            text: "On this computer"
            detail: LocalServer.running ? "storiesd is already running here. Connect to it."
                  : LocalServer.available ? "Set up storiesd here. It starts with your session."
                                          : "Needs storiesd and systemd, which weren't found."
            onClicked: { Core.clearError(); LocalServer.refresh(); root.mode = "local" }
        }
        Choice {
            visible: root.mode === ""
            icon_: "cloud"
            text: "On a server"
            detail: "Connect to a storiesd that's already running."
            onClicked: { Core.clearError(); root.mode = "remote" }
        }

        Label_ { visible: root.needsFolder; text: "Audiobook folder" }
        RowLayout {
            visible: root.needsFolder
            Layout.fillWidth: true
            spacing: 8
            Field {
                id: localFolder
                text: LocalServer.library
                placeholderText: "/home/you/audiobooks"
            }
            IconButton { icon_: "folder"; tip: "Choose a folder"; onClicked: folders.open() }
        }

        Label_ { visible: root.mode === "remote"; text: "Address" }
        Field {
            id: address
            visible: root.mode === "remote"
            text: Core.serverUrl
            placeholderText: "192.168.1.10 or https://books.example.org"
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
            onAccepted: password.forceActiveFocus()
        }

        Label_ {
            visible: root.mode !== ""
            text: root.choosingPassword ? "Choose a password" : "Password"
        }
        Field {
            id: password
            visible: root.mode !== ""
            echoMode: TextInput.Password
            placeholderText: root.choosingPassword ? "At least 8 characters, used only for Stories" : ""
            onAccepted: root.choosingPassword ? repeat.forceActiveFocus() : root.submit()
        }
        Label_ { visible: root.choosingPassword; text: "Repeat the password" }
        Field {
            id: repeat
            visible: root.choosingPassword
            echoMode: TextInput.Password
            onAccepted: root.submit()
        }

        Text {
            Layout.fillWidth: true
            visible: root.problem.length > 0
            text: root.problem
            color: Theme.danger
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }
        Text {
            visible: LocalServer.busy
            text: LocalServer.step + "…"
            color: Theme.secondary
            font.pixelSize: 13
        }

        RowLayout {
            visible: root.mode !== ""
            Layout.topMargin: 6
            spacing: 10
            Pill {
                text: "Back"
                enabled: !root.working
                onClicked: { Core.clearError(); password.text = ""; repeat.text = ""; root.mode = "" }
            }
            Item { Layout.fillWidth: true }
            Pill {
                primary: true
                text: root.working ? "Working…" : root.choosingPassword || root.needsFolder ? "Set up" : "Connect"
                enabled: !root.working && password.text.length > 0 && (!root.choosingPassword || repeat.text.length > 0)
                onClicked: root.submit()
            }
        }
    }

    function submit() {
        if (working || password.text.length === 0)
            return
        Core.clearError()
        if (mode === "local")
            LocalServer.setup(localFolder.text, password.text, repeat.text)
        else
            Core.login(address.text, password.text)
    }

    Connections {
        target: Core
        function onSignedInChanged() {
            if (Core.signedIn) {
                password.text = ""
                repeat.text = ""
            }
        }
    }
}
