/* ro_loader.c -- loading Rayman Jungle Run's two modules.
 *
 * The APK's lib/armeabi-v7a holds libRO1Mobile.so, the whole game (Ubisoft
 * Pasta Games' engine, with its GLES 1 and GLES 2 renderers, gnustl and the
 * zlib it needs linked in), and libfmodex.so (FMOD Ex), which it needs
 * (DT_NEEDED). Their other DT_NEEDED are system libraries (liblog, libz,
 * libdl, libGLESv1_CM, libGLESv2, libandroid, libstdc++, libm, libc), served by
 * the shims. libfmodex goes first, so libRO1Mobile's FMOD_* imports bind to
 * its exports (so_resolve: another loaded module's export).
 *
 * The natives are exported by name (Java_com_pastagames_android_GameActivity_*:
 * the Java loads the libraries with System.loadLibrary and binds them lazily;
 * there is no JNI_OnLoad or RegisterNatives), and are looked up once into g_n.
 * MIT.
 */
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "codespace.h"
#include "config.h"
#include "dcr_path.h"
#include "error.h"
#include "imports.h"
#include "ro.h"
#include "so_util.h"
#include "util.h"

static so_module g_mod_fmod, g_mod_game;
RoNatives g_n;

#define JNI_ "Java_com_pastagames_android_GameActivity_"

static const struct {
  const char *name;
  size_t off;
  int required;
} k_natives[] = {
#define NAT(n, req) {"native" #n, offsetof(RoNatives, n), req}
    NAT(Create, 1),       NAT(Start, 1),          NAT(Run, 1),
    NAT(Pause, 1),        NAT(Resume, 1),         NAT(Stop, 0),
    NAT(Destroy, 0),      NAT(OnWindowFocusChanged, 0), NAT(MusicStopStart, 0),
    NAT(SetWidth, 1),     NAT(SetHeight, 1),      NAT(UpdateSize, 0),
    NAT(ReloadTextures, 0),
    NAT(SetLanguage, 1),  NAT(SetRootDirectory, 1), NAT(SetPersistenceRootDirectory, 1),
    NAT(SetWriteTempRootDirectory, 1), NAT(SetBrowseDirectory, 1), NAT(SetAssetManager, 1),
    NAT(SetKindleFireMode, 0), NAT(SetNOOKMode, 0), NAT(SetAmazonStreamingBoxMode, 0),
    NAT(SetGoogleStreamingBoxMode, 0), NAT(EnableSocialNetwork, 0), NAT(EnableMoreRaymanButton, 0),
    NAT(EnableOfflineGallery, 0), NAT(EnableGalleryAsWallpaper, 0), NAT(EnableProxy, 0),
    NAT(EnableSkins, 0),  NAT(EnableSkinsInAppPurchase, 0),
    NAT(TouchScrStart, 1), NAT(TouchScrMove, 1),  NAT(TouchScrEnd, 1),
    NAT(KeyDown, 0),      NAT(KeyUp, 0),          NAT(PressKey, 0),
    NAT(ReleaseKey, 0),   NAT(PressBack, 0),      NAT(PressMenu, 0),
    NAT(SetPadAxisValues, 0),
#undef NAT
};

static int bind_natives(void) {
  int missing = 0;
  char sym[128];
  for (unsigned i = 0; i < sizeof k_natives / sizeof k_natives[0]; i++) {
    /* "nativeCreate" -> Java_..._GameActivity_nativeCreate */
    snprintf(sym, sizeof sym, JNI_ "%s", k_natives[i].name);
    uintptr_t a = so_try_find_addr_rx(&g_mod_game, sym);
    memcpy((uint8_t *)&g_n + k_natives[i].off, &a, sizeof a); /* a function pointer's slot */
    if (!a) {
      debugPrintf("[boot] %s native %s%s\n", k_natives[i].required ? "MISSING" : "no", sym,
                  k_natives[i].required ? "" : " (optional)");
      missing += k_natives[i].required;
    }
  }
  return missing;
}

