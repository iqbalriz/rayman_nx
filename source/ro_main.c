/* ro_main.c -- Rayman Jungle Run's part of the boot: the runtime's main()
 * (runtime/source/main.c) does the rest -- the log, config.ini, the NRO
 * self-update, the APK found by what it holds, its package checked -- and
 * calls these.
 *
 * The first launch (runtime/source/dcr_setup.c, from the plan below):
 * libfmodex.so and libRO1Mobile.so out of lib/armeabi-v7a/ and classes.txt
 * (the Java class names its classes*.dex define, which jni_core.c answers
 * FindClass with), made again whenever the APK changes (.setup stamps). The
 * game's own assets (config.bin, the localized strings) are read straight out
 * of the APK at every start (ro_assets.c); its data folder (actor, lvl, gfx,
 * ...) is the player's, in <root>/external/files. The bar, in permille:
 *     0- 200  (the APK found and checked: the runtime's main())
 *   200- 800  the libraries unpacked (by bytes written)
 *   800- 950  the Java class list
 *        1000 the game starts
 * MIT.
 */
#include "config.h"
#include "dcr_path.h"
#include "dcr_setup.h"
#include "error.h"
#include "ro.h"
#include "rt_boot.h"
#include "util.h"

static const char *const k_libs[] = {RO_LIB_FMOD, RO_LIB_GAME};

const RtSetupPlan port_setup_plan = {
    .libs = k_libs,
    .nlibs = 2,
    .libs_what = "Unpacking the game's libraries",
    .apk_requirement = "This port needs Rayman Jungle Run (com.pastagames.ro1mobile) for\n"
                       "32-bit ARM (armeabi-v7a): use the APK of your own copy.",
    .libs_p0 = 200,
    .libs_p1 = 800,
    .classes_p0 = 800,
    .classes_p1 = 950,
};

/* From the APK to the game's first code: the libraries and classes.txt (again
 * when the APK changed), then both modules loaded, relocated, resolved against
 * the shims and mapped as code. */
int port_load(const char *apk) {
  dcr_setup_from_apk(apk);
  if (ro_load_engine() != 0)
    fatal_error("Could not load the game engine from %s/" RO_LIB_GAME ".\n\n"
                "It is unpacked from the APK (lib/armeabi-v7a/) on launch: delete\n" RO_LIB_GAME
                ", " RO_LIB_FMOD " and .setup there to unpack them again. See debug.log.",
                dcr_game_root());
  return 0;
}

/* System.loadLibrary: the libraries' constructors; then GameActivity. */
void port_run(void) {
  ro_run_constructors();
  ro_game_run();
}

/* For the error screens. */
const char *port_apk_help(void) {
  return "Copy the APK of your own Rayman Jungle Run (com.pastagames.ro1mobile,\n"
         "armeabi-v7a) into /switch/" PORT_NAME ". Any file name ending in .apk\n"
         "works. The game's data folder goes in /switch/" PORT_NAME "/external/files.";
}
