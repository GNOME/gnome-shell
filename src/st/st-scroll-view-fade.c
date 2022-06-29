/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-scroll-view-fade.h: Edge fade effect for StScrollView
 *
 * Copyright 2010 Intel Corporation.
 * Copyright 2011 Adel Gadllah
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "st-private.h"
#include "st-scroll-view-fade.h"
#include "st-scroll-view.h"
#include "st-widget.h"
#include "st-theme-node.h"
#include "st-scroll-bar.h"
#include "st-scrollable.h"

#include <clutter/clutter.h>
#include <cogl/cogl.h>

#define DEFAULT_FADE_OFFSET 68.0f

#include "st-scroll-view-fade-generated.h"

struct _StScrollViewFade
{
  ClutterShaderEffect parent_instance;

  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  StAdjustment *vadjustment;
  StAdjustment *hadjustment;

  guint fade_edges : 1;
  guint extend_fade_area: 1;

  ClutterMargin fade_margins;
};

G_DEFINE_TYPE (StScrollViewFade,
               st_scroll_view_fade,
               CLUTTER_TYPE_SHADER_EFFECT);

enum {
  PROP_0,

  PROP_FADE_MARGINS,
  PROP_FADE_EDGES,
  PROP_EXTEND_FADE_AREA,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

static CoglTexture *
st_scroll_view_fade_create_texture (ClutterOffscreenEffect *effect,
                                    gfloat                  min_width,
                                    gfloat                  min_height)
{
  CoglContext *ctx =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());

  return COGL_TEXTURE (cogl_texture_2d_new_with_size (ctx, min_width, min_height));
}

static char *
st_scroll_view_fade_get_static_shader_source (ClutterShaderEffect *effect)
{
   return g_strdup (st_scroll_view_fade_glsl);
}


