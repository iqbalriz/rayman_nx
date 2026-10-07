/* ro_game.c -- plays the part of the game's Java: GameActivity and its
 * GLSurfaceView renderer (com.pastagames.android.gl.GLSurfaceViewWrapper).
 *
 * What the Java does, in order (read out of the APK's classes.dex), and what
 * this file does for it:
 *
 *   onCreate         nativeSetLanguage(Locale language); nativeSetBrowseDirectory
 *                    (<external storage>/Android/data/<pkg>/files, the game's
 *                    data); nativeSetRootDirectory, ...PersistenceRootDirectory
 *                    and ...WriteTempRootDirectory (<files>/saves);
 *                    nativeSetAssetManager; the device flags (Kindle Fire, NOOK,
 *                    Amazon / Google streaming box); the nativeEnable* switches
 *   onSurfaceChanged nativeSetWidth, nativeSetHeight
 *   onDrawFrame      the first time nativeCreate(), nativeStart(); then
 *                    nativeRun() once a frame (the Java sleeps to the frame
 *                    time; here the swap interval does)
 *   touch            nativeTouchScrStart(x, y), nativeTouchScrMove(previous x,
 *                    previous y, x, y), nativeTouchScrEnd(x, y), per finger
 *   gamepad          polled BY the engine: isGamePadConnected(),
 *                    isGamePadKeyPressed(), getGamePadAxisValues() (ro_java.c
 *                    answers with the pad as read here)
 *   onPause/onResume nativePause(); nativeResume() when focus is back
 *
 * On Android the UI thread (input) and the GL thread (frames) run side by side;
 * here one thread does both, in the order a frame sees them: input, then the
 * frame, then the present. The engine is never called from two threads at once.
 *
 * The Switch side: HOME pauses the engine until the game is in focus again;
 * closing the game (HOME > Close, or the game's own quit) stops the engine. MIT.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "config.h"
#include "dcr_boost.h"
#include "dcr_config.h"
#include "dcr_path.h"
#include "dcr_setup.h"
#include "error.h"
#include "gl_layer.h"
#include "jni.h"
#include "ro.h"
#include "rt_applet.h"
#include "rt_pad.h"
#include "rt_window.h"
#include "util.h"
#include "watchdog.h"

#define ENV g_jni_env
#define ACT ((void *)g_activity)

/* The shared EGL layer (gl_mesa.c / gl_null.c), as plain C: both define
 * these with the same register-level signatures. */
typedef int32_t fEGLint;
void *b_eglGetDisplay(void *native);
unsigned b_eglInitialize(void *d, fEGLint *maj, fEGLint *min);
unsigned b_eglChooseConfig(void *d, const fEGLint *attrs, void **cfgs, fEGLint cap, fEGLint *num);
void *b_eglCreateWindowSurface(void *d, void *cfg, void *win, const fEGLint *attrs);
void *b_eglCreateContext(void *d, void *cfg, void *share, const fEGLint *attrs);
unsigned b_eglMakeCurrent(void *d, void *draw, void *read, void *ctx);
unsigned b_eglSwapInterval(void *d, fEGLint interval);
unsigned b_eglSwapBuffers(void *d, void *s);
unsigned b_eglDestroySurface(void *d, void *s);
unsigned b_eglDestroyContext(void *d, void *c);
unsigned b_eglTerminate(void *d);
fEGLint b_eglGetError(void);

#define EGL_NONE 0x3038
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3026
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES_BIT 0x0001
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_OPENGL_ES_API 0x30A0

/* Android key codes (android.view.KeyEvent) the game's pad reads use. */
enum {
  AK_BACK = 4, AK_DPAD_UP = 19, AK_DPAD_DOWN = 20, AK_DPAD_LEFT = 21, AK_DPAD_RIGHT = 22,
  AK_BUTTON_A = 96, AK_BUTTON_B = 97, AK_BUTTON_X = 99, AK_BUTTON_Y = 100, AK_BUTTON_L1 = 102,
  AK_BUTTON_R1 = 103, AK_BUTTON_L2 = 104, AK_BUTTON_R2 = 105, AK_BUTTON_THUMBL = 106,
  AK_BUTTON_THUMBR = 107, AK_BUTTON_START = 108, AK_BUTTON_SELECT = 109,
};

static int g_w, g_h;
static void *g_dpy, *g_surf, *g_ctx;
static int g_engine_up;
static int g_gamepad_ui;       /* the Android TV / game-box interface */

/* For the watchdog: frames presented, and whether frames are expected. */
uint64_t dcr_boot_frames(void) { return dcr_gl_frames(); }

/* A string the engine may keep the UTF pointer of: it stays. */
static JObj *jstr(const char *s) {
  JObj *o = jni_str(s);
  if (o)
    o->immortal = 1;
  return o;
}

