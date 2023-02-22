/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-image-content.h: A content image with scaling support
 *
 * Copyright 2019 Canonical, Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "st-image-content.h"
#include "st-private.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

struct _StImageContent
{
  /*< private >*/
  ClutterImage parent_instance;
};

typedef struct _StImageContentPrivate StImageContentPrivate;
struct _StImageContentPrivate
{
  int width;
  int height;
};

enum
{
  PROP_0,
  PROP_PREFERRED_WIDTH,
  PROP_PREFERRED_HEIGHT,
};

static void clutter_content_interface_init (ClutterContentInterface *iface);
static void g_icon_interface_init (GIconIface *iface);
static void g_loadable_icon_interface_init (GLoadableIconIface *iface);

G_DEFINE_TYPE_WITH_CODE (StImageContent, st_image_content, CLUTTER_TYPE_IMAGE,
                         G_ADD_PRIVATE (StImageContent)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTENT,
                                                clutter_content_interface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ICON,
                                                g_icon_interface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LOADABLE_ICON,
                                                g_loadable_icon_interface_init))

static void
st_image_content_init (StImageContent *self)
{
}

static void
st_image_content_constructed (GObject *object)
{
  StImageContent *self = ST_IMAGE_CONTENT (object);
  StImageContentPrivate *priv = st_image_content_get_instance_private (self);

  if (priv->width < 0 || priv->height < 0)
    g_warning ("StImageContent initialized with invalid preferred size: %dx%d\n",
               priv->width, priv->height);
}

static void
st_image_content_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  StImageContent *self = ST_IMAGE_CONTENT (object);
  StImageContentPrivate *priv = st_image_content_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PREFERRED_WIDTH:
      g_value_set_int (value, priv->width);
      break;

    case PROP_PREFERRED_HEIGHT:
      g_value_set_int (value, priv->height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
st_image_content_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  StImageContent *self = ST_IMAGE_CONTENT (object);
  StImageContentPrivate *priv = st_image_content_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PREFERRED_WIDTH:
      priv->width = g_value_get_int (value);
      break;

    case PROP_PREFERRED_HEIGHT:
      priv->height = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
st_image_content_class_init (StImageContentClass *klass)
{
  GParamSpec *pspec;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = st_image_content_constructed;
  object_class->get_property = st_image_content_get_property;
  object_class->set_property = st_image_content_set_property;

  pspec = g_param_spec_int ("preferred-width",
                            "Preferred Width",
                            "Preferred Width of the Content when painted",
                             -1, G_MAXINT, -1,
                             G_PARAM_CONSTRUCT_ONLY | ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_PREFERRED_WIDTH, pspec);

  pspec = g_param_spec_int ("preferred-height",
                            "Preferred Height",
                            "Preferred Height of the Content when painted",
                             -1, G_MAXINT, -1,
                             G_PARAM_CONSTRUCT_ONLY | ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_PREFERRED_HEIGHT, pspec);
}

static gboolean
st_image_content_get_preferred_size (ClutterContent *content,
                                     float          *width,
                                     float          *height)
{
  StImageContent *self = ST_IMAGE_CONTENT (content);
  StImageContentPrivate *priv = st_image_content_get_instance_private (self);
  CoglTexture *texture;

  texture = clutter_image_get_texture (CLUTTER_IMAGE (content));

  if (texture == NULL)
    return FALSE;

  g_assert_cmpint (priv->width, >, -1);
  g_assert_cmpint (priv->height, >, -1);

  if (width != NULL)
    *width = (float) priv->width;

  if (height != NULL)
    *height = (float) priv->height;

  return TRUE;
}

static GdkPixbuf*
pixbuf_from_image (StImageContent *image)
{
  CoglTexture *texture;
  int width, height, rowstride;
  uint8_t *data;

  texture = clutter_image_get_texture (CLUTTER_IMAGE (image));
  if (!texture || !cogl_texture_is_get_data_supported (texture))
    return NULL;

  width = cogl_texture_get_width (texture);
  height = cogl_texture_get_width (texture);
  rowstride = 4 * width;
  data = g_new (uint8_t, rowstride * height);

  cogl_texture_get_data (texture, COGL_PIXEL_FORMAT_RGBA_8888, rowstride, data);

  return gdk_pixbuf_new_from_data ((const guchar *)data,
                                   GDK_COLORSPACE_RGB,
                                   TRUE, 8, width, height, rowstride,
                                   (GdkPixbufDestroyNotify)g_free, NULL);
}

static void
clutter_content_interface_init (ClutterContentInterface *iface)
{
  iface->get_preferred_size = st_image_content_get_preferred_size;
}

static guint
st_image_content_hash (GIcon *icon)
{
  return g_direct_hash (icon);
}

static gboolean
st_image_content_equal (GIcon *icon1,
                        GIcon *icon2)
{
  return g_direct_equal (icon1, icon2);
}

static GVariant *
st_image_content_serialize (GIcon *icon)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;

  pixbuf = pixbuf_from_image (ST_IMAGE_CONTENT (icon));
  if (!pixbuf)
    return NULL;

  return g_icon_serialize (G_ICON (pixbuf));
}