static void
st_scroll_view_fade_paint_target (ClutterOffscreenEffect *effect,
                                  ClutterPaintNode       *node,
                                  ClutterPaintContext    *paint_context)
{
  StScrollViewFade *self = ST_SCROLL_VIEW_FADE (effect);
  ClutterShaderEffect *shader = CLUTTER_SHADER_EFFECT (effect);
  ClutterOffscreenEffectClass *parent;

  gdouble value, lower, upper, page_size;
  ClutterActor *vscroll = st_scroll_view_get_vscroll_bar (ST_SCROLL_VIEW (self->actor));
  ClutterActor *hscroll = st_scroll_view_get_hscroll_bar (ST_SCROLL_VIEW (self->actor));
  gboolean h_scroll_visible, v_scroll_visible, rtl;

  ClutterActorBox allocation, content_box, paint_box;

  float fade_area_topleft[2];
  float fade_area_bottomright[2];
  graphene_point3d_t verts[4];

  clutter_actor_get_paint_box (self->actor, &paint_box);
  clutter_actor_get_abs_allocation_vertices (self->actor, verts);

  clutter_actor_get_allocation_box (self->actor, &allocation);
  st_theme_node_get_content_box (st_widget_get_theme_node (ST_WIDGET (self->actor)),
                                (const ClutterActorBox *)&allocation, &content_box);

  /*
   * The FBO is based on the paint_volume's size which can be larger then the actual
   * allocation, so we have to account for that when passing the positions
   */
  fade_area_topleft[0] = content_box.x1 + (verts[0].x - paint_box.x1);
  fade_area_topleft[1] = content_box.y1 + (verts[0].y - paint_box.y1);
  fade_area_bottomright[0] = content_box.x2 + (verts[3].x - paint_box.x2) + 1;
  fade_area_bottomright[1] = content_box.y2 + (verts[3].y - paint_box.y2) + 1;

  g_object_get (ST_SCROLL_VIEW (self->actor),
                "hscrollbar-visible", &h_scroll_visible,
                "vscrollbar-visible", &v_scroll_visible,
                NULL);

  if (v_scroll_visible)
    {
      if (clutter_actor_get_text_direction (self->actor) == CLUTTER_TEXT_DIRECTION_RTL)
          fade_area_topleft[0] += clutter_actor_get_width (vscroll);

      fade_area_bottomright[0] -= clutter_actor_get_width (vscroll);
    }

  if (h_scroll_visible)
      fade_area_bottomright[1] -= clutter_actor_get_height (hscroll);

  if (self->fade_margins.left < 0)
    fade_area_topleft[0] -= ABS (self->fade_margins.left);
  if (self->fade_margins.right < 0)
    fade_area_bottomright[0] += ABS (self->fade_margins.right);
  if (self->fade_margins.top < 0)
    fade_area_topleft[1] -= ABS (self->fade_margins.top);
  if (self->fade_margins.bottom < 0)
    fade_area_bottomright[1] += ABS (self->fade_margins.bottom);

  st_adjustment_get_values (self->vadjustment, &value, &lower, &upper, NULL, NULL, &page_size);
  value = (value - lower) / (upper - page_size - lower);
  clutter_shader_effect_set_uniform (shader, "fade_edges_top", G_TYPE_INT, 1, self->fade_edges ? value >= 0.0 : value > 0.0);
  clutter_shader_effect_set_uniform (shader, "fade_edges_bottom", G_TYPE_INT, 1, self->fade_edges ? value <= 1.0 : value < 1.0);

  st_adjustment_get_values (self->hadjustment, &value, &lower, &upper, NULL, NULL, &page_size);
  value = (value - lower) / (upper - page_size - lower);
  rtl = clutter_actor_get_text_direction (self->actor) == CLUTTER_TEXT_DIRECTION_RTL;
  clutter_shader_effect_set_uniform (shader, "fade_edges_left", G_TYPE_INT, 1,
                                     self->fade_edges ?
                                     value >= 0.0 :
                                     (rtl ? value < 1.0 : value > 0.0));
  clutter_shader_effect_set_uniform (shader, "fade_edges_right", G_TYPE_INT, 1,
                                     self->fade_edges ?
                                     value <= 1.0 :
                                     (rtl ? value > 0.0 : value < 1.0));

  clutter_shader_effect_set_uniform (shader, "extend_fade_area", G_TYPE_INT, 1, self->extend_fade_area);
  clutter_shader_effect_set_uniform (shader, "fade_offset_top", G_TYPE_FLOAT, 1, ABS (self->fade_margins.top));
  clutter_shader_effect_set_uniform (shader, "fade_offset_bottom", G_TYPE_FLOAT, 1, ABS (self->fade_margins.bottom));
  clutter_shader_effect_set_uniform (shader, "fade_offset_left", G_TYPE_FLOAT, 1, ABS (self->fade_margins.left));
  clutter_shader_effect_set_uniform (shader, "fade_offset_right", G_TYPE_FLOAT, 1, ABS (self->fade_margins.right));
  clutter_shader_effect_set_uniform (shader, "tex", G_TYPE_INT, 1, 0);
  clutter_shader_effect_set_uniform (shader, "height", G_TYPE_FLOAT, 1, clutter_actor_get_height (self->actor));
  clutter_shader_effect_set_uniform (shader, "width", G_TYPE_FLOAT, 1, clutter_actor_get_width (self->actor));
  clutter_shader_effect_set_uniform (shader, "fade_area_topleft", CLUTTER_TYPE_SHADER_FLOAT, 2, fade_area_topleft);
  clutter_shader_effect_set_uniform (shader, "fade_area_bottomright", CLUTTER_TYPE_SHADER_FLOAT, 2, fade_area_bottomright);

  parent = CLUTTER_OFFSCREEN_EFFECT_CLASS (st_scroll_view_fade_parent_class);
  parent->paint_target (effect, node, paint_context);
}