/* ------------------------------------------------------------- the pad */
static PadState g_pad, g_pad_hh;
static int g_pad_ready, g_pad_present;
static uint8_t g_keys[256];        /* Android key code -> held */
static int g_plus_down;            /* Plus is held (g_keys[START] is hidden from the game unless plus_button = game) */
static float g_stick[2][2];        /* left, right: x, y (y up) */
static PadState *g_pad_cur;

static void pad_poll(void) {
  if (!g_pad_ready)
    return;
  padUpdate(&g_pad);
  padUpdate(&g_pad_hh);
  u64 buttons = 0;
  float sticks[4] = {0, 0, 0, 0};
  PadState *src = padIsConnected(&g_pad) ? &g_pad : &g_pad_hh;
  g_pad_present = padIsConnected(&g_pad) || padIsConnected(&g_pad_hh);
  g_pad_cur = src;
  if (g_pad_present)
    buttons = rt_pad_read(src, sticks);
  memset(g_keys, 0, sizeof g_keys);
  g_keys[AK_BUTTON_A] = !!(buttons & HidNpadButton_A);
  g_keys[AK_BUTTON_B] = !!(buttons & HidNpadButton_B);
  g_keys[AK_BUTTON_X] = !!(buttons & HidNpadButton_X);
  g_keys[AK_BUTTON_Y] = !!(buttons & HidNpadButton_Y);
  g_keys[AK_BUTTON_L1] = !!(buttons & HidNpadButton_L);
  g_keys[AK_BUTTON_R1] = !!(buttons & HidNpadButton_R);
  g_keys[AK_BUTTON_L2] = !!(buttons & HidNpadButton_ZL);
  g_keys[AK_BUTTON_R2] = !!(buttons & HidNpadButton_ZR);
  g_keys[AK_BUTTON_THUMBL] = !!(buttons & HidNpadButton_StickL);
  g_keys[AK_BUTTON_THUMBR] = !!(buttons & HidNpadButton_StickR);
  g_plus_down = !!(buttons & HidNpadButton_Plus);
  g_keys[AK_BUTTON_START] = dcr_config()->plus_button == RO_PLUS_GAME ? g_plus_down : 0;
  g_keys[AK_BUTTON_SELECT] = !!(buttons & HidNpadButton_Minus);
  g_keys[AK_DPAD_UP] = !!(buttons & HidNpadButton_Up);
  g_keys[AK_DPAD_DOWN] = !!(buttons & HidNpadButton_Down);
  g_keys[AK_DPAD_LEFT] = !!(buttons & HidNpadButton_Left);
  g_keys[AK_DPAD_RIGHT] = !!(buttons & HidNpadButton_Right);
  g_stick[0][0] = sticks[0], g_stick[0][1] = sticks[1];
  g_stick[1][0] = sticks[2], g_stick[1][1] = sticks[3];
  if (dcr_config()->stick_dpad) { /* as Android turns a gamepad's stick into D-pad keys */
    if (sticks[0] < -0.5f) g_keys[AK_DPAD_LEFT] = 1;
    if (sticks[0] > 0.5f) g_keys[AK_DPAD_RIGHT] = 1;
    if (sticks[1] > 0.5f) g_keys[AK_DPAD_UP] = 1;
    if (sticks[1] < -0.5f) g_keys[AK_DPAD_DOWN] = 1;
  }
}

int ro_pad_connected(void) {
  static int told = -1;
  const int on = g_gamepad_ui && g_pad_present;
  if (on != told) {
    told = on;
    debugPrintf("[input] isGamePadConnected -> %d\n", on);
  }
  return on;
}

int ro_pad_key_pressed(int a0, int a1) {
  const int down = a1 >= 0 && a1 < 256 ? g_keys[a1] : 0;
  /* [debug] log_input: every key the game asks about, once, and each change of
   * its answer -- which of the pad's buttons the game's actions are on */
  if (dcr_config()->log_input && a0 >= 0 && a0 < 4 && a1 >= 0 && a1 < 256) {
    static uint8_t seen[4][256], last[4][256];
    if (!seen[a0][a1]) {
      seen[a0][a1] = 1;
      debugPrintf("[input] the game polls isGamePadKeyPressed(%d, %d)\n", a0, a1);
    }
    if (last[a0][a1] != down) {
      last[a0][a1] = (uint8_t)down;
      debugPrintf("[input] isGamePadKeyPressed(%d, %d) -> %d\n", a0, a1, down);
    }
  }
  return down;
}

void ro_pad_axis_values(int a0, int a1) {
  const int stick = a1 == 1 ? 1 : 0;
  if (dcr_config()->log_input) {
    static int logged;
    if (logged++ < 40)
      debugPrintf("[input] getGamePadAxisValues(%d, %d): stick %d = %.2f, %.2f\n", a0, a1, stick, g_stick[stick][0],
                  g_stick[stick][1]);
  }
  if (g_n.SetPadAxisValues)
    g_n.SetPadAxisValues(ENV, ACT, a0, g_stick[stick][0], g_stick[stick][1]);
}

