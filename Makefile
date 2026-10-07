#---------------------------------------------------------------------------------
# Rayman Jungle Run -- Nintendo Switch wrapper (32-bit / AArch32)
#
# Ships NO game code and NO game assets: the game's own APK (the user's copy,
# any file name) is read at run time; its libraries are unpacked from it on
# the first launch, its assets read from it at every start (source/ro_assets.c),
# and its data folder is the player's own.
#
# The build is the android32 runtime's (runtime/runtime.mk: devkitARM +
# libnx32 + mesa32 from portlibs32/); ./build.sh (or build.ps1 on Windows) runs
# it in the toolchain container. Output: rayman_nx.nsp, which the launcher NRO
# carries (launcher/).
#---------------------------------------------------------------------------------
TARGET               := rayman_nx
PORT_NPDM_PROGRAM_ID := 0x010093A06B84BFD6
PORT_NPDM_VERSION    := 0.1.0
PORT_NPDM_MAIN_STACK := 0x400000
include runtime/runtime.mk
