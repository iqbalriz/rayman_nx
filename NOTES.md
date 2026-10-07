# Notes: rayman_nx

Technical notes from porting Rayman Jungle Run 2.4.3 (armeabi-v7a) to the
Switch as a 32-bit (AArch32) process on the
[android32](https://github.com/aks796/android32) runtime. They cover what this
game needed and how each of those things was found, for anyone porting another
Android game.

Everything here was read out of the APK (`classes.dex`, `libRO1Mobile.so`,
`libfmodex.so`) and out of `debug.log` runs on hardware. Nothing was
decompiled to source; the tools used are in `tools/`.

## The game

| | |
| --- | --- |
| Package | `com.pastagames.ro1mobile`, 2.4.3, versionCode 197 |
| Native libraries | `libRO1Mobile.so` (6 MB, the whole game: Ubisoft Pasta Games' engine, gnustl and zlib linked in), `libfmodex.so` (FMOD Ex) |
| DT_NEEDED | liblog, libz, libdl, libGLESv1_CM, libGLESv2, libandroid, libstdc++, libm, libc: all served by the runtime's shims |
| JNI | no `JNI_OnLoad`, no `RegisterNatives`: the natives are exported by name (`Java_com_pastagames_android_GameActivity_native*`, about 50) and bound lazily by the Java; `ro_loader.c` looks them up once |
| Java the engine calls | `GameActivity` methods only (`readAsset`, `assetExists`, `isGamePadKeyPressed`, ...); `ro_java.c` |
| Imports | 306 symbols, 99 of them `gl*` (left to the GL layer), 207 through `tools/imports.cfg` and the generated `source/imports.c` |

## What the Java did, and where it is now

Read as the order of calls in `classes.dex` (`tools/dexcalls.py`):

1. `onCreate`: `nativeSetLanguage`; the directories; the device flags
   (`nativeSetKindleFireMode`, `...NOOKMode`, `...AmazonStreamingBoxMode`,
   `...GoogleStreamingBoxMode`); **`GameBehaviourLogger.createGameBehaviourLogger`**;
   the `nativeEnable*` switches.
2. `onSurfaceChanged`: `nativeSetWidth`, `nativeSetHeight`.
3. `onDrawFrame`, the first time: `nativeCreate()`, `nativeStart()`; then
   `nativeRun()` once a frame (the Java sleeps to the frame time; here the swap
   interval does).

That is `ro_game.c`. The game's Java does not run: there is no Java VM.

## Things that were not obvious

**1. The asset manager must not be handed over.** `nativeSetAssetManager` makes
the engine's `AndroidFileMgr::fileExists` look in the APK *only*: with a manager
it calls `AAssetManager_open` and answers "no" if that fails, without trying the
disk. Without one it uses `access()`. Every texture load asks `fileExists`
first, so with the manager the textures in the data folder all "do not exist".
This build keeps its data on external storage (`resourcesInAsset` false), so
the Java does not call it. (`ro_game.c`, `on_create`.)

**2. One directory for everything.** Browse, root, persistence and write-temp
directories are all `/storage/emulated/0/Android/data/<pkg>/files`, which the
runtime maps to `<root>/external/files`. `saves/` is under it (`saves/profile`,
`saves/Transactions`, `saves/Achievements`), as on a phone, so it must exist
before the game writes. The engine tries a relative path first ("can not open
file ..." in the log), then the browse directory: those messages are not errors.

**3. The statistics logger's singleton.** `GameBehaviourLogger::getSingleton()`
is NULL until the Java's `FlurryWrapper` constructor calls
`nativeSetupFlurry()`. The first start has no saved volume, and
`OptionsMenu::loadVolumeValues` then logs an event through the singleton: a
NULL call, a crash in `MainGameState::createMenu`. `ro_setup_flurry()` calls it
with the `FlurryWrapper` singleton; the JNI table answers every call on that
class with nothing.

**4. Two modules that import from each other.** `libRO1Mobile.so` imports
`FMOD_*` from `libfmodex.so`; `libfmodex.so` imports `operator delete` and
`__cxa_pure_virtual` from the libstdc++ inside `libRO1Mobile.so`. Both are
staged and relocated first and resolved afterwards (`ro_load_engine`).

**5. The renderer is OpenGL ES 1.** The game links both `libGLESv1_CM` and
`libGLESv2`, and `AndroidGraphicFactory::SupportsOGL2` (it reads `GL_VERSION`:
the character before the first `.` must be above `1`) does not decide what the
game draws with: the menus and scenes go through `glMatrixMode`,
`glLoadMatrixf`, `glVertexPointer`, `glTexCoordPointer`,
`glEnableClientState`, `glClientActiveTexture`. In an ES 2 context every one of
them is refused ("unsupported function called"), nothing is drawn but the clear
colour (a pale blue screen, with the sound playing). Mesa 20.1 from mesa32
makes an ES 1 context and runs them (60 fps); `[graphics] gles_version` selects
it, with a fall-back to the other version.

How it was found: `ro_gltrace.c` wraps every `gl*` function through the
runtime's `port_gl_wrap()` callback, calls `glGetError()` after each and logs
the name and arguments of the ones that fail (`[debug] gl_trace`). "Unsupported
function called" names nothing; this does.

**6. Audio.** `libfmodex.so` has an OpenSL ES output (it `dlopen`s
`libOpenSLES.so`), which the runtime provides on audout (`RT_OPENSLES 1`).
Nothing else was needed.

## Input

* **Touch**: `nativeTouchScrStart(x, y)`, `nativeTouchScrMove(previous x,
  previous y, x, y)`, `nativeTouchScrEnd(x, y)`, per finger; the 1280x720 touch
  screen mapped onto the rendering size.
* **Pad**: the engine *polls* it. In the game-box ("Google streaming box")
  interface it calls `isGamePadConnected()`, `isGamePadKeyPressed(int, int)`
  and `getGamePadAxisValues(int, int)`; the last one the Java answers with
  `nativeSetPadAxisValues(int, float, float)`. The key argument is read as an
  Android key code, which works for play; the first argument and the axis
  argument are not fully known (`[debug] log_input` logs every key polled).
* **Keys**: the Java's `onKeyDown` forwards D-pad left (21) and right (22) as
  `nativePressKey(0)` and `(1)` (with `nativeReleaseKey`), D-pad centre (23) or
  R1 (103) as `(2)`, X (99) or L1 (102) as `(3)`; Back as `nativePressBack()`,
  Menu as `nativePressMenu()`. `pad_menu_keys()` does the same for the pad.
* **Cursor**: many menus (level map, end-of-level) take nothing but a finger.
  The right stick moves a cursor, ZR taps (`TouchScr*`). It is drawn over the
  frame with a few ES 1 calls that put every piece of state back as found
  (`pointer_draw`). `pointer_tap = a` was tried first and dropped: the game
  reads A itself in some menus.

## The data folder

The game's data is the phone's `files` folder, copied to
`<root>/external/files`. The APK's own `assets/` (`config.bin`, the
`Localizable.strings`) are read with the NDK's `AAssetManager_*`, which the
runtime does not provide: `ro_assets.c` implements them over the zip directory
(miniz), together with the few libc shims the table needed (`adler32`,
`chown`, `wcscpy`, `wcsncpy`, `wcstombs`).

## Title ID

`0x010093A06B84BFD6`, picked at random, in the application range and not like
an official base title or update (low 12 bits not `000`/`800`). The runtime's
documentation suggests `0x01000000000010xx`, one per port; that is a
convention, not a requirement. If you fork, pick your own.

## Tools (`tools/`)

| | |
| --- | --- |
| `dexdump.py <dex> [class...]` | classes, fields and methods (with `native` and `static`) of a dex; no dependencies |
| `dexcalls.py <dex> <class> <method...>` | per method, in bytecode order: the invokes, const-strings, small constants, field accesses. How the call order above was read |
| `mkneeded.py <libdir> <out> <so...>` | `tools/imports_needed.txt` without `pyelftools` |
| `gen_gwslots.py <out>` | writes `source/ro_gltrace_slots.inc` (256 GL wrapper slots) |
| `get_portlibs.ps1`, `get_portlibs.sh` | download mesa32's release into `portlibs32/` |

To read the game's Java for yourself: unzip the APK, then
`python tools/dexdump.py classes.dex pastagames/android/GameActivity`.

## Known limits

* Tested by the author on one console. Touch, controller play, the cursor,
  saves, HOME/sleep and sound were checked there; nothing else was.
* Store, leaderboards, achievements, social, push, gallery wallpaper: off.
* The engine's pad polling arguments are only partly known (see Input).