static void
g_icon_interface_init (GIconIface *iface)
{
  iface->hash = st_image_content_hash;
  iface->equal = st_image_content_equal;
  iface->serialize = st_image_content_serialize;
}

static GInputStream *
st_image_load (GLoadableIcon  *icon,
               int             size,
               char          **type,
               GCancellable   *cancellable,
               GError       **error)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;

  pixbuf = pixbuf_from_image (ST_IMAGE_CONTENT (icon));
  if (!pixbuf)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Failed to read texture");
      return NULL;
    }

  return g_loadable_icon_load (G_LOADABLE_ICON (pixbuf),
                               size, type, cancellable, error);
}

static void
load_image_thread (GTask        *task,
                   gpointer      object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  GInputStream *stream;
  GError *error = NULL;
  char *type;

  stream = st_image_load (G_LOADABLE_ICON (object),
                          GPOINTER_TO_INT (task_data),
                          &type,
                          cancellable,
                          &error);

  if (error)
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_set_task_data (task, type, g_free);
      g_task_return_pointer (task, stream, g_object_unref);
    }
}

static void
st_image_load_async (GLoadableIcon       *icon,
                     int                  size,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  task = g_task_new (icon, cancellable, callback, user_data);
  g_task_set_task_data (task, GINT_TO_POINTER (size), NULL);
  g_task_run_in_thread (task, load_image_thread);
}

static GInputStream *
st_image_load_finish (GLoadableIcon  *icon,
                      GAsyncResult   *res,
                      char          **type,
                      GError        **error)
{
  GInputStream *stream;

  stream = g_task_propagate_pointer (G_TASK (res), error);
  if (!stream)
    return NULL;

  if (type)
    *type = g_strdup (g_task_get_task_data (G_TASK (res)));

  return stream;
}

static void
g_loadable_icon_interface_init (GLoadableIconIface *iface)
{
  iface->load = st_image_load;
  iface->load_async = st_image_load_async;
  iface->load_finish = st_image_load_finish;
}

/**
 * st_image_content_new_with_preferred_size:
 * @width: The preferred width to be used when drawing the content
 * @height: The preferred width to be used when drawing the content
 *
 * Creates a new #StImageContent, a simple content for sized images.
 *
 * See #ClutterImage for setting the actual image to display or #StIcon for
 * displaying icons.
 *
 * Returns: (transfer full): the newly created #StImageContent content
 *   Use g_object_unref() when done.
 */
ClutterContent *
st_image_content_new_with_preferred_size (int width,
                                          int height)
{
  return g_object_new (ST_TYPE_IMAGE_CONTENT,
                       "preferred-width", width,
                       "preferred-height", height,
                       NULL);
}
