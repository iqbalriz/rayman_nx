/* ro.h -- the Rayman Jungle Run side of the port: the pieces that stand in for
 * the game's Java (GameActivity and its GLSurfaceView renderer, the asset
 * manager, the gamepad polling), talking to each other. MIT. */
#ifndef RO_H
#define RO_H
#include <stddef.h>
#include <stdint.h>

#include "jni.h"

/* ------------------------------------------------------------ the engine */
/* The natives of com.pastagames.android.GameActivity (libRO1Mobile.so exports
 * them as Java_com_pastagames_android_GameActivity_*). Every one is an
 * instance method: (env, activity, args...). NULL when the library lacks one.
 * Signatures from the APK's classes.dex. ro_loader.c fills this in. */
typedef struct {
  /* lifecycle */
  void (*Create)(void *env, void *obj);
  void (*Start)(void *env, void *obj);
  jint (*Run)(void *env, void *obj); /* boolean: one frame */
  void (*Pause)(void *env, void *obj);
  void (*Resume)(void *env, void *obj);
  void (*Stop)(void *env, void *obj);
  void (*Destroy)(void *env, void *obj);
  void (*OnWindowFocusChanged)(void *env, void *obj, jint focus);
  void (*MusicStopStart)(void *env, void *obj, jint start);
  /* surface */
  void (*SetWidth)(void *env, void *obj, jint w);
  void (*SetHeight)(void *env, void *obj, jint h);
  void (*UpdateSize)(void *env, void *obj);
  void (*ReloadTextures)(void *env, void *obj);
  /* environment */
  void (*SetLanguage)(void *env, void *obj, void *str);
  void (*SetRootDirectory)(void *env, void *obj, void *str);
  void (*SetPersistenceRootDirectory)(void *env, void *obj, void *str);
  void (*SetWriteTempRootDirectory)(void *env, void *obj, void *str);
  void (*SetBrowseDirectory)(void *env, void *obj, void *str);
  void (*SetAssetManager)(void *env, void *obj, void *mgr);
  void (*SetKindleFireMode)(void *env, void *obj, jint on);
  void (*SetNOOKMode)(void *env, void *obj, jint on);
  void (*SetAmazonStreamingBoxMode)(void *env, void *obj, jint on);
  void (*SetGoogleStreamingBoxMode)(void *env, void *obj, jint on);
  void (*EnableSocialNetwork)(void *env, void *obj, jint on);
  void (*EnableMoreRaymanButton)(void *env, void *obj, jint on);
  void (*EnableOfflineGallery)(void *env, void *obj, jint on);
  void (*EnableGalleryAsWallpaper)(void *env, void *obj, jint on);
  void (*EnableProxy)(void *env, void *obj, jint on, void *url);
  void (*EnableSkins)(void *env, void *obj, jint on);
  void (*EnableSkinsInAppPurchase)(void *env, void *obj, jint on);
  /* input */
  void (*TouchScrStart)(void *env, void *obj, jint x, jint y);
  void (*TouchScrMove)(void *env, void *obj, jint px, jint py, jint x, jint y);
  void (*TouchScrEnd)(void *env, void *obj, jint x, jint y);
  void (*KeyDown)(void *env, void *obj, jint code);
  void (*KeyUp)(void *env, void *obj, jint code);
  void (*PressKey)(void *env, void *obj, jint code);
  void (*ReleaseKey)(void *env, void *obj, jint code);
  jint (*PressBack)(void *env, void *obj); /* boolean */
  void (*PressMenu)(void *env, void *obj);
  void (*SetPadAxisValues)(void *env, void *obj, jint idx, jfloat x, jfloat y);
} RoNatives;

extern RoNatives g_n;

int ro_load_engine(void);        /* load, relocate, resolve, map both modules; 0 on success */
void ro_setup_flurry(void);      /* GameBehaviourLogger.nativeSetupFlurry: the engine's logger singleton */
void ro_run_constructors(void);

/* ---------------------------------------------------------- the Java side */
extern JObj *g_activity;         /* com.pastagames.android.GameActivity */
void ro_java_init(void);
/* GameActivity.setCanQuit / quitApp */
int ro_quit_requested(void);

/* ------------------------------------------------------------- the game */
int ro_game_run(void);

/* ------------------------------------------------------------- input */
/* ro_game.c: the pad as the game's polls (isGamePadConnected,
 * isGamePadKeyPressed, getGamePadAxisValues) see it. Android key codes. */
int ro_pad_connected(void);
int ro_pad_key_pressed(int a0, int a1);
void ro_pad_axis_values(int a0, int a1);

/* ro_gltrace.c: the summary of the GL functions that raised errors */
void ro_gltrace_report(void);

/* ------------------------------------------------------------ assets */
/* assets/<name> out of the APK: malloc'd (free with ro_asset_free), NULL when
 * the APK has none. */
uint8_t *ro_asset_load(const char *name, size_t *len);
void ro_asset_free(void *p);
int ro_asset_exists(const char *name);

#endif /* RO_H */