/* ----------------------------------------------------------- touch */
#define MAX_FINGERS 10
static struct {
  int on;
  u32 id;
  int x, y;
} g_finger[MAX_FINGERS];
static int g_touch_ready;

static void touch_init(void) {
  hidInitializeTouchScreen();
  g_touch_ready = 1;
}

/* HidTouchScreenState -> the three natives, per finger (GameActivity.notifyTouch):
 * the 1280x720 touch screen mapped onto the rendering size. */
static void touch_poll(void) {
  if (!g_touch_ready || !dcr_config()->touch || g_gamepad_ui)
    return;
  HidTouchScreenState st;
  memset(&st, 0, sizeof st);
  if (!hidGetTouchScreenStates(&st, 1))
    return;
  int seen[MAX_FINGERS] = {0};
  for (s32 i = 0; i < st.count; i++) {
    const int x = (int)((int64_t)st.touches[i].x * g_w / 1280), y = (int)((int64_t)st.touches[i].y * g_h / 720);
    int slot = -1, free_slot = -1;
    for (int k = 0; k < MAX_FINGERS; k++) {
      if (g_finger[k].on && g_finger[k].id == st.touches[i].finger_id)
        slot = k;
      else if (!g_finger[k].on && free_slot < 0)
        free_slot = k;
    }
    if (slot < 0) {
      if (free_slot < 0)
        continue;
      slot = free_slot;
      g_finger[slot].on = 1, g_finger[slot].id = st.touches[i].finger_id;
      g_finger[slot].x = x, g_finger[slot].y = y;
      if (dcr_config()->log_input)
        debugPrintf("[input] touch start %d,%d\n", x, y);
      g_n.TouchScrStart(ENV, ACT, x, y);
    } else if (g_finger[slot].x != x || g_finger[slot].y != y) {
      g_n.TouchScrMove(ENV, ACT, g_finger[slot].x, g_finger[slot].y, x, y);
      g_finger[slot].x = x, g_finger[slot].y = y;
    }
    seen[slot] = 1;
  }
  for (int k = 0; k < MAX_FINGERS; k++)
    if (g_finger[k].on && !seen[k]) {
      if (dcr_config()->log_input)
        debugPrintf("[input] touch end %d,%d\n", g_finger[k].x, g_finger[k].y);
      g_n.TouchScrEnd(ENV, ACT, g_finger[k].x, g_finger[k].y);
      g_finger[k].on = 0;
    }
}

/* Plus is the menu key and Minus the Back key, which the Java sends as
 * nativePressMenu() and nativePressBack(); the game's own polling reads the rest. */
static void menu_keys(void) {
  static int plus, minus;
  if (!g_pad_present)
    return;
  /* Plus: the game's Menu key (nativePressMenu) only when asked for. In
   * Rayman Fiesta Run it was not a clean pause (Minus, nativePressBack, is the
   * pause, and Plus sending Menu as well paused and resumed in a flash), and
   * the two games share this engine. */
  const int p = g_plus_down, m = g_keys[AK_BUTTON_SELECT];
  if (p && !plus && dcr_config()->plus_button == RO_PLUS_MENU && g_n.PressMenu)
    g_n.PressMenu(ENV, ACT);
  if (m && !minus && g_n.PressBack) {
    const int handled = g_n.PressBack(ENV, ACT) & 0xff;
    if (dcr_config()->log_input)
      debugPrintf("[input] nativePressBack -> %d\n", handled);
  }
  plus = p, minus = m;
}

/* The Java's onKeyDown / onKeyUp hand the keyboard / remote keys the menus use
 * to the engine: D-pad left (21) and right (22) as nativePressKey(0) and (1),
 * with nativeReleaseKey for them; D-pad centre (23) or R1 (103) as
 * nativePressKey(2); X (99) or L1 (102) as nativePressKey(3). Here the D-pad,
 * the left stick as one, R1 and L1 of the pad do the same. */
static void pad_menu_keys(void) {
  static uint8_t prev[4];
  if (!g_pad_present || !dcr_config()->menu_keys || !g_n.PressKey)
    return;
  const uint8_t now[4] = {g_keys[AK_DPAD_LEFT], g_keys[AK_DPAD_RIGHT], g_keys[AK_BUTTON_R1], g_keys[AK_BUTTON_L1]};
  for (int k = 0; k < 4; k++) {
    if (now[k] && !prev[k]) {
      if (dcr_config()->log_input)
        debugPrintf("[input] nativePressKey(%d)\n", k);
      g_n.PressKey(ENV, ACT, k);
    } else if (!now[k] && prev[k] && k < 2 && g_n.ReleaseKey) {
      g_n.ReleaseKey(ENV, ACT, k);
    }
    prev[k] = now[k];
  }
}

/* ------------------------------------------------------ pointer cursor */
/* Menus the game made for fingers (the level map, the end-of-level screen) take
 * nothing from a pad. A cursor does what a finger does: the right stick moves
 * it, ZR taps. It shows when the stick moves and fades 3 s after the last move
 * or tap, so it is never in the way while playing. Drawn over the engine's
 * frame with OpenGL ES 1 calls that leave the engine's state as they found it. */
