#ifndef __CLUTTER_DEBUG_H__
#define __CLUTTER_DEBUG_H__

#include <glib.h>
#include "clutter-main.h"

G_BEGIN_DECLS

typedef enum {
  CLUTTER_DEBUG_MISC                = 1 << 0,
  CLUTTER_DEBUG_ACTOR               = 1 << 1,
  CLUTTER_DEBUG_TEXTURE             = 1 << 2,
  CLUTTER_DEBUG_EVENT               = 1 << 3,
  CLUTTER_DEBUG_PAINT               = 1 << 4,
  CLUTTER_DEBUG_PANGO               = 1 << 5,
  CLUTTER_DEBUG_BACKEND             = 1 << 6,
  CLUTTER_DEBUG_SCHEDULER           = 1 << 7,
  CLUTTER_DEBUG_SCRIPT              = 1 << 8,
  CLUTTER_DEBUG_SHADER              = 1 << 9,
  CLUTTER_DEBUG_MULTISTAGE          = 1 << 10,
  CLUTTER_DEBUG_ANIMATION           = 1 << 11,
  CLUTTER_DEBUG_LAYOUT              = 1 << 12,
  CLUTTER_DEBUG_PICK                = 1 << 13,
  CLUTTER_DEBUG_EVENTLOOP           = 1 << 14,
  CLUTTER_DEBUG_CLIPPING            = 1 << 15,
  CLUTTER_DEBUG_OOB_TRANSFORMS      = 1 << 16
} ClutterDebugFlag;

typedef enum {
  CLUTTER_DEBUG_NOP_PICKING         = 1 << 0,
  CLUTTER_DEBUG_DUMP_PICK_BUFFERS   = 1 << 1
} ClutterPickDebugFlag;

typedef enum {
  CLUTTER_DEBUG_DISABLE_SWAP_EVENTS     = 1 << 0,
  CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS = 1 << 1,
  CLUTTER_DEBUG_REDRAWS                 = 1 << 2,
  CLUTTER_DEBUG_PAINT_VOLUMES           = 1 << 3,
  CLUTTER_DEBUG_DISABLE_CULLING         = 1 << 4,
  CLUTTER_DEBUG_DISABLE_OFFSCREEN_REDIRECT = 1 << 5,
  CLUTTER_DEBUG_CONTINUOUS_REDRAW       = 1 << 6,
  CLUTTER_DEBUG_PAINT_DEFORM_TILES      = 1 << 7
} ClutterDrawDebugFlag;

#ifdef CLUTTER_ENABLE_DEBUG

#define CLUTTER_HAS_DEBUG(type)         ((clutter_debug_flags & CLUTTER_DEBUG_##type) != FALSE)

#ifdef __GNUC__

/* Try the GCC extension for valists in macros */
#define CLUTTER_NOTE(type,x,a...)                       G_STMT_START {  \
        if (G_UNLIKELY (CLUTTER_HAS_DEBUG (type))) {                    \
          _clutter_debug_message ("[" #type "]:" G_STRLOC ": " x, ##a); \
        }                                               } G_STMT_END

#else /* !__GNUC__ */
/* Try the C99 version; unfortunately, this does not allow us to pass
 * empty arguments to the macro, which means we have to
 * do an intemediate printf.
 */
#define CLUTTER_NOTE(type,...)                          G_STMT_START {   \
        if (G_UNLIKELY (CLUTTER_HAS_DEBUG (type))) {                     \
          gchar *_fmt = g_strdup_printf (__VA_ARGS__);                   \
          _clutter_debug_message ("[" #type "]:" G_STRLOC ": %s", _fmt); \
          g_free (_fmt);                                                 \
        }                                               } G_STMT_END
#endif

#else /* !CLUTTER_ENABLE_DEBUG */

#define CLUTTER_NOTE(type,...)         G_STMT_START { } G_STMT_END
#define CLUTTER_HAS_DEBUG(type)        FALSE

#endif /* CLUTTER_ENABLE_DEBUG */

extern guint clutter_debug_flags;
extern guint clutter_pick_debug_flags;
extern guint clutter_paint_debug_flags;

void    _clutter_debug_messagev         (const char *format,
                                         va_list     var_args);
void    _clutter_debug_message          (const char *format,
                                         ...);

G_END_DECLS

#endif /* __CLUTTER_DEBUG_H__ */
