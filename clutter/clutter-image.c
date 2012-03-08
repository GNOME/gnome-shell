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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
  g_type_class_add_private (klass, sizeof (ClutterImagePrivate));

  G_OBJECT_CLASS (klass)->finalize = clutter_image_finalize;
}

static void
clutter_image_init (ClutterImage *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CLUTTER_TYPE_IMAGE,
                                            ClutterImagePrivate);
}

static void
clutter_image_paint_content (ClutterContent   *content,
                             ClutterActor     *actor,
                             ClutterPaintNode *root)
{
  ClutterImagePrivate *priv = CLUTTER_IMAGE (content)->priv;
  ClutterPaintNode *node;
  ClutterActorBox box;
  ClutterColor color;
  guint8 paint_opacity;

  if (priv->texture == NULL)
    return;

  clutter_actor_get_content_box (actor, &box);
  paint_opacity = clutter_actor_get_paint_opacity (actor);

  color.red = paint_opacity;
  color.green = paint_opacity;
  color.blue = paint_opacity;
  color.alpha = paint_opacity;

  node = clutter_texture_node_new (priv->texture, &color);
  clutter_paint_node_set_name (node, "Image");
  clutter_paint_node_add_rectangle (node, &box);
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
 * FIXME
 *
 * Return value: (transfer full): FIXME
 *
 * Since: 1.10
 */
ClutterContent *
clutter_image_new (void)
{
  return g_object_new (CLUTTER_TYPE_IMAGE, NULL);
}

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
