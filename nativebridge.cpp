#include "nativebridge.h"

#include <QDateTime>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMetaObject>
#include <QProcess>
#include <QPointer>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <QUuid>
#include <QXmlStreamReader>
#include <algorithm>
#include <functional>
#include <utility>

struct DesktopEntry
{
    uint position = 0;
    QString id;
    QString name;
};
Q_DECLARE_METATYPE(DesktopEntry)

using DesktopEntries = QList<DesktopEntry>;

QDBusArgument &operator<<(QDBusArgument &argument, const DesktopEntry &entry)
{
    argument.beginStructure();
    argument << entry.position << entry.id << entry.name;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DesktopEntry &entry)
{
    argument.beginStructure();
    argument >> entry.position >> entry.id >> entry.name;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const DesktopEntries &entries)
{
    argument.beginArray(qMetaTypeId<DesktopEntry>());
    for (const auto &entry : entries)
        argument << entry;
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DesktopEntries &entries)
{
    argument.beginArray();
    while (!argument.atEnd()) {
        DesktopEntry entry;
        argument >> entry;
        entries.append(entry);
    }
    argument.endArray();
    return argument;
}

Q_DECLARE_METATYPE(DesktopEntries)

static void runInBackground(NativeBridge *bridge, std::function<void(NativeBridge *)> task)
{
    const QPointer<NativeBridge> guard(bridge);
    QThreadPool::globalInstance()->start(QRunnable::create([guard, task = std::move(task)] {
        if (guard)
            task(guard.data());
    }));
}

struct ShortcutInfo
{
    QString field0;
    QString field1;
    QString componentId;
    QString componentName;
    QString actionId;
    QString actionName;
    QList<QList<int>> shortcuts;
    QList<QList<int>> defaults;
};
Q_DECLARE_METATYPE(ShortcutInfo)
using ShortcutInfos = QList<ShortcutInfo>;

QDBusArgument &operator<<(QDBusArgument &argument, const ShortcutInfo &info)
{
    argument.beginStructure();
    argument << info.field0 << info.field1 << info.componentId << info.componentName
             << info.actionId << info.actionName << info.shortcuts << info.defaults;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ShortcutInfo &info)
{
    argument.beginStructure();
    argument >> info.field0 >> info.field1 >> info.componentId >> info.componentName
             >> info.actionId >> info.actionName >> info.shortcuts >> info.defaults;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ShortcutInfos &infos)
{
    argument.beginArray(qMetaTypeId<ShortcutInfo>());
    for (const auto &info : infos)
        argument << info;
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ShortcutInfos &infos)
{
    argument.beginArray();
    while (!argument.atEnd()) {
        ShortcutInfo info;
        argument >> info;
        infos.append(info);
    }
    argument.endArray();
    return argument;
}

Q_DECLARE_METATYPE(ShortcutInfos)

using NmConnectionSettings = QMap<QString, QVariantMap>;
QDBusArgument &operator<<(QDBusArgument &argument, const NmConnectionSettings &settings)
{
    argument.beginMap(qMetaTypeId<QString>(), qMetaTypeId<QVariantMap>());
    for (auto it = settings.cbegin(); it != settings.cend(); ++it)
        argument << it.key() << it.value();
    argument.endMap();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, NmConnectionSettings &settings)
{
    settings.clear();
    argument.beginMap();
    while (!argument.atEnd()) {
        QString key;
        QVariantMap value;
        argument.beginMapEntry();
        argument >> key >> value;
        argument.endMapEntry();
        settings.insert(key, value);
    }
    argument.endMap();
    return argument;
}
Q_DECLARE_METATYPE(NmConnectionSettings)

namespace {
constexpr auto KWinService = "org.kde.KWin";
constexpr auto DesktopPath = "/VirtualDesktopManager";
constexpr auto DesktopInterface = "org.kde.KWin.VirtualDesktopManager";
constexpr auto PropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto KGlobalAccelService = "org.kde.kglobalaccel";
constexpr auto KGlobalAccelPath = "/kglobalaccel";
constexpr auto KGlobalAccelInterface = "org.kde.KGlobalAccel";
constexpr auto KGlobalAccelComponentInterface = "org.kde.kglobalaccel.Component";
constexpr auto NetworkManagerService = "org.freedesktop.NetworkManager";
constexpr auto NetworkManagerPath = "/org/freedesktop/NetworkManager";
constexpr auto NetworkManagerSettingsPath = "/org/freedesktop/NetworkManager/Settings";
constexpr auto NmInterface = "org.freedesktop.NetworkManager";
constexpr auto NmDeviceInterface = "org.freedesktop.NetworkManager.Device";
constexpr auto NmWirelessInterface = "org.freedesktop.NetworkManager.Device.Wireless";
constexpr auto NmAccessPointInterface = "org.freedesktop.NetworkManager.AccessPoint";
constexpr auto NmSettingsInterface = "org.freedesktop.NetworkManager.Settings";
constexpr auto NmConnectionInterface = "org.freedesktop.NetworkManager.Settings.Connection";
constexpr auto NmActiveConnectionInterface = "org.freedesktop.NetworkManager.Connection.Active";
constexpr auto BluezService = "org.bluez";
constexpr auto BluezAdapterInterface = "org.bluez.Adapter1";
constexpr auto BluezDeviceInterface = "org.bluez.Device1";
constexpr auto BluezObjectManagerInterface = "org.freedesktop.DBus.ObjectManager";
constexpr auto KWinInputManagerPath = "/org/kde/KWin/InputDevice";
constexpr auto KWinInputManagerInterface = "org.kde.KWin.InputDeviceManager";
constexpr auto KWinInputDevicePrefix = "/org/kde/KWin/InputDevice/";
constexpr auto KWinInputDeviceInterface = "org.kde.KWin.InputDevice";
constexpr auto XkbRulesPath = "/usr/share/X11/xkb/rules/evdev.xml";
}

using ManagedBluezObjects = QMap<QDBusObjectPath, QMap<QString, QVariantMap>>;
Q_DECLARE_METATYPE(ManagedBluezObjects)

QDBusArgument &operator<<(QDBusArgument &argument, const ManagedBluezObjects &objects)
{
    argument.beginMap(qMetaTypeId<QDBusObjectPath>(),
                      qMetaTypeId<QMap<QString, QVariantMap>>());
    for (auto it = objects.cbegin(); it != objects.cend(); ++it)
        argument << it.key() << it.value();
    argument.endMap();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ManagedBluezObjects &objects)
{
    objects.clear();
    argument.beginMap();
    while (!argument.atEnd()) {
        QDBusObjectPath path;
        QMap<QString, QVariantMap> interfaces;
        argument.beginMapEntry();
        argument >> path >> interfaces;
        argument.endMapEntry();
        objects.insert(path, interfaces);
    }
    argument.endMap();
    return argument;
}

