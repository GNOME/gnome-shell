/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Johan Bilien  <johan.bilien@nokia.com>
 *
 * Copyright (C) 2007 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-x11-texture-pixmap
 * @short_description: A texture which displays the content of an X Pixmap.
 *
 * #ClutterX11TexturePixmap is a class for displaying the content of an
 * X Pixmap as a ClutterActor. Used together with the X Composite extension,
 * it allows to display the content of X Windows inside Clutter.
 *
 * The class uses the GLX_EXT_texture_from_pixmap OpenGL extension
 * (http://people.freedesktop.org/~davidr/GLX_EXT_texture_from_pixmap.txt)
 * if available
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../clutter-marshal.h"
#include "clutter-x11-texture-pixmap.h"
#include "clutter-x11.h"
#include "clutter-backend-x11.h"

#include "cogl.h"

enum
{
  PROP_PIXMAP = 1,
  PROP_PIXMAP_WIDTH,
  PROP_PIXMAP_HEIGHT,
  PROP_DEPTH
};

enum
{
  UPDATE_AREA,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

struct _ClutterX11TexturePixmapPrivate
{
  Pixmap        pixmap;
  guint         pixmap_width, pixmap_height;
  guint         depth;

  XImage       *image;

};

static ClutterBackendX11 *backend = NULL;

static void clutter_x11_texture_pixmap_class_init (ClutterX11TexturePixmapClass *klass);
static void clutter_x11_texture_pixmap_init       (ClutterX11TexturePixmap *self);
static GObject *clutter_x11_texture_pixmap_constructor (GType                  type,
                                                        guint                  n_construct_properties,
                                                        GObjectConstructParam *construct_properties);
static void clutter_x11_texture_pixmap_dispose    (GObject *object);
static void clutter_x11_texture_pixmap_set_property (GObject      *object,
                                                     guint         prop_id,
                                                     const GValue *value,
                                                     GParamSpec   *pspec);
static void clutter_x11_texture_pixmap_get_property (GObject      *object,
                                                     guint         prop_id,
                                                     GValue       *value,
                                                     GParamSpec   *pspec);

static void clutter_x11_texture_pixmap_realize (ClutterActor *actor);
static void clutter_x11_texture_pixmap_update_area_real (ClutterX11TexturePixmap *texture,
                                                         gint x,
                                                         gint y,
                                                         gint width,
                                                         gint height);

G_DEFINE_TYPE (ClutterX11TexturePixmap, clutter_x11_texture_pixmap, CLUTTER_TYPE_TEXTURE);

static void
clutter_x11_texture_pixmap_class_init (ClutterX11TexturePixmapClass *klass)
{
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec        *pspec;
  ClutterBackend    *default_backend;

  g_type_class_add_private (klass, sizeof (ClutterX11TexturePixmapPrivate));

  object_class->constructor = clutter_x11_texture_pixmap_constructor;
  object_class->dispose = clutter_x11_texture_pixmap_dispose;
  object_class->set_property = clutter_x11_texture_pixmap_set_property;
  object_class->get_property = clutter_x11_texture_pixmap_get_property;

  actor_class->realize   = clutter_x11_texture_pixmap_realize;

  klass->update_area = clutter_x11_texture_pixmap_update_area_real;

  pspec = g_param_spec_uint ("pixmap",
                             "Pixmap",
                             "The X Pixmap to which this texture will be bound",
                             0, G_MAXINT,
                             None,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_PIXMAP,
                                   pspec);

  pspec = g_param_spec_uint ("pixmap-width",
                             "Pixmap width",
                             "The width of the "
                             "pixmap bound to this texture",
                             0, G_MAXUINT,
                             0,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_PIXMAP_WIDTH,
                                   pspec);

  pspec = g_param_spec_uint ("pixmap-height",
                             "Pixmap height",
                             "The height of the "
                             "pixmap bound to this texture",
                             0, G_MAXUINT,
                             0,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_PIXMAP_HEIGHT,
                                   pspec);

  pspec = g_param_spec_uint ("depth",
                             "Depth",
                             "The depth (in number of bits) of the "
                             "pixmap bound to this texture",
                             0, G_MAXUINT,
                             0,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_DEPTH,
                                   pspec);
  /**
   * ClutterX11TexturePixmap::update-area:
   * @texture: the object which received the signal
   *
   * The ::hide signal is emitted to ask the texture to update its
   * content from its source pixmap.
   *
   * Since: 0.8
   */
  signals[UPDATE_AREA] =
      g_signal_new ("update-area",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (ClutterX11TexturePixmapClass, update_area),
                    NULL, NULL,
                    clutter_marshal_VOID__INT_INT_INT_INT,
                    G_TYPE_NONE, 4,
                    G_TYPE_INT,
                    G_TYPE_INT,
                    G_TYPE_INT,
                    G_TYPE_INT);

  default_backend = clutter_get_default_backend ();
  if (!CLUTTER_IS_BACKEND_X11 (default_backend))
    {
      g_critical ("ClutterX11TexturePixmap instanciated with a "
                  "non-X11 backend");
      return;
    }

  backend = (ClutterBackendX11 *)default_backend;

}

static void
clutter_x11_texture_pixmap_init (ClutterX11TexturePixmap *self)
{
  self->priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   CLUTTER_X11_TYPE_TEXTURE_PIXMAP,
                                   ClutterX11TexturePixmapPrivate);

}

static GObject *
clutter_x11_texture_pixmap_constructor (GType                  type,
                                        guint                  n_construct_properties,
                                        GObjectConstructParam *construct_properties)
{
  GObject *object = G_OBJECT_CLASS (clutter_x11_texture_pixmap_parent_class)->
      constructor (type, n_construct_properties, construct_properties);

  g_object_set (object,
                "sync-size", FALSE,
                NULL);

  return object;
}

static void
clutter_x11_texture_pixmap_dispose (GObject *object)
{
  ClutterX11TexturePixmap        *texture = CLUTTER_X11_TEXTURE_PIXMAP (object);
  ClutterX11TexturePixmapPrivate *priv = texture->priv;

  if (priv->image)
    {
      XDestroyImage (priv->image);
      priv->image = NULL;
    }

  G_OBJECT_CLASS (clutter_x11_texture_pixmap_parent_class)->dispose (object);

}

static void
clutter_x11_texture_pixmap_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ClutterX11TexturePixmap        *texture = CLUTTER_X11_TEXTURE_PIXMAP (object);
  ClutterX11TexturePixmapPrivate *priv = texture->priv;

  switch (prop_id)
    {
    case PROP_PIXMAP:
        clutter_x11_texture_pixmap_set_pixmap (texture,
                                               g_value_get_uint (value),
                                               priv->pixmap_width,
                                               priv->pixmap_height,
                                               priv->depth);
        break;
    case PROP_PIXMAP_WIDTH:
        clutter_x11_texture_pixmap_set_pixmap (texture,
                                               priv->pixmap,
                                               g_value_get_uint (value),
                                               priv->pixmap_height,
                                               priv->depth);
        break;
    case PROP_PIXMAP_HEIGHT:
        clutter_x11_texture_pixmap_set_pixmap (texture,
                                               priv->pixmap,
                                               priv->pixmap_width,
                                               g_value_get_uint (value),
                                               priv->depth);
        break;
    case PROP_DEPTH:
        clutter_x11_texture_pixmap_set_pixmap (texture,
                                               priv->pixmap,
                                               priv->pixmap_width,
                                               priv->pixmap_height,
                                               g_value_get_uint (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
clutter_x11_texture_pixmap_get_property (GObject      *object,
                                         guint         prop_id,
                                         GValue       *value,
                                         GParamSpec   *pspec)
{
  ClutterX11TexturePixmap        *texture = CLUTTER_X11_TEXTURE_PIXMAP (object);
  ClutterX11TexturePixmapPrivate *priv = texture->priv;

  switch (prop_id)
    {
    case PROP_PIXMAP:
        g_value_set_uint (value, priv->pixmap);
        break;
    case PROP_PIXMAP_WIDTH:
        g_value_set_uint (value, priv->pixmap_width);
        break;
    case PROP_PIXMAP_HEIGHT:
        g_value_set_uint (value, priv->pixmap_height);
        break;
    case PROP_DEPTH:
        g_value_set_uint (value, priv->depth);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
clutter_x11_texture_pixmap_realize (ClutterActor *actor)
{
  ClutterX11TexturePixmap        *texture = CLUTTER_X11_TEXTURE_PIXMAP (actor);
  ClutterX11TexturePixmapPrivate *priv = texture->priv;

  CLUTTER_ACTOR_CLASS (clutter_x11_texture_pixmap_parent_class)->
      realize (actor);

  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);

  clutter_x11_texture_pixmap_update_area_real (texture,
					       0, 0,
					       priv->pixmap_width,
					       priv->pixmap_height);
}

static void
clutter_x11_texture_pixmap_update_area_real (ClutterX11TexturePixmap *texture,
                                             gint                     x,
                                             gint                     y,
                                             gint                     width,
                                             gint                     height)
{
  ClutterX11TexturePixmapPrivate       *priv;
  Display                              *dpy;
  XImage                               *image;
  guint                                *pixel, *l;
  GError                               *error = NULL;
  guint                                 bytes_per_line;
  guint8                               *data;
  gboolean                              data_allocated = FALSE;
  int                                   err_code;

  if (!CLUTTER_ACTOR_IS_REALIZED (texture))
    return;

  priv = texture->priv;
  dpy  = ((ClutterBackendX11 *)backend)->xdpy;

  clutter_x11_trap_x_errors ();
  if (!priv->image)
    priv->image = XGetImage (dpy,
                             priv->pixmap,
                             0, 0,
                             priv->pixmap_width, priv->pixmap_height,
                             AllPlanes,
                             ZPixmap);
  else
    XGetSubImage (dpy,
                  priv->pixmap,
                  x, y,
                  width, height,
                  AllPlanes,
                  ZPixmap,
                  priv->image,
                  x, y);

  XSync (dpy, FALSE);
  if ((err_code = clutter_x11_untrap_x_errors ()))
    return;

  image = priv->image;

  if (priv->depth == 24)
    {
      guint *first_line = (guint *)image->data + y * image->bytes_per_line / 4;

      for (l =   first_line;
           l != (first_line + height * image->bytes_per_line / 4);
           l = l + image->bytes_per_line / 4)
        {
          for (pixel = l + x; pixel != l + x + width; pixel ++)
            {
              ((guint8 *)pixel)[3] = 0xFF;
            }
        }

      data = (guint8 *)first_line + x * 4;
      bytes_per_line = image->bytes_per_line;
    }

  else if (priv->depth == 16)
    {
      guint16 *p, *lp;

      data = g_malloc (height * width * 4);
      data_allocated = TRUE;
      bytes_per_line = priv->pixmap_width * 4;

      for (l =   (guint *)data,
           lp = (guint16 *)image->data + y * image->bytes_per_line / 2;
           l != ((guint *)data + height * width);
           l = l + width, lp = lp + image->bytes_per_line / 2)
        {
          for (pixel = l, p = lp + x; pixel != l + width; pixel ++, p++)
            {
              *pixel = 0xFF000000 |
                      (guint)((*p) & 0xf800) << 8 |
                      (guint)((*p) & 0x07e0) << 5 |
                      (guint)((*p) & 0x001f) << 3;
            }
        }

    }
  else if (priv->depth == 32)
    {
      bytes_per_line = image->bytes_per_line;
      data = (guint8 *)image->data + y * bytes_per_line + x * 4;
    }
  else
    return;

  if (x != 0 || y != 0 ||
      width != priv->pixmap_width || height != priv->pixmap_height)
    clutter_texture_set_area_from_rgb_data  (CLUTTER_TEXTURE (texture),
					     data,
					     TRUE,
					     x, y,
					     width, height,
					     bytes_per_line,
					     4,
					     CLUTTER_TEXTURE_RGB_FLAG_BGR,
					     &error);
  else
    clutter_texture_set_from_rgb_data  (CLUTTER_TEXTURE (texture),
					data,
					TRUE,
					width, height,
					bytes_per_line,
					4,
					CLUTTER_TEXTURE_RGB_FLAG_BGR,
					&error);



  if (error)
    {
      g_warning ("Error when uploading from pixbuf: %s",
                 error->message);
      g_error_free (error);
    }

  if (data_allocated)
    g_free (data);
}

/**
 * clutter_x11_texture_pixmap_new:
 *
 * Return value: A new #ClutterX11TexturePixmap
 *
 * Since: 0.8
 **/
ClutterActor *
clutter_x11_texture_pixmap_new (void)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_X11_TYPE_TEXTURE_PIXMAP, NULL);

  return actor;
}

/**
 * clutter_x11_texture_pixmap_new_with_pixmap:
 * @pixmap: the X Pixmap to which this texture should be bound
 * @width: the width of the X pixmap
 * @height: the height of the X pixmap
 * @depth: the depth of the X pixmap
 *
 * Return value: A new #ClutterX11TexturePixmap bound to the given X Pixmap
 *
 * Since 0.8
 **/
ClutterActor *
clutter_x11_texture_pixmap_new_with_pixmap (Pixmap     pixmap,
					    guint      width,
					    guint      height,
					    guint      depth)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_X11_TYPE_TEXTURE_PIXMAP,
			"pixmap", pixmap,
                        "pixmap-width",  width,
                        "pixmap-height", height,
                        "depth",  depth,
			NULL);

  return actor;
}

/**
 * clutter_x11_texture_pixmap_set_pixmap:
 * @texture: the texture to bind
 * @pixmap: the X Pixmap to which the texture should be bound
 * @width: the Pixmap width
 * @height: the Pixmap height
 * @depth: the Pixmap depth, in number of bits
 *
 * Sets the X Pixmap to which the texture should be bound.
 *
 * Since: 0.8
 **/
void
clutter_x11_texture_pixmap_set_pixmap (ClutterX11TexturePixmap *texture,
                                       Pixmap                   pixmap,
                                       guint                    width,
                                       guint                    height,
                                       guint                    depth)
{
  ClutterX11TexturePixmapPrivate *priv;

  g_return_if_fail (CLUTTER_X11_IS_TEXTURE_PIXMAP (texture));

  priv = texture->priv;

  if (priv->pixmap != pixmap)
    {
      priv->pixmap = pixmap;

      g_object_notify (G_OBJECT (texture), "pixmap");

    }

  if (priv->pixmap_width != width)
    {
      priv->pixmap_width = width;

      g_object_notify (G_OBJECT (texture), "pixmap-width");

    }

  if (priv->pixmap_height != height)
    {
      priv->pixmap_height = height;

      g_object_notify (G_OBJECT (texture), "pixmap-height");

    }

  if (priv->depth != depth)
    {
      priv->depth = depth;

      g_object_notify (G_OBJECT (texture), "depth");

    }

  if (priv->depth != 0 &&
      priv->pixmap != None &&
      priv->pixmap_width != 0 &&
      priv->pixmap_height != 0)
    {
      if (priv->image)
        {
          XDestroyImage (priv->image);
          priv->image = NULL;
        }

      if (CLUTTER_ACTOR_IS_REALIZED (texture))
        clutter_x11_texture_pixmap_update_area (texture,
                                                0, 0,
                                                priv->pixmap_width,
                                                priv->pixmap_height);
    }

}

/**
 * clutter_x11_texture_pixmap_update_area:
 * @texture: The texture whose content shall be updated.
 * @x: the X coordinate of the area to update
 * @y: the Y coordinate of the area to update
 * @width: the width of the area to update
 * @height: the height of the area to update
 *
 * Performs the actual binding of texture to the current content of
 * the pixmap. Can be called to update the texture if the pixmap
 * content has changed.
 *
 * Since: 0.8
 **/
void
clutter_x11_texture_pixmap_update_area (ClutterX11TexturePixmap *texture,
                                        gint                     x,
                                        gint                     y,
                                        gint                     width,
                                        gint                     height)
{
  g_return_if_fail (CLUTTER_X11_IS_TEXTURE_PIXMAP (texture));

  g_signal_emit (texture, signals[UPDATE_AREA], 0, x, y, width, height);
}
