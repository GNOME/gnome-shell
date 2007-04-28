#ifndef __CLUTTER_DEBUG_H__
#define __CLUTTER_DEBUG_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  CLUTTER_DEBUG_MISC            = 1 << 0,
  CLUTTER_DEBUG_ACTOR           = 1 << 1,
  CLUTTER_DEBUG_TEXTURE         = 1 << 2,
  CLUTTER_DEBUG_EVENT           = 1 << 3,
  CLUTTER_DEBUG_PAINT           = 1 << 4,
  CLUTTER_DEBUG_GL              = 1 << 5,
  CLUTTER_DEBUG_ALPHA           = 1 << 6,
  CLUTTER_DEBUG_BEHAVIOUR       = 1 << 7,
  CLUTTER_DEBUG_PANGO           = 1 << 8,
  CLUTTER_DEBUG_BACKEND         = 1 << 9
} ClutterDebugFlag;

#ifdef CLUTTER_ENABLE_DEBUG

#define CLUTTER_NOTE(type,x,a...)               G_STMT_START {  \
        if (clutter_debug_flags & CLUTTER_DEBUG_##type)         \
          { g_message ("[" #type "] " G_STRLOC ": " x, ##a); }  \
                                                } G_STMT_END

#define CLUTTER_MARK()      CLUTTER_NOTE(MISC, "== mark ==")
#define CLUTTER_DBG(x) { a }

#define CLUTTER_GLERR()                         G_STMT_START {  \
        if (clutter_debug_flags & CLUTTER_DEBUG_GL)             \
          { GLenum _err = glGetError (); /* roundtrip */        \
            if (_err != GL_NO_ERROR)                            \
              g_warning (G_STRLOC ": GL Error %x", _err);       \
          }                                     } G_STMT_END

#else /* !CLUTTER_ENABLE_DEBUG */

#define CLUTTER_NOTE(type,x,a...)
#define CLUTTER_MARK()
#define CLUTTER_DBG(x)
#define CLUTTER_GLERR()

#endif /* CLUTTER_ENABLE_DEBUG */

extern guint clutter_debug_flags;

G_END_DECLS

#endif /* __CLUTTER_DEBUG_H__ */
