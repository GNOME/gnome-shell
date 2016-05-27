/*
 * Copyright (C) 2016 Red Hat Inc.
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
 */

#include "clutter-build-config.h"

#include "clutter/clutter-stage-view.h"

#include <cairo-gobject.h>

enum
{
  PROP_0,

  PROP_LAYOUT,
  PROP_FRAMEBUFFER,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

typedef struct _ClutterStageViewPrivate
{
  cairo_rectangle_int_t layout;
  CoglFramebuffer *framebuffer;
  guint dirty_viewport   : 1;
  guint dirty_projection : 1;
} ClutterStageViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ClutterStageView, clutter_stage_view, G_TYPE_OBJECT)

void
clutter_stage_view_get_layout (ClutterStageView      *view,
                               cairo_rectangle_int_t *rect)
{
  ClutterStageViewPrivate *priv =
    clutter_stage_view_get_instance_private (view);

  *rect = priv->layout;
}

CoglFramebuffer *
clutter_stage_view_get_framebuffer (ClutterStageView *view)
{
  ClutterStageViewPrivate *priv =
    clutter_stage_view_get_instance_private (view);

  return priv->framebuffer;
}

gboolean
clutter_stage_view_is_dirty_viewport (ClutterStageView *view)
{
  ClutterStageViewPrivate *priv =
    clutter_stage_view_get_instance_private (view);

  return priv->dirty_viewport;
}

void
clutter_stage_view_set_dirty_viewport (ClutterStageView *view,
                                       gboolean          dirty)
{
  ClutterStageViewPrivate *priv =
    clutter_stage_view_get_instance_private (view);

  priv->dirty_viewport = dirty;
}

gboolean
clutter_stage_view_is_dirty_projection (ClutterStageView *view)
{
  ClutterStageViewPrivate *priv =
    clutter_stage_view_get_instance_private (view);

  return priv->dirty_projection;
}

void
clutter_stage_view_set_dirty_projection (ClutterStageView *view,
                                         gboolean          dirty)
{
  ClutterStageViewPrivate *priv =
    clutter_stage_view_get_instance_private (view);

  priv->dirty_projection = dirty;
}

static void
clutter_stage_view_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ClutterStageView *view = CLUTTER_STAGE_VIEW (object);
  ClutterStageViewPrivate *priv =
    clutter_stage_view_get_instance_private (view);

  switch (prop_id)
    {
    case PROP_LAYOUT:
      g_value_set_boxed (value, &priv->layout);
      break;
    case PROP_FRAMEBUFFER:
      g_value_set_boxed (value, priv->framebuffer);
      break;
    }
}

static void
clutter_stage_view_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterStageView *view = CLUTTER_STAGE_VIEW (object);
  ClutterStageViewPrivate *priv =
    clutter_stage_view_get_instance_private (view);
  cairo_rectangle_int_t *layout;

  switch (prop_id)
    {
    case PROP_LAYOUT:
      layout = g_value_get_boxed (value);
      priv->layout = *layout;
      break;
    case PROP_FRAMEBUFFER:
      priv->framebuffer = g_value_get_boxed (value);
      break;
    }
}

static void
clutter_stage_view_dispose (GObject *object)
{
  ClutterStageView *view = CLUTTER_STAGE_VIEW (object);
  ClutterStageViewPrivate *priv =
    clutter_stage_view_get_instance_private (view);

  g_clear_pointer (&priv->framebuffer, cogl_object_unref);
}

static void
clutter_stage_view_init (ClutterStageView *view)
{
}

static void
clutter_stage_view_class_init (ClutterStageViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = clutter_stage_view_get_property;
  object_class->set_property = clutter_stage_view_set_property;
  object_class->dispose = clutter_stage_view_dispose;

  obj_props[PROP_LAYOUT] =
    g_param_spec_boxed ("layout",
                        "View layout",
                        "The view layout on the screen",
                        CAIRO_GOBJECT_TYPE_RECTANGLE_INT,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);

  obj_props[PROP_FRAMEBUFFER] =
    g_param_spec_boxed ("framebuffer",
                        "View framebuffer",
                        "The framebuffer of the view",
                        COGL_TYPE_HANDLE,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}
