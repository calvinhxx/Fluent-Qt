#!/usr/bin/env bash
set -euo pipefail

qt_source="${1:-apt}"
if [[ "$qt_source" != "apt" && "$qt_source" != "apt5" && "$qt_source" != "apt6" && "$qt_source" != "aqt" ]]; then
    echo "Unsupported Qt source: $qt_source" >&2
    exit 2
fi

packages=(
    build-essential
    desktop-file-utils
    fonts-noto-color-emoji
    libfontconfig1
    libfreetype6
    libgl1-mesa-dev
    libx11-xcb1
    libxcb-cursor0
    libxcb-icccm4
    libxcb-image0
    libxcb-keysyms1
    libxcb-randr0
    libxcb-render-util0
    libxcb-shape0
    libxcb-sync1
    libxcb-xfixes0
    libxcb-xinerama0
    libxcb-xinput0
    libxcb-xkb1
    libxkbcommon-x11-0
    xvfb
)

if [[ "$qt_source" == "apt5" ]]; then
    packages+=(qtbase5-dev qtbase5-dev-tools)
elif [[ "$qt_source" == "apt" || "$qt_source" == "apt6" ]]; then
    packages+=(qt6-base-dev qt6-base-dev-tools)
fi

sudo apt-get update
sudo apt-get install -y --no-install-recommends "${packages[@]}"
