<div align="center">

<img src="launcher/icon.jpg" alt="Rayman Jungle Run" width="160">

# rayman_nx

**Rayman Jungle Run for Nintendo Switch**

An unofficial Nintendo Switch native wrapper for the 32-bit Android release of  
**Rayman Jungle Run**.

[![Nintendo Switch](https://img.shields.io/badge/Nintendo_Switch-Homebrew-E60012?style=for-the-badge&logo=nintendoswitch&logoColor=white)](#)
[![Version](https://img.shields.io/badge/Version-0.1.1-4C8BF5?style=for-the-badge)](#)
[![Architecture](https://img.shields.io/badge/AArch32-32--bit_Native-6A1B9A?style=for-the-badge)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge)](LICENSE)

</div>

---

## About

`rayman_nx` is a native wrapper that runs the 32-bit ARM Android build of **Rayman Jungle Run 2.4.3** (`com.pastagames.ro1mobile`) on Nintendo Switch.

It loads the game's original `libRO1Mobile.so` (Ubisoft Pasta Games' own engine) and `libfmodex.so` (FMOD Ex), and recreates what the game's Java side did: the OpenGL ES 1 context, the activity lifecycle, touch and controller input, and the data folders.

Because the Tegra X1 CPU in the Nintendo Switch natively supports 32-bit ARM (AArch32) execution, the game code runs **directly on the hardware at full native speed** (60 fps), with no CPU emulation.

> [!NOTE]
> **No game code or assets are included in this repository or in the release.**  
> Users must supply their own legitimate copy of the game's APK and data folder.

---

## Features

- **Full Native Performance:** Runs natively on the Switch ARM Cortex-A57 CPU in AArch32 mode, at 60 fps.
- **Hardware-Accelerated Graphics:** Uses OpenGL ES 1 (the game's own renderer) via `mesa32` (Nouveau), at 720p or 1080p.
- **Touchscreen & Controller Support:** Full touch controls as on mobile, controller play, and a **virtual cursor** for the menus that only take a finger.
- **Sound:** FMOD Ex played through OpenSL ES on the Switch's audio output.
- **Saves & HOME menu:** Progress is saved on the SD card; HOME and sleep pause and resume the game.
- **Atmosphère & Sphaira Integration:** A dedicated launcher NRO registers a HOME menu forwarder icon.

---

## Requirements

### For Players
- A Nintendo Switch running **Atmosphère** custom firmware.
- The [Sphaira](https://github.com/ITotalJustice/sphaira) homebrew menu (Important: you need to install Sphaira's forwarder, otherwise the port won't work — the system would start it as a 64-bit program instead of 32-bit).
- A copy of **Rayman Jungle Run 2.4.3** for Android, the **32-bit APK** (`lib/armeabi-v7a/libRO1Mobile.so` inside), not modified. The name of the APK does not matter.
- The game's data folder from an Android device, `Android/data/com.pastagames.ro1mobile/files` (about 160 MB; the APK itself does not contain the game's levels, graphics and sounds).

---

## Installation Guide

1. Download the latest release from the [Releases](../../releases) tab:
   - `rayman_nx_0.1.1.zip`
2. Copy the `switch/rayman_nx/` folder from the ZIP to the root of your SD card, so you get:
   ```text
   sdmc:/switch/rayman_nx/
   ```
3. Place your APK and your game data inside that folder:
   - `rayman_nx.nro` is already there (from the ZIP).
   - Copy your `rayman-jungle-run-v2.4.3.apk` into `sdmc:/switch/rayman_nx/` (the name of the apk does not matter).
   - Copy the **contents** of your phone's `files` folder into `sdmc:/switch/rayman_nx/external/files/`.
4. The final folder structure on your SD card must look like:
   ```text
   sdmc:/switch/rayman_nx/
   ├── rayman_nx.nro
   ├── rayman-jungle-run-v2.4.3.apk
   └── external/
       └── files/
           ├── actor/
           ├── gfx/
           ├── lvl/
           ├── pasta/
           ├── sfx/
           ├── shaders/
           ├── en.lproj/
           └── ...
   ```
5. Launch **Sphaira** on your Switch:
   - Navigate to **Homebrew** › **Rayman Jungle Run**.
   - Choose **Install Forwarder**.
   - Return to the Switch HOME Menu and launch the game directly from its icon!

The first start unpacks the game's libraries from the APK (a few seconds, with a progress bar; again only when the APK changes). Your progress is saved in `external/files/saves/`.

---

## Controls

| Input | Action |
| :--- | :--- |
| **Touchscreen** | Direct touch controls (identical to the mobile version) |
| **Controller buttons** | The game's own gamepad controls (jump, punch, ...) |
| **D-Pad / Left Stick (left, right)** | Move through the menus (level map) |
| **L / R** | The select / action keys of an Android TV remote |
| **Right Stick** | Show and move the **cursor** (it fades 3 seconds after you stop) |
| **ZR** | Tap with the cursor (for menus that only take a finger) |
| **+ (Plus)** | Nothing by default (`plus_button` in `config.ini`) |
| **− (Minus)** | Back |

---

## Configuration

`config.ini` is written to `sdmc:/switch/rayman_nx/` on the first start. Changes apply the next time the game starts.

<details>
<summary>All options</summary>

| Section | Option | Values |
| :--- | :--- | :--- |
| `[game]` | `interface` | `auto` (touch when handheld, gamepad interface when docked), `touch`, `gamepad` |
| | `language` | `en` `fr` `de` `es` `it` `ja` `pt` `zh` |
| `[graphics]` | `gles_version` | `1` (the game's renderer, default) or `2` |
| `[controls]` | `touch_screen`, `left_stick_as_dpad`, `menu_keys`, `pointer_cursor` | `true` / `false` |
| | `plus_button` | `off` (default), `menu` (the Android Menu key), `game` (Start, read by the game) |
| | `pointer_tap` | `zr` (default), `a`, `both` |
| `[display]` | `resolution` | `auto`, `720`, `1080` |
| `[performance]` | `boost_cpu_when_loading` | CPU at 1785 MHz until the first picture |
| `[debug]` | `log_input`, `log_file_access`, `gl_trace`, `log_java_calls`, `gl_selftest`, `boot_log_on_screen` | for bug reports |

</details>

If something goes wrong, `debug.log` and `crash.log` are written next to `config.ini`. For a bug report, turn on `log_input` or `log_file_access` and send the log.

---

## Not in This Port

The store and purchases, leaderboards and achievements, social networks, push notifications, the "More Rayman" button and the gallery wallpaper are switched off: the game's Java side answers as a phone with no account and no network.

---

## Building from Source

### Prerequisites
- Linux (Ubuntu / Debian / Linux Mint recommended), or Windows with PowerShell
- **Docker**
- Git

> [!IMPORTANT]
> **On Windows, run `git config --global core.autocrlf false` before cloning.** With `autocrlf=true` the runtime's scripts and Makefiles are checked out with CRLF line endings and do not run in the Linux container.

### Build Instructions

1. **Clone the repository with submodules:**
   ```bash
   git clone --recursive https://github.com/iqbalriz/rayman_nx.git
   cd rayman_nx
   ```

2. **Pull the required Docker toolchains:**
   ```bash
   docker pull ghcr.io/vita2hos/devcontainer/vita2hos
   docker pull devkitpro/devkita64
   ```

3. **Set up `libnx32` and `mesa32`:**
   - Compile `libnx32` next to this folder (the build looks for `../libnx32/prefix`):
     ```bash
     git clone https://github.com/aks796/libnx32.git ../libnx32
     ../libnx32/build.sh
     ```
   - Download the prebuilt `mesa32` release into `portlibs32/`:
     ```bash
     tools/get_portlibs.sh
     ```
     (On Windows: `tools\get_portlibs.ps1`.)

4. **Compile the 32-bit program:**
   ```bash
   ./build.sh
   ```
   (On Windows: `.\build.ps1`.) This makes `rayman_nx.nsp`.

5. **Compile the 64-bit launcher NRO:**
   ```bash
   launcher/build.sh
   ```
   The compiled launcher will be located at `launcher/rayman_nx.nro`.

---

## Credits & Acknowledgments

- **Ubisoft & Pasta Games**: Original creators of Rayman Jungle Run. Rayman is a trademark of Ubisoft.
- **[aks796](https://github.com/aks796)**: For the [`android32`](https://github.com/aks796/android32) runtime, [`libnx32`](https://github.com/aks796/libnx32), [`mesa32`](https://github.com/aks796/mesa32), and the [`flappybirdsfamily_nx`](https://github.com/aks796/flappybirdsfamily_nx) reference port this one started from.
- **Andy Nguyen (TheOfficialFloW) & fgsfds**: Dynamic `.so` loader implementations.
- **xerpi**: For `vita2hos`, pioneer of AArch32 native execution on Switch.
- **Switchbrew**: For `libnx` and tools.
- **ITotalJustice**: For Sphaira.

---

## License

This project is licensed under the [MIT License](LICENSE).
Rayman Jungle Run is a trademark of Ubisoft Entertainment. This project is not affiliated with or endorsed by Ubisoft or Nintendo.
