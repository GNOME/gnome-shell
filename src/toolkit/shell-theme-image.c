/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include "shell-theme-image.h"

struct _ShellThemeImage {
  GObject parent;

  char *filename;
  int border_top;
  int border_right;
  int border_bottom;
  int border_left;
};

struct _ShellThemeImageClass {
  GObjectClass parent_class;

};

G_DEFINE_TYPE (ShellThemeImage, shell_theme_image, G_TYPE_OBJECT)

static void
shell_theme_image_finalize (GObject *object)
{
  ShellThemeImage *image = SHELL_THEME_IMAGE (object);

  g_free (image->filename);

  G_OBJECT_CLASS (shell_theme_image_parent_class)->finalize (object);
}

static void
shell_theme_image_class_init (ShellThemeImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = shell_theme_image_finalize;
}

static void
shell_theme_image_init (ShellThemeImage *image)
{
}

ShellThemeImage *
shell_theme_image_new (const char *filename,
                       int         border_top,
                       int         border_right,
                       int         border_bottom,
                       int         border_left)
{
  ShellThemeImage *image;

  image = g_object_new (SHELL_TYPE_THEME_IMAGE, NULL);

  image->filename = g_strdup (filename);
  image->border_top = border_top;
  image->border_right = border_right;
  image->border_bottom = border_bottom;
  image->border_left = border_left;

  return image;
}

const char *
shell_theme_image_get_filename (ShellThemeImage *image)
{
  g_return_val_if_fail (SHELL_IS_THEME_IMAGE (image), NULL);

  return image->filename;
}

void
shell_theme_image_get_borders (ShellThemeImage *image,
                               int             *border_top,
                               int             *border_right,
                               int             *border_bottom,
                               int             *border_left)
{
  g_return_if_fail (SHELL_IS_THEME_IMAGE (image));

  if (border_top)
    *border_top = image->border_top;
  if (border_right)
    *border_right = image->border_right;
  if (border_bottom)
    *border_bottom = image->border_bottom;
  if (border_left)
    *border_left = image->border_left;
}
