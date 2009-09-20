/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include "st-theme-image.h"

struct _StThemeImage {
  GObject parent;

  char *filename;
  int border_top;
  int border_right;
  int border_bottom;
  int border_left;
};

struct _StThemeImageClass {
  GObjectClass parent_class;

};

G_DEFINE_TYPE (StThemeImage, st_theme_image, G_TYPE_OBJECT)

static void
st_theme_image_finalize (GObject *object)
{
  StThemeImage *image = ST_THEME_IMAGE (object);

  g_free (image->filename);

  G_OBJECT_CLASS (st_theme_image_parent_class)->finalize (object);
}

static void
st_theme_image_class_init (StThemeImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = st_theme_image_finalize;
}

static void
st_theme_image_init (StThemeImage *image)
{
}

StThemeImage *
st_theme_image_new (const char *filename,
                    int         border_top,
                    int         border_right,
                    int         border_bottom,
                    int         border_left)
{
  StThemeImage *image;

  image = g_object_new (ST_TYPE_THEME_IMAGE, NULL);

  image->filename = g_strdup (filename);
  image->border_top = border_top;
  image->border_right = border_right;
  image->border_bottom = border_bottom;
  image->border_left = border_left;

  return image;
}

const char *
st_theme_image_get_filename (StThemeImage *image)
{
  g_return_val_if_fail (ST_IS_THEME_IMAGE (image), NULL);

  return image->filename;
}

void
st_theme_image_get_borders (StThemeImage *image,
                            int          *border_top,
                            int          *border_right,
                            int          *border_bottom,
                            int          *border_left)
{
  g_return_if_fail (ST_IS_THEME_IMAGE (image));

  if (border_top)
    *border_top = image->border_top;
  if (border_right)
    *border_right = image->border_right;
  if (border_bottom)
    *border_bottom = image->border_bottom;
  if (border_left)
    *border_left = image->border_left;
}
