#ifndef _APP_VERSION_H_
#define _APP_VERSION_H_

/* The template values come from cmake/version.cmake
 * BUILD_VERSION related template values will be 'git describe',
 * alternatively user defined BUILD_VERSION.
 */

/* #undef ZEPHYR_VERSION_CODE */
/* #undef ZEPHYR_VERSION */

#define APPVERSION                   0x2090000
#define APP_VERSION_NUMBER           0x20900
#define APP_VERSION_MAJOR            2
#define APP_VERSION_MINOR            9
#define APP_PATCHLEVEL               0
#define APP_TWEAK                    0
#define APP_VERSION_STRING           "2.9.0"
#define APP_VERSION_EXTENDED_STRING  "2.9.0+0"
#define APP_VERSION_TWEAK_STRING     "2.9.0+0"

#define APP_BUILD_VERSION 769ce3148ef4


#endif /* _APP_VERSION_H_ */
