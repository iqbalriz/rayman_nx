/* dcr_config.c -- Rayman Jungle Run's settings: config.ini's options, on the
 * runtime's INI engine (runtime/source/rt_cfg.c).
 *
 * Written whole on the first start, the options a newer build adds appended at
 * the end. Booleans take true/false, yes/no, on/off, 1/0. Read once at
 * start-up: changes apply the next time the game starts. MIT.
 */
#include <switch.h>

#include "dcr_config.h"
#include "rt_cfg.h"
#include "util.h"

const char *const ro_language_codes[] = {"en", "fr", "de", "es", "it", "ja", "pt", "zh"};

static DcrConfig g_cfg = {
    .ui = RO_UI_AUTO,
    .language = 0,
    .gles = 0,
    .touch = 1,
    .stick_dpad = 1,
    .menu_keys = 1,
    .pointer_cursor = 1,
    .pointer_tap = RO_TAP_ZR,
    .volume = 100,
    .res_w = 1280,
    .res_h = 720,
    .boost = 1,
};

const DcrConfig *dcr_config(void) { return &g_cfg; }

static const CfgOpt k_opts[] = {
    {"game", "interface", "auto",
     "Which way the game is played. touch: the phone's interface (handheld mode's\n"
     "# touch screen). gamepad: the Android TV / game-box interface, driven by a\n"
     "# controller. auto: touch when handheld, gamepad when docked.",
     CFG_CHOICE, "auto,touch,gamepad", &g_cfg.ui},
    {"game", "language", "en",
     "The game's language: en, fr, de, es, it, ja, pt or zh.", CFG_CHOICE,
     "en,fr,de,es,it,ja,pt,zh", &g_cfg.language},
    {"graphics", "gles_version", "1",
     "The OpenGL ES version the game is given: 1 (fixed function) or 2 (shaders).\n"
     "# The game's renderer on this port is OpenGL ES 1: with 2, every drawing call\n"
     "# is refused. If the driver cannot make the one asked for, the other is tried.",
     CFG_CHOICE, "2,1", &g_cfg.gles},
    {"controls", "touch_screen", "true",
     "The touch screen plays the game (handheld mode).", CFG_BOOL, NULL, &g_cfg.touch},
    {"controls", "left_stick_as_dpad", "true",
     "The left stick also works as the D-pad in the menus.", CFG_BOOL, NULL, &g_cfg.stick_dpad},
    {"controls", "menu_keys", "true",
     "The D-pad (and the left stick) left and right, R and L reach the menus the\n"
     "# way a TV remote's keys do on Android: they move through the level map.",
     CFG_BOOL, NULL, &g_cfg.menu_keys},
    {"controls", "pointer_cursor", "true",
     "A cursor for the menus that only take a finger: the right stick moves it (it\n"
     "# shows while you move it and fades after 3 seconds), ZR taps (see pointer_tap).",
     CFG_BOOL, NULL, &g_cfg.pointer_cursor},
    {"controls", "pointer_tap", "zr",
     "The button that taps with the cursor: zr, a or both. zr leaves A to the game\n"
     "# (it picks with A in some menus). With a, the cursor keeps A while it shows\n"
     "# (3 seconds after the right stick moved), so the game's own A does not work then.",
     CFG_CHOICE, "a,zr,both", &g_cfg.pointer_tap},
    CFG_ROW_RESOLUTION("auto",
                       "Rendering resolution: 720, 1080 or auto (1080 if docked when the game\n"
                       "# starts). The Switch scales the result to the screen."),
    CFG_ROW_BOOST("CPU at 1785 MHz while the game starts (until its first picture).", &g_cfg.boost),
    CFG_ROW_GL_SELFTEST(&g_cfg.gl_selftest),
    CFG_ROW_BOOT_LOG("Show the start-up log on screen at every launch. Off: the log appears only\n"
                     "# while something is being set up (first launch, a new APK or NRO).",
                     &g_cfg.boot_log),
    CFG_ROW_LOG_JNI("Write every Java method the game calls to debug.log (for bug reports).", &g_cfg.log_jni),
    {"debug", "log_input", "false",
     "Write the game's controller reads and every touch it receives to debug.log\n"
     "# (for bug reports).",
     CFG_BOOL, NULL, &g_cfg.log_input},
    {"debug", "log_file_access", "false",
     "Write every opening of the game's data files (textures, sounds, scenes) to\n"
     "# debug.log, with the SD card path it became (for bug reports; makes the log long).",
     CFG_BOOL, NULL, &g_cfg.log_files},
    {"debug", "gl_trace", "false",
     "Check every OpenGL call for errors and write the failing ones (their name and\n"
     "# arguments), and the shaders that do not build, to debug.log (for bug reports;\n"
     "# it slows the game a little).",
     CFG_BOOL, NULL, &g_cfg.gl_trace},
    /* [config] version = 1: the engine's row, last (CfgTable.version) */
};

static void apply(void) {
  const RtConfig *rt = rt_config(); /* the resolution: rt_cfg.c sets the window to it */
  g_cfg.res_w = rt->res_w;
  g_cfg.res_h = rt->res_h;
  const int docked = appletGetOperationMode() == AppletOperationMode_Console;
  debugPrintf("[config] %dx%d (%s, %s); interface %s, language %s, OpenGL ES %d, touch %s, CPU boost %s\n",
              g_cfg.res_w, g_cfg.res_h, rt_config_get("display", "resolution"), docked ? "docked" : "handheld",
              rt_config_get("game", "interface"), ro_language_codes[g_cfg.language],
              g_cfg.gles ? 1 : 2, g_cfg.touch ? "on" : "off", g_cfg.boost ? "on" : "off");
}

/* version 2: gles_version's default changed from 2 to 1;
 * version 3: gl_trace (a bring-up tool) is off by default */
static const CfgMigrate k_migrate[] = {
    {"graphics", "gles_version", "2", "1", 2},
    {"debug", "gl_trace", "true", "false", 3},
    /* version 4: the cursor taps with ZR; A stays the game's */
    {"controls", "pointer_tap", "a", "zr", 4},
};

static const CfgTable k_table = {
    .opts = k_opts,
    .nopts = CFG_COUNT(k_opts),
    .migrate = k_migrate,
    .nmigrate = CFG_COUNT(k_migrate),
    .version = 4,
    .apply = apply,
};

void dcr_config_load(void) { rt_config_load(&k_table); }