static void
on_adjustment_changed (StAdjustment *adjustment,
                       ClutterEffect *effect)
{
  gdouble value, lower, upper, page_size;
  gboolean needs_fade;
  StScrollViewFade *self = ST_SCROLL_VIEW_FADE (effect);

  st_adjustment_get_values (self->vadjustment, &value, &lower, &upper, NULL, NULL, &page_size);
  needs_fade = (value > lower + 0.1) || (value < upper - page_size - 0.1);

  if (!needs_fade)
    {
      st_adjustment_get_values (self->hadjustment, &value, &lower, &upper, NULL, NULL, &page_size);
      needs_fade = (value > lower + 0.1) || (value < upper - page_size - 0.1);
    }

  clutter_actor_meta_set_enabled (CLUTTER_ACTOR_META (effect), needs_fade);
}

static void
st_scroll_view_fade_set_actor (ClutterActorMeta *meta,
                               ClutterActor *actor)
{
  StScrollViewFade *self = ST_SCROLL_VIEW_FADE (meta);
  ClutterActorMetaClass *parent;

  g_return_if_fail (actor == NULL || ST_IS_SCROLL_VIEW (actor));

  if (self->vadjustment)
    {
      g_signal_handlers_disconnect_by_func (self->vadjustment,
                                            (gpointer)on_adjustment_changed,
                                            self);
      self->vadjustment = NULL;
    }

  if (self->hadjustment)
    {
      g_signal_handlers_disconnect_by_func (self->hadjustment,
                                            (gpointer)on_adjustment_changed,
                                            self);
      self->hadjustment = NULL;
    }


  if (actor)
    {
        StScrollView *scroll_view = ST_SCROLL_VIEW (actor);
        StScrollBar *vscroll = ST_SCROLL_BAR (st_scroll_view_get_vscroll_bar (scroll_view));
        StScrollBar *hscroll = ST_SCROLL_BAR (st_scroll_view_get_hscroll_bar (scroll_view));
        self->vadjustment = ST_ADJUSTMENT (st_scroll_bar_get_adjustment (vscroll));
        self->hadjustment = ST_ADJUSTMENT (st_scroll_bar_get_adjustment (hscroll));

        g_signal_connect (self->vadjustment, "changed",
                          G_CALLBACK (on_adjustment_changed),
                          self);

        g_signal_connect (self->hadjustment, "changed",
                          G_CALLBACK (on_adjustment_changed),
                          self);

        on_adjustment_changed (NULL, CLUTTER_EFFECT (self));
    }

  parent = CLUTTER_ACTOR_META_CLASS (st_scroll_view_fade_parent_class);
  parent->set_actor (meta, actor);

  /* we keep a back pointer here, to avoid going through the ActorMeta */
  self->actor = clutter_actor_meta_get_actor (meta);
}

static void
st_scroll_view_fade_dispose (GObject *gobject)
{
  StScrollViewFade *self = ST_SCROLL_VIEW_FADE (gobject);

  if (self->vadjustment)
    {
      g_signal_handlers_disconnect_by_func (self->vadjustment,
                                            (gpointer)on_adjustment_changed,
                                            self);
      self->vadjustment = NULL;
    }

  if (self->hadjustment)
    {
      g_signal_handlers_disconnect_by_func (self->hadjustment,
                                            (gpointer)on_adjustment_changed,
                                            self);
      self->hadjustment = NULL;
    }

  self->actor = NULL;

  G_OBJECT_CLASS (st_scroll_view_fade_parent_class)->dispose (gobject);
}

static void
st_scroll_view_set_fade_margins (StScrollViewFade *self,
                                 ClutterMargin    *fade_margins)
{
  if (self->fade_margins.left == fade_margins->left &&
      self->fade_margins.right == fade_margins->right &&
      self->fade_margins.top == fade_margins->top &&
      self->fade_margins.bottom == fade_margins->bottom)
    return;

  self->fade_margins = *fade_margins;

  if (self->actor != NULL)
    clutter_actor_queue_redraw (self->actor);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FADE_MARGINS]);
}

static void
st_scroll_view_fade_set_fade_edges (StScrollViewFade *self,
                                    gboolean          fade_edges)
{
  if (self->fade_edges == fade_edges)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  self->fade_edges = fade_edges;

  if (self->actor != NULL)
    clutter_actor_queue_redraw (self->actor);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FADE_EDGES]);
  g_object_thaw_notify (G_OBJECT (self));
}

