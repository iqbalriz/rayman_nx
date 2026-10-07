/* dcr_config.h -- the user's settings, from <game folder>/config.ini (dcr_config.c). */
#ifndef DCR_USER_CONFIG_H
#define DCR_USER_CONFIG_H

enum { RO_UI_AUTO, RO_UI_TOUCH, RO_UI_GAMEPAD };
enum { RO_TAP_A, RO_TAP_ZR, RO_TAP_BOTH };
enum { RO_PLUS_OFF, RO_PLUS_MENU, RO_PLUS_GAME };

typedef struct {
  int ui;          /* [game] interface: auto, touch, gamepad */
  int language;    /* [game] language: index into ro_language_codes[] */
  int gles;        /* [graphics] gles_version: 0 = 2, 1 = 1 */
  int touch;       /* [controls] touch_screen */
  int stick_dpad;  /* [controls] left_stick_as_dpad */
  int menu_keys;   /* [controls] menu_keys */
  int pointer_cursor; /* [controls] pointer_cursor */
  int pointer_tap;    /* [controls] pointer_tap: RO_TAP_* */
  int plus_button;    /* [controls] plus_button: RO_PLUS_* */
  int log_files;      /* [debug] log_file_access */
  int volume;      /* [sound] volume 0-100 (reserved) */
  int res_w, res_h;/* [display] resolution */
  int boost;       /* [performance] boost_cpu_when_loading */
  int gl_selftest; /* [debug] gl_selftest */
  int boot_log;    /* [debug] boot_log_on_screen */
  int log_jni;     /* [debug] log_java_calls */
  int log_input;   /* [debug] log_input */
  int gl_trace;    /* [debug] gl_trace */
} DcrConfig;

/* The language codes Locale.getLanguage() gives, in the order of the option. */
extern const char *const ro_language_codes[];

/* Read config.ini (writing it with the defaults, or adding missing options,
 * first). Early in main(); the defaults hold until then. */
void dcr_config_load(void);
const DcrConfig *dcr_config(void);

#endif