#define PTR_HIDE_NS 3000000000ull
#define PTR_SPEED 900.0f /* pixels a second at full stick */

static float g_ptr_x, g_ptr_y;
static u64 g_ptr_seen, g_ptr_tick;
static int g_ptr_down, g_ptr_init;

static void pointer_update(void) {
  if (!g_pad_present || !dcr_config()->pointer_cursor || !g_n.TouchScrStart)
    return;
  const u64 now = armGetSystemTick();
  if (!g_ptr_init) {
    g_ptr_init = 1;
    g_ptr_x = (float)g_w / 2, g_ptr_y = (float)g_h / 2;
    g_ptr_tick = now;
  }
  const float dt = (float)armTicksToNs(now - g_ptr_tick) / 1e9f;
  g_ptr_tick = now;
  float sx = g_stick[1][0], sy = -g_stick[1][1]; /* the pad's y is up, the screen's down */
  const float mag = sqrtf(sx * sx + sy * sy);
  const int visible_before = g_ptr_seen && armTicksToNs(now - g_ptr_seen) < PTR_HIDE_NS;
  if (mag > 0.2f) {
    const float px = g_ptr_x, py = g_ptr_y;
    g_ptr_x += sx * PTR_SPEED * dt * (float)g_w / 1280.0f;
    g_ptr_y += sy * PTR_SPEED * dt * (float)g_h / 720.0f;
    if (g_ptr_x < 0) g_ptr_x = 0;
    if (g_ptr_y < 0) g_ptr_y = 0;
    if (g_ptr_x > (float)(g_w - 1)) g_ptr_x = (float)(g_w - 1);
    if (g_ptr_y > (float)(g_h - 1)) g_ptr_y = (float)(g_h - 1);
    g_ptr_seen = now;
    if (g_ptr_down && g_n.TouchScrMove)
      g_n.TouchScrMove(ENV, ACT, (int)px, (int)py, (int)g_ptr_x, (int)g_ptr_y);
  }
  const int tap = dcr_config()->pointer_tap;
  const int a_taps = tap == RO_TAP_A || tap == RO_TAP_BOTH, zr_taps = tap == RO_TAP_ZR || tap == RO_TAP_BOTH;
  const int click = (a_taps && g_keys[AK_BUTTON_A]) || (zr_taps && g_keys[AK_BUTTON_R2]);
  /* While the cursor is up, A is the cursor's: the game polls it for a jump,
   * and a tap on a button must not also be one. */
  if (a_taps && (visible_before || mag > 0.2f || g_ptr_down))
    g_keys[AK_BUTTON_A] = 0;
  if (click && !g_ptr_down && (visible_before || mag > 0.2f)) {
    g_ptr_down = 1;
    g_ptr_seen = now;
    if (dcr_config()->log_input)
      debugPrintf("[input] pointer tap down at %d,%d\n", (int)g_ptr_x, (int)g_ptr_y);
    g_n.TouchScrStart(ENV, ACT, (int)g_ptr_x, (int)g_ptr_y);
  } else if (!click && g_ptr_down) {
    g_ptr_down = 0;
    g_ptr_seen = now;
    g_n.TouchScrEnd(ENV, ACT, (int)g_ptr_x, (int)g_ptr_y);
  } else if (g_ptr_down) {
    g_ptr_seen = now; /* held: stays up */
  }
}

static struct {
  void (*GetIntegerv)(unsigned, int *);
  void (*GetFloatv)(unsigned, float *);
  unsigned char (*IsEnabled)(unsigned);
  void (*Enable)(unsigned);
  void (*Disable)(unsigned);
  void (*EnableClientState)(unsigned);
  void (*DisableClientState)(unsigned);
  void (*ClientActiveTexture)(unsigned);
  void (*MatrixMode)(unsigned);
  void (*PushMatrix)(void);
  void (*PopMatrix)(void);
  void (*LoadMatrixf)(const float *);
  void (*LoadIdentity)(void);
  void (*Color4f)(float, float, float, float);
  void (*BlendFunc)(unsigned, unsigned);
  void (*BindBuffer)(unsigned, unsigned);
  void (*VertexPointer)(int, unsigned, int, const void *);
  void (*DrawArrays)(unsigned, int, int);
  int ok;
} g_gl;

