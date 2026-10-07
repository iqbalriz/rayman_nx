/* ro_assets.c -- the game's APK assets, and the few libc shims the runtime
 * does not have.
 *
 * libRO1Mobile.so reads its APK assets through the NDK's AAssetManager (config.bin,
 * the Localizable.strings, the font pictures: assets/ in the APK) and, in a few
 * places, through GameActivity.readAsset. Both end here: an entry of the player's
 * own APK, inflated whole (they are small) out of the zip's directory. The
 * AAssetManager is only a tag: the Java object nativeSetAssetManager hands over
 * has nothing the shims need.
 *
 * The shims are named b_<symbol> (tools/gen_imports.py binds an import to the
 * b_ function of its name). MIT.
 */
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif
#include <miniz/miniz.h>

#include "dcr_config.h"
#include "dcr_path.h"
#include "ro.h"
#include "util.h"

/* ------------------------------------------------------------ the APK */
static mz_zip_archive g_zip;
static int g_zip_state; /* 0 not tried, 1 open, -1 failed */
static Mutex g_zip_lock;

static int zip_open(void) {
  if (g_zip_state)
    return g_zip_state > 0;
  memset(&g_zip, 0, sizeof g_zip);
  if (!mz_zip_reader_init_file(&g_zip, dcr_apk_path(), 0)) {
    debugPrintf("[assets] cannot open the APK %s\n", dcr_apk_path());
    g_zip_state = -1;
    return 0;
  }
  g_zip_state = 1;
  return 1;
}

/* "assets/<name>"; a leading "/" or "./" on name is dropped. */
static int entry_of(const char *name, char *out, size_t cap) {
  while (name[0] == '/' || (name[0] == '.' && name[1] == '/'))
    name += name[0] == '/' ? 1 : 2;
  if (snprintf(out, cap, "assets/%s", name) >= (int)cap)
    return -1;
  return 0;
}

uint8_t *ro_asset_load(const char *name, size_t *len) {
  char entry[512];
  if (entry_of(name, entry, sizeof entry))
    return NULL;
  uint8_t *p = NULL;
  size_t n = 0;
  mutexLock(&g_zip_lock);
  if (zip_open())
    p = mz_zip_reader_extract_file_to_heap(&g_zip, entry, &n, 0);
  mutexUnlock(&g_zip_lock);
  if (len)
    *len = n;
  return p;
}

void ro_asset_free(void *p) { mz_free(p); }

int ro_asset_exists(const char *name) {
  char entry[512];
  if (entry_of(name, entry, sizeof entry))
    return 0;
  mutexLock(&g_zip_lock);
  const int ok = zip_open() && mz_zip_reader_locate_file(&g_zip, entry, NULL, 0) >= 0;
  mutexUnlock(&g_zip_lock);
  return ok;
}

/* ------------------------------------------------------------ file trace */
/* The runtime logs the translation of the paths this answers 1 for (the first
 * 64, and the first opens: dcr_path.c, bionic_io.c). The engine's own data
 * files, which is where this port's bring-up failed: how it names them, and
 * where they end up on the SD card, is what debug.log must show. */
int dcr_path_traced(const char *p) {
  if (!p)
    return 0;
  if (strstr(p, ".apk"))
    return 1;
  return dcr_config()->log_files && (strstr(p, ".lproj") || strstr(p, "pasta/") || strstr(p, "gfx/") ||
                                     strstr(p, "sfx/") || strstr(p, "saves") || strstr(p, "external"));
}

/* -------------------------------------------------------------- NDK AAsset */
typedef struct {
  uint8_t *data;
  size_t len, pos;
} RoAsset;

static int g_mgr_tag;
static unsigned g_open_logged;

void *b_AAssetManager_fromJava(void *env, void *asset_manager) {
  (void)env, (void)asset_manager;
  return &g_mgr_tag;
}

void *b_AAssetManager_open(void *mgr, const char *filename, int mode) {
  (void)mgr, (void)mode;
  if (!filename)
    return NULL;
  size_t len = 0;
  uint8_t *p = ro_asset_load(filename, &len);
  if (!p) {
    if (g_open_logged++ < 16)
      debugPrintf("[assets] AAssetManager_open(%s): not in the APK\n", filename);
    return NULL;
  }
  RoAsset *a = calloc(1, sizeof *a);
  if (!a) {
    ro_asset_free(p);
    return NULL;
  }
  a->data = p;
  a->len = len;
  return a;
}

void b_AAsset_close(void *asset) {
  RoAsset *a = asset;
  if (!a)
    return;
  ro_asset_free(a->data);
  free(a);
}

/* off_t is 32 bits in bionic's armeabi-v7a */
int32_t b_AAsset_getLength(void *asset) { return asset ? (int32_t)((RoAsset *)asset)->len : 0; }

int b_AAsset_read(void *asset, void *buf, size_t count) {
  RoAsset *a = asset;
  if (!a || !buf)
    return -1;
  size_t left = a->len - a->pos;
  if (count > left)
    count = left;
  memcpy(buf, a->data + a->pos, count);
  a->pos += count;
  return (int)count;
}

/* ------------------------------------------------------------- libc shims */
/* zlib's adler32: miniz has it (the engine's own zlib is linked in; this is
 * a leftover import). */
unsigned int b_adler32(unsigned int adler, const unsigned char *buf, unsigned int len) {
  return (unsigned int)mz_adler32(adler, buf, len);
}

/* The files are the SD card's: nobody owns them. */
int b_chown(const char *path, unsigned int owner, unsigned int group) {
  (void)path, (void)owner, (void)group;
  return 0;
}

/* wchar_t is 32 bits on bionic and on newlib's ARM. */
uint32_t *b_wcscpy(uint32_t *dst, const uint32_t *src) {
  uint32_t *d = dst;
  while ((*d++ = *src++))
    ;
  return dst;
}

uint32_t *b_wcsncpy(uint32_t *dst, const uint32_t *src, size_t n) {
  size_t i = 0;
  for (; i < n && src[i]; i++)
    dst[i] = src[i];
  for (; i < n; i++)
    dst[i] = 0;
  return dst;
}

/* UTF-8, as bionic's C locale does for the code points the game has. */
size_t b_wcstombs(char *dst, const uint32_t *src, size_t n) {
  size_t w = 0;
  for (; *src; src++) {
    const uint32_t c = *src;
    char tmp[4];
    unsigned k;
    if (c < 0x80) {
      tmp[0] = (char)c, k = 1;
    } else if (c < 0x800) {
      tmp[0] = (char)(0xC0 | (c >> 6)), tmp[1] = (char)(0x80 | (c & 0x3F)), k = 2;
    } else if (c < 0x10000) {
      tmp[0] = (char)(0xE0 | (c >> 12)), tmp[1] = (char)(0x80 | ((c >> 6) & 0x3F)),
      tmp[2] = (char)(0x80 | (c & 0x3F)), k = 3;
    } else {
      tmp[0] = (char)(0xF0 | (c >> 18)), tmp[1] = (char)(0x80 | ((c >> 12) & 0x3F)),
      tmp[2] = (char)(0x80 | ((c >> 6) & 0x3F)), tmp[3] = (char)(0x80 | (c & 0x3F)), k = 4;
    }
    if (dst) {
      if (w + k > n)
        return w;
      memcpy(dst + w, tmp, k);
    }
    w += k;
  }
  if (dst && w < n)
    dst[w] = 0;
  return w;
}