namespace {
QVariant unbox(QVariant value)
{
    while (value.metaType() == QMetaType::fromType<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    return value;
}

QString dbusString(const QVariant &value)
{
    const QVariant normalized = unbox(value);
    if (normalized.metaType() == QMetaType::fromType<QDBusObjectPath>())
        return normalized.value<QDBusObjectPath>().path();
    return normalized.toString();
}

bool dbusCall(const QString &service, const QString &path, const QString &interface,
              const QString &method, const QVariantList &arguments,
              QVariantList *result, QString *error,
              const QDBusConnection &bus = QDBusConnection::sessionBus())
{
    QDBusMessage call = QDBusMessage::createMethodCall(service, path, interface, method);
    call.setArguments(arguments);
    const QDBusMessage response = bus.call(call);
    if (response.type() == QDBusMessage::ErrorMessage) {
        *error = response.errorMessage();
        return false;
    }
    if (result)
        *result = response.arguments();
    return true;
}

bool getAll(const QString &service, const QString &path, const QString &interface,
            QVariantMap *properties, QString *error,
            const QDBusConnection &bus = QDBusConnection::sessionBus())
{
    QVariantList args;
    if (!dbusCall(service, path, QString::fromLatin1(PropertiesInterface),
                  QStringLiteral("GetAll"), {interface}, &args, error, bus)
        || args.isEmpty())
        return false;
    *properties = unbox(args.first()).toMap();
    for (auto it = properties->begin(); it != properties->end(); ++it)
        it.value() = unbox(it.value());
    return true;
}

bool getProperty(const QString &service, const QString &path, const QString &interface,
                 const QString &name, QVariant *value, QString *error,
                 const QDBusConnection &bus = QDBusConnection::sessionBus())
{
    QVariantList args;
    if (!dbusCall(service, path, QString::fromLatin1(PropertiesInterface),
                  QStringLiteral("Get"), {interface, name}, &args, error, bus)
        || args.isEmpty())
        return false;
    *value = unbox(args.first());
    return true;
}

QList<QDBusObjectPath> objectPaths(const QVariant &value)
{
    const QVariant unpacked = unbox(value);
    if (unpacked.canConvert<QList<QDBusObjectPath>>())
        return unpacked.value<QList<QDBusObjectPath>>();
    if (unpacked.canConvert<QDBusArgument>())
        return qdbus_cast<QList<QDBusObjectPath>>(unpacked.value<QDBusArgument>());
    QList<QDBusObjectPath> paths;
    for (const QVariant &entry : unpacked.toList()) {
        if (entry.canConvert<QDBusObjectPath>())
            paths.append(entry.value<QDBusObjectPath>());
        else
            paths.append(QDBusObjectPath(entry.toString()));
    }
    return paths;
}

QString bytesToSsid(const QByteArray &ssid)
{
    QString text = QString::fromUtf8(ssid);
    QString printable;
    for (const QChar ch : text) {
        if (!ch.isNull() && ch.isPrint())
            printable.append(ch);
    }
    return printable.isEmpty() ? QStringLiteral("Unnamed network") : printable;
}

QVariantMap bytesMap(const NmConnectionSettings &settings, const QString &group)
{
    QVariantMap fields = settings.value(group);
    for (auto it = fields.begin(); it != fields.end(); ++it)
        it.value() = unbox(it.value());
    return fields;
}

QStringList splitEscapedConfigList(const QString &value)
{
    QStringList values;
    QString item;
    bool escaped = false;
    for (const QChar ch : value) {
        if (escaped) {
            item.append(ch);
            escaped = false;
        } else if (ch == QLatin1Char('\\')) {
            escaped = true;
        } else if (ch == QLatin1Char(',')) {
            values.append(item);
            item.clear();
        } else {
            item.append(ch);
        }
    }
    if (escaped)
        item.append(QLatin1Char('\\'));
    if (!item.isEmpty() || !value.isEmpty())
        values.append(item);
    return values;
}

bool runConfigTool(const QString &program, const QStringList &arguments,
                   QString *output, QString *error)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted() || !process.waitForFinished(10000)) {
        *error = QStringLiteral("%1 could not complete: %2").arg(program, process.errorString());
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = QStringLiteral("%1 failed: %2").arg(program,
                    QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
        return false;
    }
    if (output)
        *output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    return true;
}

QString xkbConfigPath()
{
    const QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    const QString config = xdg.isEmpty()
        ? QDir::home().filePath(QStringLiteral(".config"))
        : xdg;
    return QDir(config).filePath(QStringLiteral("kxkbrc"));
}

bool readConfigList(const QString &key, QStringList *values, QString *error)
{
    QString raw;
    if (!runConfigTool(QStringLiteral("kreadconfig6"),
                       {QStringLiteral("--file"), xkbConfigPath(),
                        QStringLiteral("--group"), QStringLiteral("Layout"),
                        QStringLiteral("--key"), key}, &raw, error))
        return false;
    *values = splitEscapedConfigList(raw);
    return true;
}

bool writeConfigList(const QString &key, const QStringList &values, QString *error)
{
    QStringList args{QStringLiteral("--file"), xkbConfigPath(),
                     QStringLiteral("--group"), QStringLiteral("Layout"),
                     QStringLiteral("--key"), key};
    if (values.isEmpty() || (values.size() == 1 && values.first().isEmpty())) {
        args << QStringLiteral("--delete");
    } else {
        QStringList encoded;
        for (QString value : values) {
            value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
            value.replace(QStringLiteral(","), QStringLiteral("\\,"));
            encoded.append(value);
        }
        args << QStringLiteral("--type") << QStringLiteral("string")
             << encoded.join(QLatin1Char(','));
    }
    return runConfigTool(QStringLiteral("kwriteconfig6"), args, nullptr, error);
}

bool readNetworkProfiles(QVariantList *profiles, QString *error)
{
    QVariantList result;
    if (!dbusCall(QString::fromLatin1(NetworkManagerService),
                  QString::fromLatin1(NetworkManagerSettingsPath),
                  QString::fromLatin1(NmSettingsInterface), QStringLiteral("ListConnections"),
                  {}, &result, error, QDBusConnection::systemBus()) || result.isEmpty())
        return false;
    const auto paths = objectPaths(result.first());
    for (const QDBusObjectPath &path : paths) {
        QVariantList settingsReply;
        if (!dbusCall(QString::fromLatin1(NetworkManagerService), path.path(),
                      QString::fromLatin1(NmConnectionInterface), QStringLiteral("GetSettings"),
                      {}, &settingsReply, error, QDBusConnection::systemBus()))
            return false;
        if (settingsReply.isEmpty())
            continue;
        const QVariant settingsValue = unbox(settingsReply.first());
        NmConnectionSettings settings;
        if (settingsValue.canConvert<NmConnectionSettings>())
            settings = settingsValue.value<NmConnectionSettings>();
        else if (settingsValue.canConvert<QDBusArgument>())
            settings = qdbus_cast<NmConnectionSettings>(settingsValue.value<QDBusArgument>());
        else {
            *error = QStringLiteral("NetworkManager returned a connection profile in an unsupported format.");
            return false;
        }
        const QVariantMap connection = bytesMap(settings, QStringLiteral("connection"));
        if (connection.value(QStringLiteral("type")).toString()
            != QLatin1String("802-11-wireless"))
            continue;
        const QVariantMap wireless = bytesMap(settings, QStringLiteral("802-11-wireless"));
        const QByteArray ssid = wireless.value(QStringLiteral("ssid")).toByteArray();
        QVariantMap profile{{QStringLiteral("path"), path.path()},
                            {QStringLiteral("name"), connection.value(QStringLiteral("id")).toString()},
                            {QStringLiteral("ssid"), bytesToSsid(ssid)},
                            {QStringLiteral("ssidBytes"), ssid}};
        profiles->append(profile);
    }
    std::sort(profiles->begin(), profiles->end(), [](const QVariant &left, const QVariant &right) {
        return left.toMap().value(QStringLiteral("name")).toString().compare(
            right.toMap().value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    return true;
}

bool wifiDevice(QString *path, QVariantMap *properties, QString *error)
{
    QVariantList result;
    if (!dbusCall(QString::fromLatin1(NetworkManagerService),
                  QString::fromLatin1(NetworkManagerPath), QString::fromLatin1(NmInterface),
                  QStringLiteral("GetDevices"), {}, &result, error,
                  QDBusConnection::systemBus()) || result.isEmpty())
        return false;
    QString firstPath;
    for (const QDBusObjectPath &devicePath : objectPaths(result.first())) {
        QVariantMap device;
        if (!getAll(QString::fromLatin1(NetworkManagerService), devicePath.path(),
                    QString::fromLatin1(NmDeviceInterface), &device, error,
                    QDBusConnection::systemBus()))
            return false;
        if (device.value(QStringLiteral("DeviceType")).toUInt() != 2)
            continue;
        if (firstPath.isEmpty()) {
            firstPath = devicePath.path();
            *properties = device;
        }
        if (dbusString(device.value(QStringLiteral("ActiveConnection"))) != QLatin1String("/")) {
            *path = devicePath.path();
            *properties = device;
            return true;
        }
    }
    *path = firstPath;
    return !firstPath.isEmpty();
}

bool waitForWifi(const QString &devicePath, const QString &connectionPath,
                 const QString &accessPointPath, QString *error)
{
    for (int attempt = 0; attempt < 60; ++attempt) {
        QVariantMap device;
        if (!getAll(QString::fromLatin1(NetworkManagerService), devicePath,
                    QString::fromLatin1(NmDeviceInterface), &device, error,
                    QDBusConnection::systemBus()))
            return false;
        const QString activePath = dbusString(device.value(QStringLiteral("ActiveConnection")));
        if (activePath != QLatin1String("/") && !activePath.isEmpty()) {
            QVariantMap active;
            if (!getAll(QString::fromLatin1(NetworkManagerService), activePath,
                        QString::fromLatin1(NmActiveConnectionInterface), &active, error,
                        QDBusConnection::systemBus()))
                return false;
            const bool correctConnection = connectionPath.isEmpty()
                || active.value(QStringLiteral("Connection")).toString() == connectionPath;
            const bool activeState = active.value(QStringLiteral("State")).toUInt() == 2;
            if (correctConnection && activeState) {
                if (accessPointPath.isEmpty())
                    return true;
                QVariantMap wireless;
                if (!getAll(QString::fromLatin1(NetworkManagerService), devicePath,
                            QString::fromLatin1(NmWirelessInterface), &wireless, error,
                            QDBusConnection::systemBus()))
                    return false;
                if (dbusString(wireless.value(QStringLiteral("ActiveAccessPoint"))) == accessPointPath)
                    return true;
            }
        }
        QThread::msleep(500);
    }
    *error = QStringLiteral("The Wi-Fi connection did not become active within 30 seconds. Check the password, signal, and network availability.");
    return false;
}

QVariantMap networkSecurity(const QVariantMap &ap)
{
    const uint flags = ap.value(QStringLiteral("Flags")).toUInt();
    const uint keyFlags = ap.value(QStringLiteral("WpaFlags")).toUInt()
        | ap.value(QStringLiteral("RsnFlags")).toUInt();
    QString security;
    bool password = false;
    if ((keyFlags & 0x400) && !(keyFlags & 0x100)) {
        security = QStringLiteral("WPA3");
        password = true;
    } else if (keyFlags & (0x800 | 0x1000)) {
        security = QStringLiteral("Enhanced open");
    } else if (keyFlags & (0x100 | 0x400)) {
        security = QStringLiteral("Secured");
        password = true;
    } else if (keyFlags & 0x200) {
        security = QStringLiteral("Enterprise");
        password = true;
    } else if ((flags & 1) || keyFlags) {
        security = QStringLiteral("Unsupported security");
        password = true;
    } else {
        security = QStringLiteral("Open");
    }
    return {{QStringLiteral("security"), security},
            {QStringLiteral("requiresPassword"), password},
            {QStringLiteral("keyFlags"), keyFlags},
            {QStringLiteral("connectable"), !password || (keyFlags & (0x100 | 0x400 | 0x800 | 0x1000))}};
}

bool bluetoothAction(const QString &devicePath, const QString &member, QString *error)
{
    ManagedBluezObjects objects;
    if (!bluezObjects(&objects, error))
        return false;
    const QString adapter = bluezAdapterPath(objects);
    if (adapter.isEmpty() || !devicePath.startsWith(adapter + QStringLiteral("/dev_"))) {
        *error = QStringLiteral("Choose a Bluetooth device from the current adapter.");
        return false;
    }
    const QVariantMap device = objectProperties(objects, devicePath,
                                                 QString::fromLatin1(BluezDeviceInterface));
    if (device.isEmpty() || dbusString(device.value(QStringLiteral("Adapter"))) != adapter) {
        *error = QStringLiteral("The Bluetooth device is no longer available.");
        return false;
    }
    QString property;
    bool expected = true;
    if (member == QLatin1String("Pair"))
        property = QStringLiteral("Paired");
    else if (member == QLatin1String("Connect"))
        property = QStringLiteral("Connected");
    else {
        property = QStringLiteral("Connected");
        expected = false;
    }
    if (device.value(property).toBool() == expected)
        return true;
    QVariantList ignored;
    if (!dbusCall(QString::fromLatin1(BluezService), devicePath,
                  QString::fromLatin1(BluezDeviceInterface), member, {},
                  &ignored, error, QDBusConnection::systemBus()))
        return false;
    for (int attempt = 0; attempt < 80; ++attempt) {
        ManagedBluezObjects current;
        if (!bluezObjects(&current, error))
            return false;
        const QVariantMap currentDevice = objectProperties(
            current, devicePath, QString::fromLatin1(BluezDeviceInterface));
        if (currentDevice.isEmpty()) {
            *error = QStringLiteral("The Bluetooth device disappeared before the operation completed.");
            return false;
        }
        if (currentDevice.value(property).toBool() == expected)
            return true;
        QThread::msleep(250);
    }
    *error = QStringLiteral("BlueZ did not update the device's %1 state.").arg(property.toLower());
    return false;
}

QString shortcutCategory(const QString &componentId, const QString &componentName,
                         const QString &actionName)
{
    const QString component = (componentId + QLatin1Char(' ') + componentName).toLower();
    const QRegularExpression desktop(QStringLiteral("\\b(desktop|virtual|switch.*workspace|workspace)\\b"),
                                     QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression window(QStringLiteral("\\b(close|maximi[sz]e|minimi[sz]e|move|resize|window|snap|tile)\\b"),
                                    QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression common(QStringLiteral("\\b(overview|screenshot|screen capture|volume|audio|brightness|mute|microphone|media|play|pause|lock screen|suspend)\\b"),
                                    QRegularExpression::CaseInsensitiveOption);
    if (component.contains(QStringLiteral("obsidian")))
        return QStringLiteral("Obsidian");
    const bool kwin = component.contains(QStringLiteral("kwin"));
    if (kwin && desktop.match(actionName).hasMatch())
        return QStringLiteral("Virtual desktops");
    if (kwin && window.match(actionName).hasMatch())
        return QStringLiteral("Window management");
    for (const QString &commonComponent : {QStringLiteral("plasmashell"), QStringLiteral("ksmserver"),
                                           QStringLiteral("powerdevil"), QStringLiteral("spectacle"),
                                           QStringLiteral("kaccess"), QStringLiteral("mediacontrol"),
                                           QStringLiteral("kded")}) {
        if ((kwin || component.contains(commonComponent)) && common.match(actionName).hasMatch())
            return QStringLiteral("Common KDE");
    }
    return {};
}

QStringList obsidianActionIds()
{
    return {QStringLiteral("open-settings"), QStringLiteral("toggle-kitty"),
            QStringLiteral("toggle-fish")};
}

QString obsidianActionName(const QString &id)
{
    if (id == QLatin1String("open-settings")) return QStringLiteral("Open Obsidian Settings");
    if (id == QLatin1String("toggle-kitty")) return QStringLiteral("Toggle Kitty module");
    return QStringLiteral("Toggle Fish prompt");
}

QVariantMap objectProperties(const ManagedBluezObjects &objects, const QString &path,
                             const QString &interface)
{
    return objects.value(QDBusObjectPath(path)).value(interface);
}

bool bluezObjects(ManagedBluezObjects *objects, QString *error)
{
    QVariantList result;
    if (!dbusCall(QString::fromLatin1(BluezService), QStringLiteral("/"),
                  QString::fromLatin1(BluezObjectManagerInterface),
                  QStringLiteral("GetManagedObjects"), {}, &result, error,
                  QDBusConnection::systemBus()) || result.isEmpty())
        return false;
    const QVariant value = unbox(result.first());
    if (value.canConvert<ManagedBluezObjects>()) {
        *objects = value.value<ManagedBluezObjects>();
    } else if (value.canConvert<QDBusArgument>()) {
        *objects = qdbus_cast<ManagedBluezObjects>(value.value<QDBusArgument>());
    } else {
        *error = QStringLiteral("BlueZ returned object data in an unsupported format.");
        return false;
    }
    for (auto object = objects->begin(); object != objects->end(); ++object) {
        for (auto interface = object.value().begin(); interface != object.value().end(); ++interface) {
            for (auto property = interface.value().begin(); property != interface.value().end(); ++property) {
                property.value() = unbox(property.value());
                if (property.value().metaType() == QMetaType::fromType<QDBusObjectPath>())
                    property.value() = property.value().value<QDBusObjectPath>().path();
            }
        }
    }
    return true;
}

QString bluezAdapterPath(const ManagedBluezObjects &objects)
{
    QString first;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        if (!it.value().contains(QString::fromLatin1(BluezAdapterInterface)))
            continue;
        const QString path = it.key().path();
        if (first.isEmpty())
            first = path;
        if (it.value().value(QString::fromLatin1(BluezAdapterInterface))
                .value(QStringLiteral("Powered")).toBool())
            return path;
    }
    return first;
}

DesktopEntries readDesktops(QString *error)
{
    QDBusMessage request = QDBusMessage::createMethodCall(
        QString::fromLatin1(KWinService), QString::fromLatin1(DesktopPath),
        QString::fromLatin1(PropertiesInterface), QStringLiteral("Get"));
    request.setArguments({QString::fromLatin1(DesktopInterface), QStringLiteral("desktops")});
    const QDBusMessage reply = QDBusConnection::sessionBus().call(request);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        *error = reply.errorMessage().isEmpty()
            ? QStringLiteral("KWin returned no virtual desktop data.")
            : reply.errorMessage();
        return {};
    }
    QVariant value = reply.arguments().constFirst().value<QDBusVariant>().variant();
    if (value.canConvert<DesktopEntries>())
        return value.value<DesktopEntries>();
    if (value.canConvert<QDBusArgument>())
        return qdbus_cast<DesktopEntries>(value.value<QDBusArgument>());
    *error = QStringLiteral("KWin returned virtual desktop data in an unsupported format.");
    return {};
}

QString shortcutKeyText(const QList<QList<int>> &sequences)
{
    if (sequences.isEmpty() || sequences.first().isEmpty())
        return {};
    return QKeySequence(sequences.first().first()).toString(QKeySequence::PortableText);
}
}

namespace {
const QString settingsPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("obsidian/settings.json"));
}

bool writeAtomically(const QString &path, const QByteArray &contents, QString *error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        *error = QStringLiteral("Could not create %1").arg(info.absolutePath());
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = file.errorString();
        return false;
    }
    if (file.write(contents) != contents.size() || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

bool preserveFile(const QString &path, const QString &suffix, QString *error)
{
    if (!QFileInfo::exists(path))
        return true;
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    if (QFile::rename(path, path + suffix + stamp))
        return true;
    *error = QStringLiteral("Could not preserve existing configuration file %1").arg(path);
    return false;
}
}

NativeBridge::NativeBridge(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<DesktopEntry>();
    qDBusRegisterMetaType<DesktopEntries>();
    qDBusRegisterMetaType<ShortcutInfo>();
    qDBusRegisterMetaType<ShortcutInfos>();
    qDBusRegisterMetaType<NmConnectionSettings>();
    qDBusRegisterMetaType<ManagedBluezObjects>();
    m_settings = {
        {QStringLiteral("accent"), QStringLiteral("#8ab4f8")},
        {QStringLiteral("background"), QStringLiteral("#111318")},
        {QStringLiteral("foreground"), QStringLiteral("#e6e8ef")},
        {QStringLiteral("font_size"), 11},
        {QStringLiteral("kitty_enabled"), false},
        {QStringLiteral("fish_enabled"), false},
        {QStringLiteral("kitty_managed"), false},
        {QStringLiteral("fish_managed"), false},
        {QStringLiteral("obsidian_shortcuts"), QVariantMap{}},
        {QStringLiteral("wallpaper_directory"), QString()},
        {QStringLiteral("wallpaper"), QString()}
    };

    QFile file(settingsPath());
    if (file.exists() && !file.open(QIODevice::ReadOnly)) {
        m_settingsLoadError = QStringLiteral("Could not read settings at %1: %2")
                                  .arg(settingsPath(), file.errorString());
    } else if (file.isOpen()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            m_settingsLoadError = QStringLiteral("Could not read settings at %1: %2")
                                      .arg(settingsPath(), parseError.errorString());
        } else if (!document.isObject()) {
            m_settingsLoadError = QStringLiteral("Settings file must contain a JSON object: %1")
                                      .arg(settingsPath());
        } else {
            const auto stored = document.object().toVariantMap();
            for (auto it = stored.cbegin(); it != stored.cend(); ++it)
                m_settings.insert(it.key(), it.value());
        }
    }

    m_wifiData = {{QStringLiteral("enabled"), false},
                  {QStringLiteral("hardwareEnabled"), false},
                  {QStringLiteral("canManage"), false},
                  {QStringLiteral("hasAdapter"), false},
                  {QStringLiteral("connectedSsid"), QString()},
                  {QStringLiteral("available"), QVariantList{}},
                  {QStringLiteral("saved"), QVariantList{}}};
    m_bluetoothData = {{QStringLiteral("hasAdapter"), false},
                       {QStringLiteral("adapterPath"), QString()},
                       {QStringLiteral("adapterName"), QString()},
                       {QStringLiteral("powered"), false},
                       {QStringLiteral("discovering"), false},
                       {QStringLiteral("nearby"), QVariantList{}},
                       {QStringLiteral("paired"), QVariantList{}}};
    m_inputSettingsData = {{QStringLiteral("keyboardLayouts"), QVariantList{}},
                           {QStringLiteral("availableKeyboardLayouts"), QVariantList{}},
                           {QStringLiteral("mouseDevices"), QVariantList{}},
                           {QStringLiteral("touchpadDevices"), QVariantList{}}};
}

void NativeBridge::setValue(const QString &key, const QVariant &value,
                            void (NativeBridge::*changed)())
{
    if (m_settings.value(key) == value)
        return;
    m_settings.insert(key, value);
    emit (this->*changed)();
    emit settingsDataChanged();
}

QString NativeBridge::accent() const { return m_settings.value(QStringLiteral("accent")).toString(); }
void NativeBridge::setAccent(const QString &v) { setValue(QStringLiteral("accent"), v, &NativeBridge::accentChanged); }
QString NativeBridge::background() const { return m_settings.value(QStringLiteral("background")).toString(); }
void NativeBridge::setBackground(const QString &v) { setValue(QStringLiteral("background"), v, &NativeBridge::backgroundChanged); }
QString NativeBridge::foreground() const { return m_settings.value(QStringLiteral("foreground")).toString(); }
void NativeBridge::setForeground(const QString &v) { setValue(QStringLiteral("foreground"), v, &NativeBridge::foregroundChanged); }
int NativeBridge::fontSize() const { return m_settings.value(QStringLiteral("font_size")).toInt(); }
void NativeBridge::setFontSize(int v) { setValue(QStringLiteral("font_size"), v, &NativeBridge::fontSizeChanged); }
bool NativeBridge::kittyEnabled() const { return m_settings.value(QStringLiteral("kitty_enabled")).toBool(); }
void NativeBridge::setKittyEnabled(bool v) { setValue(QStringLiteral("kitty_enabled"), v, &NativeBridge::kittyEnabledChanged); }
bool NativeBridge::fishEnabled() const { return m_settings.value(QStringLiteral("fish_enabled")).toBool(); }
void NativeBridge::setFishEnabled(bool v) { setValue(QStringLiteral("fish_enabled"), v, &NativeBridge::fishEnabledChanged); }
QString NativeBridge::wallpaper() const { return m_settings.value(QStringLiteral("wallpaper")).toString(); }
void NativeBridge::setWallpaper(const QString &v) { setValue(QStringLiteral("wallpaper"), v, &NativeBridge::wallpaperChanged); }
QString NativeBridge::wallpaperDirectory() const { return m_settings.value(QStringLiteral("wallpaper_directory")).toString(); }
QVariantList NativeBridge::wallpaperChoices() const { return m_wallpaperChoices; }
QVariantMap NativeBridge::settingsData() const { return m_settings; }
QVariantList NativeBridge::shortcutActions() const { return m_shortcutActions; }
QVariantList NativeBridge::shortcutBindings() const { return m_shortcutBindings; }
int NativeBridge::desktopCount() const { return m_desktopCount; }
QVariantMap NativeBridge::wifiData() const { return m_wifiData; }
QVariantMap NativeBridge::bluetoothData() const { return m_bluetoothData; }
QVariantMap NativeBridge::inputSettingsData() const { return m_inputSettingsData; }

void NativeBridge::setWallpaperFile(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    setWallpaper(url.isLocalFile() ? url.toLocalFile() : fileUrl);
}

void NativeBridge::loadWallpaperDirectory(const QString &directory)
{
    if (QThread::currentThread() != thread()) {
        postError(QStringLiteral("Choose the wallpaper folder from the Wallpaper Picker window."));
        return;
    }

    const quint64 generation = ++m_wallpaperLoadGeneration;
    const QString requestedDirectory = directory.trimmed();
    runInBackground(this, [requestedDirectory, generation](NativeBridge *bridge) {
        const QFileInfo directoryInfo(requestedDirectory);
        if (requestedDirectory.isEmpty() || !directoryInfo.isDir() || !directoryInfo.isReadable()) {
            QMetaObject::invokeMethod(bridge, [bridge, generation] {
                if (generation == bridge->m_wallpaperLoadGeneration)
                    emit bridge->errorOccurred(QStringLiteral("Choose a readable folder containing wallpaper images."));
            }, Qt::QueuedConnection);
            return;
        }

        const QString absoluteDirectory = directoryInfo.absoluteFilePath();
        const QStringList imageExtensions{
            QStringLiteral("bmp"), QStringLiteral("jpeg"), QStringLiteral("jpg"),
            QStringLiteral("png"), QStringLiteral("webp")
        };
        QVariantList choices;
        QDirIterator iterator(absoluteDirectory, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QFileInfo imageInfo(iterator.next());
            if (!imageExtensions.contains(imageInfo.suffix().toLower()))
                continue;
            choices.append(QVariantMap{
                {QStringLiteral("path"), imageInfo.absoluteFilePath()},
                {QStringLiteral("url"), QUrl::fromLocalFile(imageInfo.absoluteFilePath()).toString()},
                {QStringLiteral("name"), imageInfo.fileName()}
            });
        }
        std::sort(choices.begin(), choices.end(), [](const QVariant &left, const QVariant &right) {
            return left.toMap().value(QStringLiteral("name")).toString().compare(
                right.toMap().value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
        });

        QMetaObject::invokeMethod(bridge, [bridge, generation, absoluteDirectory, choices] {
            if (generation != bridge->m_wallpaperLoadGeneration)
                return;

            QVariantMap updatedSettings = bridge->m_settings;
            updatedSettings.insert(QStringLiteral("wallpaper_directory"), absoluteDirectory);
            QString error = bridge->m_settingsLoadError;
            if (error.isEmpty())
                writeAtomically(settingsPath(),
                    QJsonDocument(QJsonObject::fromVariantMap(updatedSettings))
                        .toJson(QJsonDocument::Indented), &error);
            bridge->m_settings = updatedSettings;
            bridge->m_wallpaperChoices = choices;
            emit bridge->wallpaperDirectoryChanged();
            emit bridge->settingsDataChanged();
            emit bridge->wallpaperChoicesChanged();
            if (!error.isEmpty())
                emit bridge->errorOccurred(error);
        }, Qt::QueuedConnection);
    });
}

void NativeBridge::applyWallpaper(const QString &path)
{
    if (QThread::currentThread() != thread()) {
        postError(QStringLiteral("Choose a wallpaper from the Wallpaper Picker window."));
        return;
    }
    if (!m_settingsLoadError.isEmpty()) {
        emit errorOccurred(m_settingsLoadError);
        return;
    }

    const QFileInfo imageInfo(path);
    if (!imageInfo.isFile()) {
        emit errorOccurred(QStringLiteral("Wallpaper image does not exist: %1").arg(path));
        return;
    }
    const QString absolutePath = imageInfo.absoluteFilePath();
    runInBackground(this, [absolutePath](NativeBridge *bridge) {
        const int result = QProcess::execute(QStringLiteral("plasma-apply-wallpaperimage"),
                                             {absolutePath});
        if (result != 0) {
            bridge->postError(QStringLiteral("Could not apply the wallpaper. Check that plasma-apply-wallpaperimage is installed."));
            return;
        }
        QMetaObject::invokeMethod(bridge, [bridge, absolutePath] {
            QVariantMap updatedSettings = bridge->m_settings;
            updatedSettings.insert(QStringLiteral("wallpaper"), absolutePath);
            QString error;
            if (!writeAtomically(settingsPath(),
                    QJsonDocument(QJsonObject::fromVariantMap(updatedSettings))
                        .toJson(QJsonDocument::Indented), &error)) {
                emit bridge->errorOccurred(error);
                return;
            }
            bridge->m_settings = updatedSettings;
            emit bridge->wallpaperChanged();
            emit bridge->settingsDataChanged();
            emit bridge->applied(QStringLiteral("Wallpaper applied."));
        }, Qt::QueuedConnection);
    });
}

void NativeBridge::applySettings(const QString &settingsJson,
                                 const QString &kittyConfig,
                                 const QString &fishConfig)
{
    if (!m_settingsLoadError.isEmpty()) {
        emit errorOccurred(m_settingsLoadError);
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(settingsJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit errorOccurred(QStringLiteral("Settings data is invalid."));
        return;
    }

    m_settings = document.object().toVariantMap();
    emit settingsDataChanged();
    const QRegularExpression colorPattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    for (const QString &key : {QStringLiteral("accent"), QStringLiteral("background"),
                               QStringLiteral("foreground")}) {
        if (!colorPattern.match(m_settings.value(key).toString()).hasMatch()) {
            emit errorOccurred(QStringLiteral("%1 must be a six-digit hex color (for example #8ab4f8).")
                                   .arg(key.left(1).toUpper() + key.mid(1)));
            return;
        }
    }

    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString kittyPath = QDir(config).filePath(QStringLiteral("kitty/kitty.conf"));
    const QString fishPath = QDir(config).filePath(QStringLiteral("fish/conf.d/obsidian.fish"));
    QString error;

    auto applyManaged = [&](const QString &path, const QString &content,
                            const QString &enabledKey, const QString &managedKey) {
        const bool enabled = m_settings.value(enabledKey).toBool();
        const bool managed = m_settings.value(managedKey).toBool();
        if (enabled) {
            if (!managed && !preserveFile(path, QStringLiteral(".obsidian-backup-"), &error))
                return false;
            if (!writeAtomically(path, content.toUtf8(), &error))
                return false;
            m_settings.insert(managedKey, true);
        } else if (managed) {
            if (!preserveFile(path, QStringLiteral(".disabled-"), &error))
                return false;
            m_settings.insert(managedKey, false);
        }
        return true;
    };

    if (!applyManaged(kittyPath, kittyConfig, QStringLiteral("kitty_enabled"),
                      QStringLiteral("kitty_managed"))
        || !applyManaged(fishPath, fishConfig, QStringLiteral("fish_enabled"),
                         QStringLiteral("fish_managed"))) {
        emit errorOccurred(error);
        return;
    }

    const QJsonDocument settingsDocument(QJsonObject::fromVariantMap(m_settings));
    if (!writeAtomically(settingsPath(), settingsDocument.toJson(QJsonDocument::Indented), &error)) {
        emit errorOccurred(error);
        return;
    }

    const QString wallpaperPath = m_settings.value(QStringLiteral("wallpaper")).toString().trimmed();
    if (!wallpaperPath.isEmpty()) {
        if (!QFileInfo(wallpaperPath).isFile()) {
            emit errorOccurred(QStringLiteral("Wallpaper image does not exist: %1").arg(wallpaperPath));
            return;
        }
        const int result = QProcess::execute(QStringLiteral("plasma-apply-wallpaperimage"),
                                             {wallpaperPath});
        if (result != 0) {
            emit errorOccurred(QStringLiteral("Could not apply the wallpaper. Check that plasma-apply-wallpaperimage is installed."));
            return;
        }
    }
    emit applied(QStringLiteral("Obsidian settings saved."));
}

void NativeBridge::reportDbusError(const QString &feature, const QString &detail)
{
    postError(QStringLiteral("%1 failed: %2").arg(feature, detail));
}

void NativeBridge::postError(const QString &message)
{
    if (QThread::currentThread() == thread()) {
        emit errorOccurred(message);
        return;
    }
    QMetaObject::invokeMethod(this, [this, message] { emit errorOccurred(message); },
                              Qt::QueuedConnection);
}

void NativeBridge::postShortcutMessage(const QString &message)
{
    if (QThread::currentThread() == thread()) {
        emit shortcutMessage(message);
        return;
    }
    QMetaObject::invokeMethod(this, [this, message] { emit shortcutMessage(message); },
                              Qt::QueuedConnection);
}

void NativeBridge::refreshShortcuts()
{
    if (QThread::currentThread() == thread()) {
        const QVariantMap settings = m_settings;
        runInBackground(this, [settings](NativeBridge *bridge) {
            bridge->refreshShortcutsInternal(settings);
        });
        return;
    }
    refreshShortcutsInternal(m_settings);
}

void NativeBridge::refreshShortcutsInternal(const QVariantMap &settings)
{
    QVariantList result;
    QString error;
    if (!dbusCall(QString::fromLatin1(KGlobalAccelService), QString::fromLatin1(KGlobalAccelPath),
                  QString::fromLatin1(KGlobalAccelInterface), QStringLiteral("allComponents"),
                  {}, &result, &error)) {
        reportDbusError(QStringLiteral("KDE global shortcuts"), error);
        return;
    }

    QVariantList actions;
    QVariantList bindings;
    const auto paths = result.isEmpty() ? QList<QDBusObjectPath>{} : objectPaths(result.first());
    for (const QDBusObjectPath &path : paths) {
        QVariantList infosReply;
        if (!dbusCall(QString::fromLatin1(KGlobalAccelService), path.path(),
                      QString::fromLatin1(KGlobalAccelComponentInterface),
                      QStringLiteral("allShortcutInfos"), {}, &infosReply, &error)) {
            reportDbusError(QStringLiteral("KDE global shortcuts"), error);
            return;
        }
        if (infosReply.isEmpty())
            continue;
        const QVariant value = unbox(infosReply.first());
        ShortcutInfos infos;
        if (value.canConvert<ShortcutInfos>())
            infos = value.value<ShortcutInfos>();
        else if (value.canConvert<QDBusArgument>())
            infos = qdbus_cast<ShortcutInfos>(value.value<QDBusArgument>());
        else {
            reportDbusError(QStringLiteral("KDE global shortcuts"),
                            QStringLiteral("KGlobalAccel returned shortcut data in an unsupported format."));
            return;
        }
        for (const ShortcutInfo &info : infos) {
            const QString category = shortcutCategory(info.componentId, info.componentName, info.actionName);
            if (category.isEmpty())
                continue;
            const QJsonArray identity{info.componentId, info.actionId, info.componentName, info.actionName};
            const QString actionId = QString::fromUtf8(QJsonDocument(identity).toJson(QJsonDocument::Compact));
            QVariantMap row{{QStringLiteral("actionId"), actionId},
                            {QStringLiteral("label"), category + QStringLiteral(" · ")
                                 + info.componentName + QStringLiteral(" — ") + info.actionName},
                            {QStringLiteral("actionName"), info.actionName},
                            {QStringLiteral("componentName"), info.componentName},
                            {QStringLiteral("category"), category}};
            actions.append(row);
            QStringList sequenceTexts;
            for (const QList<int> &sequence : info.shortcuts) {
                if (!sequence.isEmpty()) {
                    const QString text = QKeySequence(sequence.first()).toString(QKeySequence::PortableText);
                    if (!text.isEmpty())
                        sequenceTexts.append(text);
                }
            }
            if (!sequenceTexts.isEmpty()) {
                row.insert(QStringLiteral("shortcut"), sequenceTexts.join(QStringLiteral(" / ")));
                bindings.append(row);
            }
        }
    }

    const QStringList ids = obsidianActionIds();
    for (const QString &id : ids) {
        const QString name = obsidianActionName(id);
        const QJsonArray identity{QStringLiteral("obsidian"), id, QStringLiteral("Obsidian"), name};
        const QString actionId = QString::fromUtf8(QJsonDocument(identity).toJson(QJsonDocument::Compact));
        QVariantMap row{{QStringLiteral("actionId"), actionId},
                        {QStringLiteral("label"), QStringLiteral("Obsidian · ") + name},
                        {QStringLiteral("actionName"), name},
                        {QStringLiteral("componentName"), QStringLiteral("Obsidian")},
                        {QStringLiteral("category"), QStringLiteral("Obsidian")}};
        actions.append(row);
        const QString shortcut = settings.value(QStringLiteral("obsidian_shortcuts")).toMap()
                                     .value(id).toString().trimmed();
        if (!shortcut.isEmpty()) {
            row.insert(QStringLiteral("shortcut"), shortcut);
            bindings.append(row);
        }
    }
    auto order = [](const QVariant &left, const QVariant &right) {
        const QVariantMap a = left.toMap();
        const QVariantMap b = right.toMap();
        const QStringList categories{QStringLiteral("Window management"), QStringLiteral("Virtual desktops"),
                                     QStringLiteral("Common KDE"), QStringLiteral("Obsidian")};
        const int ai = categories.indexOf(a.value(QStringLiteral("category")).toString());
        const int bi = categories.indexOf(b.value(QStringLiteral("category")).toString());
        if (ai != bi)
            return ai < bi;
        return a.value(QStringLiteral("label")).toString().compare(
            b.value(QStringLiteral("label")).toString(), Qt::CaseInsensitive) < 0;
    };
    std::sort(actions.begin(), actions.end(), order);
    std::sort(bindings.begin(), bindings.end(), order);
    const int actionCount = actions.size();
    QMetaObject::invokeMethod(this, [this, actions, bindings, actionCount] {
        m_shortcutActions = actions;
        m_shortcutBindings = bindings;
        emit shortcutActionsChanged(m_shortcutActions);
        emit shortcutBindingsChanged(m_shortcutBindings);
        emit shortcutMessage(QStringLiteral("Loaded %1 available actions.").arg(actionCount));
    }, Qt::QueuedConnection);
}

void NativeBridge::assignShortcut(const QString &actionIdJson, const QString &shortcut)
{
    if (QThread::currentThread() == thread()) {
        const QVariantMap settings = m_settings;
        runInBackground(this, [actionIdJson, shortcut, settings](NativeBridge *bridge) {
            bridge->assignShortcutInternal(actionIdJson, shortcut, settings);
        });
        return;
    }
    assignShortcutInternal(actionIdJson, shortcut, m_settings);
}

void NativeBridge::assignShortcutInternal(const QString &actionIdJson, const QString &shortcut,
                                          const QVariantMap &settings)
{
    const QJsonDocument document = QJsonDocument::fromJson(actionIdJson.toUtf8());
    if (!document.isArray() || document.array().size() != 4) {
        postError(QStringLiteral("The selected KDE shortcut action is malformed."));
        return;
    }
    const QJsonArray identityArray = document.array();
    QStringList identity;
    for (const QJsonValue &part : identityArray)
        identity.append(part.toString());

    QKeySequence sequence = QKeySequence::fromString(shortcut, QKeySequence::PortableText);
    if (sequence.isEmpty() || sequence.count() != 1 || sequence[0] == 0) {
        postError(QStringLiteral("Record one key combination, such as Meta+E or Ctrl+Alt+T."));
        return;
    }
    QList<QList<int>> requested;
    requested.append({sequence[0]});
    QVariantList result;
    QString error;
    if (!dbusCall(QString::fromLatin1(KGlobalAccelService), QString::fromLatin1(KGlobalAccelPath),
                  QString::fromLatin1(KGlobalAccelInterface), QStringLiteral("setShortcutKeys"),
                  {identity, QVariant::fromValue(requested), uint(6)}, &result, &error)) {
        reportDbusError(QStringLiteral("Assigning shortcut"), error);
        return;
    }
    QString actual;
    if (!result.isEmpty()) {
        const QVariant value = unbox(result.first());
        QList<QList<int>> assigned;
        if (value.canConvert<QList<QList<int>>>())
            assigned = value.value<QList<QList<int>>>();
        else if (value.canConvert<QDBusArgument>())
            assigned = qdbus_cast<QList<QList<int>>>(value.value<QDBusArgument>());
        if (!assigned.isEmpty() && !assigned.first().isEmpty())
            actual = QKeySequence(assigned.first().first()).toString(QKeySequence::PortableText);
    }
    if (!shortcut.isEmpty() && actual.isEmpty()) {
        postError(QStringLiteral("KDE did not accept that shortcut. It may already be in use."));
        refreshShortcutsInternal(settings);
        return;
    }
    if (identity.at(0) == QLatin1String("obsidian")) {
        QVariantMap updatedSettings = settings;
        QVariantMap saved = updatedSettings.value(QStringLiteral("obsidian_shortcuts")).toMap();
        saved.insert(identity.at(1), actual);
        updatedSettings.insert(QStringLiteral("obsidian_shortcuts"), saved);
        QString writeError;
        if (!writeAtomically(settingsPath(),
                             QJsonDocument(QJsonObject::fromVariantMap(updatedSettings))
                                 .toJson(QJsonDocument::Indented), &writeError)) {
            reportDbusError(QStringLiteral("Saving custom shortcut"), writeError);
            return;
        }
        const QString message = actual == shortcut ? QStringLiteral("Assigned %1.").arg(actual)
            : QStringLiteral("KDE assigned %1 instead of %2.")
                  .arg(actual.isEmpty() ? QStringLiteral("no shortcut") : actual, shortcut);
        QMetaObject::invokeMethod(this, [this, updatedSettings, message] {
            m_settings = updatedSettings;
            emit settingsDataChanged();
            emit shortcutMessage(message);
            refreshShortcuts();
        }, Qt::QueuedConnection);
        return;
    }
    postShortcutMessage(actual == shortcut ? QStringLiteral("Assigned %1.").arg(actual)
                                           : QStringLiteral("KDE assigned %1 instead of %2.")
                                                 .arg(actual.isEmpty() ? QStringLiteral("no shortcut") : actual,
                                                      shortcut));
    refreshShortcutsInternal(settings);
}

void NativeBridge::clearShortcut(const QString &actionIdJson)
{
    if (QThread::currentThread() == thread()) {
        const QVariantMap settings = m_settings;
        runInBackground(this, [actionIdJson, settings](NativeBridge *bridge) {
            bridge->clearShortcutInternal(actionIdJson, settings);
        });
        return;
    }
    clearShortcutInternal(actionIdJson, m_settings);
}

void NativeBridge::clearShortcutInternal(const QString &actionIdJson, const QVariantMap &settings)
{
    const QJsonDocument document = QJsonDocument::fromJson(actionIdJson.toUtf8());
    if (!document.isArray() || document.array().size() != 4) {
        postError(QStringLiteral("The selected KDE shortcut action is malformed."));
        return;
    }
    QStringList identity;
    for (const QJsonValue &part : document.array())
        identity.append(part.toString());
    QVariantList result;
    QString error;
    if (!dbusCall(QString::fromLatin1(KGlobalAccelService), QString::fromLatin1(KGlobalAccelPath),
                  QString::fromLatin1(KGlobalAccelInterface), QStringLiteral("setShortcutKeys"),
                  {identity, QVariant::fromValue(QList<QList<int>>{}), uint(6)},
                  &result, &error)) {
        reportDbusError(QStringLiteral("Clearing shortcut"), error);
        return;
    }
    if (identity.value(0) == QLatin1String("obsidian")) {
        QVariantMap updatedSettings = settings;
        QVariantMap saved = updatedSettings.value(QStringLiteral("obsidian_shortcuts")).toMap();
        saved.remove(identity.value(1));
        updatedSettings.insert(QStringLiteral("obsidian_shortcuts"), saved);
        QString writeError;
        if (!writeAtomically(settingsPath(),
                             QJsonDocument(QJsonObject::fromVariantMap(updatedSettings))
                                 .toJson(QJsonDocument::Indented), &writeError)) {
            reportDbusError(QStringLiteral("Saving custom shortcut"), writeError);
            return;
        }
        QMetaObject::invokeMethod(this, [this, updatedSettings] {
            m_settings = updatedSettings;
            emit settingsDataChanged();
            emit shortcutMessage(QStringLiteral("Shortcut cleared."));
            refreshShortcuts();
        }, Qt::QueuedConnection);
        return;
    }
    postShortcutMessage(QStringLiteral("Shortcut cleared."));
    refreshShortcutsInternal(settings);
}
void NativeBridge::refreshDesktopCount()
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [](NativeBridge *bridge) { bridge->refreshDesktopCount(); });
        return;
    }
    QString error;
    const DesktopEntries desktops = readDesktops(&error);
    if (desktops.isEmpty()) {
        postError(error.isEmpty() ? QStringLiteral("KWin reports no virtual desktops.") : error);
        return;
    }
    const int count = desktops.size();
    QMetaObject::invokeMethod(this, [this, count] {
        m_desktopCount = count;
        emit desktopCountChanged(m_desktopCount);
        emit shortcutMessage(QStringLiteral("KWin has %1 virtual desktop(s).").arg(count));
    }, Qt::QueuedConnection);
}

