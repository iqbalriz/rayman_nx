/* port_config.h -- Rayman Jungle Run's settings for the android32 runtime.
 *
 * Macros only: the runtime's C files, its assembly and the launcher all read
 * this (runtime/source/rt_settings.h). What each setting does is next to its
 * default in the runtime; runtime/docs/ lists them all. MIT.
 */
#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

/* ------------------------------------------------------------------ the game */
#define PORT_TITLE    "Rayman Jungle Run"
#define PORT_NAME     "rayman_nx"
#define PORT_PACKAGE  "com.pastagames.ro1mobile"
#define PORT_BANNER   "rayman_nx: Rayman Jungle Run (Pasta Games engine, armeabi-v7a)"
/* libRO1Mobile.so is 6 MB on disk; each module is mapped into its own region
 * (a larger module is refused by so_load, -3) */
#define PORT_SO_REGION_BYTES (24u * 1024 * 1024)

/* The APK, by what is in it -- the game's engine and its starting data --
 * whatever it is called; with several, the highest version code. */
#define PORT_APK_DESC "Rayman Jungle Run (com.pastagames.ro1mobile, armeabi-v7a)"
#define PORT_APK_ROLES                                                                       \
  {.what = "the game",                                                                       \
   .need = (const char *const[]){"lib/armeabi-v7a/libRO1Mobile.so",                          \
                                 "lib/armeabi-v7a/libfmodex.so", NULL},                      \
   .flags = RT_APK_HIGHEST_VERSION}

/* ------------------------------------------------------------------ launcher */
#define PORT_LAUNCHER_START_NOTE "(the first start unpacks the game's libraries from the APK)"
#define PORT_LAUNCHER_BYLINE     "by Iqbalriz (the Switch port); the game by Ubisoft Pasta Games"

/* ------------------------------------------------------------------ sound, input, frames */
/* FMOD Ex dlopens libOpenSLES.so and plays through it: audout implements it. */
#define RT_OPENSLES           1
#define RT_BOOST_WATCH_THREAD 1  /* boost the long (loading) frames */
#define RT_PAD_MAX_PLAYERS    1  /* one player: the game's pad reads are for player 1 */

#endif