/* Stage and relocate one module (not yet resolved). */
static int stage_one(so_module *mod, const char *lib) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s", dcr_game_root(), lib);
  int rc = so_load(mod, path, NULL, PORT_SO_REGION_BYTES);
  if (rc < 0) {
    const char *why = rc == -1 ? "cannot open it, or it is not a 32-bit ARM ELF"
                    : rc == -2 ? "out of memory"
                    : rc == -3 ? "larger than PORT_SO_REGION_BYTES"
                    : rc == -4 ? "too many program headers" : "?";
    debugPrintf("[boot] so_load(%s) failed rc=%d: %s\n", path, rc, why);
    return -1;
  }
  so_relocate(mod);
  return 0;
}

/* The two modules import from each other: libRO1Mobile's FMOD_* from
 * libfmodex, and libfmodex's operator delete and __cxa_pure_virtual from the
 * libstdc++ linked into libRO1Mobile. So both are staged first, and only then
 * resolved (so_resolve looks at the other loaded modules' exports). */
static void resolve_one(so_module *mod) {
  int missing = so_resolve(mod, dcr_imports, dcr_imports_count, 1);
  debugPrintf("[boot] %s %u KB  staged %p -> %p  (%d unresolved imports)\n", mod->base_name,
              (unsigned)(mod->load_size >> 10), mod->load_base, mod->load_virtbase, missing);
  /* libgcc's __sync_* on ARM Linux call the kernel's user helpers through
   * literal pools: point any at the runtime's kuser.S. */
  so_fix_kuser_helpers(mod);
}

int ro_load_engine(void) {
  if (stage_one(&g_mod_fmod, RO_LIB_FMOD) != 0 || stage_one(&g_mod_game, RO_LIB_GAME) != 0)
    return -1;
  resolve_one(&g_mod_fmod);
  resolve_one(&g_mod_game);
  so_finalize(&g_mod_fmod);
  so_flush_caches(&g_mod_fmod);
  so_finalize(&g_mod_game);
  so_flush_caches(&g_mod_game);
  if (bind_natives()) {
    debugPrintf("[boot] %s is not the Rayman Jungle Run engine this port knows\n", RO_LIB_GAME);
    return -2;
  }
  debugPrintf("[boot] engine: Rayman Jungle Run, mapped at %p (FMOD Ex at %p)\n", g_mod_game.load_virtbase,
              g_mod_fmod.load_virtbase);
  return 0;
}

/* GameActivity.onCreate makes the statistics logger (a FlurryWrapper, which is a
 * GameBehaviourLogger), and its Java side asks the native one to set itself
 * up: the engine's GameBehaviourLogger::getSingleton() is NULL until then,
 * and the first start's code that logs an event (OptionsMenu::loadVolumeValues
 * with no saved volume) calls through it. Its Java side here is the
 * FlurryWrapper singleton, which answers every call with nothing (ro_java.c). */
void ro_setup_flurry(void) {
  void (*fn)(void *, void *) = (void (*)(void *, void *))so_try_find_addr_rx(
      &g_mod_game, "Java_com_pastagames_android_stats_GameBehaviourLogger_nativeSetupFlurry");
  if (!fn) {
    debugPrintf("[boot] no nativeSetupFlurry in %s: the statistics logger is not made\n", RO_LIB_GAME);
    return;
  }
  fn(g_jni_env, jni_singleton("com/pastagames/android/stats/flurry/FlurryWrapper"));
  debugPrintf("[game] nativeSetupFlurry() done\n");
}

/* Android runs a library's constructors inside System.loadLibrary: gnustl's
 * locale and iostream set-up, the engine's statics. */
void ro_run_constructors(void) {
  so_execute_init_array(&g_mod_fmod);
  debugPrintf("[boot] %s constructors done\n", RO_LIB_FMOD);
  so_execute_init_array(&g_mod_game);
  debugPrintf("[boot] %s constructors done\n", RO_LIB_GAME);
}