void NativeBridge::setDesktopCount(int targetCount)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [targetCount](NativeBridge *bridge) {
            bridge->setDesktopCount(targetCount);
        });
        return;
    }
    if (targetCount < 1 || targetCount > 10) {
        postError(QStringLiteral("Choose between 1 and 10 desktops."));
        return;
    }

    QString error;
    DesktopEntries desktops = readDesktops(&error);
    if (desktops.isEmpty()) {
        postError(error.isEmpty() ? QStringLiteral("KWin reports no virtual desktops.") : error);
        return;
    }

    QDBusInterface kwin(QString::fromLatin1(KWinService), QString::fromLatin1(DesktopPath),
                        QString::fromLatin1(DesktopInterface), QDBusConnection::sessionBus());
    while (desktops.size() < targetCount) {
        const qsizetype previousCount = desktops.size();
        const QDBusMessage reply = kwin.call(QStringLiteral("createDesktop"),
                                             uint(desktops.size()), QString());
        if (reply.type() == QDBusMessage::ErrorMessage) {
            postError(QStringLiteral("KWin could not create a virtual desktop: %1")
                          .arg(reply.errorMessage()));
            refreshDesktopCount();
            return;
        }
        desktops = readDesktops(&error);
        if (desktops.isEmpty()) {
            postError(error);
            return;
        }
        if (desktops.size() <= previousCount) {
            postError(QStringLiteral("KWin accepted the request but did not add a virtual desktop."));
            return;
        }
    }
    while (desktops.size() > targetCount) {
        const qsizetype previousCount = desktops.size();
        const auto lastDesktop = *std::max_element(
            desktops.cbegin(), desktops.cend(),
            [](const DesktopEntry &left, const DesktopEntry &right) {
                return left.position < right.position;
            });
        const QDBusMessage reply = kwin.call(QStringLiteral("removeDesktop"), lastDesktop.id);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            postError(QStringLiteral("KWin could not remove a virtual desktop: %1")
                          .arg(reply.errorMessage()));
            refreshDesktopCount();
            return;
        }
        desktops = readDesktops(&error);
        if (desktops.isEmpty()) {
            postError(error);
            return;
        }
        if (desktops.size() >= previousCount) {
            postError(QStringLiteral("KWin accepted the request but did not remove a virtual desktop."));
            return;
        }
    }
    const int count = desktops.size();
    QMetaObject::invokeMethod(this, [this, count] {
        m_desktopCount = count;
        emit desktopCountChanged(m_desktopCount);
        emit shortcutMessage(QStringLiteral("Virtual desktop count set to %1.").arg(count));
    }, Qt::QueuedConnection);
}
void NativeBridge::refreshWifi()
{
    runInBackground(this, [](NativeBridge *bridge) {
        bridge->refreshWifiInternal(false);
    });
}

