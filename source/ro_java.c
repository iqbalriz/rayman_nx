/* ro_java.c -- the Java side of Rayman Jungle Run, as the engine sees it.
 *
 * The game's Java (com.pastagames.android.GameActivity, its GLSurfaceView
 * wrapper, FMODAudioDevice, the store, Flurry and Chartboost) does not run
 * here: ro_game.c does what it did. What the ENGINE calls back through JNI is
 * a list of GameActivity methods, listed below with the signatures of the
 * APK's classes.dex. The answers are the ones of a phone with no Google
 * account, no network and no store: no leaderboards, no achievements, no
 * purchases. Anything else the engine asks for is logged once as unhandled
 * (jni_core.c): that list is the to-do list. MIT.
 */
#include <string.h>
#include <switch.h>

#include "config.h"
#include "dcr_config.h"
#include "dcr_manifest.h"
#include "dcr_path.h"
#include "jni.h"
#include "ro.h"
#include "dcr_setup.h"
#include "util.h"

#define GA "com/pastagames/android/GameActivity"
#define S "Ljava/lang/String;"

#define H(fn) static jvalue fn(JObj *self, const jvalue *a, const JMethod *m)

JObj *g_activity;
static int g_can_quit, g_quit;

int ro_quit_requested(void) { return g_quit; }

/* ------------------------------------------------------------- the activity */
H(h_getPackageName) {
  const char *pkg = dcr_manifest_loaded() && dcr_manifest_package()[0] ? dcr_manifest_package() : RO_PACKAGE;
  return jv_l(jni_str(pkg));
}

H(h_setCanQuit) {
  g_can_quit = a[0].z;
  return jv_none();
}

/* quitApp(): finish() only when the game allowed it, as the Java does */
H(h_quitApp) {
  if (g_can_quit) {
    debugPrintf("[java] quitApp: closing the game\n");
    g_quit = 1;
  }
  return jv_none();
}

H(h_isTablet) { return jv_z(1); } /* the game's tablet layout suits a 1280x720 screen */

/* Assets: the APK's, through the zip directory (ro_assets.c) */
H(h_assetExists) { return jv_z(ro_asset_exists(jni_utf(a[0].l))); }

H(h_readAsset) {
  const char *name = jni_utf(a[0].l);
  size_t len = 0;
  uint8_t *p = ro_asset_load(name, &len);
  if (!p) {
    debugPrintf("[java] readAsset(%s): not in the APK\n", name);
    return jv_l(NULL);
  }
  JObj *arr = jni_array('B', (jsize)len);
  memcpy(arr->a.data, p, len);
  ro_asset_free(p);
  return jv_l(arr);
}

/* ignoreCasePath(path): the Java walks AssetManager.list() to find the
 * entry whatever its case. The SD card's FAT is case-insensitive already. */
H(h_ignoreCasePath) { return jv_l(jni_str(jni_utf(a[0].l))); }

H(h_makeDir) {
  char real[DCR_PATH_MAX];
  const char *p = dcr_translate_path(jni_utf(a[0].l), real, sizeof real);
  rt_mkdirs(p);
  return jv_z(1);
}

/* The pad, polled by the engine */
H(h_isGamePadConnected) { return jv_z(ro_pad_connected()); }
H(h_isGamePadKeyPressed) { return jv_z(ro_pad_key_pressed(a[0].i, a[1].i)); }
H(h_getGamePadAxisValues) {
  ro_pad_axis_values(a[0].i, a[1].i);
  return jv_none();
}

H(h_getDLCName) { return jv_l(jni_str("")); }

