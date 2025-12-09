# Moonlight PC

[Moonlight PC](https://moonlight-stream.org) is an open source PC client for NVIDIA GameStream and [Sunshine](https://github.com/LizardByte/Sunshine).

Moonlight also has mobile versions for [Android](https://github.com/moonlight-stream/moonlight-android) and [iOS](https://github.com/moonlight-stream/moonlight-ios).

You can follow development on our [Discord server](https://moonlight-stream.org/discord) and help translate Moonlight into your language on [Weblate](https://hosted.weblate.org/projects/moonlight/moonlight-qt/).

 [![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/glj5cxqwy2w3bglv/branch/master?svg=true)](https://ci.appveyor.io/project/cgutman/moonlight-qt/branch/master)
 [![Downloads](https://img.shields.io/github/downloads/moonlight-stream/moonlight-qt/total)](https://github.com/moonlight-stream/moonlight-qt/releases)
 [![Translation Status](https://hosted.weblate.org/widgets/moonlight/-/moonlight-qt/svg-badge.svg)](https://hosted.weblate.org/projects/moonlight/moonlight-qt/)

## Fork Features

This fork adds enhanced remote desktop functionality and custom key remapping:

- **Remote display cycling**: Switch between host displays in a loop using hotkeys
- **Custom hotkey remapping**: Remap Moonlight and Sunshine hotkeys for better remote desktop workflow
- **Sunshine compatibility**: For Sunshine hosts, Moonlight combinations are automatically converted to expected Sunshine equivalents before sending
- **Display count persistence**: Host display count is stored in the paired computer data for proper cycling functionality
- **SDL-based key mapping**: Uses SDL scancode encoding for keymap.xml storage/reading with SDL_scancode<=>string translators per SDL specifications
- **Dual-array architecture**: 
  - **Intermediate array**: Token-based string array matching XML hierarchy, used in settings UI
  - **Runtime array**: Binary encoding array created at session start and passed to SdlInputHandler for on-the-fly key substitution via `applyUserKeyCombo(event)`
- **Game mode isolation**: Custom remapping is automatically disabled when switching to game mouse capture mode, ensuring no impact on gaming performance
- **Auto-detect UI**: Settings menu includes key capture functionality with SDL scancode/modifier substitution (currently tested on Windows with some key detection issues, but manual input/editing works using SDL_scancode and SDL_Keymod definitions)
- **Cross-platform support**: Keyboard mapping tables and translators in `keyboardmapping.cpp` (AI-generated, requires testing and debugging across platforms)

## Implementation Details

### **Core Components Added**
- **Configuration Manager**: `streaming/input/userkeycombos.(h|cpp)` - Core logic for key mapping management
- **QML Bridge**: `gui/userkeycombobridge.(h|cpp)` - Integration layer for QML UI
- **SDL Conversion Tools**: `streaming/input/keyboardmapping.(h|cpp)` - SDL scancode ↔ native scancode translators
- **UI Extension**: `app/gui/SettingsView.qml` - Complete interface for editing/detecting/saving keymap.xml

### **Input Handling Changes**
- **`streaming/input/input.cpp` & `keyboard.cpp`**: 
  - User combo application via `applyUserKeyCombo(event)`
  - Synthetic event playback for remapped keys
  - New special key combinations:
    - `Ctrl+Alt+Shift+U` - Toggle custom combos
    - Cyclic display switching
    - Pointer region lock controls
- **StreamingPreferences**: Added `pointerRegionLock` and `userCombosEnabled` options with persistence

### **Display Management**
- **`app/backend/nvcomputer.(h|cpp)`**: Added `clientDisplayCount` persistence in paired computer data
- **Session Integration**: Display count updates and cyclic switching mechanisms

### **Build & Integration Changes**
- **`app/app.pro`**: Added new source files to build configuration
- **`app/main.cpp`**: QML type registration for `UserKeyComboBridge`
- **Integration Files**: `app/gui/appmodel.cpp`, `app/gui/computermodel.cpp`, `app/cli/startstream.cpp`, `app/streaming/session.cpp/.h` - Parameter/object integration

### **Complete File List**
```
README.md
app/app.pro
app/backend/nvcomputer.cpp
app/backend/nvcomputer.h
app/cli/startstream.cpp
app/gui/SettingsView.qml
app/gui/appmodel.cpp
app/gui/computermodel.cpp
app/gui/userkeycombobridge.cpp (new)
app/gui/userkeycombobridge.h (new)
app/main.cpp
app/settings/streamingpreferences.cpp
app/settings/streamingpreferences.h
app/streaming/input/input.cpp
app/streaming/input/input.h
app/streaming/input/keyboard.cpp
app/streaming/input/keyboardmapping.cpp (new)
app/streaming/input/keyboardmapping.h (new)
app/streaming/input/userkeycombos.cpp (new)
app/streaming/input/userkeycombos.h (new)
app/streaming/session.cpp
app/streaming/session.h
```

## Features
 - Hardware accelerated video decoding on Windows, Mac, and Linux
 - H.264, HEVC, and AV1 codec support (AV1 requires Sunshine and a supported host GPU)
 - YUV 4:4:4 support (Sunshine only)
 - HDR streaming support
 - 7.1 surround sound audio support
 - 10-point multitouch support (Sunshine only)
 - Gamepad support with force feedback and motion controls for up to 16 players
 - Support for both pointer capture (for games) and direct mouse control (for remote desktop)
 - Support for passing system-wide keyboard shortcuts like Alt+Tab to the host
 
## Downloads
- [Windows, macOS, and Steam Link](https://github.com/moonlight-stream/moonlight-qt/releases)
- [Snap (for Ubuntu-based Linux distros)](https://snapcraft.io/moonlight)
- [Flatpak (for other Linux distros)](https://flathub.org/apps/details/com.moonlight_stream.Moonlight)
- [AppImage](https://github.com/moonlight-stream/moonlight-qt/releases)
- [Raspberry Pi 4 and 5](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-Raspberry-Pi-4)
- [Generic ARM 32-bit and 64-bit Debian packages](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-ARM%E2%80%90based-Single-Board-Computers) (not for Raspberry Pi)
- [Experimental RISC-V Debian packages](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-RISC%E2%80%90V-Single-Board-Computers)
- [NVIDIA Jetson and Nintendo Switch (Ubuntu L4T)](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-Linux4Tegra-(L4T)-Ubuntu)

#### Special Thanks

[![Hosted By: Cloudsmith](https://img.shields.io/badge/OSS%20hosting%20by-cloudsmith-blue?logo=cloudsmith&style=flat-square)](https://cloudsmith.com)

Hosting for Moonlight's Debian and L4T package repositories is graciously provided for free by [Cloudsmith](https://cloudsmith.com).

## Building

### Windows Build Requirements
* Qt 6.7 SDK or later (earlier versions may work but are not officially supported)
* [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (Community edition is fine)
* Select **MSVC** option during Qt installation. MinGW is not supported.
* [7-Zip](https://www.7-zip.org/) (only if building installers for non-development PCs)
* Graphics Tools (only if running debug builds)
  * Install "Graphics Tools" in the Optional Features page of the Windows Settings app.
  * Alternatively, run `dism /online /add-capability /capabilityname:Tools.Graphics.DirectX~~~~0.0.1.0` and reboot.

### macOS Build Requirements
* Qt 6.7 SDK or later (earlier versions may work but are not officially supported)
* Xcode 14 or later (earlier versions may work but are not officially supported)
* [create-dmg](https://github.com/sindresorhus/create-dmg) (only if building DMGs for use on non-development Macs)

### Linux/Unix Build Requirements
* Qt 6 is recommended, but Qt 5.12 or later is also supported (replace `qmake6` with `qmake` when using Qt 5).
* GCC or Clang
* FFmpeg 4.0 or later
* Install the required packages:
  * Debian/Ubuntu:
    * Base Requirements: `libegl1-mesa-dev libgl1-mesa-dev libopus-dev libsdl2-dev libsdl2-ttf-dev libssl-dev libavcodec-dev libavformat-dev libswscale-dev libva-dev libvdpau-dev libxkbcommon-dev wayland-protocols libdrm-dev`
    * Qt 6 (Recommended): `qt6-base-dev qt6-declarative-dev libqt6svg6-dev qml6-module-qtquick-controls qml6-module-qtquick-templates qml6-module-qtquick-layouts qml6-module-qtqml-workerscript qml6-module-qtquick-window qml6-module-qtquick`
    * Qt 5: `qtbase5-dev qt5-qmake qtdeclarative5-dev qtquickcontrols2-5-dev qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qtquick-window2 qml-module-qtquick2 qtwayland5`
  * RedHat/Fedora (RPM Fusion repo required):
    * Base Requirements: `openssl-devel SDL2-devel SDL2_ttf-devel ffmpeg-devel libva-devel libvdpau-devel opus-devel pulseaudio-libs-devel alsa-lib-devel libdrm-devel`
    * Qt 6 (Recommended): `qt6-qtsvg-devel qt6-qtdeclarative-devel`
    * Qt 5: `qt5-qtsvg-devel qt5-qtquickcontrols2-devel`
* Building the Vulkan renderer requires a `libplacebo-dev`/`libplacebo-devel` version of at least v7.349.0 and FFmpeg 6.1 or later.

### Steam Link Build Requirements
* [Steam Link SDK](https://github.com/ValveSoftware/steamlink-sdk) cloned on your build system
* STEAMLINK_SDK_PATH environment variable set to the Steam Link SDK path

**Steam Link Hardware Limitations**  
Moonlight builds for Steam Link are subject to hardware limitations of the Steam Link device:
* Maximum resolution: **1080p (1920x1080)**
* Maximum framerate: **60 FPS**
* Maximum video bitrate: **40 Mbps**
* **HDR streaming is not supported** on the original hardware

### Build Setup Steps
1. Install the latest Qt SDK (and optionally, the Qt Creator IDE) from https://www.qt.io/download
    * You can install Qt via Homebrew on macOS, but you will need to use `brew install qt --with-debug` to be able to create debug builds of Moonlight.
    * You may also use your Linux distro's package manager for the Qt SDK as long as the packages are Qt 5.12 or later.
    * This step is not required for building on Steam Link, because the Steam Link SDK includes Qt 5.14.
2. Run `git submodule update --init --recursive` from within `moonlight-qt/`
3. Open the project in Qt Creator or build from qmake on the command line.
    * To build a binary for use on non-development machines, use the scripts in the `scripts` folder.
        * For Windows builds, use `scripts\build-arch.bat` and `scripts\generate-bundle.bat`. Execute these scripts from the root of the repository within a Qt command prompt. Ensure  7-Zip binary directory is on your `%PATH%`.
        * For macOS builds, use `scripts/generate-dmg.sh`. Execute this script from the root of the repository and ensure Qt's `bin` folder is in your `$PATH`.
        * For Steam Link builds, run `scripts/build-steamlink-app.sh` from the root of the repository.
    * To build from the command line for development use on macOS or Linux, run `qmake6 moonlight-qt.pro` then `make debug` or `make release`
    * To create an embedded build for a single-purpose device, use `qmake6 "CONFIG+=embedded" moonlight-qt.pro` and build normally.
        * This build will lack windowed mode, Discord/Help links, and other features that don't make sense on an embedded device.
        * For platforms with poor GPU performance, add `"CONFIG+=gpuslow"` to prefer direct KMSDRM rendering over GL/Vulkan renderers. Direct KMSDRM rendering can use dedicated YUV/RGB conversion and scaling hardware rather than slower GPU shaders for these operations.

## Contribute
1. Fork us
2. Write code
3. Send Pull Requests

Check out our [website](https://moonlight-stream.org) for project links and information.
