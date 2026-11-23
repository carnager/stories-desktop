pragma Singleton
import QtQuick

QtObject {
    readonly property bool dark: Application.styleHints.colorScheme !== Qt.Light

    readonly property color bg: dark ? "#16140f" : "#f7f3ea"
    readonly property color surface: dark ? "#211e17" : "#fffcf5"
    readonly property color raised: dark ? "#2e2a20" : "#ece6d8"
    readonly property color line: dark ? "#3a3528" : "#ddd5c4"
    readonly property color text: dark ? "#f1eadb" : "#2a251b"
    readonly property color secondary: dark ? "#bdb39e" : "#5f5645"
    readonly property color muted: dark ? "#8b826f" : "#8a806c"
    readonly property color accent: dark ? "#e9a94e" : "#b5701a"
    readonly property color onAccent: dark ? "#241802" : "#fffaf0"
    readonly property color done: dark ? "#8fb573" : "#55803a"
    readonly property color danger: dark ? "#ec8a78" : "#b53a26"

    function clock(ms) {
        const total = Math.max(0, Math.floor(ms / 1000))
        const h = Math.floor(total / 3600), m = Math.floor(total / 60) % 60, s = total % 60
        const two = n => String(n).padStart(2, "0")
        return h > 0 ? `${h}:${two(m)}:${two(s)}` : `${m}:${two(s)}`
    }

    function span(ms) {
        const minutes = Math.floor(Math.max(0, ms) / 60000)
        if (minutes < 1) return "under a minute"
        return minutes < 60 ? `${minutes} min` : `${Math.floor(minutes / 60)} h ${minutes % 60} min`
    }

    function speedLabel(s) {
        return `${Number(s).toFixed(2).replace(/0$/, "").replace(/\.0$/, "")}×`
    }
}