void NativeBridge::refreshWifiInternal(bool scanRequested)
{
    QString error;
    QVariantMap wifiData;
    QVariant enabledValue, hardwareValue, canManageValue;
    QVariantMap device;
    QString devicePath;
    if (!getProperty(QString::fromLatin1(NetworkManagerService), QString::fromLatin1(NetworkManagerPath),
                     QString::fromLatin1(NmInterface), QStringLiteral("WirelessEnabled"),
                     &enabledValue, &error, QDBusConnection::systemBus())
        || !getProperty(QString::fromLatin1(NetworkManagerService), QString::fromLatin1(NetworkManagerPath),
                        QString::fromLatin1(NmInterface), QStringLiteral("WirelessHardwareEnabled"),
                        &hardwareValue, &error, QDBusConnection::systemBus())
        || !getProperty(QString::fromLatin1(NetworkManagerService), QString::fromLatin1(NetworkManagerSettingsPath),
                        QString::fromLatin1(NmSettingsInterface), QStringLiteral("CanModify"),
                        &canManageValue, &error, QDBusConnection::systemBus())) {
        reportDbusError(QStringLiteral("NetworkManager Wi-Fi"), error);
        return;
    }
    const bool hasDevice = wifiDevice(&devicePath, &device, &error);
    if (!hasDevice && !error.isEmpty()) {
        reportDbusError(QStringLiteral("NetworkManager Wi-Fi"), error);
        return;
    }
    QVariantList profiles;
    if (!readNetworkProfiles(&profiles, &error)) {
        reportDbusError(QStringLiteral("NetworkManager Wi-Fi"), error);
        return;
    }
    QString activeProfilePath;
    QString activeApPath;
    QString connectedSsid;
    if (hasDevice) {
        if (scanRequested && enabledValue.toBool() && hardwareValue.toBool()) {
            QVariant lastScan;
            if (!getProperty(QString::fromLatin1(NetworkManagerService), devicePath,
                             QString::fromLatin1(NmWirelessInterface), QStringLiteral("LastScan"),
                             &lastScan, &error, QDBusConnection::systemBus())) {
                reportDbusError(QStringLiteral("Scanning Wi-Fi"), error);
                return;
            }
            QVariantList scanReply;
            if (!dbusCall(QString::fromLatin1(NetworkManagerService), devicePath,
                          QString::fromLatin1(NmWirelessInterface), QStringLiteral("RequestScan"),
                          {QVariantMap{}}, &scanReply, &error, QDBusConnection::systemBus())) {
                reportDbusError(QStringLiteral("Scanning Wi-Fi"), error);
                return;
            }
            bool scanFinished = false;
            for (int attempt = 0; attempt < 40; ++attempt) {
                QThread::msleep(500);
                QVariant currentScan;
                if (!getProperty(QString::fromLatin1(NetworkManagerService), devicePath,
                                 QString::fromLatin1(NmWirelessInterface), QStringLiteral("LastScan"),
                                 &currentScan, &error, QDBusConnection::systemBus())) {
                    reportDbusError(QStringLiteral("Scanning Wi-Fi"), error);
                    return;
                }
                if (currentScan.toLongLong() > lastScan.toLongLong()) {
                    scanFinished = true;
                    break;
                }
            }
            if (!scanFinished) {
                emit errorOccurred(QStringLiteral("Wi-Fi scanning did not finish within 20 seconds."));
                return;
            }
        }
        const QString activePath = dbusString(device.value(QStringLiteral("ActiveConnection")));
        if (activePath != QLatin1String("/") && !activePath.isEmpty()) {
            QVariantMap active;
            if (!getAll(QString::fromLatin1(NetworkManagerService), activePath,
                        QString::fromLatin1(NmActiveConnectionInterface), &active, &error,
                        QDBusConnection::systemBus())) {
                reportDbusError(QStringLiteral("NetworkManager Wi-Fi"), error);
                return;
            }
            activeProfilePath = dbusString(active.value(QStringLiteral("Connection")));
        }
        QVariantMap wireless;
        if (!getAll(QString::fromLatin1(NetworkManagerService), devicePath,
                    QString::fromLatin1(NmWirelessInterface), &wireless, &error,
                    QDBusConnection::systemBus())) {
            reportDbusError(QStringLiteral("NetworkManager Wi-Fi"), error);
            return;
        }
        activeApPath = dbusString(wireless.value(QStringLiteral("ActiveAccessPoint")));

        QVariantList apReply;
        if (enabledValue.toBool() && hardwareValue.toBool()
            && !dbusCall(QString::fromLatin1(NetworkManagerService), devicePath,
                         QString::fromLatin1(NmWirelessInterface),
                         QStringLiteral("GetAllAccessPoints"), {}, &apReply, &error,
                         QDBusConnection::systemBus())) {
            reportDbusError(QStringLiteral("NetworkManager Wi-Fi"), error);
            return;
        }
        QVariantMap grouped;
        if (!apReply.isEmpty()) {
            for (const QDBusObjectPath &apPath : objectPaths(apReply.first())) {
                QVariantMap ap;
                if (!getAll(QString::fromLatin1(NetworkManagerService), apPath.path(),
                            QString::fromLatin1(NmAccessPointInterface), &ap, &error,
                            QDBusConnection::systemBus())) {
                    reportDbusError(QStringLiteral("NetworkManager Wi-Fi"), error);
                    return;
                }
                const QByteArray ssidBytes = ap.value(QStringLiteral("Ssid")).toByteArray();
                if (ssidBytes.isEmpty())
                    continue;
                const QString ssidKey = QString::fromLatin1(ssidBytes.toHex());
                const QVariantMap security = networkSecurity(ap);
                bool saved = false;
                for (const QVariant &profile : profiles) {
                    if (profile.toMap().value(QStringLiteral("ssidBytes")).toByteArray() == ssidBytes) {
                        saved = true;
                        break;
                    }
                }
                const bool connected = apPath.path() == activeApPath;
                QVariantMap network{{QStringLiteral("accessPointPath"), apPath.path()},
                                    {QStringLiteral("ssid"), bytesToSsid(ssidBytes)},
                                    {QStringLiteral("strength"), ap.value(QStringLiteral("Strength")).toUInt()},
                                    {QStringLiteral("security"), security.value(QStringLiteral("security"))},
                                    {QStringLiteral("saved"), saved},
                                    {QStringLiteral("connected"), connected},
                                    {QStringLiteral("requiresPassword"), security.value(QStringLiteral("requiresPassword"))},
                                    {QStringLiteral("connectable"), saved || security.value(QStringLiteral("connectable")).toBool()}};
                if (!grouped.contains(ssidKey)
                    || network.value(QStringLiteral("strength")).toUInt()
                        > grouped.value(ssidKey).toMap().value(QStringLiteral("strength")).toUInt()) {
                    if (grouped.contains(ssidKey))
                        network.insert(QStringLiteral("connected"), connected
                            || grouped.value(ssidKey).toMap().value(QStringLiteral("connected")).toBool());
                    grouped.insert(ssidKey, network);
                } else if (connected) {
                    QVariantMap previous = grouped.value(ssidKey).toMap();
                    previous.insert(QStringLiteral("connected"), true);
                    grouped.insert(ssidKey, previous);
                }
                if (connected)
                    connectedSsid = bytesToSsid(ssidBytes);
            }
        }
        QVariantList networks = grouped.values();
        std::sort(networks.begin(), networks.end(), [](const QVariant &left, const QVariant &right) {
            const QVariantMap a = left.toMap(), b = right.toMap();
            if (a.value(QStringLiteral("strength")).toUInt() != b.value(QStringLiteral("strength")).toUInt())
                return a.value(QStringLiteral("strength")).toUInt() > b.value(QStringLiteral("strength")).toUInt();
            return a.value(QStringLiteral("ssid")).toString().compare(b.value(QStringLiteral("ssid")).toString(),
                                                                        Qt::CaseInsensitive) < 0;
        });
        wifiData.insert(QStringLiteral("available"), networks);
    } else {
        wifiData.insert(QStringLiteral("available"), QVariantList{});
    }
    for (QVariant &profile : profiles) {
        QVariantMap row = profile.toMap();
        row.remove(QStringLiteral("ssidBytes"));
        row.insert(QStringLiteral("connected"), row.value(QStringLiteral("path")).toString() == activeProfilePath);
        profile = row;
    }
    wifiData.insert(QStringLiteral("enabled"), enabledValue.toBool());
    wifiData.insert(QStringLiteral("hardwareEnabled"), hardwareValue.toBool());
    wifiData.insert(QStringLiteral("canManage"), canManageValue.toBool());
    wifiData.insert(QStringLiteral("hasAdapter"), hasDevice);
    wifiData.insert(QStringLiteral("connectedSsid"), connectedSsid);
    wifiData.insert(QStringLiteral("saved"), profiles);
    QMetaObject::invokeMethod(this, [this, wifiData] {
        m_wifiData = wifiData;
        emit wifiDataChanged(m_wifiData);
        emit shortcutMessage(QStringLiteral("Found %1 available Wi-Fi network(s).")
                                 .arg(m_wifiData.value(QStringLiteral("available")).toList().size()));
    }, Qt::QueuedConnection);
}