static void pointer_gl_init(void) {
  if (g_gl.ok)
    return;
#define L(n) g_gl.n = (void *)dcr_gl_lookup("gl" #n)
  L(GetIntegerv); L(GetFloatv); L(IsEnabled); L(Enable); L(Disable); L(EnableClientState);
  L(DisableClientState); L(ClientActiveTexture); L(MatrixMode); L(PushMatrix); L(PopMatrix);
  L(LoadMatrixf); L(LoadIdentity); L(Color4f); L(BlendFunc); L(BindBuffer); L(VertexPointer); L(DrawArrays);
#undef L
  const int have = g_gl.GetIntegerv && g_gl.GetFloatv && g_gl.IsEnabled && g_gl.Enable && g_gl.Disable &&
            g_gl.EnableClientState && g_gl.DisableClientState && g_gl.ClientActiveTexture && g_gl.MatrixMode &&
            g_gl.PushMatrix && g_gl.PopMatrix && g_gl.LoadMatrixf && g_gl.LoadIdentity && g_gl.Color4f &&
            g_gl.BlendFunc && g_gl.BindBuffer && g_gl.VertexPointer && g_gl.DrawArrays;
  g_gl.ok = have ? 1 : -1;
  if (!have)
    debugPrintf("[game] the pointer cursor cannot be drawn: a GL function is missing\n");
}

/* An arrow, 22 x 32 pixels at 1280x720, black with a white face. */
static void pointer_draw(void) {
  const u64 now = armGetSystemTick();
  if (!g_pad_present || !dcr_config()->pointer_cursor || !g_ptr_seen || armTicksToNs(now - g_ptr_seen) >= PTR_HIDE_NS)
    return;
  pointer_gl_init();
  if (g_gl.ok != 1)
    return;
  /* what the engine has set, kept to be put back */
  static const unsigned caps[] = {0x0DE1 /*TEXTURE_2D*/, 0x0B71 /*DEPTH_TEST*/, 0x0B90 /*STENCIL_TEST*/,
                                  0x0B44 /*CULL_FACE*/,  0x0BE2 /*BLEND*/,      0x0C11 /*SCISSOR_TEST*/,
                                  0x0B50 /*LIGHTING*/,   0x0B60 /*FOG*/,        0x0BC0 /*ALPHA_TEST*/};
  static const unsigned arrays[] = {0x8074 /*VERTEX*/, 0x8075 /*NORMAL*/, 0x8076 /*COLOR*/, 0x8078 /*TEXCOORD*/};
  unsigned char cap_on[sizeof caps / sizeof caps[0]], arr_on[sizeof arrays / sizeof arrays[0]];
  int matrix_mode = 0, client_tex = 0x84C0, blend_src = 1, blend_dst = 0, array_buf = 0;
  float color[4] = {1, 1, 1, 1};
  g_gl.GetIntegerv(0x0BA0 /*MATRIX_MODE*/, &matrix_mode);
  g_gl.GetIntegerv(0x84E1 /*CLIENT_ACTIVE_TEXTURE*/, &client_tex);
  g_gl.GetIntegerv(0x0BE1 /*BLEND_SRC*/, &blend_src);
  g_gl.GetIntegerv(0x0BE0 /*BLEND_DST*/, &blend_dst);
  g_gl.GetIntegerv(0x8894 /*ARRAY_BUFFER_BINDING*/, &array_buf);
  g_gl.GetFloatv(0x0B00 /*CURRENT_COLOR*/, color);
  for (unsigned i = 0; i < sizeof caps / sizeof caps[0]; i++)
    cap_on[i] = g_gl.IsEnabled(caps[i]);
  g_gl.ClientActiveTexture(0x84C0);
  for (unsigned i = 0; i < sizeof arrays / sizeof arrays[0]; i++)
    arr_on[i] = g_gl.IsEnabled(arrays[i]);

  /* draw */
  for (unsigned i = 0; i < sizeof caps / sizeof caps[0]; i++)
    g_gl.Disable(caps[i]);
  g_gl.Enable(0x0BE2 /*BLEND*/);
  g_gl.BlendFunc(0x0302 /*SRC_ALPHA*/, 0x0303 /*ONE_MINUS_SRC_ALPHA*/);
  g_gl.DisableClientState(0x8075);
  g_gl.DisableClientState(0x8076);
  g_gl.DisableClientState(0x8078);
  g_gl.EnableClientState(0x8074);
  g_gl.BindBuffer(0x8892 /*ARRAY_BUFFER*/, 0);
  const float W = (float)g_w, H = (float)g_h;
  const float proj[16] = {2.0f / W, 0, 0, 0, 0, -2.0f / H, 0, 0, 0, 0, -1, 0, -1, 1, 0, 1};
  g_gl.MatrixMode(0x1701 /*PROJECTION*/);
  g_gl.PushMatrix();
  g_gl.LoadMatrixf(proj);
  g_gl.MatrixMode(0x1700 /*MODELVIEW*/);
  g_gl.PushMatrix();
  g_gl.LoadIdentity();
  const float s = H / 720.0f * (g_ptr_down ? 0.85f : 1.0f);
  const float x = g_ptr_x, y = g_ptr_y;
  const float fade = armTicksToNs(now - g_ptr_seen) > 2000000000ull
                         ? 1.0f - (float)(armTicksToNs(now - g_ptr_seen) - 2000000000ull) / 1e9f : 1.0f;
  /* the arrow's outline, then its face a little inside */
  const float out[6] = {x, y, x, y + 32 * s, x + 22 * s, y + 23 * s};
  const float in[6] = {x + 2.5f * s, y + 6.5f * s, x + 2.5f * s, y + 25 * s, x + 17 * s, y + 21 * s};
  g_gl.Color4f(0.0f, 0.0f, 0.0f, 0.9f * fade);
  g_gl.VertexPointer(2, 0x1406 /*FLOAT*/, 0, out);
  g_gl.DrawArrays(0x0004 /*TRIANGLES*/, 0, 3);
  g_gl.Color4f(1.0f, 1.0f, 1.0f, 0.95f * fade);
  g_gl.VertexPointer(2, 0x1406, 0, in);
  g_gl.DrawArrays(0x0004, 0, 3);

  /* put back */
  g_gl.PopMatrix();
  g_gl.MatrixMode(0x1701);
  g_gl.PopMatrix();
  g_gl.MatrixMode((unsigned)matrix_mode);
  for (unsigned i = 0; i < sizeof caps / sizeof caps[0]; i++)
    cap_on[i] ? g_gl.Enable(caps[i]) : g_gl.Disable(caps[i]);
  for (unsigned i = 0; i < sizeof arrays / sizeof arrays[0]; i++)
    arr_on[i] ? g_gl.EnableClientState(arrays[i]) : g_gl.DisableClientState(arrays[i]);
  g_gl.ClientActiveTexture((unsigned)client_tex);
  g_gl.BlendFunc((unsigned)blend_src, (unsigned)blend_dst);
  g_gl.BindBuffer(0x8892, (unsigned)array_buf);
  g_gl.Color4f(color[0], color[1], color[2], color[3]);
}

