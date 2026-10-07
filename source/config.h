/* config.h -- Rayman Jungle Run's own constants (the runtime's settings are in
 * port_config.h).
 *
 * Rayman Jungle Run (com.pastagames.ro1mobile 2.4.3, Ubisoft Pasta Games),
 * armeabi-v7a: libRO1Mobile.so (the whole game: engine, GLES 1 and 2 renderers)
 * and libfmodex.so (FMOD Ex, through OpenSL ES). MIT.
 */
#ifndef RO_CONFIG_H
#define RO_CONFIG_H

#include "rt_settings.h"

#define RO_LIB_GAME "libRO1Mobile.so"
#define RO_LIB_FMOD "libfmodex.so"
#define RO_PACKAGE  PORT_PACKAGE

/* What the Java did with these, at the paths the runtime maps onto the SD
 * card (runtime/source/dcr_path.c):
 *   browse directory  /storage/emulated/0/Android/data/<pkg>/files -> <root>/external/files
 *                     the game's data (actor, lvl, gfx, ...) lives here
 *   root directory    /data/data/<pkg>/files/saves                  -> <root>/data/files/saves */
#define RO_DATA_DIR  "/storage/emulated/0/Android/data/" RO_PACKAGE "/files"
#define RO_SAVES_DIR "/data/data/" RO_PACKAGE "/files/saves"

#endif /* RO_CONFIG_H */