void NativeBridge::scanWifi()
{
    runInBackground(this, [](NativeBridge *bridge) {
        bridge->refreshWifiInternal(true);
    });
}

void NativeBridge::setWifiEnabled(bool enabled)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [enabled](NativeBridge *bridge) { bridge->setWifiEnabled(enabled); });
        return;
    }
    QString error;
    QVariantList result;
    if (!dbusCall(QString::fromLatin1(NetworkManagerService), QString::fromLatin1(NetworkManagerPath),
                  QString::fromLatin1(PropertiesInterface), QStringLiteral("Set"),
                  {QString::fromLatin1(NmInterface), QStringLiteral("WirelessEnabled"),
                   QVariant::fromValue(QDBusVariant(QVariant(enabled)))}, &result, &error,
                  QDBusConnection::systemBus())) {
        reportDbusError(QStringLiteral("Changing Wi-Fi power"), error);
        return;
    }
    bool updated = false;
    for (int attempt = 0; attempt < 40; ++attempt) {
        QVariant current;
        if (!getProperty(QString::fromLatin1(NetworkManagerService), QString::fromLatin1(NetworkManagerPath),
                         QString::fromLatin1(NmInterface), QStringLiteral("WirelessEnabled"),
                         &current, &error, QDBusConnection::systemBus())) {
            reportDbusError(QStringLiteral("Changing Wi-Fi power"), error);
            return;
        }
        if (current.toBool() == enabled) {
            updated = true;
            break;
        }
        QThread::msleep(250);
    }
    if (!updated) {
        emit errorOccurred(QStringLiteral("NetworkManager did not finish changing the Wi-Fi power state."));
        return;
    }
    refreshWifi();
}