/* --------------------------------------------------------- lifecycle */
/* The runtime's applet lifecycle (rt_applet.c) calls these from the frame
 * loop's rt_applet_poll(): what GameActivity's onPause / onWindowFocusChanged
 * did. */
void port_focus_lost(void) {
  if (!g_engine_up)
    return;
  if (g_n.MusicStopStart)
    g_n.MusicStopStart(ENV, ACT, 0);
  g_n.Pause(ENV, ACT);
}

void port_focus_gained(void) {
  if (!g_engine_up)
    return;
  if (g_n.MusicStopStart)
    g_n.MusicStopStart(ENV, ACT, 1);
  g_n.Resume(ENV, ACT);
}

/* HOME and sleep freeze the whole process; the runtime's clocks find each
 * freeze: what Android does around it, onPause then onResume. */
void port_process_frozen(unsigned count) {
  debugPrintf("[game] the process was held (HOME menu or sleep; freeze %u)\n", count);
  port_focus_lost();
  port_focus_gained();
}

/* ---------------------------------------------------------------- EGL */
static int choose_config(void *dpy, int es2, void **cfg) {
  const fEGLint api_bit = es2 ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_ES_BIT;
  /* the best first: RGBA8 with 24-bit depth and stencil (the GLSurfaceView's
   * chooser asks for what the game's renderers draw with), then less */
  static const struct { int depth, stencil; } tries[] = {{24, 8}, {16, 8}, {24, 0}, {16, 0}, {0, 0}};
  for (unsigned t = 0; t < sizeof tries / sizeof tries[0]; t++) {
    const fEGLint attrs[] = {EGL_RENDERABLE_TYPE, api_bit, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8,
                             EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, tries[t].depth,
                             EGL_STENCIL_SIZE, tries[t].stencil, EGL_NONE};
    fEGLint n = 0;
    if (b_eglChooseConfig(dpy, attrs, cfg, 1, &n) && n >= 1) {
      debugPrintf("[game] EGL config: RGB8, depth %d, stencil %d\n", tries[t].depth, tries[t].stencil);
      return 0;
    }
  }
  return -1;
}

/* One try at an OpenGL ES version: config, context, window surface, current.
 * 0 on success; on failure everything it made is gone again. */
static int egl_try(int es2) {
  void *cfg = NULL;
  if (choose_config(g_dpy, es2, &cfg)) {
    debugPrintf("[game] no OpenGL ES %d window configuration (0x%x)\n", es2 ? 2 : 1, (unsigned)b_eglGetError());
    return -1;
  }
  const fEGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, es2 ? 2 : 1, EGL_NONE};
  g_ctx = b_eglCreateContext(g_dpy, cfg, NULL, ctx_attrs);
  if (!g_ctx) {
    debugPrintf("[game] OpenGL ES %d: no context (0x%x)\n", es2 ? 2 : 1, (unsigned)b_eglGetError());
    return -1;
  }
  g_surf = b_eglCreateWindowSurface(g_dpy, cfg, nwindowGetDefault(), NULL);
  if (!g_surf || !b_eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
    debugPrintf("[game] OpenGL ES %d: surface %p, make current failed (0x%x)\n", es2 ? 2 : 1, g_surf,
                (unsigned)b_eglGetError());
    b_eglMakeCurrent(g_dpy, NULL, NULL, NULL);
    if (g_surf)
      b_eglDestroySurface(g_dpy, g_surf);
    b_eglDestroyContext(g_dpy, g_ctx);
    g_surf = g_ctx = NULL;
    return -1;
  }
  return 0;
}

