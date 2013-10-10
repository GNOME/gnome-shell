/*
 * Clutter.
 *
 * An OpenGL based 'interactive image' library.
 *
 * Copyright (C) 2012  Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-image
 * @Title: ClutterImage
 * @Short_Description: Image data content
 *
 * #ClutterImage is a #ClutterContent implementation that displays
 * image data.
 *
 * <informalexample><programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/image-content.c">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 * </programlisting></informalexample>
 *
 * #ClutterImage is available since Clutter 1.10.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-image.h"

#include "clutter-color.h"
#include "clutter-content-private.h"
#include "clutter-debug.h"
#include "clutter-paint-node.h"
#include "clutter-paint-nodes.h"
#include "clutter-private.h"

struct _ClutterImagePrivate
{
  CoglTexture *texture;
};

static void clutter_content_iface_init (ClutterContentIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterImage, clutter_image, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (ClutterImage)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTENT,
                                                clutter_content_iface_init))

GQuark
clutter_image_error_quark (void)
{
  return g_quark_from_static_string ("clutter-image-error-quark");
}

static void
clutter_image_finalize (GObject *gobject)
{
  ClutterImagePrivate *priv = CLUTTER_IMAGE (gobject)->priv;

  if (priv->texture != NULL)
    {
      cogl_object_unref (priv->texture);
      priv->texture = NULL;
    }

  G_OBJECT_CLASS (clutter_image_parent_class)->finalize (gobject);
}

static void
clutter_image_class_init (ClutterImageClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = clutter_image_finalize;
}

static void
clutter_image_init (ClutterImage *self)
{
  self->priv = clutter_image_get_instance_private (self);
}

static void
clutter_image_paint_content (ClutterContent   *content,
                             ClutterActor     *actor,
                             ClutterPaintNode *root)
{
  ClutterImagePrivate *priv = CLUTTER_IMAGE (content)->priv;
  ClutterScalingFilter min_f, mag_f;
  ClutterContentRepeat repeat;
  ClutterPaintNode *node;
  ClutterActorBox box;
  ClutterColor color;
  guint8 paint_opacity;

  if (priv->texture == NULL)
    return;

  clutter_actor_get_content_box (actor, &box);
  paint_opacity = clutter_actor_get_paint_opacity (actor);
  clutter_actor_get_content_scaling_filters (actor, &min_f, &mag_f);
  repeat = clutter_actor_get_content_repeat (actor);

  /* ClutterTextureNode will premultiply the blend color, so we
   * want it to be white with the paint opacity
   */
  color.red = 255;
  color.green = 255;
  color.blue = 255;
  color.alpha = paint_opacity;

  node = clutter_texture_node_new (priv->texture, &color, min_f, mag_f);
  clutter_paint_node_set_name (node, "Image");

  if (repeat == CLUTTER_REPEAT_NONE)
    clutter_paint_node_add_rectangle (node, &box);
  else
    {
      float t_w = 1.f, t_h = 1.f;

      if ((repeat & CLUTTER_REPEAT_X_AXIS) != FALSE)
        t_w = (box.x2 - box.x1) / cogl_texture_get_width (priv->texture);

      if ((repeat & CLUTTER_REPEAT_Y_AXIS) != FALSE)
        t_h = (box.y2 - box.y1) / cogl_texture_get_height (priv->texture);

      clutter_paint_node_add_texture_rectangle (node, &box,
                                                0.f, 0.f,
                                                t_w, t_h);
    }

  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);
}

static gboolean
clutter_image_get_preferred_size (ClutterContent *content,
                                  gfloat         *width,
                                  gfloat         *height)
{
  ClutterImagePrivate *priv = CLUTTER_IMAGE (content)->priv;

  if (priv->texture == NULL)
    return FALSE;

  if (width != NULL)
    *width = cogl_texture_get_width (priv->texture);

  if (height != NULL)
    *height = cogl_texture_get_height (priv->texture);

  return TRUE;
}

static void
clutter_content_iface_init (ClutterContentIface *iface)
{
  iface->get_preferred_size = clutter_image_get_preferred_size;
  iface->paint_content = clutter_image_paint_content;
}

/**
 * clutter_image_new:
 *
 * Creates a new #ClutterImage instance.
 *
 * Return value: (transfer full): the newly created #ClutterImage instance.
 *   Use g_object_unref() when done.
 *
 * Since: 1.10
 */
ClutterContent *
clutter_image_new (void)
{
  return g_object_new (CLUTTER_TYPE_IMAGE, NULL);
}

/**
 * clutter_image_set_data:
 * @image: a #ClutterImage
 * @data: (array): the image data, as an array of bytes
 * @pixel_format: the Cogl pixel format of the image data
 * @width: the width of the image data
 * @height: the height of the image data
 * @row_stride: the length of each row inside @data
 * @error: return location for a #GError, or %NULL
 *
 * Sets the image data to be displayed by @image.
 *
 * If the image data was successfully loaded, the @image will be invalidated.
 *
 * In case of error, the @error value will be set, and this function will
 * return %FALSE.
 *
 * The image data is copied in texture memory.
 *
 * Return value: %TRUE if the image data was successfully loaded,
 *   and %FALSE otherwise.
 *
 * Since: 1.10
 */
