cmake_minimum_required(VERSION 3.21)
project(stories-desktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(STORIES_DEV "Screenshot and null-audio switches for testing" OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network DBus)
find_package(Qt6Keychain REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(stories-desktop
    src/main.cpp
    src/Api.cpp src/Api.h
    src/BookModel.cpp src/BookModel.h
    src/Core.cpp src/Core.h
    src/LocalServer.cpp src/LocalServer.h
    src/Mpris.cpp src/Mpris.h
    src/MpvPlayer.cpp src/MpvPlayer.h
    src/QrProvider.h
    src/Types.h
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(stories-desktop
    URI Stories
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Icon.qml
        qml/IconButton.qml
        qml/Pill.qml
        qml/Cover.qml
        qml/BookTile.qml
        qml/Scrubber.qml
        qml/LoginView.qml
        qml/LibraryView.qml
        qml/BookView.qml
        qml/PlayerBar.qml
        qml/NowPlaying.qml
        qml/SettingsDialog.qml
        qml/AddPhoneDialog.qml
        qml/NewerDialog.qml
        qml/ChoicePopup.qml
)

target_link_libraries(stories-desktop PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::DBus
    Qt6Keychain::Qt6Keychain PkgConfig::MPV PkgConfig::QRENCODE)

if(STORIES_DEV)
    target_compile_definitions(stories-desktop PRIVATE STORIES_DEV)
endif()

install(TARGETS stories-desktop RUNTIME DESTINATION bin)
install(FILES stories-desktop.desktop DESTINATION share/applications)
