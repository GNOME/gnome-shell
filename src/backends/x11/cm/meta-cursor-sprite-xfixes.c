/*
 * Copyright 2013, 2018 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "backends/x11/cm/meta-cursor-sprite-xfixes.h"

#include <X11/extensions/Xfixes.h>

#include "core/display-private.h"
#include "meta/meta-x11-display.h"

enum
{
  PROP_0,

  PROP_DISPLAY,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

struct _MetaCursorSpriteXfixes
{
  MetaCursorSprite parent;

  MetaDisplay *display;
};

static void
meta_screen_cast_xfixes_init_initable_iface (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaCursorSpriteXfixes,
                         meta_cursor_sprite_xfixes,
                         META_TYPE_CURSOR_SPRITE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                meta_screen_cast_xfixes_init_initable_iface))

static void
meta_cursor_sprite_xfixes_realize_texture (MetaCursorSprite *sprite)
{
}

static gboolean
meta_cursor_sprite_xfixes_is_animated (MetaCursorSprite *sprite)
{
  return FALSE;
}

static void
meta_cursor_sprite_xfixes_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  MetaCursorSpriteXfixes *sprite_xfixes = META_CURSOR_SPRITE_XFIXES (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, sprite_xfixes->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_cursor_sprite_xfixes_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  MetaCursorSpriteXfixes *sprite_xfixes = META_CURSOR_SPRITE_XFIXES (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      sprite_xfixes->display = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

MetaCursorSpriteXfixes *
meta_cursor_sprite_xfixes_new (MetaDisplay  *display,
                               GError      **error)
{
  return g_initable_new (META_TYPE_CURSOR_SPRITE_XFIXES,
                         NULL, error,
                         "display", display,
                         NULL);
}

static gboolean
meta_cursor_sprite_xfixes_initable_init (GInitable     *initable,
                                         GCancellable  *cancellable,
                                         GError       **error)
{
  MetaCursorSpriteXfixes *sprite_xfixes =
    META_CURSOR_SPRITE_XFIXES (initable);
  MetaCursorSprite *sprite = META_CURSOR_SPRITE (sprite_xfixes);
  MetaX11Display *x11_display;
  Display *xdisplay;
  XFixesCursorImage *cursor_image;
  CoglTexture2D *texture;
  uint8_t *cursor_data;
  gboolean free_cursor_data;
  ClutterBackend *clutter_backend;
  CoglContext *cogl_context;

  x11_display = meta_display_get_x11_display (sprite_xfixes->display);
  xdisplay = meta_x11_display_get_xdisplay (x11_display);
  cursor_image = XFixesGetCursorImage (xdisplay);
  if (!cursor_image)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to get cursor image");
      return FALSE;
    }

  /*
   * Like all X APIs, XFixesGetCursorImage() returns arrays of 32-bit
   * quantities as arrays of long; we need to convert on 64 bit
   */
  if (sizeof (long) == 4)
    {
      cursor_data = (uint8_t *) cursor_image->pixels;
      free_cursor_data = FALSE;
    }
  else
    {
      int i, j;
      uint32_t *cursor_words;
      unsigned long *p;
      uint32_t *q;

      cursor_words = g_new (uint32_t,
                            cursor_image->width * cursor_image->height);
      cursor_data = (uint8_t *) cursor_words;

      p = cursor_image->pixels;
      q = cursor_words;
      for (j = 0; j < cursor_image->height; j++)
        {
          for (i = 0; i < cursor_image->width; i++)
            *(q++) = *(p++);
        }

      free_cursor_data = TRUE;
    }

  clutter_backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  texture = cogl_texture_2d_new_from_data (cogl_context,
                                          cursor_image->width,
                                          cursor_image->height,
                                          CLUTTER_CAIRO_FORMAT_ARGB32,
                                          cursor_image->width * 4, /* stride */
                                          cursor_data,
                                          error);

  if (free_cursor_data)
    g_free (cursor_data);

  if (!sprite)
    return FALSE;

  meta_cursor_sprite_set_texture (sprite,
                                  COGL_TEXTURE (texture),
                                  cursor_image->xhot,
                                  cursor_image->yhot);
  cogl_object_unref (texture);
  XFree (cursor_image);

  return TRUE;
}

static void
meta_screen_cast_xfixes_init_initable_iface (GInitableIface *iface)
{
  iface->init = meta_cursor_sprite_xfixes_initable_init;
}

static void
meta_cursor_sprite_xfixes_init (MetaCursorSpriteXfixes *sprite_xfixes)
{
}

static void
meta_cursor_sprite_xfixes_class_init (MetaCursorSpriteXfixesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MetaCursorSpriteClass *cursor_sprite_class = META_CURSOR_SPRITE_CLASS (klass);

  object_class->get_property = meta_cursor_sprite_xfixes_get_property;
  object_class->set_property = meta_cursor_sprite_xfixes_set_property;

  cursor_sprite_class->realize_texture =
    meta_cursor_sprite_xfixes_realize_texture;
  cursor_sprite_class->is_animated = meta_cursor_sprite_xfixes_is_animated;

  obj_props[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         "display",
                         "MetaDisplay",
                         META_TYPE_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, obj_props);
}