gboolean
clutter_image_set_data (ClutterImage     *image,
                        const guint8     *data,
                        CoglPixelFormat   pixel_format,
                        guint             width,
                        guint             height,
                        guint             row_stride,
                        GError          **error)
{
  ClutterImagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_IMAGE (image), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  priv = image->priv;

  if (priv->texture != NULL)
    cogl_object_unref (priv->texture);

  priv->texture = cogl_texture_new_from_data (width, height,
                                              COGL_TEXTURE_NONE,
                                              pixel_format,
                                              COGL_PIXEL_FORMAT_ANY,
                                              row_stride,
                                              data);
  if (priv->texture == NULL)
    {
      g_set_error_literal (error, CLUTTER_IMAGE_ERROR,
                           CLUTTER_IMAGE_ERROR_INVALID_DATA,
                           _("Unable to load image data"));
      return FALSE;
    }

  clutter_content_invalidate (CLUTTER_CONTENT (image));

  return TRUE;
}

/**
 * clutter_image_set_bytes:
 * @image: a #ClutterImage
 * @data: the image data, as a #GBytes
 * @pixel_format: the Cogl pixel format of the image data
 * @width: the width of the image data
 * @height: the height of the image data
 * @row_stride: the length of each row inside @data
 * @error: return location for a #GError, or %NULL
 *
 * Sets the image data stored inside a #GBytes to be displayed by @image.
 *
 * If the image data was successfully loaded, the @image will be invalidated.
 *
 * In case of error, the @error value will be set, and this function will
 * return %FALSE.
 *
 * The image data contained inside the #GBytes is copied in texture memory,
 * and no additional reference is acquired on the @data.
 *
 * Return value: %TRUE if the image data was successfully loaded,
 *   and %FALSE otherwise.
 *
 * Since: 1.12
 */
gboolean
clutter_image_set_bytes (ClutterImage     *image,
                         GBytes           *data,
                         CoglPixelFormat   pixel_format,
                         guint             width,
                         guint             height,
                         guint             row_stride,
                         GError          **error)
{
  ClutterImagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_IMAGE (image), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  priv = image->priv;

  if (priv->texture != NULL)
    cogl_object_unref (priv->texture);

  priv->texture = cogl_texture_new_from_data (width, height,
                                              COGL_TEXTURE_NONE,
                                              pixel_format,
                                              COGL_PIXEL_FORMAT_ANY,
                                              row_stride,
                                              g_bytes_get_data (data, NULL));
  if (priv->texture == NULL)
    {
      g_set_error_literal (error, CLUTTER_IMAGE_ERROR,
                           CLUTTER_IMAGE_ERROR_INVALID_DATA,
                           _("Unable to load image data"));
      return FALSE;
    }

  clutter_content_invalidate (CLUTTER_CONTENT (image));

  return TRUE;
}

/**
 * clutter_image_set_area:
 * @image: a #ClutterImage
 * @data: (array): the image data, as an array of bytes
 * @pixel_format: the Cogl pixel format of the image data
 * @rect: a rectangle indicating the area that should be set
 * @row_stride: the length of each row inside @data
 * @error: return location for a #GError, or %NULL
 *
 * Sets the image data to be display by @image, using @rect to indicate
 * the position and size of the image data to be set.
 *
 * If the @image does not have any image data set when this function is
 * called, a new texture will be created with the size of the width and
 * height of the rectangle, i.e. calling this function on a newly created
 * #ClutterImage will be the equivalent of calling clutter_image_set_data().
 *
 * If the image data was successfully loaded, the @image will be invalidated.
 *
 * In case of error, the @error value will be set, and this function will
 * return %FALSE.
 *
 * The image data is copied in texture memory.
 *
 * Return value: %TRUE if the image data was successfully loaded,
 *   and %FALSE otherwise.
 *
 * Since: 1.10
 */
gboolean
clutter_image_set_area (ClutterImage                 *image,
                        const guint8                 *data,
                        CoglPixelFormat               pixel_format,
                        const cairo_rectangle_int_t  *area,
                        guint                         row_stride,
                        GError                      **error)
{
  ClutterImagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_IMAGE (image), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (area != NULL, FALSE);

  priv = image->priv;

  if (priv->texture == NULL)
    {
      priv->texture = cogl_texture_new_from_data (area->width,
                                                  area->height,
                                                  COGL_TEXTURE_NONE,
                                                  pixel_format,
                                                  COGL_PIXEL_FORMAT_ANY,
                                                  row_stride,
                                                  data);
    }
  else
    {
      gboolean res;

      res = cogl_texture_set_region (priv->texture,
                                     0, 0,
                                     area->x, area->y,
                                     area->width, area->height,
                                     area->width, area->height,
                                     pixel_format,
                                     row_stride,
                                     data);

      if (!res)
        {
          cogl_object_unref (priv->texture);
          priv->texture = NULL;
        }
    }

  if (priv->texture == NULL)
    {
      g_set_error_literal (error, CLUTTER_IMAGE_ERROR,
                           CLUTTER_IMAGE_ERROR_INVALID_DATA,
                           _("Unable to load image data"));
      return FALSE;
    }

  clutter_content_invalidate (CLUTTER_CONTENT (image));

  return TRUE;
}

/**
 * clutter_image_get_texture:
 * @image: a #ClutterImage
 *
 * Retrieves a pointer to the Cogl texture used by @image.
 *
 * If you change the contents of the returned Cogl texture you will need
 * to manually invalidate the @image with clutter_content_invalidate()
 * in order to update the actors using @image as their content.
 *
 * Return value: (transfer none): a pointer to the Cogl texture, or %NULL
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglTexture *
clutter_image_get_texture (ClutterImage *image)
{
  g_return_val_if_fail (CLUTTER_IS_IMAGE (image), NULL);

  return image->priv->texture;
}
