# Obsidian Settings — CachyOS + KDE Plasma

Obsidian is a native Qt 6 application with a QML/JavaScript interface and a
C++ system-integration bridge. `Main.qml` contains the window and navigation;
shortcuts, desktops, Wi-Fi, Bluetooth, input, and Obsidian settings each have
their own QML page. `InputSettings.js` handles input-model transformations
and keyboard-layout changes, while `AppSettings.js` generates managed
Kitty/Fish configuration. C++ is used for QtDBus, file/configuration access,
and native process integration that QML cannot perform directly.

## Features

- **Shortcuts:** registered KWin/common KDE actions through KGlobalAccel,
  plus Obsidian actions for opening settings and toggling Kitty/Fish. The
  optional user service maintains Obsidian's custom global actions. Listing,
  assigning, and clearing shortcuts run off the UI thread.
- **Virtual desktops:** read and change KWin's desktop count (1–10), without
  blocking the UI.
- **Wi-Fi:** NetworkManager scan, power control, nearby and saved networks,
  WPA/WPA2 personal, WPA3 personal, enhanced-open/open networks, connect,
  disconnect, and forget. Scanning and connection operations run off the UI
  thread. Enterprise and WEP remain unsupported. Passwords are passed to
  NetworkManager and not retained by Obsidian.
- **Bluetooth:** BlueZ power control, scan, pair, connect, disconnect, and
  remove paired devices; these operations run off the UI thread. Pairing
  prompts are handled by the system's BlueZ agent (Plasma's agent is
  recommended).
- **Input:** KWin mouse/touchpad pointer settings and KDE user XKB layout
  management through `kxkbrc`/`kreadconfig6`/`kwriteconfig6`; refresh and
  update operations run off the UI thread.
- **Obsidian:** persistent color/font options, managed Kitty/Fish configs,
  and wallpaper application through Plasma.
- **Wallpaper picker:** a separate application-menu launcher with a scrollable
  image gallery. Choose a wallpaper folder once, then apply images immediately
  through Plasma.

All DBus call failures are reported in the UI; the bridge does not announce a
successful operation when a requested DBus call fails.

## Install

On CachyOS with KDE Plasma:

```bash
bash install.sh
```

The guided installer installs Qt 6, CMake, Ninja, a C++ compiler,
NetworkManager, KConfig, and XKB rules. Fish, Kitty, and BlueZ/BlueDevil are
optional. KWin and KDE's global shortcut service are expected from the KDE
desktop installation. NetworkManager and (if selected) Bluetooth services
must be running for their respective controls to work. The installer builds
the app under `~/.local/share/obsidian/`, adds an application-menu entry, and
adds an Obsidian Wallpapers launcher. It also enables the per-user
`obsidian-shortcutd.service`.

Open **Obsidian Wallpapers** from the application menu, choose a folder
containing wallpaper images, then scroll through the gallery and select
**Set wallpaper** on an image. The selected folder is remembered in
`~/.config/obsidian/settings.json`.

The shortcut daemon can be controlled with:

```bash
systemctl --user disable --now obsidian-shortcutd.service
systemctl --user enable --now obsidian-shortcutd.service
```

Manual build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## User files

- Application: `~/.local/share/obsidian/`
- Preferences: `~/.config/obsidian/settings.json`
- Managed Kitty config: `~/.config/kitty/kitty.conf`
- Managed Fish prompt: `~/.config/fish/conf.d/obsidian.fish`
- Shortcut daemon: `~/.config/systemd/user/obsidian-shortcutd.service`

The first time a managed Kitty or Fish configuration is enabled, an existing
file is preserved as a timestamped `*.obsidian-backup-*` file. Disabling an
Obsidian-managed module moves its file aside with a `.disabled-*` suffix.
