cmake_minimum_required(VERSION 3.21)

project(obsidian-settings VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick DBus)
qt_standard_project_setup()

qt_add_executable(obsidian-settings
    main.cpp
    nativebridge.cpp
    nativebridge.h
)

qt_add_qml_module(obsidian-settings
    URI Obsidian.Settings
    VERSION 1.0
    QML_FILES
        Main.qml
        SettingsCard.qml
        ShortcutsPage.qml
        DesktopsPage.qml
        WifiPage.qml
        BluetoothPage.qml
        InputPage.qml
        ObsidianPage.qml
        WallpaperPicker.qml
        ShortcutRecorder.qml
        ObsidianStyle.js
        InputSettings.js
        AppSettings.js
)

target_link_libraries(obsidian-settings PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::DBus
)

include(GNUInstallDirs)
install(TARGETS obsidian-settings
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