void NativeBridge::connectWifi(const QString &accessPointPath, const QString &password)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [accessPointPath, password](NativeBridge *bridge) {
            bridge->connectWifi(accessPointPath, password);
        });
        return;
    }
    QString error;
    if (!accessPointPath.startsWith(QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/"))) {
        emit errorOccurred(QStringLiteral("Choose a Wi-Fi network from the available list."));
        return;
    }
    QVariantMap ap;
    if (!getAll(QString::fromLatin1(NetworkManagerService), accessPointPath,
                QString::fromLatin1(NmAccessPointInterface), &ap, &error,
                QDBusConnection::systemBus())) {
        reportDbusError(QStringLiteral("Connecting to Wi-Fi"), error);
        return;
    }
    QString devicePath;
    QVariantMap device;
    if (!wifiDevice(&devicePath, &device, &error) || devicePath.isEmpty()) {
        reportDbusError(QStringLiteral("Connecting to Wi-Fi"),
                        error.isEmpty() ? QStringLiteral("No Wi-Fi adapter is available.") : error);
        return;
    }
    const QByteArray ssid = ap.value(QStringLiteral("Ssid")).toByteArray();
    if (ssid.isEmpty()) {
        emit errorOccurred(QStringLiteral("This network does not broadcast a usable network name."));
        return;
    }
    QVariantList profiles;
    if (!readNetworkProfiles(&profiles, &error)) {
        reportDbusError(QStringLiteral("Connecting to Wi-Fi"), error);
        return;
    }
    QString savedPath;
    for (const QVariant &profile : profiles) {
        if (profile.toMap().value(QStringLiteral("ssidBytes")).toByteArray() == ssid) {
            savedPath = profile.toMap().value(QStringLiteral("path")).toString();
            break;
        }
    }
    if (!savedPath.isEmpty()) {
        QVariantList activation;
        if (!dbusCall(QString::fromLatin1(NetworkManagerService), QString::fromLatin1(NetworkManagerPath),
                      QString::fromLatin1(NmInterface), QStringLiteral("ActivateConnection"),
                      {QDBusObjectPath(savedPath), QDBusObjectPath(devicePath), QDBusObjectPath(accessPointPath)},
                      &activation, &error, QDBusConnection::systemBus())
            || !waitForWifi(devicePath, savedPath, accessPointPath, &error)) {
            reportDbusError(QStringLiteral("Connecting to saved Wi-Fi"), error);
            return;
        }
    } else {
        const QVariantMap security = networkSecurity(ap);
        const uint keyFlags = security.value(QStringLiteral("keyFlags")).toUInt();
        QString keyMgmt;
        if (security.value(QStringLiteral("requiresPassword")).toBool()) {
            if (keyFlags & 0x400) keyMgmt = QStringLiteral("sae");
            else if (keyFlags & 0x100) keyMgmt = QStringLiteral("wpa-psk");
            else if (keyFlags & (0x800 | 0x1000)) keyMgmt = QStringLiteral("owe");
            else {
                emit errorOccurred(QStringLiteral("This network uses WEP or enterprise authentication, which is not supported."));
                return;
            }
            if ((keyMgmt == QLatin1String("sae") || keyMgmt == QLatin1String("wpa-psk"))
                && password.isEmpty()) {
                emit errorOccurred(QStringLiteral("Enter the Wi-Fi password to connect."));
                return;
            }
        }
        NmConnectionSettings settings;
        settings.insert(QStringLiteral("connection"),
                        {{QStringLiteral("id"), bytesToSsid(ssid)},
                         {QStringLiteral("uuid"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
                         {QStringLiteral("type"), QStringLiteral("802-11-wireless")},
                         {QStringLiteral("autoconnect"), true}});
        settings.insert(QStringLiteral("802-11-wireless"),
                        {{QStringLiteral("ssid"), ssid},
                         {QStringLiteral("mode"), QStringLiteral("infrastructure")}});
        if (!keyMgmt.isEmpty()) {
            QVariantMap wifiSecurity{{QStringLiteral("key-mgmt"), keyMgmt}};
            if (!password.isEmpty())
                wifiSecurity.insert(QStringLiteral("psk"), password);
            settings.insert(QStringLiteral("802-11-wireless-security"), wifiSecurity);
        }
        QVariantList activation;
        if (!dbusCall(QString::fromLatin1(NetworkManagerService), QString::fromLatin1(NetworkManagerPath),
                      QString::fromLatin1(NmInterface), QStringLiteral("AddAndActivateConnection"),
                      {QVariant::fromValue(settings), QDBusObjectPath(devicePath), QDBusObjectPath(accessPointPath)},
                      &activation, &error, QDBusConnection::systemBus())) {
            reportDbusError(QStringLiteral("Connecting to Wi-Fi"), error);
            return;
        }
        if (activation.isEmpty()) {
            reportDbusError(QStringLiteral("Connecting to Wi-Fi"),
                            QStringLiteral("NetworkManager returned no active connection path."));
            return;
        }
        const QString activePath = dbusString(activation.first());
        bool connectionReady = false;
        for (int attempt = 0; attempt < 60; ++attempt) {
            QVariantMap active;
            if (!getAll(QString::fromLatin1(NetworkManagerService), activePath,
                        QString::fromLatin1(NmActiveConnectionInterface), &active, &error,
                        QDBusConnection::systemBus())) {
                reportDbusError(QStringLiteral("Connecting to Wi-Fi"), error);
                return;
            }
            if (active.value(QStringLiteral("State")).toUInt() == 2) {
                QVariantMap wireless;
                if (!getAll(QString::fromLatin1(NetworkManagerService), devicePath,
                            QString::fromLatin1(NmWirelessInterface), &wireless, &error,
                            QDBusConnection::systemBus())) {
                    reportDbusError(QStringLiteral("Connecting to Wi-Fi"), error);
                    return;
                }
                if (dbusString(wireless.value(QStringLiteral("ActiveAccessPoint"))) == accessPointPath) {
                    connectionReady = true;
                    break;
                }
            }
            QThread::msleep(500);
        }
        if (!connectionReady) {
            error = QStringLiteral("The Wi-Fi connection did not become active within 30 seconds. Check the password, signal, and network availability.");
            reportDbusError(QStringLiteral("Connecting to Wi-Fi"), error);
            return;
        }
    }
    refreshWifi();
}

void NativeBridge::connectSavedWifi(const QString &connectionPath)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [connectionPath](NativeBridge *bridge) {
            bridge->connectSavedWifi(connectionPath);
        });
        return;
    }
    if (!connectionPath.startsWith(QStringLiteral("/org/freedesktop/NetworkManager/Settings/"))) {
        emit errorOccurred(QStringLiteral("Choose a saved Wi-Fi network from the list."));
        return;
    }
    QVariantList profiles;
    QString error;
    if (!readNetworkProfiles(&profiles, &error)) {
        reportDbusError(QStringLiteral("Connecting to saved Wi-Fi"), error);
        return;
    }
    bool found = false;
    for (const QVariant &entry : profiles)
        found = found || entry.toMap().value(QStringLiteral("path")).toString() == connectionPath;
    if (!found) {
        emit errorOccurred(QStringLiteral("The saved Wi-Fi network no longer exists."));
        return;
    }
    QString devicePath;
    QVariantMap device;
    if (!wifiDevice(&devicePath, &device, &error) || devicePath.isEmpty()) {
        reportDbusError(QStringLiteral("Connecting to saved Wi-Fi"),
                        error.isEmpty() ? QStringLiteral("No Wi-Fi adapter is available.") : error);
        return;
    }
    QVariantList ignored;
    if (!dbusCall(QString::fromLatin1(NetworkManagerService), QString::fromLatin1(NetworkManagerPath),
                  QString::fromLatin1(NmInterface), QStringLiteral("ActivateConnection"),
                  {QDBusObjectPath(connectionPath), QDBusObjectPath(devicePath), QDBusObjectPath("/")},
                  &ignored, &error, QDBusConnection::systemBus())
        || !waitForWifi(devicePath, connectionPath, {}, &error)) {
        reportDbusError(QStringLiteral("Connecting to saved Wi-Fi"), error);
        return;
    }
    refreshWifi();
}

void NativeBridge::disconnectWifi()
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [](NativeBridge *bridge) { bridge->disconnectWifi(); });
        return;
    }
    QString error, devicePath;
    QVariantMap device;
    if (!wifiDevice(&devicePath, &device, &error) || devicePath.isEmpty()) {
        reportDbusError(QStringLiteral("Disconnecting Wi-Fi"),
                        error.isEmpty() ? QStringLiteral("No Wi-Fi adapter is available.") : error);
        return;
    }
    const QString active = dbusString(device.value(QStringLiteral("ActiveConnection")));
    if (active.isEmpty() || active == QLatin1String("/")) {
        emit errorOccurred(QStringLiteral("There is no active Wi-Fi connection."));
        return;
    }
    QVariantList ignored;
    if (!dbusCall(QString::fromLatin1(NetworkManagerService), QString::fromLatin1(NetworkManagerPath),
                  QString::fromLatin1(NmInterface), QStringLiteral("DeactivateConnection"),
                  {QDBusObjectPath(active)}, &ignored, &error, QDBusConnection::systemBus())) {
        reportDbusError(QStringLiteral("Disconnecting Wi-Fi"), error);
        return;
    }
    refreshWifi();
}

void NativeBridge::forgetSavedWifi(const QString &connectionPath)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [connectionPath](NativeBridge *bridge) {
            bridge->forgetSavedWifi(connectionPath);
        });
        return;
    }
    if (!connectionPath.startsWith(QStringLiteral("/org/freedesktop/NetworkManager/Settings/"))) {
        emit errorOccurred(QStringLiteral("Choose a saved Wi-Fi network from the list."));
        return;
    }
    QVariantList profiles;
    QString error;
    if (!readNetworkProfiles(&profiles, &error)) {
        reportDbusError(QStringLiteral("Forgetting Wi-Fi network"), error);
        return;
    }
    bool found = false;
    for (const QVariant &entry : profiles)
        found = found || entry.toMap().value(QStringLiteral("path")).toString() == connectionPath;
    if (!found) {
        emit errorOccurred(QStringLiteral("The saved Wi-Fi network no longer exists."));
        return;
    }
    QVariantList ignored;
    if (!dbusCall(QString::fromLatin1(NetworkManagerService), connectionPath,
                  QString::fromLatin1(NmConnectionInterface), QStringLiteral("Delete"),
                  {}, &ignored, &error, QDBusConnection::systemBus())) {
        reportDbusError(QStringLiteral("Forgetting Wi-Fi network"), error);
        return;
    }
    refreshWifi();
}

void NativeBridge::refreshBluetooth()
{
    runInBackground(this, [](NativeBridge *bridge) {
        bridge->refreshBluetoothInternal();
    });
}

