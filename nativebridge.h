#pragma once

#include <QObject>
#include <QVariantMap>

class NativeBridge final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY accentChanged)
    Q_PROPERTY(QString background READ background WRITE setBackground NOTIFY backgroundChanged)
    Q_PROPERTY(QString foreground READ foreground WRITE setForeground NOTIFY foregroundChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(bool kittyEnabled READ kittyEnabled WRITE setKittyEnabled NOTIFY kittyEnabledChanged)
    Q_PROPERTY(bool fishEnabled READ fishEnabled WRITE setFishEnabled NOTIFY fishEnabledChanged)
    Q_PROPERTY(QString wallpaper READ wallpaper WRITE setWallpaper NOTIFY wallpaperChanged)
    Q_PROPERTY(QString wallpaperDirectory READ wallpaperDirectory NOTIFY wallpaperDirectoryChanged)
    Q_PROPERTY(QVariantList wallpaperChoices READ wallpaperChoices NOTIFY wallpaperChoicesChanged)
    Q_PROPERTY(QVariantMap settingsData READ settingsData NOTIFY settingsDataChanged)
    Q_PROPERTY(QVariantList shortcutActions READ shortcutActions NOTIFY shortcutActionsChanged)
    Q_PROPERTY(QVariantList shortcutBindings READ shortcutBindings NOTIFY shortcutBindingsChanged)
    Q_PROPERTY(int desktopCount READ desktopCount NOTIFY desktopCountChanged)
    Q_PROPERTY(QVariantMap wifiData READ wifiData NOTIFY wifiDataChanged)
    Q_PROPERTY(QVariantMap bluetoothData READ bluetoothData NOTIFY bluetoothDataChanged)
    Q_PROPERTY(QVariantMap inputSettingsData READ inputSettingsData NOTIFY inputSettingsDataChanged)

public:
    explicit NativeBridge(QObject *parent = nullptr);

    QString accent() const;
    void setAccent(const QString &value);
    QString background() const;
    void setBackground(const QString &value);
    QString foreground() const;
    void setForeground(const QString &value);
    int fontSize() const;
    void setFontSize(int value);
    bool kittyEnabled() const;
    void setKittyEnabled(bool value);
    bool fishEnabled() const;
    void setFishEnabled(bool value);
    QString wallpaper() const;
    void setWallpaper(const QString &value);
    QString wallpaperDirectory() const;
    QVariantList wallpaperChoices() const;
    QVariantMap settingsData() const;
    QVariantList shortcutActions() const;
    QVariantList shortcutBindings() const;
    int desktopCount() const;
    QVariantMap wifiData() const;
    QVariantMap bluetoothData() const;
    QVariantMap inputSettingsData() const;

    Q_INVOKABLE void setWallpaperFile(const QString &fileUrl);
    Q_INVOKABLE void loadWallpaperDirectory(const QString &directory);
    Q_INVOKABLE void applyWallpaper(const QString &path);
    Q_INVOKABLE void applySettings(const QString &settingsJson,
                                   const QString &kittyConfig,
                                   const QString &fishConfig);
    Q_INVOKABLE void refreshShortcuts();
    Q_INVOKABLE void assignShortcut(const QString &actionId, const QString &shortcut);
    Q_INVOKABLE void clearShortcut(const QString &actionId);
    Q_INVOKABLE void refreshDesktopCount();
    Q_INVOKABLE void setDesktopCount(int count);
    Q_INVOKABLE void refreshWifi();
    Q_INVOKABLE void scanWifi();
    Q_INVOKABLE void setWifiEnabled(bool enabled);
    Q_INVOKABLE void connectWifi(const QString &accessPoint, const QString &password);
    Q_INVOKABLE void connectSavedWifi(const QString &connection);
    Q_INVOKABLE void disconnectWifi();
    Q_INVOKABLE void forgetSavedWifi(const QString &connection);
    Q_INVOKABLE void refreshBluetooth();
    Q_INVOKABLE void scanBluetooth();
    Q_INVOKABLE void setBluetoothPowered(bool powered);
    Q_INVOKABLE void pairBluetoothDevice(const QString &path);
    Q_INVOKABLE void connectBluetoothDevice(const QString &path);
    Q_INVOKABLE void disconnectBluetoothDevice(const QString &path);
    Q_INVOKABLE void forgetBluetoothDevice(const QString &path);
    Q_INVOKABLE void refreshInputSettings();
    Q_INVOKABLE void setInputDeviceProperties(const QString &name, const QString &properties);
    Q_INVOKABLE void saveKeyboardLayouts(const QString &layouts);
    bool startShortcutDaemon();

private slots:
    void onGlobalShortcutPressed(const QString &component, const QString &action, uint timestamp);

signals:
    void accentChanged();
    void backgroundChanged();
    void foregroundChanged();
    void fontSizeChanged();
    void kittyEnabledChanged();
    void fishEnabledChanged();
    void wallpaperChanged();
    void wallpaperDirectoryChanged();
    void wallpaperChoicesChanged();
    void settingsDataChanged();
    void shortcutActionsChanged(const QVariantList &actions);
    void shortcutBindingsChanged(const QVariantList &bindings);
    void desktopCountChanged(int count);
    void wifiDataChanged(const QVariantMap &data);
    void bluetoothDataChanged(const QVariantMap &data);
    void inputSettingsDataChanged(const QVariantMap &data);
    void errorOccurred(const QString &message);
    void applied(const QString &message);
    void shortcutMessage(const QString &message);

private:
    void setValue(const QString &key, const QVariant &value, void (NativeBridge::*changed)());
    void reportDbusError(const QString &feature, const QString &detail);
    void postError(const QString &message);
    void postShortcutMessage(const QString &message);
    void refreshShortcutsInternal(const QVariantMap &settings);
    void assignShortcutInternal(const QString &actionId, const QString &shortcut,
                                const QVariantMap &settings);
    void clearShortcutInternal(const QString &actionId, const QVariantMap &settings);
    void refreshWifiInternal(bool scanRequested);
    void refreshBluetoothInternal();
    void saveKeyboardLayoutsInternal(const QString &layouts, const QVariantList &availableLayouts);
    QVariantMap m_settings;
    QString m_settingsLoadError;
    QVariantList m_wallpaperChoices;
    quint64 m_wallpaperLoadGeneration = 0;
    int m_desktopCount = 0;
    QVariantList m_shortcutActions;
    QVariantList m_shortcutBindings;
    QVariantMap m_wifiData;
    QVariantMap m_bluetoothData;
    QVariantMap m_inputSettingsData;
};
