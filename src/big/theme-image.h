/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* Copyright 2008 litl, LLC. All Rights Reserved. */

#ifndef __BIG_THEME_IMAGE_H__
#define __BIG_THEME_IMAGE_H__

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define BIG_TYPE_THEME_IMAGE            (big_theme_image_get_type ())
#define BIG_THEME_IMAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), BIG_TYPE_THEME_IMAGE, BigThemeImage))
#define BIG_THEME_IMAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  BIG_TYPE_THEME_IMAGE, BigThemeImageClass))
#define BIG_IS_THEME_IMAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BIG_TYPE_THEME_IMAGE))
#define BIG_IS_THEME_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  BIG_TYPE_THEME_IMAGE))
#define BIG_THEME_IMAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  BIG_TYPE_THEME_IMAGE, BigThemeImageClass))

typedef struct BigThemeImage      BigThemeImage;
typedef struct BigThemeImageClass BigThemeImageClass;

GType          big_theme_image_get_type           (void) G_GNUC_CONST;

ClutterActor * big_theme_image_new_from_file      (const gchar *filename,
                                                   guint        border_top,
                                                   guint        border_bottom,
                                                   guint        border_left,
                                                   guint        border_right);

ClutterActor * big_theme_image_new_from_pixbuf    (GdkPixbuf   *pixbuf,
                                                   guint        border_top,
                                                   guint        border_bottom,
                                                   guint        border_left,
                                                   guint        border_right);

G_END_DECLS

#endif  /* __BIG_THEME_IMAGE_H__ */