void NativeBridge::refreshBluetoothInternal()
{
    ManagedBluezObjects objects;
    QString error;
    if (!bluezObjects(&objects, &error)) {
        reportDbusError(QStringLiteral("Bluetooth"), error);
        return;
    }
    const QString adapterPath = bluezAdapterPath(objects);
    if (adapterPath.isEmpty()) {
        const QVariantMap data{{QStringLiteral("hasAdapter"), false},
                               {QStringLiteral("adapterPath"), QString()},
                               {QStringLiteral("adapterName"), QString()},
                               {QStringLiteral("powered"), false},
                               {QStringLiteral("discovering"), false},
                               {QStringLiteral("nearby"), QVariantList{}},
                               {QStringLiteral("paired"), QVariantList{}}};
        QMetaObject::invokeMethod(this, [this, data] {
            m_bluetoothData = data;
            emit bluetoothDataChanged(m_bluetoothData);
            emit shortcutMessage(QStringLiteral("No Bluetooth adapter is available."));
        }, Qt::QueuedConnection);
        return;
    }
    const QVariantMap adapter = objectProperties(objects, adapterPath,
                                                  QString::fromLatin1(BluezAdapterInterface));
    QVariantList nearby, paired;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const QVariantMap properties = it.value().value(QString::fromLatin1(BluezDeviceInterface));
        if (properties.isEmpty() || dbusString(properties.value(QStringLiteral("Adapter"))) != adapterPath)
            continue;
        const bool isPaired = properties.value(QStringLiteral("Paired")).toBool();
        QString name = properties.value(QStringLiteral("Alias")).toString();
        if (name.isEmpty())
            name = properties.value(QStringLiteral("Name")).toString();
        if (name.isEmpty())
            name = properties.value(QStringLiteral("Address")).toString();
        QVariantMap item{{QStringLiteral("path"), it.key().path()},
                         {QStringLiteral("name"), name},
                         {QStringLiteral("address"), properties.value(QStringLiteral("Address")).toString()},
                         {QStringLiteral("icon"), properties.value(QStringLiteral("Icon")).toString()},
                         {QStringLiteral("connected"), properties.value(QStringLiteral("Connected")).toBool()}};
        if (properties.contains(QStringLiteral("RSSI")))
            item.insert(QStringLiteral("rssi"), properties.value(QStringLiteral("RSSI")).toInt());
        else
            item.insert(QStringLiteral("rssi"), QVariant());
        if (isPaired)
            paired.append(item);
        else if (properties.contains(QStringLiteral("RSSI")))
            nearby.append(item);
    }
    std::sort(nearby.begin(), nearby.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap a = left.toMap(), b = right.toMap();
        if (a.value(QStringLiteral("rssi")).toInt() != b.value(QStringLiteral("rssi")).toInt())
            return a.value(QStringLiteral("rssi")).toInt() > b.value(QStringLiteral("rssi")).toInt();
        return a.value(QStringLiteral("name")).toString().compare(
            b.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    std::sort(paired.begin(), paired.end(), [](const QVariant &left, const QVariant &right) {
        return left.toMap().value(QStringLiteral("name")).toString().compare(
            right.toMap().value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    QString adapterName = adapter.value(QStringLiteral("Alias")).toString();
    if (adapterName.isEmpty())
        adapterName = adapter.value(QStringLiteral("Address")).toString();
    const QVariantMap data{{QStringLiteral("hasAdapter"), true},
                           {QStringLiteral("adapterPath"), adapterPath},
                           {QStringLiteral("adapterName"), adapterName.isEmpty()
                                ? QStringLiteral("Bluetooth adapter") : adapterName},
                           {QStringLiteral("powered"), adapter.value(QStringLiteral("Powered")).toBool()},
                           {QStringLiteral("discovering"), adapter.value(QStringLiteral("Discovering")).toBool()},
                           {QStringLiteral("nearby"), nearby},
                           {QStringLiteral("paired"), paired}};
    QMetaObject::invokeMethod(this, [this, data] {
        m_bluetoothData = data;
        emit bluetoothDataChanged(m_bluetoothData);
        emit shortcutMessage(QStringLiteral("Bluetooth adapter ready: %1.")
                                 .arg(m_bluetoothData.value(QStringLiteral("adapterName")).toString()));
    }, Qt::QueuedConnection);
}

void NativeBridge::scanBluetooth()
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [](NativeBridge *bridge) { bridge->scanBluetooth(); });
        return;
    }
    ManagedBluezObjects objects;
    QString error;
    if (!bluezObjects(&objects, &error)) {
        reportDbusError(QStringLiteral("Bluetooth scan"), error);
        return;
    }
    const QString adapterPath = bluezAdapterPath(objects);
    const QVariantMap adapter = objectProperties(objects, adapterPath,
                                                  QString::fromLatin1(BluezAdapterInterface));
    if (adapterPath.isEmpty() || !adapter.value(QStringLiteral("Powered")).toBool()) {
        emit errorOccurred(QStringLiteral("Turn Bluetooth on before scanning for devices."));
        return;
    }
    const bool startedHere = !adapter.value(QStringLiteral("Discovering")).toBool();
    QVariantList ignored;
    if (startedHere && !dbusCall(QString::fromLatin1(BluezService), adapterPath,
                                 QString::fromLatin1(BluezAdapterInterface),
                                 QStringLiteral("StartDiscovery"), {}, &ignored, &error,
                                 QDBusConnection::systemBus())) {
        reportDbusError(QStringLiteral("Bluetooth scan"), error);
        return;
    }
    QThread::sleep(6);
    if (startedHere && !dbusCall(QString::fromLatin1(BluezService), adapterPath,
                                 QString::fromLatin1(BluezAdapterInterface),
                                 QStringLiteral("StopDiscovery"), {}, &ignored, &error,
                                 QDBusConnection::systemBus())) {
        reportDbusError(QStringLiteral("Bluetooth scan"), error);
        return;
    }
    refreshBluetooth();
}

void NativeBridge::setBluetoothPowered(bool powered)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [powered](NativeBridge *bridge) {
            bridge->setBluetoothPowered(powered);
        });
        return;
    }
    ManagedBluezObjects objects;
    QString error;
    if (!bluezObjects(&objects, &error)) {
        reportDbusError(QStringLiteral("Bluetooth power"), error);
        return;
    }
    const QString adapterPath = bluezAdapterPath(objects);
    if (adapterPath.isEmpty()) {
        emit errorOccurred(QStringLiteral("No Bluetooth adapter is available."));
        return;
    }
    QVariantList ignored;
    if (!dbusCall(QString::fromLatin1(BluezService), adapterPath,
                  QString::fromLatin1(PropertiesInterface), QStringLiteral("Set"),
                  {QString::fromLatin1(BluezAdapterInterface), QStringLiteral("Powered"),
                   QVariant::fromValue(QDBusVariant(QVariant(powered)))},
                  &ignored, &error, QDBusConnection::systemBus())) {
        reportDbusError(QStringLiteral("Bluetooth power"), error);
        return;
    }
    for (int attempt = 0; attempt < 40; ++attempt) {
        ManagedBluezObjects current;
        if (!bluezObjects(&current, &error)) {
            reportDbusError(QStringLiteral("Bluetooth power"), error);
            return;
        }
        if (objectProperties(current, adapterPath, QString::fromLatin1(BluezAdapterInterface))
                .value(QStringLiteral("Powered")).toBool() == powered) {
            refreshBluetooth();
            return;
        }
        QThread::msleep(250);
    }
    emit errorOccurred(QStringLiteral("BlueZ did not finish changing the Bluetooth power state."));
}

void NativeBridge::pairBluetoothDevice(const QString &path)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [path](NativeBridge *bridge) { bridge->pairBluetoothDevice(path); });
        return;
    }
    QString error;
    if (!bluetoothAction(path, QStringLiteral("Pair"), &error)) {
        reportDbusError(QStringLiteral("Pairing Bluetooth device"), error);
        return;
    }
    refreshBluetooth();
}

void NativeBridge::connectBluetoothDevice(const QString &path)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [path](NativeBridge *bridge) { bridge->connectBluetoothDevice(path); });
        return;
    }
    QString error;
    ManagedBluezObjects objects;
    if (!bluezObjects(&objects, &error)) {
        reportDbusError(QStringLiteral("Connecting Bluetooth device"), error);
        return;
    }
    if (!objectProperties(objects, path, QString::fromLatin1(BluezDeviceInterface))
             .value(QStringLiteral("Paired")).toBool()) {
        emit errorOccurred(QStringLiteral("Pair with this Bluetooth device before connecting."));
        return;
    }
    if (!bluetoothAction(path, QStringLiteral("Connect"), &error)) {
        reportDbusError(QStringLiteral("Connecting Bluetooth device"), error);
        return;
    }
    refreshBluetooth();
}

void NativeBridge::disconnectBluetoothDevice(const QString &path)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [path](NativeBridge *bridge) { bridge->disconnectBluetoothDevice(path); });
        return;
    }
    QString error;
    if (!bluetoothAction(path, QStringLiteral("Disconnect"), &error)) {
        reportDbusError(QStringLiteral("Disconnecting Bluetooth device"), error);
        return;
    }
    refreshBluetooth();
}

void NativeBridge::forgetBluetoothDevice(const QString &path)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [path](NativeBridge *bridge) { bridge->forgetBluetoothDevice(path); });
        return;
    }
    ManagedBluezObjects objects;
    QString error;
    if (!bluezObjects(&objects, &error)) {
        reportDbusError(QStringLiteral("Removing Bluetooth device"), error);
        return;
    }
    const QString adapterPath = bluezAdapterPath(objects);
    const QVariantMap device = objectProperties(objects, path, QString::fromLatin1(BluezDeviceInterface));
    if (adapterPath.isEmpty() || !path.startsWith(adapterPath + QStringLiteral("/dev_"))
        || device.isEmpty() || dbusString(device.value(QStringLiteral("Adapter"))) != adapterPath
        || !device.value(QStringLiteral("Paired")).toBool()) {
        emit errorOccurred(QStringLiteral("This device is no longer a saved Bluetooth device."));
        return;
    }
    QVariantList ignored;
    if (!dbusCall(QString::fromLatin1(BluezService), adapterPath,
                  QString::fromLatin1(BluezAdapterInterface), QStringLiteral("RemoveDevice"),
                  {QDBusObjectPath(path)}, &ignored, &error, QDBusConnection::systemBus())) {
        reportDbusError(QStringLiteral("Removing Bluetooth device"), error);
        return;
    }
    for (int attempt = 0; attempt < 40; ++attempt) {
        ManagedBluezObjects current;
        if (!bluezObjects(&current, &error)) {
            reportDbusError(QStringLiteral("Removing Bluetooth device"), error);
            return;
        }
        if (!current.contains(QDBusObjectPath(path))) {
            refreshBluetooth();
            return;
        }
        QThread::msleep(250);
    }
    emit errorOccurred(QStringLiteral("BlueZ accepted the request but did not remove the paired device."));
}

void NativeBridge::refreshInputSettings()
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [](NativeBridge *bridge) { bridge->refreshInputSettings(); });
        return;
    }
    QStringList layouts, variants, displayNames;
    QString error;
    if (!readConfigList(QStringLiteral("LayoutList"), &layouts, &error)
        || !readConfigList(QStringLiteral("VariantList"), &variants, &error)
        || !readConfigList(QStringLiteral("DisplayNames"), &displayNames, &error)) {
        reportDbusError(QStringLiteral("KDE keyboard layouts"), error);
        return;
    }
    QFile rules(QString::fromLatin1(XkbRulesPath));
    if (!rules.open(QIODevice::ReadOnly)) {
        reportDbusError(QStringLiteral("XKB keyboard layouts"),
                        QStringLiteral("Could not read XKB rules at %1").arg(QString::fromLatin1(XkbRulesPath)));
        return;
    }
    QVariantList availableLayouts;
    QXmlStreamReader xml(&rules);
    QString layoutCode, layoutName, variantCode, variantName;
    bool insideLayout = false, insideVariant = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name == QLatin1String("layout")) {
                insideLayout = true;
                layoutCode.clear();
                layoutName.clear();
            } else if (name == QLatin1String("variant")) {
                insideVariant = true;
                variantCode.clear();
                variantName.clear();
            } else if (name == QLatin1String("name") && insideLayout) {
                const QString value = xml.readElementText().trimmed();
                if (insideVariant) variantCode = value; else layoutCode = value;
            } else if (name == QLatin1String("description") && insideLayout) {
                const QString value = xml.readElementText().trimmed();
                if (insideVariant) variantName = value; else layoutName = value;
            }
        } else if (xml.isEndElement()) {
            if (xml.name() == QLatin1String("variant") && insideLayout) {
                if (!variantCode.isEmpty()) {
                    availableLayouts.append(QVariantMap{
                        {QStringLiteral("layout"), layoutCode},
                        {QStringLiteral("variant"), variantCode},
                        {QStringLiteral("label"), QStringLiteral("%1 - %2 (%3/%4)")
                            .arg(layoutName, variantName.isEmpty() ? variantCode : variantName,
                                 layoutCode, variantCode)}});
                }
                insideVariant = false;
            } else if (xml.name() == QLatin1String("layout")) {
                if (!layoutCode.isEmpty())
                    availableLayouts.append(QVariantMap{
                        {QStringLiteral("layout"), layoutCode},
                        {QStringLiteral("variant"), QString()},
                        {QStringLiteral("label"), QStringLiteral("%1 (%2)")
                            .arg(layoutName.isEmpty() ? layoutCode : layoutName, layoutCode)}});
                insideLayout = false;
            }
        }
    }
    if (xml.hasError()) {
        reportDbusError(QStringLiteral("XKB keyboard layouts"), xml.errorString());
        return;
    }
    std::sort(availableLayouts.begin(), availableLayouts.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("label")).toString().compare(
            b.toMap().value(QStringLiteral("label")).toString(), Qt::CaseInsensitive) < 0;
    });
    QVariantList keyboardLayouts;
    for (qsizetype i = 0; i < layouts.size(); ++i) {
        const QString variant = i < variants.size() ? variants.at(i) : QString();
        const QString displayName = i < displayNames.size() ? displayNames.at(i) : QString();
        QString label = displayName;
        if (label.isEmpty()) {
            label = layouts.at(i) + QLatin1Char('/') + variant;
            for (const QVariant &available : availableLayouts) {
                const QVariantMap row = available.toMap();
                if (row.value(QStringLiteral("layout")).toString() == layouts.at(i)
                    && row.value(QStringLiteral("variant")).toString() == variant) {
                    label = row.value(QStringLiteral("label")).toString();
                    break;
                }
            }
        }
        keyboardLayouts.append(QVariantMap{{QStringLiteral("layout"), layouts.at(i)},
                                           {QStringLiteral("variant"), variant},
                                           {QStringLiteral("displayName"), displayName},
                                           {QStringLiteral("label"), label}});
    }

    QVariantMap manager;
    if (!getAll(QString::fromLatin1(KWinService), QString::fromLatin1(KWinInputManagerPath),
                QString::fromLatin1(KWinInputManagerInterface), &manager, &error)) {
        reportDbusError(QStringLiteral("KWin input devices"), error);
        return;
    }
    QVariantList devices;
    QStringList sysNames = manager.value(QStringLiteral("devicesSysNames")).toStringList();
    if (sysNames.isEmpty() && manager.value(QStringLiteral("devicesSysNames")).canConvert<QDBusArgument>())
        sysNames = qdbus_cast<QStringList>(manager.value(QStringLiteral("devicesSysNames")).value<QDBusArgument>());
    for (const QString &sysName : sysNames) {
        QVariantMap properties;
        if (!getAll(QString::fromLatin1(KWinService),
                    QString::fromLatin1(KWinInputDevicePrefix) + sysName,
                    QString::fromLatin1(KWinInputDeviceInterface), &properties, &error)) {
            reportDbusError(QStringLiteral("KWin input devices"), error);
            return;
        }
        if (!properties.value(QStringLiteral("mouse")).toBool()
            && !properties.value(QStringLiteral("touchpad")).toBool())
            continue;
        properties.insert(QStringLiteral("sysName"), sysName);
        if (properties.value(QStringLiteral("name")).toString().isEmpty())
            properties.insert(QStringLiteral("name"), sysName);
        devices.append(properties);
    }
    QVariantList mice, touchpads;
    for (const QVariant &item : devices) {
        const QVariantMap device = item.toMap();
        if (device.value(QStringLiteral("touchpad")).toBool())
            touchpads.append(device);
        else if (device.value(QStringLiteral("mouse")).toBool())
            mice.append(device);
    }
    const QVariantMap data{{QStringLiteral("LayoutList"), layouts},
                           {QStringLiteral("VariantList"), variants},
                           {QStringLiteral("DisplayNames"), displayNames},
                           {QStringLiteral("availableLayouts"), availableLayouts},
                           {QStringLiteral("devices"), devices},
                           {QStringLiteral("keyboardLayouts"), keyboardLayouts},
                           {QStringLiteral("availableKeyboardLayouts"), availableLayouts},
                           {QStringLiteral("mouseDevices"), mice},
                           {QStringLiteral("touchpadDevices"), touchpads}};
    QMetaObject::invokeMethod(this, [this, data] {
        m_inputSettingsData = data;
        emit inputSettingsDataChanged(m_inputSettingsData);
        emit shortcutMessage(QStringLiteral("Input settings refreshed."));
    }, Qt::QueuedConnection);
}

