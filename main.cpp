#include "nativebridge.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int main(int argc, char *argv[])
{
    bool daemonMode = false;
    bool wallpaperPickerMode = false;
    for (int i = 1; i < argc; ++i) {
        daemonMode = daemonMode || QByteArray(argv[i]) == QByteArrayLiteral("--daemon");
        wallpaperPickerMode = wallpaperPickerMode
            || QByteArray(argv[i]) == QByteArrayLiteral("--wallpaper-picker");
    }
    if (daemonMode) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("Obsidian Settings"));
        QCoreApplication::setOrganizationName(QStringLiteral("Obsidian"));
        NativeBridge bridge;
        QObject::connect(&bridge, &NativeBridge::errorOccurred, &app,
                         [](const QString &message) { qWarning().noquote() << message; });
        return bridge.startShortcutDaemon() ? app.exec() : 1;
    }

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Obsidian Settings"));
    QCoreApplication::setOrganizationName(QStringLiteral("Obsidian"));

    NativeBridge bridge;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("obsidian"), &bridge);
    const QString qmlFile = wallpaperPickerMode
        ? QStringLiteral("qrc:/qt/qml/Obsidian/Settings/WallpaperPicker.qml")
        : QStringLiteral("qrc:/qt/qml/Obsidian/Settings/Main.qml");
    engine.load(QUrl(qmlFile));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
