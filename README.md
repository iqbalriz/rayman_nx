# rayman_nx

**Rayman Jungle Run on Nintendo Switch**

An unofficial Nintendo Switch wrapper for the Android version of
**Rayman Jungle Run** (Ubisoft Pasta Games), by Iqbalriz.

Version 0.1.0 · 32-bit (armeabi-v7a) · OpenGL ES 1 · 60 fps on hardware

---

## About

`rayman_nx` runs the 32-bit Android build of **Rayman Jungle Run 2.4.3**
(`com.pastagames.ro1mobile`) on a Nintendo Switch with custom firmware, as a
32-bit process. It loads the game's own libraries, `libRO1Mobile.so` (the
whole game) and `libfmodex.so` (FMOD Ex, played through OpenSL ES on audout),
and does what the game's Java side did: the OpenGL ES 1 context, the activity
lifecycle, touch and controller input, and the data folders.

**Nothing of the game is in this repository or in the release.** You need your
own copy: the APK and the game's data folder from an Android device.

It is built on the [android32](https://github.com/aks796/android32) runtime
(loader, bionic, JNI, audio, launcher) by aks796, with
[libnx32](https://github.com/aks796/libnx32) and
[mesa32](https://github.com/aks796/mesa32). See [NOTES.md](NOTES.md) for how
the port works and what this particular game needed.

## What you need

* A Switch with Atmosphere and [sphaira](https://github.com/ITotalJustice/sphaira).
* The **32-bit APK** of Rayman Jungle Run 2.4.3 (`lib/armeabi-v7a/libRO1Mobile.so`
  inside). Any file name ending in `.apk`.
* The game's data folder from your phone:
  `Android/data/com.pastagames.ro1mobile/files` (it holds `actor`, `gfx`,
  `lvl`, `pasta`, `sfx`, `shaders`, `texts`, `en.lproj`, ...).

## Install

```
/switch/rayman_nx/rayman_nx.nro            the launcher (from the release)
/switch/rayman_nx/<any name>.apk           your APK
/switch/rayman_nx/external/files/          the CONTENTS of your phone's "files" folder
                                           (so /switch/rayman_nx/external/files/lvl, .../pasta, ...)
```

1. Copy the files above to the SD card.
2. In sphaira: Homebrew → Rayman Jungle Run → **Install Forwarder**.
3. Start the new icon on the HOME menu. The launcher installs the 32-bit game
   program for that icon and restarts it. The first start unpacks the libraries
   from the APK (a few seconds, with a progress bar; again only when the APK
   changes).

Saves are kept in `external/files/saves/`.

## Controls

| | |
| --- | --- |
| Touch screen (handheld) | as on the phone |
| Controller | the game reads it itself (jump, punch, ...). `+` is Menu, `-` is Back |
| D-pad / left stick, left and right | move through the menus (level map) |
| R / L | the select / action keys of an Android TV remote |
| **Cursor** | move the **right stick**: a cursor shows (it fades 3 s after you stop). **ZR** taps. For the menus that only take a finger |

## config.ini

Written on the first start in `/switch/rayman_nx/config.ini`; changes apply the
next time the game starts.

| Section | Option | |
| --- | --- | --- |
| `[game]` | `interface` | `auto` (touch when handheld, gamepad interface when docked), `touch`, `gamepad` |
| | `language` | `en fr de es it ja pt zh` |
| `[graphics]` | `gles_version` | `1` (the game's renderer; default) or `2` |
| `[controls]` | `touch_screen`, `left_stick_as_dpad`, `menu_keys`, `pointer_cursor` | on/off |
| | `pointer_tap` | `zr` (default), `a`, `both` |
| `[display]` | `resolution` | `auto`, `720`, `1080` |
| `[performance]` | `boost_cpu_when_loading` | CPU at 1785 MHz until the first picture |
| `[debug]` | `log_input`, `log_file_access`, `gl_trace`, `log_java_calls`, `gl_selftest`, `boot_log_on_screen` | for bug reports |

## Not in this port

The store and purchases, leaderboards and achievements, social networks, push
notifications, the "More Rayman" button and the gallery wallpaper are switched
off: the Java answers as a phone with no account and no network.

## Build

You need Docker. **On Windows, run `git config --global core.autocrlf false`
before cloning** (or clone inside WSL): with `autocrlf=true` the runtime's
scripts and Makefiles become CRLF and do not run in the Linux container. The
first two lines are what the android32 runtime expects, next to this folder:

```
git clone --recurse-submodules <this repository>
git clone https://github.com/aks796/libnx32      # then run its ./build.sh
tools/get_portlibs.ps1                           # mesa32's lib/ and include/ into portlibs32/
```

Then build the wrapper (the toolchain image is
`ghcr.io/vita2hos/devcontainer/vita2hos`) and the launcher:

```
./build.sh                  # Windows PowerShell: .\build.ps1   -> rayman_nx.nsp
launcher/build.sh           # devkitpro/devkita64               -> launcher/rayman_nx.nro
```

`tools/` also has the helpers used to read the game's `classes.dex` and
libraries (see NOTES.md).

## Credits

* **Rayman Jungle Run**: Ubisoft Pasta Games. Rayman is a trademark of Ubisoft.
* **android32**, **libnx32**, **mesa32**: aks796; the loader derives from the
  work of Andy Nguyen (TheOfficialFloW) and fgsfds, with reference to vita2hos
  by xerpi.
* libnx by the switchbrew authors; Mesa and nouveau.
* The port: Iqbalriz.

## License

MIT for this repository's own files, see [LICENSE](LICENSE). The runtime,
libnx32 and mesa32 keep their own licenses. This project is not affiliated
with or endorsed by Ubisoft or Nintendo.

---

## Bahasa Indonesia (ringkas)

Wrapper tidak resmi untuk menjalankan **Rayman Jungle Run** (APK Android
32-bit, versi 2.4.3) di Nintendo Switch ber-CFW. Repositori ini **tidak
berisi game apa pun**: kamu perlu APK dan folder data game dari salinan milikmu
sendiri.

1. Salin `rayman_nx.nro` dan APK ke `/switch/rayman_nx/`.
2. Salin ISI folder `Android/data/com.pastagames.ro1mobile/files` dari HP ke
   `/switch/rayman_nx/external/files/`.
3. Di sphaira: *Install Forwarder*, lalu jalankan ikonnya dari menu HOME.

Kursor virtual untuk menu yang hanya mendukung sentuhan: gerakkan stick kanan,
tekan ZR untuk mengetuk. Opsi lengkap ada di tabel `config.ini` di atas.