/* The game's renderer is fixed-function OpenGL ES 1 (glMatrixMode,
 * glVertexPointer, ...: libRO1Mobile imports both libGLESv1_CM and libGLESv2),
 * which an ES 2 context refuses call by call. [graphics] gles_version says
 * which to ask for; if the driver cannot make that one, the other is tried. */
static void egl_up(void) {
  dcr_window_size(&g_w, &g_h);
  const int want_es2 = !dcr_config()->gles;
  g_dpy = b_eglGetDisplay(NULL);
  fEGLint maj = 0, min = 0;
  if (!g_dpy || !b_eglInitialize(g_dpy, &maj, &min))
    fatal_error("The graphics driver did not start (eglInitialize 0x%x).", (unsigned)b_eglGetError());
  unsigned (*bind_api)(unsigned) = (unsigned (*)(unsigned))dcr_gl_lookup("eglBindAPI");
  if (bind_api)
    bind_api(EGL_OPENGL_ES_API);
  int es2 = want_es2;
  if (egl_try(es2) != 0) {
    debugPrintf("[game] OpenGL ES %d is not available here: trying OpenGL ES %d\n", es2 ? 2 : 1, es2 ? 1 : 2);
    es2 = !es2;
    if (egl_try(es2) != 0)
      fatal_error("Could not create an OpenGL ES context (0x%x). See debug.log.", (unsigned)b_eglGetError());
  }
  b_eglSwapInterval(g_dpy, 1);
  debugPrintf("[game] EGL %d.%d: OpenGL ES %d on the window, %dx%d\n", (int)maj, (int)min, es2 ? 2 : 1, g_w, g_h);
}

static void egl_down(void) {
  if (!g_dpy)
    return;
  b_eglMakeCurrent(g_dpy, NULL, NULL, NULL);
  if (g_ctx)
    b_eglDestroyContext(g_dpy, g_ctx);
  if (g_surf)
    b_eglDestroySurface(g_dpy, g_surf);
  b_eglTerminate(g_dpy);
  g_dpy = g_surf = g_ctx = NULL;
}

/* ---------------------------------------------------------------- report */
static void report(void) {
  static u64 last_tick;
  static unsigned long last_frames;
  const u64 tick = armGetSystemTick();
  const unsigned long frames = (unsigned long)dcr_gl_frames();
  const double fps = last_tick ? (double)(frames - last_frames) * 1e9 / (double)armTicksToNs(tick - last_tick) : 0.0;
  last_tick = tick;
  last_frames = frames;
  char rate[24] = "";
  if (fps > 0.0)
    snprintf(rate, sizeof rate, " (%.1f fps)", fps);
  debugPrintf("[game] %lu frames%s, %d Java objects\n", frames, rate, jni_live_objects());
  dcr_boost_report();
  ro_gltrace_report();
}

/* ---------------------------------------------------------- onCreate */
static void ensure_dir(const char *android_path) {
  char real[DCR_PATH_MAX];
  rt_mkdirs(dcr_translate_path(android_path, real, sizeof real));
}

