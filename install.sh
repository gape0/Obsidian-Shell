#!/usr/bin/env bash
set -euo pipefail

APP_NAME="obsidian"
APP_DIR="${HOME}/.local/share/${APP_NAME}"
CONFIG_DIR="${HOME}/.config/${APP_NAME}"
APP_ENTRY="${HOME}/.local/share/applications/${APP_NAME}-settings.desktop"
WALLPAPER_ENTRY="${HOME}/.local/share/applications/${APP_NAME}-wallpapers.desktop"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

ask_yes_no() {
    local prompt="$1"
    local answer
    while true; do
        read -r -p "${prompt} [y/N] " answer
        case "${answer,,}" in
            y|yes) return 0 ;;
            n|no|"") return 1 ;;
            *) printf 'Please answer y or n.\n' ;;
        esac
    done
}

if [[ "$(uname -s)" != "Linux" ]]; then
    printf 'This installer supports Linux only.\n' >&2
    exit 1
fi

if [[ ! -r /etc/os-release ]]; then
    printf 'Cannot identify this Linux distribution (/etc/os-release is missing).\n' >&2
    exit 1
fi

source /etc/os-release
if [[ "${ID:-}" != "cachyos" ]]; then
    printf 'This guided installer currently targets CachyOS; detected %s.\n' "${PRETTY_NAME:-unknown Linux}" >&2
    exit 1
fi

if [[ ! -t 0 ]]; then
    printf 'Run this installer in an interactive terminal.\n' >&2
    exit 1
fi

printf '\nObsidian for CachyOS + KDE Plasma\n'
printf 'No system config is changed until you confirm the relevant prompts.\n\n'

printf 'Required packages: Qt 6, CMake, Ninja, a C++ compiler, NetworkManager, KConfig, and XKB rules.\n'
printf 'KDE/KWin, NetworkManager, BlueZ, and XKB integrations are implemented using native DBus/config APIs.\n'
printf 'Optional packages: fish, kitty, and Bluetooth support (BlueZ/BlueDevil).\n'
printf 'All current packages are available from the official repositories; no AUR package is required.\n'

install_fish=false
install_kitty=false
install_bluetooth=false
make_fish_default=false
if ask_yes_no 'Install Fish (you can enable its Obsidian prompt after installation)?'; then
    install_fish=true
    if ask_yes_no 'Make Fish your default login shell?'; then
        make_fish_default=true
    fi
fi
if ask_yes_no 'Install Kitty (the Obsidian theme can be enabled in the settings GUI)?'; then
    install_kitty=true
fi
if ask_yes_no 'Install Bluetooth support (BlueZ and Plasma pairing agent)?'; then
    install_bluetooth=true
fi
printf '\nInstallation plan:\n'
printf '  - Build and install the native Qt/QML settings GUI\n'
printf '  - Install Fish: %s\n' "$install_fish"
printf '  - Install Kitty: %s\n' "$install_kitty"
printf '  - Install Bluetooth support and enable bluetooth.service: %s\n' "$install_bluetooth"
printf '  - Make Fish the default login shell: %s\n' "$make_fish_default"
printf '  - Install app under %s\n' "$APP_DIR"
printf '  - Save app preferences under %s\n' "$CONFIG_DIR"

if ! ask_yes_no 'Continue with this plan?'; then
    printf 'Cancelled. No changes were made.\n'
    exit 0
fi

command -v pacman >/dev/null 2>&1 || {
    printf 'pacman was not found; cannot install CachyOS packages.\n' >&2
    exit 1
}
command -v sudo >/dev/null 2>&1 || {
    printf 'sudo was not found; install packages manually, then rerun this script.\n' >&2
    exit 1
}

packages=(qt6-base qt6-declarative qt6-imageformats cmake ninja gcc networkmanager kconfig xkeyboard-config)
if "$install_fish"; then packages+=(fish); fi
if "$install_kitty"; then packages+=(kitty); fi
if "$install_bluetooth"; then packages+=(bluez bluez-utils bluedevil); fi

printf '\nInstalling packages: %s\n' "${packages[*]}"
sudo pacman -S --needed "${packages[@]}"

if "$install_bluetooth"; then
    if ! sudo systemctl enable --now bluetooth.service; then
        printf 'Bluetooth packages were installed, but bluetooth.service could not be enabled. Enable it later with: sudo systemctl enable --now bluetooth.service\n' >&2
    fi
fi

if "$make_fish_default"; then
    fish_path="$(command -v fish)"
    if ! chsh -s "$fish_path"; then
        printf 'Could not change the default shell. Fish is installed; change it later with: chsh -s %s\n' "$fish_path" >&2
    fi
fi

mkdir -p -- "$APP_DIR" "$CONFIG_DIR" "${HOME}/.local/share/applications"
cmake -S "$SCRIPT_DIR" -B "${SCRIPT_DIR}/build" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$APP_DIR"
cmake --build "${SCRIPT_DIR}/build" --parallel
cmake --install "${SCRIPT_DIR}/build"

cat > "$APP_ENTRY" <<EOF
[Desktop Entry]
Type=Application
Name=Obsidian Settings
Comment=Configure Obsidian appearance and desktop settings
Exec="${APP_DIR}/bin/obsidian-settings"
Icon=preferences-desktop-theme
Terminal=false
Categories=Settings;DesktopSettings;
EOF
chmod 0644 "$APP_ENTRY"

cat > "$WALLPAPER_ENTRY" <<EOF
[Desktop Entry]
Type=Application
Name=Obsidian Wallpapers
Comment=Browse and apply desktop wallpapers
Exec="${APP_DIR}/bin/obsidian-settings" --wallpaper-picker
Icon=preferences-desktop-wallpaper
Terminal=false
Categories=Settings;DesktopSettings;
EOF
chmod 0644 "$WALLPAPER_ENTRY"

unit_dir="${HOME}/.config/systemd/user"
mkdir -p -- "$unit_dir"
cat > "${unit_dir}/obsidian-shortcutd.service" <<EOF
[Unit]
Description=Obsidian global shortcut handler
After=graphical-session.target

[Service]
ExecStart="${APP_DIR}/bin/obsidian-settings" --daemon
Restart=on-failure
RestartSec=3

[Install]
WantedBy=default.target
EOF

if command -v systemctl >/dev/null 2>&1 \
    && systemctl --user daemon-reload \
    && systemctl --user enable --now obsidian-shortcutd.service; then
    printf 'Obsidian global shortcut handler enabled for this user.\n'
else
    printf 'Could not start the user service now. Enable it from a KDE terminal with:\n'
    printf '  systemctl --user daemon-reload && systemctl --user enable --now obsidian-shortcutd.service\n'
fi

printf '\nDone. Launch Obsidian Settings from the application menu.\n'
printf 'Global shortcuts run through the per-user obsidian-shortcutd.service.\n'