const JMethodDef jni_method_defs[] = {
    {"android/content/Context", "getPackageName", "()" S, h_getPackageName},
    {GA, "setCanQuit", "(Z)V", h_setCanQuit},
    {GA, "quitApp", "()V", h_quitApp},
    {GA, "isTablet", "()Z", h_isTablet},
    {GA, "assetExists", "(" S ")Z", h_assetExists},
    {GA, "readAsset", "(" S ")[B", h_readAsset},
    {GA, "ignoreCasePath", "(" S ")" S, h_ignoreCasePath},
    {GA, "makeDir", "(" S ")Z", h_makeDir},
    {GA, "isGamePadConnected", "()Z", h_isGamePadConnected},
    {GA, "isGamePadKeyPressed", "(II)Z", h_isGamePadKeyPressed},
    {GA, "getGamePadAxisValues", "(II)V", h_getGamePadAxisValues},
    {GA, "getDLCName", "(I)" S, h_getDLCName},
    /* no account, no network, no store: a phone that can do none of these */
    {GA, "isNetworkAvailable", "()Z", jni_h_false},
    {GA, "scoringEnabled", "()Z", jni_h_false},
    {GA, "isBillingTransactionsRestorable", "()Z", jni_h_false},
    {GA, "shareScore", "(" S "I)Z", jni_h_false},
    {GA, "isMogaControllerActive", "()Z", jni_h_false},
    {GA, "isMogaControllerConnected", "()Z", jni_h_false},
    {GA, "HasAccountsPermission", "()Z", jni_h_false},
    {GA, "HasPhoneStatePermission", "()Z", jni_h_false},
    {GA, "IsRemote", "()Z", jni_h_false},
    {GA, "shouldClearData", "()Z", jni_h_false},
    {GA, "showAchievements", "()V", jni_h_void},
    {GA, "showLeaderboards", "()V", jni_h_void},
    {GA, "showLeaderboard", "(" S ")V", jni_h_void},
    {GA, "unlockAchievement", "(" S ")V", jni_h_void},
    {GA, "showTextFieldAndKeyboard", "()V", jni_h_void},
    {GA, "hideTextFieldAndKeyboard", "()V", jni_h_void},
    {GA, "openURLInBrowser", "(" S ")V", jni_h_void},
    {GA, "openProductPage", "(" S ")V", jni_h_void},
    {GA, "openAmazonProductPage", "(" S ")V", jni_h_void},
    {GA, "setAssetFileAsWallPaper", "(" S ")V", jni_h_void},
    {GA, "storeBuyProduct", "(" S ")V", jni_h_void},
    {GA, "restoreTransactions", "()V", jni_h_void},
    {GA, "displayWarningDialog", "(" S S ")V", jni_h_void},
    {GA, "displayWarningDialogFromRes", "(" S S ")V", jni_h_void},
    {GA, "displayDashboard", "()V", jni_h_void},
    {GA, "onMoreGamesPressed", "()V", jni_h_void},
    {GA, "onPlayPressed", "()V", jni_h_void},
    {GA, "onDisplayRaymanChannel", "()V", jni_h_void},
    /* statistics (Flurry), the store, cross-promotion: nothing to report to */
    {"com/pastagames/android/stats/GameBehaviourLogger", NULL, NULL, jni_h_void},
    {"com/pastagames/android/stats/flurry/FlurryWrapper", NULL, NULL, jni_h_void},
    {"com/pastagames/android/store/Store", NULL, NULL, jni_h_void},
    {NULL, NULL, NULL, NULL},
};

const JFieldDef jni_field_defs[] = {
    {NULL, NULL, NULL, 0, NULL},
};

const char *const jni_class_supers[][2] = {
    {GA, "android/app/Activity"},
    {"android/app/Activity", "android/view/ContextThemeWrapper"},
    {"android/view/ContextThemeWrapper", "android/content/ContextWrapper"},
    {"android/content/ContextWrapper", "android/content/Context"},
    {"com/pastagames/android/stats/flurry/FlurryWrapper", "com/pastagames/android/stats/GameBehaviourLogger"},
    {NULL, NULL},
};

/* The game's own dex has no optional classes the engine probes for. */
const char *const jni_missing_classes[] = {
    NULL,
};

void ro_java_init(void) {
  jni_init();
  g_activity = jni_singleton(GA);
  debugPrintf("[java] GameActivity %p; package %s\n", (void *)g_activity,
              dcr_manifest_loaded() ? dcr_manifest_package() : RO_PACKAGE " (default)");
}