static void
st_scroll_view_fade_set_extend_fade_area (StScrollViewFade *self,
                                          gboolean          extend_fade_area)
{
  if (self->extend_fade_area == extend_fade_area)
    return;

  self->extend_fade_area = extend_fade_area;

  if (self->actor != NULL)
    clutter_actor_queue_redraw (self->actor);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_EXTEND_FADE_AREA]);
}

static void
st_scroll_view_fade_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  StScrollViewFade *self = ST_SCROLL_VIEW_FADE (object);

  switch (prop_id)
    {
    case PROP_FADE_MARGINS:
      st_scroll_view_set_fade_margins (self, g_value_get_boxed (value));
      break;
    case PROP_FADE_EDGES:
      st_scroll_view_fade_set_fade_edges (self, g_value_get_boolean (value));
      break;
    case PROP_EXTEND_FADE_AREA:
      st_scroll_view_fade_set_extend_fade_area (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
st_scroll_view_fade_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  StScrollViewFade *self = ST_SCROLL_VIEW_FADE (object);

  switch (prop_id)
    {
    case PROP_FADE_MARGINS:
      g_value_set_boxed (value, &self->fade_margins);
      break;
    case PROP_FADE_EDGES:
      g_value_set_boolean (value, self->fade_edges);
      break;
    case PROP_EXTEND_FADE_AREA:
      g_value_set_boolean (value, self->extend_fade_area);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
st_scroll_view_fade_class_init (StScrollViewFadeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterShaderEffectClass *shader_class;
  ClutterOffscreenEffectClass *offscreen_class;
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);

  gobject_class->dispose = st_scroll_view_fade_dispose;
  gobject_class->get_property = st_scroll_view_fade_get_property;
  gobject_class->set_property = st_scroll_view_fade_set_property;

  meta_class->set_actor = st_scroll_view_fade_set_actor;

  shader_class = CLUTTER_SHADER_EFFECT_CLASS (klass);
  shader_class->get_static_shader_source = st_scroll_view_fade_get_static_shader_source;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->create_texture = st_scroll_view_fade_create_texture;
  offscreen_class->paint_target = st_scroll_view_fade_paint_target;

  /**
   * StScrollViewFade:fade-margins:
   *
   * The margins widths that are faded.
   */
  props[PROP_FADE_MARGINS] =
    g_param_spec_boxed ("fade-margins",
                        "Fade margins",
                        "The margin widths that are faded",
                        CLUTTER_TYPE_MARGIN,
                        ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StScrollViewFade:fade-edges:
   *
   * Whether the faded area should extend to the edges of the #StScrollViewFade.
   */
  props[PROP_FADE_EDGES] =
    g_param_spec_boolean ("fade-edges",
                          "Fade Edges",
                          "Whether the faded area should extend to the edges",
                          FALSE,
                          ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StScrollViewFade:extend-fade-area:
   *
   * Whether faded edges should extend beyond the faded area of the #StScrollViewFade.
   */
  props[PROP_EXTEND_FADE_AREA] =
    g_param_spec_boolean ("extend-fade-area",
                          "Extend Fade Area",
                          "Whether faded edges should extend beyond the faded area",
                          FALSE,
                          ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, props);
}

static void
st_scroll_view_fade_init (StScrollViewFade *self)
{
  self->fade_margins = (ClutterMargin) {
    DEFAULT_FADE_OFFSET,
    DEFAULT_FADE_OFFSET,
    DEFAULT_FADE_OFFSET,
    DEFAULT_FADE_OFFSET,
  };
}

/**
 * st_scroll_view_fade_new:
 *
 * Create a new #StScrollViewFade.
 *
 * Returns: (transfer full): a new #StScrollViewFade
 */
ClutterEffect *
st_scroll_view_fade_new (void)
{
  return g_object_new (ST_TYPE_SCROLL_VIEW_FADE, NULL);
}
