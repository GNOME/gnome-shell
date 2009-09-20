/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include "st-border-image.h"

struct _StBorderImage {
  GObject parent;

  char *filename;
  int border_top;
  int border_right;
  int border_bottom;
  int border_left;
};

struct _StBorderImageClass {
  GObjectClass parent_class;

};

G_DEFINE_TYPE (StBorderImage, st_border_image, G_TYPE_OBJECT)

static void
st_border_image_finalize (GObject *object)
{
  StBorderImage *image = ST_BORDER_IMAGE (object);

  g_free (image->filename);

  G_OBJECT_CLASS (st_border_image_parent_class)->finalize (object);
}

static void
st_border_image_class_init (StBorderImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = st_border_image_finalize;
}

static void
st_border_image_init (StBorderImage *image)
{
}

StBorderImage *
st_border_image_new (const char *filename,
                       int         border_top,
                       int         border_right,
                       int         border_bottom,
                       int         border_left)
{
  StBorderImage *image;

  image = g_object_new (ST_TYPE_BORDER_IMAGE, NULL);

  image->filename = g_strdup (filename);
  image->border_top = border_top;
  image->border_right = border_right;
  image->border_bottom = border_bottom;
  image->border_left = border_left;

  return image;
}

const char *
st_border_image_get_filename (StBorderImage *image)
{
  g_return_val_if_fail (ST_IS_BORDER_IMAGE (image), NULL);

  return image->filename;
}

void
st_border_image_get_borders (StBorderImage *image,
                             int           *border_top,
                             int           *border_right,
                             int           *border_bottom,
                             int           *border_left)
{
  g_return_if_fail (ST_IS_BORDER_IMAGE (image));

  if (border_top)
    *border_top = image->border_top;
  if (border_right)
    *border_right = image->border_right;
  if (border_bottom)
    *border_bottom = image->border_bottom;
  if (border_left)
    *border_left = image->border_left;
}