static void on_create(void) {
  const DcrConfig *c = dcr_config();
  const int docked = appletGetOperationMode() == AppletOperationMode_Console;
  g_gamepad_ui = c->ui == RO_UI_GAMEPAD || (c->ui == RO_UI_AUTO && docked);

  ensure_dir(RO_DATA_DIR);
  ensure_dir(RO_DATA_DIR "/saves"); /* profile, Transactions, Achievements: files/saves, as on the phone */
  {
    char real[DCR_PATH_MAX], probe[DCR_PATH_MAX + 16];
    snprintf(probe, sizeof probe, "%s/lvl", dcr_translate_path(RO_DATA_DIR, real, sizeof real));
    struct stat sb;
    if (stat(probe, &sb) != 0)
      debugPrintf("[game] WARNING: no game data at %s -- copy the game's files folder there\n", real);
    else
      debugPrintf("[game] game data: %s\n", real);
  }

  g_n.SetLanguage(ENV, ACT, jstr(ro_language_codes[c->language]));
  /* This build keeps its data on the phone's storage, not in the APK
   * (GameActivity.resourcesInAsset false): the browse, root and temp
   * directories are all the data folder, and "saves/..." is under it. The
   * asset manager is NOT handed over: with one, AndroidFileMgr::fileExists
   * (what every texture load asks first) looks in the APK alone and answers
   * "no"; without one it asks access() about the file on disk. */
  g_n.SetBrowseDirectory(ENV, ACT, jstr(RO_DATA_DIR));
  g_n.SetRootDirectory(ENV, ACT, jstr(RO_DATA_DIR));
  g_n.SetPersistenceRootDirectory(ENV, ACT, jstr(RO_DATA_DIR));
  g_n.SetWriteTempRootDirectory(ENV, ACT, jstr(RO_DATA_DIR));
  if (g_n.SetKindleFireMode) g_n.SetKindleFireMode(ENV, ACT, 0);
  if (g_n.SetNOOKMode) g_n.SetNOOKMode(ENV, ACT, 0);
  if (g_n.SetAmazonStreamingBoxMode) g_n.SetAmazonStreamingBoxMode(ENV, ACT, 0);
  ro_setup_flurry(); /* createGameBehaviourLogger: before the engine logs anything */
  /* the TV interface: a device with no touch screen (Android TV / a game box) */
  if (g_n.SetGoogleStreamingBoxMode) g_n.SetGoogleStreamingBoxMode(ENV, ACT, g_gamepad_ui);
  /* the game's own properties (its assets) say what is on; this port
   * has no accounts, no store, no gallery, no proxy */
  if (g_n.EnableSocialNetwork) g_n.EnableSocialNetwork(ENV, ACT, 0);
  if (g_n.EnableMoreRaymanButton) g_n.EnableMoreRaymanButton(ENV, ACT, 0);
  if (g_n.EnableOfflineGallery) g_n.EnableOfflineGallery(ENV, ACT, 0);
  if (g_n.EnableGalleryAsWallpaper) g_n.EnableGalleryAsWallpaper(ENV, ACT, 0);
  if (g_n.EnableProxy) g_n.EnableProxy(ENV, ACT, 0, jstr(""));
  if (g_n.EnableSkins) g_n.EnableSkins(ENV, ACT, 0);
  if (g_n.EnableSkinsInAppPurchase) g_n.EnableSkinsInAppPurchase(ENV, ACT, 0);
  debugPrintf("[game] onCreate done: language %s, %s interface\n", ro_language_codes[c->language],
              g_gamepad_ui ? "gamepad (streaming box)" : "touch");
}

/* ----------------------------------------------------------------- run */
int ro_game_run(void) {
  egl_up();
  ro_java_init();

  /* a pad, handheld Joy-Cons included, and the touch screen */
  rt_pad_setup(1, 1);
  rt_pad_slot(&g_pad, 0);
  rt_pad_slot(&g_pad_hh, RT_PAD_HANDHELD);
  g_pad_ready = 1;
  touch_init();

  on_create();

  /* ---- onSurfaceChanged, then onDrawFrame's first call ---- */
  g_n.SetWidth(ENV, ACT, g_w);
  g_n.SetHeight(ENV, ACT, g_h);
  debugPrintf("[game] nativeCreate()\n");
  g_n.Create(ENV, ACT);
  debugPrintf("[game] nativeStart()\n");
  g_n.Start(ENV, ACT);
  if (g_n.OnWindowFocusChanged)
    g_n.OnWindowFocusChanged(ENV, ACT, 1);
  g_engine_up = 1;
  debugPrintf("[game] the engine is up\n");
  log_flush_ring();

  dcr_watchdog_start();

  u64 last_report = armGetSystemTick();
  int first = 1;
  unsigned long quiet_at = 0;
  while (!ro_quit_requested() && !rt_exit_requested() && appletMainLoop()) {
    rt_applet_poll(); /* focus, freezes: port_focus_lost/gained, port_process_frozen */
    if (!rt_focused()) {
      svcSleepThread(50000000ll);
      continue;
    }
    pad_poll();
    touch_poll();
    menu_keys();
    pad_menu_keys();
    pointer_update();
    const int more = g_n.Run(ENV, ACT) & 0xff;
    pointer_draw();
    b_eglSwapBuffers(g_dpy, g_surf);

    const unsigned long frames = (unsigned long)dcr_gl_frames();
    if (first) {
      first = 0;
      dcr_boost_launch_end();
      debugPrintf("[game] first frame presented (nativeRun -> %d)\n", more);
      quiet_at = frames + 180;
    }
    /* From ~3 s after the first picture the log goes to a RAM ring (util.c),
     * written out every 10 s and by the watchdog. */
    if (quiet_at && frames >= quiet_at) {
      quiet_at = 0;
      log_set_quiet(1);
    }
    const u64 now = armGetSystemTick();
    if (armTicksToNs(now - last_report) >= 10000000000ull) {
      last_report = now;
      report();
      log_flush_ring();
    }
  }

  /* ---- onPause, onStop, onDestroy ---- */
  debugPrintf("[game] leaving (%s)\n", ro_quit_requested() ? "the game quit" : "closed from the system");
  log_set_quiet(0);
  rt_applet_stop();
  if (rt_focused())
    g_n.Pause(ENV, ACT);
  if (g_n.Stop)
    g_n.Stop(ENV, ACT);
  if (g_n.Destroy)
    g_n.Destroy(ENV, ACT);
  egl_down();
  debugPrintf("[game] closed\n");
  log_flush_ring();
  return 0;
}
