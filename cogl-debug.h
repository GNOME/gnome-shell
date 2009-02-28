#ifndef __COGL_DEBUG_H__
#define __COGL_DEBUG_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  COGL_DEBUG_MISC      = 1 << 0,
  COGL_DEBUG_TEXTURE   = 1 << 1,
  COGL_DEBUG_MATERIAL  = 1 << 2,
  COGL_DEBUG_SHADER    = 1 << 3,
  COGL_DEBUG_OFFSCREEN = 1 << 4,
  COGL_DEBUG_DRAW      = 1 << 5,
  COGL_DEBUG_PANGO     = 1 << 6
} CoglDebugFlags;

#ifdef COGL_ENABLE_DEBUG

#ifdef __GNUC__
#define COGL_NOTE(type,x,a...)                  G_STMT_START {  \
        if (cogl_debug_flags & COGL_DEBUG_##type) {             \
          g_message ("[" #type "] " G_STRLOC ": " x, ##a);      \
        }                                       } G_STMT_END

#else
#define COGL_NOTE(type,...)                     G_STMT_START {  \
        if (cogl_debug_flags & COGL_DEBUG_##type) {             \
          gchar *_fmt = g_strdup_printf (__VA_ARGS__);          \
          g_message ("[" #type "] " G_STRLOC ": %s", _fmt);     \
          g_free (_fmt);                                        \
        }                                       } G_STMT_END

#endif /* __GNUC__ */

#else /* !COGL_ENABLE_DEBUG */

#define COGL_NOTE(type,...)

#endif /* COGL_ENABLE_DEBUG */

extern guint cogl_debug_flags;

G_END_DECLS

#endif /* __COGL_DEBUG_H__ */