void NativeBridge::setInputDeviceProperties(const QString &sysName, const QString &propertiesJson)
{
    if (QThread::currentThread() == thread()) {
        runInBackground(this, [sysName, propertiesJson](NativeBridge *bridge) {
            bridge->setInputDeviceProperties(sysName, propertiesJson);
        });
        return;
    }
    if (sysName.isEmpty() || sysName.contains(QLatin1Char('/'))) {
        postError(QStringLiteral("Choose an input device from the current KWin session."));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(propertiesJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || document.object().isEmpty()) {
        postError(QStringLiteral("Input device changes are not valid JSON."));
        return;
    }
    const QString path = QString::fromLatin1(KWinInputDevicePrefix) + sysName;
    QVariantMap current;
    QString error;
    if (!getAll(QString::fromLatin1(KWinService), path,
                QString::fromLatin1(KWinInputDeviceInterface), &current, &error)) {
        reportDbusError(QStringLiteral("KWin input device settings"), error);
        return;
    }
    const QVariantMap changes = document.object().toVariantMap();
    const QVariantMap allowed{
        {QStringLiteral("pointerAcceleration"), current.value(QStringLiteral("supportsPointerAcceleration"))},
        {QStringLiteral("pointerAccelerationProfileAdaptive"), current.value(QStringLiteral("supportsPointerAccelerationProfileAdaptive"))},
        {QStringLiteral("pointerAccelerationProfileFlat"), current.value(QStringLiteral("supportsPointerAccelerationProfileFlat"))},
        {QStringLiteral("tapToClick"), current.value(QStringLiteral("touchpad")).toBool()
             && current.value(QStringLiteral("tapFingerCount")).toInt() > 0},
        {QStringLiteral("naturalScroll"), current.value(QStringLiteral("supportsNaturalScroll"))},
        {QStringLiteral("scrollFactor"), current.value(QStringLiteral("touchpad"))}};
    const QList<double> scrollFactors{0.1, 0.3, 0.5, 0.75, 1, 1.5, 2, 3, 4, 5, 7, 9, 12, 15, 20};
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        const QString key = it.key();
        const QVariant value = it.value();
        if (!allowed.contains(key)) {
            postError(QStringLiteral("%1 cannot be changed from Obsidian.").arg(key));
            return;
        }
        if (!allowed.value(key).toBool()) {
            postError(QStringLiteral("%1 is not supported by this device.").arg(key));
            return;
        }
        if (key == QLatin1String("pointerAcceleration")) {
            bool ok = false;
            const double number = value.toDouble(&ok);
            if (!ok || number < -1 || number > 1) {
                postError(QStringLiteral("Pointer acceleration must be between -1 and 1."));
                return;
            }
        } else if (key == QLatin1String("scrollFactor")) {
            const double factor = value.toDouble();
            if (std::none_of(scrollFactors.cbegin(), scrollFactors.cend(),
                             [&](double candidate) { return qFuzzyCompare(candidate, factor); })) {
                postError(QStringLiteral("Choose a supported touchpad scroll speed."));
                return;
            }
        } else if (value.metaType() != QMetaType::fromType<bool>()) {
            postError(QStringLiteral("%1 must be a boolean.").arg(key));
            return;
        }
        const bool isDouble = key == QLatin1String("pointerAcceleration")
            || key == QLatin1String("scrollFactor");
        const QVariant payload = isDouble ? QVariant::fromValue(value.toDouble())
                                          : QVariant::fromValue(value.toBool());
        QVariantList ignored;
        if (!dbusCall(QString::fromLatin1(KWinService), path,
                      QString::fromLatin1(PropertiesInterface), QStringLiteral("Set"),
                      {QString::fromLatin1(KWinInputDeviceInterface), key,
                       QVariant::fromValue(QDBusVariant(payload))}, &ignored, &error)) {
            reportDbusError(QStringLiteral("KWin input device settings"), error);
            return;
        }
    }
    refreshInputSettings();
}

void NativeBridge::saveKeyboardLayouts(const QString &layoutsJson)
{
    if (QThread::currentThread() == thread()) {
        const QVariantList availableLayouts =
            m_inputSettingsData.value(QStringLiteral("availableKeyboardLayouts")).toList();
        runInBackground(this, [layoutsJson, availableLayouts](NativeBridge *bridge) {
            bridge->saveKeyboardLayoutsInternal(layoutsJson, availableLayouts);
        });
        return;
    }
    saveKeyboardLayoutsInternal(
        layoutsJson, m_inputSettingsData.value(QStringLiteral("availableKeyboardLayouts")).toList());
}

void NativeBridge::saveKeyboardLayoutsInternal(const QString &layoutsJson,
                                               const QVariantList &availableLayouts)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(layoutsJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        postError(QStringLiteral("Keyboard layouts are not valid JSON."));
        return;
    }
    QSet<QString> available;
    for (const QVariant &entry : availableLayouts) {
        const QVariantMap layout = entry.toMap();
        available.insert(layout.value(QStringLiteral("layout")).toString() + QLatin1Char('\n')
                         + layout.value(QStringLiteral("variant")).toString());
    }
    QStringList layouts, variants, displayNames;
    QSet<QString> configured;
    for (const QJsonValue &entry : document.array()) {
        if (!entry.isObject()) {
            postError(QStringLiteral("Keyboard layout entries must be objects."));
            return;
        }
        const QJsonObject item = entry.toObject();
        const QString layout = item.value(QStringLiteral("layout")).toString();
        const QString variant = item.value(QStringLiteral("variant")).toString();
        const QString displayName = item.value(QStringLiteral("displayName")).toString();
        const QString key = layout + QLatin1Char('\n') + variant;
        if (layout.isEmpty() || !available.contains(key)) {
            postError(QStringLiteral("A keyboard layout is not available in the XKB rules."));
            return;
        }
        if (configured.contains(key)) {
            postError(QStringLiteral("Keyboard layouts must not contain duplicates."));
            return;
        }
        configured.insert(key);
        layouts.append(layout);
        variants.append(variant);
        displayNames.append(displayName);
    }
    QString error;
    if (!writeConfigList(QStringLiteral("LayoutList"), layouts, &error)
        || !writeConfigList(QStringLiteral("VariantList"), variants, &error)
        || !writeConfigList(QStringLiteral("DisplayNames"), displayNames, &error)
        || !runConfigTool(QStringLiteral("kwriteconfig6"),
                          {QStringLiteral("--file"), xkbConfigPath(),
                           QStringLiteral("--group"), QStringLiteral("Layout"),
                           QStringLiteral("--key"), QStringLiteral("Use"),
                           QStringLiteral("--type"), QStringLiteral("bool"),
                           layouts.isEmpty() ? QStringLiteral("false") : QStringLiteral("true")},
                          nullptr, &error)) {
        reportDbusError(QStringLiteral("Saving KDE keyboard layouts"), error);
        return;
    }
    refreshInputSettings();
}

bool NativeBridge::startShortcutDaemon()
{
    const QStringList actionIds = obsidianActionIds();
    for (const QString &id : actionIds) {
        const QStringList identity{QStringLiteral("obsidian"), id, QStringLiteral("Obsidian"),
                                   obsidianActionName(id)};
        QVariantList ignored;
        QString error;
        if (!dbusCall(QString::fromLatin1(KGlobalAccelService), QString::fromLatin1(KGlobalAccelPath),
                      QString::fromLatin1(KGlobalAccelInterface), QStringLiteral("doRegister"),
                      {identity}, &ignored, &error)) {
            reportDbusError(QStringLiteral("Registering custom shortcuts"), error);
            return false;
        }
    }

    QVariantList result;
    QString error;
    if (!dbusCall(QString::fromLatin1(KGlobalAccelService), QString::fromLatin1(KGlobalAccelPath),
                  QString::fromLatin1(KGlobalAccelInterface), QStringLiteral("getComponent"),
                  {QStringLiteral("obsidian")}, &result, &error) || result.isEmpty()) {
        reportDbusError(QStringLiteral("Registering custom shortcuts"), error);
        return false;
    }
    const QString componentPath = result.first().value<QDBusObjectPath>().path();
    const QVariantMap stored = m_settings.value(QStringLiteral("obsidian_shortcuts")).toMap();
    for (const QString &id : actionIds) {
        const QString text = stored.value(id).toString().trimmed();
        if (text.isEmpty())
            continue;
        const QKeySequence sequence = QKeySequence::fromString(text, QKeySequence::PortableText);
        if (sequence.isEmpty() || sequence.count() != 1 || sequence[0] == 0) {
            reportDbusError(QStringLiteral("Registering custom shortcuts"),
                            QStringLiteral("Invalid saved shortcut for %1.").arg(obsidianActionName(id)));
            return false;
        }
        const QStringList identity{QStringLiteral("obsidian"), id, QStringLiteral("Obsidian"),
                                   obsidianActionName(id)};
        const QList<QList<int>> shortcuts{{sequence[0]}};
        QVariantList ignored;
        if (!dbusCall(QString::fromLatin1(KGlobalAccelService), QString::fromLatin1(KGlobalAccelPath),
                      QString::fromLatin1(KGlobalAccelInterface), QStringLiteral("setShortcutKeys"),
                      {identity, QVariant::fromValue(shortcuts), uint(6)}, &ignored, &error)) {
            reportDbusError(QStringLiteral("Registering custom shortcuts"), error);
            return false;
        }
    }
    if (!QDBusConnection::sessionBus().connect(
            QString::fromLatin1(KGlobalAccelService), componentPath,
            QString::fromLatin1(KGlobalAccelComponentInterface),
            QStringLiteral("globalShortcutPressed"), this,
            SLOT(onGlobalShortcutPressed(QString,QString,uint)))) {
        reportDbusError(QStringLiteral("Registering custom shortcuts"),
                        QStringLiteral("Could not subscribe to KGlobalAccel shortcut events."));
        return false;
    }
    emit shortcutMessage(QStringLiteral("Obsidian global shortcut handler is ready."));
    return true;
}

void NativeBridge::onGlobalShortcutPressed(const QString &component,
                                           const QString &action,
                                           uint timestamp)
{
    Q_UNUSED(timestamp);
    if (component != QLatin1String("obsidian") || !obsidianActionIds().contains(action))
        return;
    if (action == QLatin1String("open-settings")) {
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {}))
            reportDbusError(QStringLiteral("Opening Obsidian Settings"),
                            QStringLiteral("Could not launch the settings application."));
        return;
    }

    QFile settingsFile(settingsPath());
    if (settingsFile.exists()) {
        if (!settingsFile.open(QIODevice::ReadOnly)) {
            reportDbusError(QStringLiteral("Toggling Obsidian module"), settingsFile.errorString());
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument current = QJsonDocument::fromJson(settingsFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !current.isObject()) {
            reportDbusError(QStringLiteral("Toggling Obsidian module"),
                            QStringLiteral("Saved settings are not valid JSON."));
            return;
        }
        const QVariantMap stored = current.object().toVariantMap();
        for (auto it = stored.cbegin(); it != stored.cend(); ++it)
            m_settings.insert(it.key(), it.value());
    }

    QVariantMap updatedSettings = m_settings;
    const QString key = action == QLatin1String("toggle-kitty")
        ? QStringLiteral("kitty_enabled") : QStringLiteral("fish_enabled");
    updatedSettings.insert(key, !updatedSettings.value(key).toBool());
    const QString accentColor = updatedSettings.value(QStringLiteral("accent")).toString();
    QString fishAccent = accentColor;
    fishAccent.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    fishAccent.replace(QStringLiteral("'"), QStringLiteral("\\'"));
    const QString kitty = QStringLiteral(
        "# Managed by Obsidian Settings. Change options in the Obsidian app.\n"
        "background %1\nforeground %2\nselection_background %3\ncursor %3\n"
        "font_size %4\nenable_audio_bell no\nconfirm_os_window_close 0\n")
        .arg(updatedSettings.value(QStringLiteral("background")).toString(),
             updatedSettings.value(QStringLiteral("foreground")).toString(),
             accentColor,
             QString::number(updatedSettings.value(QStringLiteral("font_size")).toInt()));
    const QString fish = QStringLiteral(
        "# Managed by Obsidian Settings. Change options in the Obsidian app.\n"
        "function fish_prompt\n    set_color '%1'\n    printf '%%s' (prompt_pwd)\n"
        "    set_color normal\n    printf ' > '\nend\n").arg(fishAccent);
    applySettings(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(updatedSettings))
                                        .toJson(QJsonDocument::Compact)),
                  kitty, fish);
}
