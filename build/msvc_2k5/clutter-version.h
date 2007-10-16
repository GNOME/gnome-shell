#ifndef __CLUTTER_VERSION_H__
#define __CLUTTER_VERSION_H__

/* Version info, needs to be set for each release
 * NB: this file lives in the msvc project directory, not
 *     the clutter root !!!
 */
#define CLUTTER_MAJOR_VERSION   0
#define CLUTTER_MINOR_VERSION   5
#define CLUTTER_MICRO_VERSION   0
#define CLUTTER_VERSION         0.5.0
#define CLUTTER_VERSION_S       "0.5.0"

#define CLUTTER_FLAVOUR         "sdl"
#define CLUTTER_COGL            "gl"
#define CLUTTER_NO_FPU          0

/* The rest needs no modificactions */

#define CLUTTER_VERSION_HEX     ((CLUTTER_MAJOR_VERSION << 24) | \
                                 (CLUTTER_MINOR_VERSION << 16) | \
                                 (CLUTTER_MICRO_VERSION << 8))
#define CLUTTER_CHECK_VERSION(major,minor,micro) \
        (CLUTTER_MAJOR_VERSION > (major) || \
         (CLUTTER_MAJOR_VERSION == (major) && CLUTTER_MINOR_VERSION > (minor)) || \
         (CLUTTER_MAJOR_VERSION == (major) && CLUTTER_MINOR_VERSION == (minor) && CLUTTER_MICRO_VERSION >= (micro)))

#endif /* __CLUTTER_VERSION_H__ */
