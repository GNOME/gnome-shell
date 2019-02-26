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

typedef struct _StImageContentPrivate StImageContentPrivate;
struct _StImageContentPrivate {
  int width;
  int height;
};

enum
{
  PROP_0,
  PROP_PREFERRED_WIDTH,
  PROP_PREFERRED_HEIGHT,
};

static void clutter_content_iface_init (ClutterContentIface *iface);

G_DEFINE_TYPE_WITH_CODE (StImageContent, st_image_content, CLUTTER_TYPE_IMAGE,
                         G_ADD_PRIVATE (StImageContent)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTENT,
                                                clutter_content_iface_init))

static void
st_image_content_init (StImageContent *self)
{
}

static void
st_image_content_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  StImageContent *self = ST_IMAGE_CONTENT (gobject);
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
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_image_content_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  StImageContent *self = ST_IMAGE_CONTENT (gobject);
  StImageContentPrivate *priv = st_image_content_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PREFERRED_WIDTH:
      st_image_content_set_preferred_size (self,
                                           g_value_get_int (value),
                                           priv->height);
      break;

    case PROP_PREFERRED_HEIGHT:
      st_image_content_set_preferred_size (self,
                                           priv->width,
                                           g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_image_content_class_init (StImageContentClass *klass)
{
  GParamSpec *pspec;
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = st_image_content_get_property;
  gobject_class->set_property = st_image_content_set_property;

  pspec = g_param_spec_int ("preferred-width",
                            "Preferred Width",
                            "Preferred Width of the Content when painted",
                             -1, G_MAXINT, -1,
                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_PREFERRED_WIDTH, pspec);

  pspec = g_param_spec_int ("preferred-height",
                            "Preferred Height",
                            "Preferred Height of the Content when painted",
                             -1, G_MAXINT, -1,
                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_PREFERRED_HEIGHT, pspec);
}

static gboolean
st_image_content_get_preferred_size (ClutterContent *content,
                                     gfloat         *width,
                                     gfloat         *height)
{
  StImageContent *self = ST_IMAGE_CONTENT (content);
  StImageContentPrivate *priv = st_image_content_get_instance_private (self);
  ClutterTexture *texture;

  texture = clutter_image_get_texture (CLUTTER_IMAGE (content));

  if (texture == NULL)
    return FALSE;

  if (width != NULL)
    {
      if (priv->width > -1)
        *width = (gfloat) priv->width;
      else
        *width = cogl_texture_get_width (texture);
    }

  if (height != NULL)
    {
      if (priv->height > -1)
        *height = (gfloat) priv->height;
      else
        *height = cogl_texture_get_height (texture);
    }

  return TRUE;
}

static void
clutter_content_iface_init (ClutterContentIface *iface)
{
  iface->get_preferred_size = st_image_content_get_preferred_size;
}

/**
 * st_image_content_new_with_preferred_size:
 * @width: The preferred width to be used when drawing the content
 * @height: The preferred width to be used when drawing the content
 *
 * Creates a new #StImageContent, a simple content for sized images.
 *
 * Return value: (transfer full): the newly created #StImageContent content
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

/**
 * st_image_content_set_preferred_size:
 * @self: a #StImageContent
 * @width: The preferred width to be used when drawing the content
 * @height: The preferred height to be used when drawing the content
 *
 * Set the preferred size for the #StImageContent, that will be used by the
 * attached actors as hint for the paint size, and in case they use
 * %CLUTTER_REQUEST_CONTENT_SIZE it will match the actual actor size.
 */
void
st_image_content_set_preferred_size (StImageContent *self,
                                     int             width,
                                     int             height)
{
  StImageContentPrivate *priv = st_image_content_get_instance_private (self);

  if (priv->width != width || priv->height != height)
    {
      priv->width = width;
      priv->height = height;

      clutter_content_invalidate_size (CLUTTER_CONTENT (self));
    }
}
