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

#include "st-image-content-private.h"
#include "st-private.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

struct _StImageContent
{
  GObject parent_instance;

  CoglTexture *texture;

  int width;
  int height;
  gboolean is_symbolic;
};

enum
{
  PROP_0,

  PROP_PREFERRED_WIDTH,
  PROP_PREFERRED_HEIGHT,

  N_PROPS,
};

static GParamSpec *props[N_PROPS] = { NULL, };

static void clutter_content_interface_init (ClutterContentInterface *iface);
static void g_icon_interface_init (GIconIface *iface);
static void g_loadable_icon_interface_init (GLoadableIconIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (StImageContent, st_image_content, G_TYPE_OBJECT,
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

  if (self->width < 0 || self->height < 0)
    g_warning ("StImageContent initialized with invalid preferred size: %dx%d\n",
               self->width, self->height);
}

static void
st_image_content_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  StImageContent *self = ST_IMAGE_CONTENT (object);

  switch (prop_id)
    {
    case PROP_PREFERRED_WIDTH:
      g_value_set_int (value, self->width);
      break;

    case PROP_PREFERRED_HEIGHT:
      g_value_set_int (value, self->height);
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

  switch (prop_id)
    {
    case PROP_PREFERRED_WIDTH:
      st_image_content_set_preferred_width (self, g_value_get_int (value));
      break;

    case PROP_PREFERRED_HEIGHT:
      st_image_content_set_preferred_height (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
st_image_content_finalize (GObject *gobject)
{
  StImageContent *content = ST_IMAGE_CONTENT (gobject);

  g_clear_object (&content->texture);

  G_OBJECT_CLASS (st_image_content_parent_class)->finalize (gobject);
}

static void
st_image_content_class_init (StImageContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = st_image_content_constructed;
  object_class->get_property = st_image_content_get_property;
  object_class->set_property = st_image_content_set_property;
  object_class->finalize = st_image_content_finalize;

  props[PROP_PREFERRED_WIDTH] =
    g_param_spec_int ("preferred-width", NULL, NULL,
                      -1, G_MAXINT, -1,
                      G_PARAM_CONSTRUCT_ONLY | ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PREFERRED_HEIGHT] =
    g_param_spec_int ("preferred-height", NULL, NULL,
                      -1, G_MAXINT, -1,
                      G_PARAM_CONSTRUCT_ONLY | ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static gboolean
st_image_content_get_preferred_size (ClutterContent *content,
                                     float          *width,
                                     float          *height)
{
  StImageContent *self = ST_IMAGE_CONTENT (content);

  if (self->texture == NULL)
    return FALSE;

  g_assert_cmpint (self->width, >, -1);
  g_assert_cmpint (self->height, >, -1);

  if (width != NULL)
    *width = (float) self->width;

  if (height != NULL)
    *height = (float) self->height;

  return TRUE;
}

static GdkPixbuf*
pixbuf_from_image (StImageContent *image)
{
  CoglTexture *texture;
  int width, height, rowstride;
  uint8_t *data;

  texture = st_image_content_get_texture (image);
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
st_image_content_paint_content (ClutterContent      *content,
                                ClutterActor        *actor,
                                ClutterPaintNode    *root,
                                ClutterPaintContext *paint_context)
{
  StImageContent *image_content = ST_IMAGE_CONTENT (content);
  ClutterPaintNode *node;

  if (image_content->texture == NULL)
    return;

  node = clutter_actor_create_texture_paint_node (actor, image_content->texture);
  clutter_paint_node_set_static_name (node, "Image Content");
  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);
}

static void
clutter_content_interface_init (ClutterContentInterface *iface)
{
  iface->get_preferred_size = st_image_content_get_preferred_size;
  iface->paint_content = st_image_content_paint_content;
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

void
st_image_content_set_preferred_width (StImageContent *content,
                                      int             width)
{
  g_return_if_fail (ST_IS_IMAGE_CONTENT (content));

  if (content->width == width)
    return;

  content->width = width;
  g_object_notify_by_pspec (G_OBJECT (content), props[PROP_PREFERRED_WIDTH]);
}

int
st_image_content_get_preferred_width (StImageContent *content)
{
  g_return_val_if_fail (ST_IS_IMAGE_CONTENT (content), -1);

  return content->width;
}

void
st_image_content_set_preferred_height (StImageContent *content,
                                       int             height)
{
  g_return_if_fail (ST_IS_IMAGE_CONTENT (content));

  if (content->height == height)
    return;

  content->height = height;
  g_object_notify_by_pspec (G_OBJECT (content), props[PROP_PREFERRED_HEIGHT]);
}

int
st_image_content_get_preferred_height (StImageContent *content)
{
  g_return_val_if_fail (ST_IS_IMAGE_CONTENT (content), -1);

  return content->height;
}

void
st_image_content_set_is_symbolic (StImageContent *content,
                                  gboolean        is_symbolic)
{
  g_return_if_fail (ST_IS_IMAGE_CONTENT (content));

  content->is_symbolic = is_symbolic;
}

gboolean
st_image_content_get_is_symbolic (StImageContent *content)
{
  g_return_val_if_fail (ST_IS_IMAGE_CONTENT (content), FALSE);

  return content->is_symbolic;
}

/**
 * st_image_content_set_data:
 * @content: a #StImageContentImage
 * @cogl_context: The context to use
 * @data: (array): the image data, as an array of bytes
 * @pixel_format: the Cogl pixel format of the image data
 * @width: the width of the image data
 * @height: the height of the image data
 * @row_stride: the length of each row inside @data
 * @error: return location for a #GError, or %NULL
 *
 * Sets the image data to be displayed by @content.
 *
 * If the image data was successfully loaded, the @content will be invalidated.
 *
 * In case of error, the @error value will be set, and this function will
 * return %FALSE.
 *
 * The image data is copied in texture memory.
 *
 * The image data is expected to be a linear array of RGBA or RGB pixel data;
 * how to retrieve that data is left to platform specific image loaders. For
 * instance, if you use the GdkPixbuf library:
 *
 * ```c
 *   StImageContent *content =
 *     st_image_content_new_with_preferred_size ();
 *   GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
 *
 *   st_image_content_set_data (content,
 *                              gdk_pixbuf_get_pixels (pixbuf),
 *                              gdk_pixbuf_get_has_alpha (pixbuf)
 *                                ? COGL_PIXEL_FORMAT_RGBA_8888
 *                                : COGL_PIXEL_FORMAT_RGB_888,
 *                              gdk_pixbuf_get_width (pixbuf),
 *                              gdk_pixbuf_get_height (pixbuf),
 *                              gdk_pixbuf_get_rowstride (pixbuf),
 *                              &error);
 *
 *   g_object_unref (pixbuf);
 * ```
 *
 * Return value: %TRUE if the image data was successfully loaded,
 *   and %FALSE otherwise.
 */
gboolean
st_image_content_set_data (StImageContent   *content,
                           CoglContext      *cogl_context,
                           const guint8     *data,
                           CoglPixelFormat   pixel_format,
                           guint             width,
                           guint             height,
                           guint             row_stride,
                           GError          **error)
{
  int old_width = 0;
  int old_height = 0;

  g_return_val_if_fail (ST_IS_IMAGE_CONTENT (content), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (content->texture != NULL)
    {
      old_width = cogl_texture_get_width (content->texture);
      old_height = cogl_texture_get_height (content->texture);

      g_object_unref (content->texture);
    }

  content->texture = cogl_texture_2d_new_from_data (cogl_context,
                                                    width,
                                                    height,
                                                    pixel_format,
                                                    row_stride,
                                                    data,
                                                    error);

  if (content->texture == NULL)
    return FALSE;

  clutter_content_invalidate (CLUTTER_CONTENT (content));

  if (old_width != width || old_height != height)
    clutter_content_invalidate_size (CLUTTER_CONTENT (content));

  return TRUE;
}

/**
 * st_image_content_set_bytes:
 * @content: a #StImageContent
 * @cogl_context: The context to use
 * @data: the image data, as a #GBytes
 * @pixel_format: the Cogl pixel format of the image data
 * @width: the width of the image data
 * @height: the height of the image data
 * @row_stride: the length of each row inside @data
 * @error: return location for a #GError, or %NULL
 *
 * Sets the image data stored inside a #GBytes to be displayed by @content.
 *
 * If the image data was successfully loaded, the @content will be invalidated.
 *
 * In case of error, the @error value will be set, and this function will
 * return %FALSE.
 *
 * The image data contained inside the #GBytes is copied in texture memory,
 * and no additional reference is acquired on the @data.
 *
 * Return value: %TRUE if the image data was successfully loaded,
 *   and %FALSE otherwise.
 */
gboolean
st_image_content_set_bytes (StImageContent   *content,
                            CoglContext      *cogl_context,
                            GBytes           *data,
                            CoglPixelFormat   pixel_format,
                            guint             width,
                            guint             height,
                            guint             row_stride,
                            GError          **error)
{

  int old_width = 0;
  int old_height = 0;

  g_return_val_if_fail (ST_IS_IMAGE_CONTENT (content), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (content->texture != NULL)
    {
      old_width = cogl_texture_get_width (content->texture);
      old_height = cogl_texture_get_height (content->texture);

      g_object_unref (content->texture);
    }

  content->texture = cogl_texture_2d_new_from_data (cogl_context,
                                                    width,
                                                    height,
                                                    pixel_format,
                                                    row_stride,
                                                    g_bytes_get_data (data, NULL),
                                                    error);

  if (content->texture == NULL)
    return FALSE;

  clutter_content_invalidate (CLUTTER_CONTENT (content));

  if (old_width != width || old_height != height)
    clutter_content_invalidate_size (CLUTTER_CONTENT (content));

  return TRUE;
}

/**
 * st_image_content_get_texture:
 * @content: a #StcontentContent
 *
 * Retrieves a pointer to the Cogl texture used by @content.
 *
 * If you change the contents of the returned Cogl texture you will need
 * to manually invalidate the @content with [method@Clutter.Content.invalidate]
 * in order to update the actors using @content as their content.
 *
 * Return value: (transfer none): a pointer to the Cogl texture, or %NULL
 */
CoglTexture *
st_image_content_get_texture (StImageContent *content)
{
  g_return_val_if_fail (ST_IS_IMAGE_CONTENT (content), NULL);

  return content->texture;
}
