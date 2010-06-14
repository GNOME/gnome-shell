#ifndef __GDK_COMPAT_H__
#define __GDK_COMPAT_H__

#include <gdk/gdk.h>

/* Provide a compatibility layer for accessor function introduced
 * in GTK+ 2.22 which we need to build without deprecated GDK symbols.
 * That way it is still possible to build with GTK+ 2.18 when not
 * using GDK_DISABLE_DEPRECATED.
 */

#if !GTK_CHECK_VERSION (2, 21, 1)

#define gdk_window_get_background(w,c)    *c = GDK_WINDOW_OBJECT (w)->bg_color
#define gdk_visual_get_depth(v)           GDK_VISUAL(v)->depth
#define gdk_window_get_back_pixmap(w,p,r)                           \
  G_STMT_START {                                                    \
    GdkWindowObject *priv = GDK_WINDOW_OBJECT (w);                  \
                                                                    \
    if (p != NULL)                                                  \
      {                                                             \
        if (priv->bg_pixmap == GDK_PARENT_RELATIVE_BG ||            \
            priv->bg_pixmap == GDK_NO_BG)                           \
          *p = NULL;                                                \
        else                                                        \
          *p = priv->bg_pixmap;                                     \
      }                                                             \
                                                                    \
    if (r != NULL)                                                  \
      *r = (priv->bg_pixmap == GDK_PARENT_RELATIVE_BG);             \
  } G_STMT_END

#endif /*GTK_CHECK_VERSION */

#endif /* __GDK_COMPAT_H__ */
