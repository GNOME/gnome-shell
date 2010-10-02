#ifndef __GDK_COMPAT_H__
#define __GDK_COMPAT_H__

#include <gdk/gdk.h>
#include <math.h>

/* Provide a compatibility layer for accessor function introduced
 * in GTK+ 2.22 which we need to build without deprecated GDK symbols.
 * That way it is still possible to build with GTK+ 2.18 when not
 * using GDK_DISABLE_DEPRECATED.
 */

#if !GTK_CHECK_VERSION (2, 21, 1)

#define gdk_visual_get_depth(v)           GDK_VISUAL(v)->depth

#endif /* GTK_CHECK_VERSION (2, 21, 1) */

#if !GTK_CHECK_VERSION (2, 90, 8)

#define gdk_window_get_screen gdk_drawable_get_screen
#define gdk_pixbuf_get_from_window(window, src_x, src_y, width, height) \
    gdk_pixbuf_get_from_drawable(NULL, window, NULL, src_x, src_y, 0, 0, width, height)

static inline int
gdk_window_get_width (GdkWindow *window)
{
  int width;

  gdk_drawable_get_size (window, &width, NULL);

  return width;
}

static inline int
gdk_window_get_height (GdkWindow *window)
{
  int height;

  gdk_drawable_get_size (window, NULL, &height);

  return height;
}


static inline gboolean
gdk_cairo_get_clip_rectangle (cairo_t      *cr,
                              GdkRectangle *rect)
{
  double x1, y1, x2, y2;
  gboolean clip_exists;

  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

  clip_exists = x1 < x2 && y1 < y2;

  if (rect)
    {
      x1 = floor (x1);
      y1 = floor (y1);
      x2 = ceil (x2);
      y2 = ceil (y2);

      rect->x      = CLAMP (x1,      G_MININT, G_MAXINT);
      rect->y      = CLAMP (y1,      G_MININT, G_MAXINT);
      rect->width  = CLAMP (x2 - x1, G_MININT, G_MAXINT);
      rect->height = CLAMP (y2 - y1, G_MININT, G_MAXINT);
    }

  return clip_exists;
}

#endif /* GTK_CHECK_VERSION (2, 90, 8) */

/* Compatibility with old GDK key symbols */
#ifndef GDK_KEY_Escape
#define GDK_KEY_Escape GDK_Escape
#endif /* GDK_KEY_Escape */

#endif /* __GDK_COMPAT_H__ */
