/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_THEME_IMAGE_H__
#define __ST_THEME_IMAGE_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* A StThemeImage encapsulates an image with specified unscaled borders on each edge.
 */
typedef struct _StThemeImage      StThemeImage;
typedef struct _StThemeImageClass StThemeImageClass;

#define ST_TYPE_THEME_IMAGE             (st_theme_image_get_type ())
#define ST_THEME_IMAGE(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), ST_TYPE_THEME_IMAGE, StThemeImage))
#define ST_THEME_IMAGE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_THEME_IMAGE, StThemeImageClass))
#define ST_IS_THEME_IMAGE(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), ST_TYPE_THEME_IMAGE))
#define ST_IS_THEME_IMAGE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_THEME_IMAGE))
#define ST_THEME_IMAGE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_THEME_IMAGE, StThemeImageClass))

GType             st_theme_image_get_type          (void) G_GNUC_CONST;

StThemeImage *st_theme_image_new (const char *filename,
                                  int         border_top,
                                  int         border_right,
                                  int         border_bottom,
                                  int         border_left);

const char *st_theme_image_get_filename (StThemeImage *image);
void        st_theme_image_get_borders  (StThemeImage *image,
                                         int          *border_top,
                                         int          *border_right,
                                         int          *border_bottom,
                                         int          *border_left);

G_END_DECLS

#endif /* __ST_THEME_IMAGE_H__ */
