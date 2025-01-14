#define BUILD_TIME 19:29:37
#define BUILD_DATE Sep 4 2021
#define BUILD_CHAR ANSI
#define BUILD_CPU  X86
#define BUILD_COMP Visual C++ 13.2
#ifndef _MSC_VER
#define _MSC_VER  1928
#endif
#define VER_MAJOR  0
#define VER_MINOR  2
#define VER_BETA  97
#define VER_ALPHA 44
#define BURN_VERSION (VER_MAJOR * 0x100000) + (VER_MINOR * 0x010000) + (((VER_BETA / 10) * 0x001000) + ((VER_BETA % 10) * 0x000100)) + (((VER_ALPHA / 10) * 0x000010) + (VER_ALPHA % 10))
#define FIGHTCADE_VERSION 38

#define MAKE_STRING_2(s) #s
#define MAKE_STRING(s) MAKE_STRING_2(s)

#ifdef FBNEO_DEBUG
#define APP_TITLE "Moscade FBNeo [DEBUG]"
#else
#define APP_TITLE "Moscade FBNeo"
#endif
