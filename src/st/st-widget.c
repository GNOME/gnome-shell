/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-widget.c: Base class for St actors
 *
 * Copyright 2007 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 *             Thomas Wood <thomas@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <clutter/clutter.h>

#include "st-widget.h"

#include "st-marshal.h"
#include "st-private.h"
#include "st-shadow-texture.h"
#include "st-texture-cache.h"
#include "st-texture-frame.h"
#include "st-theme-context.h"
#include "st-tooltip.h"

#include <big/rectangle.h>

/*
 * Forward declaration for sake of StWidgetChild
 */
struct _StWidgetPrivate
{
  StTheme      *theme;
  StThemeNode  *theme_node;
  gchar        *pseudo_class;
  gchar        *style_class;
  gchar        *inline_style;

  ClutterActor *border_image;
  ClutterActor *background_image;
  ClutterActor *background_image_shadow;
  ClutterColor  bg_color;

  guint         border_width;
  ClutterColor  border_color;

  StGradientType bg_gradient_type;
  ClutterColor  bg_gradient_end;

  gdouble       shadow_xoffset;
  gdouble       shadow_yoffset;

  gboolean      is_stylable : 1;
  gboolean      has_tooltip : 1;
  gboolean      is_style_dirty : 1;
  gboolean      draw_bg_color : 1;
  gboolean      draw_border_internal : 1;

  StTooltip    *tooltip;

  StTextDirection   direction;
};

/**
 * SECTION:st-widget
 * @short_description: Base class for stylable actors
 *
 * #StWidget is a simple abstract class on top of #ClutterActor. It
 * provides basic themeing properties.
 *
 * Actors in the St library should subclass #StWidget if they plan
 * to obey to a certain #StStyle.
 */

enum
{
  PROP_0,

  PROP_THEME,
  PROP_PSEUDO_CLASS,
  PROP_STYLE_CLASS,
  PROP_STYLE,

  PROP_STYLABLE,

  PROP_HAS_TOOLTIP,
  PROP_TOOLTIP_TEXT
};

enum
{
  STYLE_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE (StWidget, st_widget, CLUTTER_TYPE_ACTOR);

#define ST_WIDGET_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ST_TYPE_WIDGET, StWidgetPrivate))

static void st_widget_recompute_style (StWidget    *widget,
                                       StThemeNode *old_theme_node);
static void st_widget_redraw_gradient (StWidget  *widget);

static void
st_widget_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  StWidget *actor = ST_WIDGET (gobject);

  switch (prop_id)
    {
    case PROP_THEME:
      st_widget_set_theme (actor, g_value_get_object (value));
      break;

    case PROP_PSEUDO_CLASS:
      st_widget_set_style_pseudo_class (actor, g_value_get_string (value));
      break;

    case PROP_STYLE_CLASS:
      st_widget_set_style_class_name (actor, g_value_get_string (value));
      break;

    case PROP_STYLE:
      st_widget_set_style (actor, g_value_get_string (value));
      break;

    case PROP_STYLABLE:
      if (actor->priv->is_stylable != g_value_get_boolean (value))
        {
          actor->priv->is_stylable = g_value_get_boolean (value);
          clutter_actor_queue_relayout ((ClutterActor *) gobject);
        }
      break;

    case PROP_HAS_TOOLTIP:
      st_widget_set_has_tooltip (actor, g_value_get_boolean (value));
      break;

    case PROP_TOOLTIP_TEXT:
      st_widget_set_tooltip_text (actor, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_widget_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  StWidget *actor = ST_WIDGET (gobject);
  StWidgetPrivate *priv = actor->priv;

  switch (prop_id)
    {
    case PROP_THEME:
      g_value_set_object (value, priv->theme);
      break;

    case PROP_PSEUDO_CLASS:
      g_value_set_string (value, priv->pseudo_class);
      break;

    case PROP_STYLE_CLASS:
      g_value_set_string (value, priv->style_class);
      break;

    case PROP_STYLE:
      g_value_set_string (value, priv->inline_style);
      break;

    case PROP_STYLABLE:
      g_value_set_boolean (value, priv->is_stylable);
      break;

    case PROP_HAS_TOOLTIP:
      g_value_set_boolean (value, priv->has_tooltip);
      break;

    case PROP_TOOLTIP_TEXT:
      g_value_set_string (value, st_widget_get_tooltip_text (actor));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_widget_dispose (GObject *gobject)
{
  StWidget *actor = ST_WIDGET (gobject);
  StWidgetPrivate *priv = ST_WIDGET (actor)->priv;

  if (priv->theme)
    {
      g_object_unref (priv->theme);
      priv->theme = NULL;
    }

  if (priv->border_image)
    {
      clutter_actor_unparent (priv->border_image);
      priv->border_image = NULL;
    }

  if (priv->background_image_shadow)
    {
      clutter_actor_unparent (priv->background_image_shadow);
      priv->background_image_shadow = NULL;
    }

  if (priv->tooltip)
    {
      ClutterContainer *parent;
      ClutterActor *tooltip = CLUTTER_ACTOR (priv->tooltip);

      /* this is just a little bit awkward because the tooltip is parented
       * on the stage, but we still want to "own" it */
      parent = CLUTTER_CONTAINER (clutter_actor_get_parent (tooltip));

      if (parent)
        clutter_container_remove_actor (parent, tooltip);

      priv->tooltip = NULL;
    }

  G_OBJECT_CLASS (st_widget_parent_class)->dispose (gobject);
}

static void
st_widget_finalize (GObject *gobject)
{
  StWidgetPrivate *priv = ST_WIDGET (gobject)->priv;

  g_free (priv->style_class);
  g_free (priv->pseudo_class);

  G_OBJECT_CLASS (st_widget_parent_class)->finalize (gobject);
}

static void
st_widget_allocate (ClutterActor          *actor,
                    const ClutterActorBox *box,
                    ClutterAllocationFlags flags)
{
  StWidgetPrivate *priv = ST_WIDGET (actor)->priv;
  StThemeNode *theme_node;
  ClutterActorClass *klass;
  ClutterGeometry area;
  ClutterVertex in_v, out_v;

  theme_node = st_widget_get_theme_node ((StWidget*) actor);

  klass = CLUTTER_ACTOR_CLASS (st_widget_parent_class);
  klass->allocate (actor, box, flags);

  /* update tooltip position */
  if (priv->tooltip)
    {
      in_v.x = in_v.y = in_v.z = 0;
      clutter_actor_apply_transform_to_point (actor, &in_v, &out_v);
      area.x = out_v.x;
      area.y = out_v.y;

      in_v.x = box->x2 - box->x1;
      in_v.y = box->y2 - box->y1;
      clutter_actor_apply_transform_to_point (actor, &in_v, &out_v);
      area.width = out_v.x - area.x;
      area.height = out_v.y - area.y;

      st_tooltip_set_tip_area (priv->tooltip, &area);
    }



  if (priv->border_image && priv->bg_gradient_type == ST_GRADIENT_NONE)
    {
      ClutterActorBox frame_box = {
        0,
        0,
        box->x2 - box->x1,
        box->y2 - box->y1
      };

      clutter_actor_allocate (CLUTTER_ACTOR (priv->border_image),
                              &frame_box,
                              flags);
    }
  else if (priv->bg_gradient_type != ST_GRADIENT_NONE)
    {
      guint width,  old_width,
            height, old_height;
      ClutterActorBox frame_box;

      frame_box.x1 = frame_box.y1 = 0;
      frame_box.x2 = box->x2 - box->x1;
      frame_box.y2 = box->y2 - box->y1;

      width = (guint)(0.5 + frame_box.x2);
      height = (guint)(0.5 + frame_box.y2);

      clutter_cairo_texture_get_surface_size (CLUTTER_CAIRO_TEXTURE (priv->border_image),
                                              &old_width, &old_height);

      if (width > 0 && height > 0 &&
          (old_width != width || old_height != height))
        {

          clutter_cairo_texture_set_surface_size (CLUTTER_CAIRO_TEXTURE (priv->border_image),
                                                  width, height);
          st_widget_redraw_gradient ((StWidget*) actor);
        }
      clutter_actor_allocate (CLUTTER_ACTOR (priv->border_image),
                              &frame_box,
                              flags);
    }

  if (priv->background_image)
    {
      ClutterActorBox frame_box = {
        0, 0, box->x2 - box->x1, box->y2 - box->y1
      };
      gfloat w, h;

      clutter_actor_get_size (CLUTTER_ACTOR (priv->background_image), &w, &h);

      /* scale the background into the allocated bounds */
      if (w > frame_box.x2 || h > frame_box.y2)
        {
          gint new_h, new_w, offset;
          gint box_w, box_h;

          box_w = (int) frame_box.x2;
          box_h = (int) frame_box.y2;

          /* scale to fit */
          new_h = (int)((h / w) * ((gfloat) box_w));
          new_w = (int)((w / h) * ((gfloat) box_h));

          if (new_h > box_h)
            {
              /* center for new width */
              offset = ((box_w) - new_w) * 0.5;
              frame_box.x1 = offset;
              frame_box.x2 = offset + new_w;

              frame_box.y2 = box_h;
            }
          else
            {
              /* center for new height */
              offset = ((box_h) - new_h) * 0.5;
              frame_box.y1 = offset;
              frame_box.y2 = offset + new_h;

              frame_box.x2 = box_w;
            }

        }
      else
        {
          /* center the background on the widget */
          frame_box.x1 = (int)(((box->x2 - box->x1) / 2) - (w / 2));
          frame_box.y1 = (int)(((box->y2 - box->y1) / 2) - (h / 2));
          frame_box.x2 = frame_box.x1 + w;
          frame_box.y2 = frame_box.y1 + h;
        }

        if (priv->background_image_shadow)
          {
            StShadowTexture *shadow;
            ClutterActorBox  shadow_box;

            shadow_box.x1 = frame_box.x1 + priv->shadow_xoffset;
            shadow_box.y1 = frame_box.y1 + priv->shadow_yoffset;
            shadow_box.x2 = frame_box.x2 + priv->shadow_xoffset;
            shadow_box.y2 = frame_box.y2 + priv->shadow_yoffset;

            /* The shadow texture is larger than the original image due
               to blurring, so we let it adjust its size.
               When the original image has been scaled, this will change
               the effective blur radius - we ignore this for now. */
            shadow = ST_SHADOW_TEXTURE (priv->background_image_shadow);
            st_shadow_texture_adjust_allocation (shadow, &shadow_box);

            clutter_actor_allocate (priv->background_image_shadow,
                                    &shadow_box, flags);
          }


      clutter_actor_allocate (CLUTTER_ACTOR (priv->background_image),
                              &frame_box,
                              flags);
    }
}

static void
st_widget_real_draw_background (StWidget *self)
{
  StWidgetPrivate *priv = self->priv;
  ClutterActor *actor = CLUTTER_ACTOR (self);
  ClutterActorBox allocation = { 0, };
  gfloat w, h;
  guint8 opacity;

  clutter_actor_get_allocation_box (actor, &allocation);
  w = allocation.x2 - allocation.x1;
  h = allocation.y2 - allocation.y1;

  opacity = clutter_actor_get_paint_opacity (actor);

  /* Default implementation just draws the background
   * colour and the image on top
   */
  if (priv->draw_bg_color)
    {
      ClutterColor bg_color = priv->bg_color;

      bg_color.alpha = opacity * bg_color.alpha / 255;

      cogl_set_source_color4ub (bg_color.red,
                                bg_color.green,
                                bg_color.blue,
                                bg_color.alpha);
      cogl_rectangle (0, 0, w, h);
    }

  if (priv->draw_border_internal)
    {
      StThemeNode *node = st_widget_get_theme_node (self);
      int side;
      double border_top, border_right, border_bottom, border_left;

      border_top = st_theme_node_get_border_width (node, ST_SIDE_TOP);
      border_right = st_theme_node_get_border_width (node, ST_SIDE_RIGHT);
      border_bottom = st_theme_node_get_border_width (node, ST_SIDE_BOTTOM);
      border_left = st_theme_node_get_border_width (node, ST_SIDE_LEFT);

      for (side = 0; side < 4; side++)
        {
          ClutterColor color;

          switch (side)
          {
            case ST_SIDE_TOP:
              if (border_top <= 0)
                continue;
              break;
            case ST_SIDE_RIGHT:
              if (border_right <= 0)
                continue;
              break;
            case ST_SIDE_BOTTOM:
              if (border_bottom <= 0)
                continue;
              break;
            case ST_SIDE_LEFT:
              if (border_left <= 0)
                continue;
              break;
          }

          st_theme_node_get_border_color (node, side, &color);

          color.alpha = (color.alpha * opacity) / 0xff;

          cogl_set_source_color4ub (color.red,
                                    color.green,
                                    color.blue,
                                    color.alpha);

          /* Note top and bottom extend to the ends, left/right
           * are constrained by them.  See comment above about CSS
           * conformance.
           */
          switch (side)
          {
            case ST_SIDE_TOP:
              cogl_rectangle (0, 0,
                              w, border_top);
              break;
            case ST_SIDE_RIGHT:
              cogl_rectangle (w - border_right, border_top,
                              w, h - border_bottom);
              break;
            case ST_SIDE_BOTTOM:
              cogl_rectangle (0, h - border_bottom,
                              w, h);
              break;
            case ST_SIDE_LEFT:
              cogl_rectangle (0, border_top,
                              border_left, h - border_bottom);
              break;
            }
        }
    }

  if (priv->border_image)
    clutter_actor_paint (priv->border_image);
}

static void
st_widget_paint (ClutterActor *self)
{
  StWidgetPrivate *priv = ST_WIDGET (self)->priv;
  StWidgetClass *klass = ST_WIDGET_GET_CLASS (self);

  klass->draw_background (ST_WIDGET (self));

  if (priv->background_image != NULL)
    {
      if (priv->background_image_shadow)
        clutter_actor_paint (priv->background_image_shadow);
      clutter_actor_paint (priv->background_image);
    }
}

static void
st_widget_parent_set (ClutterActor *widget,
                      ClutterActor *old_parent)
{
  ClutterActorClass *parent_class;
  ClutterActor *new_parent;

  parent_class = CLUTTER_ACTOR_CLASS (st_widget_parent_class);
  if (parent_class->parent_set)
    parent_class->parent_set (widget, old_parent);

  new_parent = clutter_actor_get_parent (widget);

  /* don't send the style changed signal if we no longer have a parent actor */
  if (new_parent)
    st_widget_style_changed (ST_WIDGET (widget));
}

static void
st_widget_map (ClutterActor *actor)
{
  StWidgetPrivate *priv = ST_WIDGET (actor)->priv;

  CLUTTER_ACTOR_CLASS (st_widget_parent_class)->map (actor);

  st_widget_ensure_style ((StWidget*) actor);

  if (priv->background_image_shadow)
    clutter_actor_map (priv->background_image_shadow);

  if (priv->border_image)
    clutter_actor_map (priv->border_image);

  if (priv->background_image)
    clutter_actor_map (priv->background_image);

  if (priv->tooltip)
    clutter_actor_map ((ClutterActor *) priv->tooltip);
}

static void
st_widget_unmap (ClutterActor *actor)
{
  StWidgetPrivate *priv = ST_WIDGET (actor)->priv;

  CLUTTER_ACTOR_CLASS (st_widget_parent_class)->unmap (actor);

  if (priv->background_image_shadow)
    clutter_actor_unmap (priv->background_image_shadow);

  if (priv->border_image)
    clutter_actor_unmap (priv->border_image);

  if (priv->background_image)
    clutter_actor_unmap (priv->background_image);

  if (priv->tooltip)
    clutter_actor_unmap ((ClutterActor *) priv->tooltip);
}

static void
st_widget_redraw_gradient (StWidget  *widget)
{
  ClutterCairoTexture *texture;
  ClutterColor *start, *end;
  StWidgetPrivate *priv;
  guint width, height;
  guint radius[4], i;
  cairo_t *cr;
  cairo_pattern_t *pattern;
  gboolean round_border = FALSE;

  if (widget->priv->bg_gradient_type == ST_GRADIENT_NONE)
    return;

  texture = CLUTTER_CAIRO_TEXTURE (widget->priv->border_image);
  priv  = widget->priv;
  start = &widget->priv->bg_color;
  end   = &widget->priv->bg_gradient_end;

  for (i = 0; i < 4; i++)
    {
      radius[i] = st_theme_node_get_border_radius (priv->theme_node, i);
      if (radius[i] > 0)
        round_border = TRUE;
    }

  clutter_cairo_texture_get_surface_size (texture, &width, &height);
  clutter_cairo_texture_clear (texture);
  cr = clutter_cairo_texture_create (texture);

  if (priv->bg_gradient_type == ST_GRADIENT_VERTICAL)
    pattern = cairo_pattern_create_linear (0, 0, 0, height);
  else if (priv->bg_gradient_type == ST_GRADIENT_HORIZONTAL)
    pattern = cairo_pattern_create_linear (0, 0, width, 0);
  else
    {
      gdouble cx, cy;

      cx = width / 2.;
      cy = height / 2.;
      pattern = cairo_pattern_create_radial (cx, cy, 0, cx, cy, MIN (cx, cy));
    }

  cairo_pattern_add_color_stop_rgba (pattern, 0,
                                     start->red / 255.,
                                     start->green / 255.,
                                     start->blue / 255.,
                                     start->alpha / 255.);
  cairo_pattern_add_color_stop_rgba (pattern, 1,
                                     end->red / 255.,
                                     end->green / 255.,
                                     end->blue / 255.,
                                     end->alpha / 255.);

  if (round_border)
    {
      if (radius[ST_CORNER_TOPLEFT] > 0)
        cairo_arc (cr,
                   radius[ST_CORNER_TOPLEFT],
                   radius[ST_CORNER_TOPLEFT],
                   radius[ST_CORNER_TOPLEFT], M_PI, 3 * M_PI / 2);
      else
        cairo_move_to (cr, 0, 0);
      cairo_line_to (cr, width - radius[ST_CORNER_TOPRIGHT], 0);
      if (radius[ST_CORNER_TOPRIGHT] > 0)
        cairo_arc (cr,
                   width - radius[ST_CORNER_TOPRIGHT],
                   radius[ST_CORNER_TOPRIGHT],
                   radius[ST_CORNER_TOPRIGHT], 3 * M_PI / 2, 2 * M_PI);
      cairo_line_to (cr, width, height - radius[ST_CORNER_BOTTOMRIGHT]);
      if (radius[ST_CORNER_BOTTOMRIGHT])
        cairo_arc (cr,
                   width - radius[ST_CORNER_BOTTOMRIGHT],
                   height - radius[ST_CORNER_BOTTOMRIGHT],
                   radius[ST_CORNER_BOTTOMRIGHT], 0, M_PI / 2);
      cairo_line_to (cr, radius[ST_CORNER_BOTTOMLEFT], height);
      if (radius[ST_CORNER_BOTTOMLEFT])
        cairo_arc (cr,
                   radius[ST_CORNER_BOTTOMLEFT],
                   height - radius[ST_CORNER_BOTTOMLEFT],
                   radius[ST_CORNER_BOTTOMLEFT], M_PI / 2, M_PI);
      cairo_close_path (cr);
    }
  else
    cairo_rectangle (cr, 0, 0, width, height);

  if (priv->border_width > 0)
    {
      guint8 opacity;
      gdouble effective_alpha;
      cairo_path_t *path;

      path = cairo_copy_path (cr);
      opacity = clutter_actor_get_paint_opacity (CLUTTER_ACTOR (widget));
      effective_alpha = priv->border_color.alpha * opacity / (255. * 255.);

      cairo_set_source_rgba (cr,
                             priv->border_color.red / 255.,
                             priv->border_color.green / 255.,
                             priv->border_color.blue / 255.,
                             effective_alpha);
      cairo_fill (cr);

      cairo_translate (cr, priv->border_width, priv->border_width);
      cairo_scale (cr,
                   (gdouble)(width - 2 * priv->border_width) / width,
                   (gdouble)(height - 2 * priv->border_width) / height);
      cairo_append_path (cr, path);
      cairo_path_destroy (path);
    }

  cairo_set_source (cr, pattern);
  cairo_fill (cr);

  cairo_pattern_destroy (pattern);
  cairo_destroy (cr);
}

static void notify_children_of_style_change (ClutterContainer *container);

static void
notify_children_of_style_change_foreach (ClutterActor *actor,
                                         gpointer      user_data)
{
  if (ST_IS_WIDGET (actor))
    st_widget_style_changed (ST_WIDGET (actor));
  else if (CLUTTER_IS_CONTAINER (actor))
    notify_children_of_style_change ((ClutterContainer *)actor);
}

static void
notify_children_of_style_change (ClutterContainer *container)
{
  /* notify our children that their parent stylable has changed */
  clutter_container_foreach (container,
                             notify_children_of_style_change_foreach,
                             NULL);
}

static void
st_widget_real_style_changed (StWidget *self)
{
  StWidgetPrivate *priv = ST_WIDGET (self)->priv;
  StThemeNode *theme_node;
  StBorderImage *border_image;
  StShadow *shadow;
  StTextureCache *texture_cache;
  ClutterTexture *texture;
  const char *bg_file = NULL;
  gboolean relayout_needed = FALSE;
  gboolean has_changed = FALSE;
  ClutterColor color;
  guint border_radius = 0;
  StGradientType gradient;
  ClutterColor gradient_end;
  StSide side;
  StCorner corner;
  gboolean uniform_border_width;

  /* application has request this widget is not stylable */
  if (!priv->is_stylable)
    return;

  theme_node = st_widget_get_theme_node (self);

  st_theme_node_get_background_gradient (theme_node, &gradient, &color, &gradient_end);

  if (gradient == ST_GRADIENT_NONE)
    {
      st_theme_node_get_background_color (theme_node, &color);
      if (gradient != priv->bg_gradient_type ||
          !clutter_color_equal (&color, &priv->bg_color))
        {
          priv->bg_gradient_type = gradient;
          priv->bg_color = color;
          priv->draw_bg_color = color.alpha != 0;
          has_changed = TRUE;
        }
    }
  else if (gradient != priv->bg_gradient_type ||
           !clutter_color_equal (&color, &priv->bg_color) ||
           !clutter_color_equal (&gradient_end, &priv->bg_gradient_end))
    {
      priv->bg_gradient_type = gradient;
      priv->bg_color = color;
      priv->bg_gradient_end = gradient_end;
      priv->draw_bg_color = TRUE;
      has_changed = TRUE;
    }

  if (priv->background_image_shadow)
    {
      clutter_actor_unparent (priv->background_image_shadow);
      priv->background_image_shadow = NULL;
    }

  if (priv->border_image)
    {
      clutter_actor_unparent (priv->border_image);
      priv->border_image = NULL;
    }

  if (priv->background_image)
    {
      clutter_actor_unparent (priv->background_image);
      priv->background_image = NULL;
    }

  texture_cache = st_texture_cache_get_default ();


  /* Rough notes about the relationship of borders and backgrounds in CSS3;
   * see http://www.w3.org/TR/css3-background/ for more accurate details.
   *
   * - Things are drawn in 4 layers, from the bottom:
   *     Background color
   *     Background image
   *     Border color or border image
   *     Content
   * - The background color, gradient and image extend to and are clipped by
   *   the edge of the border area, so will be rounded if the border is
   *   rounded. (CSS3 background-clip property modifies this)
   * - The border image replaces what would normally be drawn by the border
   * - The border image is not clipped by a rounded border-radius
   * - The border radius rounds the background even if the border is
   *   zero width or a border image is being used.
   *
   * Deviations from the above as implemented here:
   *  - Nonuniform border widths combined with a non-zero border radius result
   *    in the border radius being ignored
   *  - The combination of border image and a non-zero border radius is
   *    not supported; the background color will be drawn with square
   *    corners.
   *  - The combination of border image and a background gradient is not
   *    supported; the background will be drawn as a solid color
   *  - The background image is drawn above the border color or image,
   *    not below it.
   *  - We don't clip the background image to the (rounded) border area.
   *
   * The first three allow us to always draw with no more than a single
   * border_image and a single background image above it.
   */

  /* Check whether all border widths are the same.  Also, acquire the
   * first nonzero border width as well as the border color.
   */
  uniform_border_width = TRUE;
  priv->border_width = st_theme_node_get_border_width (theme_node,
                                                       ST_SIDE_TOP);
  if (priv->border_width > 0.5)
    priv->border_width = (int)(0.5 + priv->border_width);
  for (side = 0; side < 4; side++)
    {
      double width = st_theme_node_get_border_width (theme_node, side);
      if (width > 0.5)
        width = (int)(0.5 + width);
      if (width > 0)
        {
          priv->border_width = width;
          st_theme_node_get_border_color (theme_node,
                                          side, &priv->border_color);
        }
      if ((int)width != priv->border_width)
        {
          uniform_border_width = FALSE;
          break;
        }
    }

  /* Pick the first nonzero border radius, but only if we have a uniform border. */
  if (uniform_border_width)
    {
      for (corner = 0; corner < 4; corner++)
        {
          double radius = st_theme_node_get_border_radius (theme_node, corner);
          if (radius > 0.5)
            {
              border_radius = (int)(0.5 + radius);
              break;
            }
        }
    }

  border_image = st_theme_node_get_border_image (theme_node);
  if (border_image)
    {
      const char *filename;
      gint border_left, border_right, border_top, border_bottom;
      gint width, height;

      filename = st_border_image_get_filename (border_image);

      /* `border-image' takes precedence over `background-image'.
       * Firefox lets the background-image shine thru when border-image has
       * alpha an channel, maybe that would be an option for the future. */
      texture = st_texture_cache_get_texture (texture_cache,
                                                filename);

      clutter_texture_get_base_size (CLUTTER_TEXTURE (texture),
                                     &width, &height);

      st_border_image_get_borders (border_image,
                                   &border_left, &border_right, &border_top, &border_bottom);

      priv->border_image = st_texture_frame_new (texture,
                                                 border_top,
                                                 border_right,
                                                 border_bottom,
                                                 border_left);
      clutter_actor_set_parent (priv->border_image, CLUTTER_ACTOR (self));

      has_changed = TRUE;
      relayout_needed = TRUE;
    }
  else if (priv->bg_gradient_type != ST_GRADIENT_NONE)
    {
      priv->draw_border_internal = FALSE;
      priv->draw_bg_color = FALSE;
      texture = g_object_new (CLUTTER_TYPE_CAIRO_TEXTURE, NULL);
      priv->border_image = CLUTTER_ACTOR (texture);
      clutter_actor_set_parent (priv->border_image, CLUTTER_ACTOR (self));

      has_changed = TRUE;
      relayout_needed = TRUE;
    }
  else if (border_radius > 0)
    {
      priv->draw_border_internal = FALSE;
      priv->draw_bg_color = FALSE;
      priv->border_image = g_object_new (BIG_TYPE_RECTANGLE,
					 "color", &priv->bg_color,
					 "border-width", priv->border_width,
					 "border-color", &priv->border_color,
					 "corner-radius", border_radius,
					 NULL);

      clutter_actor_set_parent (priv->border_image, CLUTTER_ACTOR (self));

      has_changed = TRUE;
      relayout_needed = TRUE;
    }
  else if (priv->border_width > 0 && priv->border_color.alpha != 0)
    {
      priv->draw_bg_color = TRUE;
      priv->draw_border_internal = TRUE;
      has_changed = TRUE;
      relayout_needed = TRUE;
    }
  else if (priv->draw_border_internal)
    {
      priv->draw_border_internal = FALSE;
      has_changed = TRUE;
      relayout_needed = TRUE;
    }

  bg_file = st_theme_node_get_background_image (theme_node);
  if (bg_file != NULL)
    {
      texture = st_texture_cache_get_texture (texture_cache, bg_file);
      priv->background_image = (ClutterActor*) texture;

      if (priv->background_image != NULL)
        {
          clutter_actor_set_parent (priv->background_image,
                                    CLUTTER_ACTOR (self));
        }
      else
        g_warning ("Could not load %s", bg_file);

      has_changed = TRUE;
      relayout_needed = TRUE;
    }

  /* CSS based drop shadows
   *
   * Drop shadows in ST are modelled after the CSS3 box-shadow property;
   * see http://www.css3.info/preview/box-shadow/ for a detailed description.
   *
   * While the syntax of the property is mostly identical - we do not support
   * multiple shadows and allow for a more liberal placement of the color
   * parameter - its interpretation defers significantly in that the shadow's
   * shape is not determined by the bounding box, but by the CSS background
   * image (we could exend this in the future to take other CSS properties
   * like boder and background color into account).
   */
  shadow = st_theme_node_get_shadow (theme_node);
  if (shadow != NULL)
    {
      priv->shadow_xoffset = shadow->xoffset;
      priv->shadow_yoffset = shadow->yoffset;

      if (priv->background_image)
        {
          priv->background_image_shadow =
              st_shadow_texture_new (priv->background_image,
                                     &shadow->color,
                                     shadow->blur);

          clutter_actor_set_parent (priv->background_image_shadow,
                                    CLUTTER_ACTOR (self));
          has_changed = TRUE;
          relayout_needed = TRUE;
        }
    }

  /* If there are any properties above that need to cause a relayout they
   * should set this flag.
   */
  if (has_changed)
    {
      if (relayout_needed)
        clutter_actor_queue_relayout ((ClutterActor *) self);
      else
        clutter_actor_queue_redraw ((ClutterActor *) self);
    }

  if (CLUTTER_IS_CONTAINER (self))
    notify_children_of_style_change ((ClutterContainer *)self);
}

void
st_widget_style_changed (StWidget *widget)
{
  StThemeNode *old_theme_node = NULL;

  widget->priv->is_style_dirty = TRUE;
  if (widget->priv->theme_node)
    {
      old_theme_node = widget->priv->theme_node;
      widget->priv->theme_node = NULL;
    }

  /* update the style only if we are mapped */
  if (CLUTTER_ACTOR_IS_MAPPED (CLUTTER_ACTOR (widget)))
    st_widget_recompute_style (widget, old_theme_node);

  if (old_theme_node)
    g_object_unref (old_theme_node);
}

static void
on_theme_context_changed (StThemeContext *context,
                          ClutterStage      *stage)
{
  notify_children_of_style_change (CLUTTER_CONTAINER (stage));
}

static StThemeNode *
get_root_theme_node (ClutterStage *stage)
{
  StThemeContext *context = st_theme_context_get_for_stage (stage);

  if (!g_object_get_data (G_OBJECT (context), "st-theme-initialized"))
    {
      g_object_set_data (G_OBJECT (context), "st-theme-initialized", GUINT_TO_POINTER (1));
      g_signal_connect (G_OBJECT (context), "changed",
                        G_CALLBACK (on_theme_context_changed), stage);
    }

  return st_theme_context_get_root_node (context);
}

/**
 * st_widget_get_theme_node:
 * @widget: a #StWidget
 *
 * Gets the theme node holding style information for the widget.
 * The theme node is used to access standard and custom CSS
 * properties of the widget.
 *
 * Return value: (transfer none): the theme node for the widget.
 *   This is owned by the widget. When attributes of the widget
 *   or the environment that affect the styling change (for example
 *   the style_class property of the widget), it will be recreated,
 *   and the ::style-changed signal will be emitted on the widget.
 */
StThemeNode *
st_widget_get_theme_node (StWidget *widget)
{
  StWidgetPrivate *priv = widget->priv;

  if (priv->theme_node == NULL)
    {
      StThemeNode *parent_node = NULL;
      ClutterStage *stage = NULL;
      ClutterActor *parent;

      parent = clutter_actor_get_parent (CLUTTER_ACTOR (widget));
      while (parent != NULL)
        {
          if (parent_node == NULL && ST_IS_WIDGET (parent))
            parent_node = st_widget_get_theme_node (ST_WIDGET (parent));
          else if (CLUTTER_IS_STAGE (parent))
            stage = CLUTTER_STAGE (parent);

          parent = clutter_actor_get_parent (parent);
        }

      if (stage == NULL)
        {
          g_warning ("st_widget_get_theme_node called on a widget not in a stage");
          stage = CLUTTER_STAGE (clutter_stage_get_default ());
        }

      if (parent_node == NULL)
        parent_node = get_root_theme_node (CLUTTER_STAGE (stage));

      priv->theme_node = st_theme_node_new (st_theme_context_get_for_stage (stage),
                                            parent_node, priv->theme,
                                            G_OBJECT_TYPE (widget),
                                            clutter_actor_get_name (CLUTTER_ACTOR (widget)),
                                            priv->style_class,
                                            priv->pseudo_class,
                                            priv->inline_style);
    }

  return priv->theme_node;
}

static gboolean
st_widget_enter (ClutterActor         *actor,
                 ClutterCrossingEvent *event)
{
  StWidgetPrivate *priv = ST_WIDGET (actor)->priv;


  if (priv->has_tooltip)
    st_widget_show_tooltip ((StWidget*) actor);

  if (CLUTTER_ACTOR_CLASS (st_widget_parent_class)->enter_event)
    return CLUTTER_ACTOR_CLASS (st_widget_parent_class)->enter_event (actor, event);
  else
    return FALSE;
}

static gboolean
st_widget_leave (ClutterActor         *actor,
                 ClutterCrossingEvent *event)
{
  StWidgetPrivate *priv = ST_WIDGET (actor)->priv;

  if (priv->has_tooltip)
    st_tooltip_hide (priv->tooltip);

  if (CLUTTER_ACTOR_CLASS (st_widget_parent_class)->leave_event)
    return CLUTTER_ACTOR_CLASS (st_widget_parent_class)->leave_event (actor, event);
  else
    return FALSE;
}

static void
st_widget_hide (ClutterActor *actor)
{
  StWidget *widget = (StWidget *) actor;

  /* hide the tooltip, if there is one */
  if (widget->priv->tooltip)
    st_tooltip_hide (ST_TOOLTIP (widget->priv->tooltip));

  CLUTTER_ACTOR_CLASS (st_widget_parent_class)->hide (actor);
}



static void
st_widget_class_init (StWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (StWidgetPrivate));

  gobject_class->set_property = st_widget_set_property;
  gobject_class->get_property = st_widget_get_property;
  gobject_class->dispose = st_widget_dispose;
  gobject_class->finalize = st_widget_finalize;

  actor_class->allocate = st_widget_allocate;
  actor_class->paint = st_widget_paint;
  actor_class->parent_set = st_widget_parent_set;
  actor_class->map = st_widget_map;
  actor_class->unmap = st_widget_unmap;

  actor_class->enter_event = st_widget_enter;
  actor_class->leave_event = st_widget_leave;
  actor_class->hide = st_widget_hide;

  klass->draw_background = st_widget_real_draw_background;
  klass->style_changed = st_widget_real_style_changed;

  /**
   * StWidget:pseudo-class:
   *
   * The pseudo-class of the actor. Typical values include "hover", "active",
   * "focus".
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PSEUDO_CLASS,
                                   g_param_spec_string ("pseudo-class",
                                                        "Pseudo Class",
                                                        "Pseudo class for styling",
                                                        "",
                                                        ST_PARAM_READWRITE));
  /**
   * StWidget:style-class:
   *
   * The style-class of the actor for use in styling.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STYLE_CLASS,
                                   g_param_spec_string ("style-class",
                                                        "Style Class",
                                                        "Style class for styling",
                                                        "",
                                                        ST_PARAM_READWRITE));

  /**
   * StWidget:style:
   *
   * Inline style information for the actor as a ';'-separated list of
   * CSS properties.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STYLE,
                                   g_param_spec_string ("style",
                                                        "Style",
                                                        "Inline style string",
                                                        "",
                                                        ST_PARAM_READWRITE));

  /**
   * StWidget:theme
   *
   * A theme set on this actor overriding the global theming for this actor
   * and its descendants
   */
  g_object_class_install_property (gobject_class,
                                   PROP_THEME,
                                   g_param_spec_object ("theme",
                                                        "Theme",
                                                        "Theme override",
                                                        ST_TYPE_THEME,
                                                        ST_PARAM_READWRITE));

  /**
   * StWidget:stylable:
   *
   * Enable or disable styling of the widget
   */
  pspec = g_param_spec_boolean ("stylable",
                                "Stylable",
                                "Whether the table should be styled",
                                TRUE,
                                ST_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_STYLABLE,
                                   pspec);

  /**
   * StWidget:has-tooltip:
   *
   * Determines whether the widget has a tooltip. If set to TRUE, causes the
   * widget to monitor enter and leave events (i.e. sets the widget reactive).
   */
  pspec = g_param_spec_boolean ("has-tooltip",
                                "Has Tooltip",
                                "Determines whether the widget has a tooltip",
                                FALSE,
                                ST_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_HAS_TOOLTIP,
                                   pspec);


  /**
   * StWidget:tooltip-text:
   *
   * text displayed on the tooltip
   */
  pspec = g_param_spec_string ("tooltip-text",
                               "Tooltip Text",
                               "Text displayed on the tooltip",
                               "",
                               ST_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TOOLTIP_TEXT, pspec);

  /**
   * StWidget::style-changed:
   *
   * Emitted when the style information that the widget derives from the
   * theme changes
   */
  signals[STYLE_CHANGED] =
    g_signal_new ("style-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StWidgetClass, style_changed),
                  NULL, NULL,
                  _st_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

/**
 * st_widget_set_theme:
 * @actor: a #StWidget
 * @theme: a new style class string
 *
 * Overrides the theme that would be inherited from the actor's parent
 * or the stage with an entirely new theme (set of stylesheets).
 */
void
st_widget_set_theme (StWidget  *actor,
                      StTheme  *theme)
{
  StWidgetPrivate *priv = actor->priv;

  g_return_if_fail (ST_IS_WIDGET (actor));

  priv = actor->priv;

  if (theme !=priv->theme)
    {
      if (priv->theme)
        g_object_unref (priv->theme);
      priv->theme = g_object_ref (priv->theme);

      st_widget_style_changed (actor);

      g_object_notify (G_OBJECT (actor), "theme");
    }
}

/**
 * st_widget_get_theme:
 * @actor: a #StWidget
 *
 * Gets the overriding theme set on the actor. See st_widget_set_theme()
 *
 * Return value: (transfer none): the overriding theme, or %NULL
 */
StTheme *
st_widget_get_theme (StWidget *actor)
{
  g_return_val_if_fail (ST_IS_WIDGET (actor), NULL);

  return actor->priv->theme;
}

/**
 * st_widget_set_style_class_name:
 * @actor: a #StWidget
 * @style_class: a new style class string
 *
 * Set the style class name
 */
void
st_widget_set_style_class_name (StWidget    *actor,
                                const gchar *style_class)
{
  StWidgetPrivate *priv = actor->priv;

  g_return_if_fail (ST_IS_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (style_class, priv->style_class))
    {
      g_free (priv->style_class);
      priv->style_class = g_strdup (style_class);

      st_widget_style_changed (actor);

      g_object_notify (G_OBJECT (actor), "style-class");
    }
}


/**
 * st_widget_get_style_class_name:
 * @actor: a #StWidget
 *
 * Get the current style class name
 *
 * Returns: the class name string. The string is owned by the #StWidget and
 * should not be modified or freed.
 */
const gchar*
st_widget_get_style_class_name (StWidget *actor)
{
  g_return_val_if_fail (ST_IS_WIDGET (actor), NULL);

  return actor->priv->style_class;
}

/**
 * st_widget_get_style_pseudo_class:
 * @actor: a #StWidget
 *
 * Get the current style pseudo class
 *
 * Returns: the pseudo class string. The string is owned by the #StWidget and
 * should not be modified or freed.
 */
const gchar*
st_widget_get_style_pseudo_class (StWidget *actor)
{
  g_return_val_if_fail (ST_IS_WIDGET (actor), NULL);

  return actor->priv->pseudo_class;
}

/**
 * st_widget_set_style_pseudo_class:
 * @actor: a #StWidget
 * @pseudo_class: (allow-none): a new pseudo class string
 *
 * Set the style pseudo class
 */
void
st_widget_set_style_pseudo_class (StWidget    *actor,
                                  const gchar *pseudo_class)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (pseudo_class, priv->pseudo_class))
    {
      g_free (priv->pseudo_class);
      priv->pseudo_class = g_strdup (pseudo_class);

      st_widget_style_changed (actor);

      g_object_notify (G_OBJECT (actor), "pseudo-class");
    }
}

/**
 * st_widget_set_style:
 * @actor: a #StWidget
 * @style_class: (allow-none): a inline style string, or %NULL
 *
 * Set the inline style string for this widget. The inline style string is an
 * optional ';'-separated list of CSS properties that override the style as
 * determined from the stylesheets of the current theme.
 */
void
st_widget_set_style (StWidget  *actor,
                       const gchar *style)
{
  StWidgetPrivate *priv = actor->priv;

  g_return_if_fail (ST_IS_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (style, priv->inline_style))
    {
      g_free (priv->inline_style);
      priv->inline_style = g_strdup (style);

      st_widget_style_changed (actor);

      g_object_notify (G_OBJECT (actor), "style");
    }
}

/**
 * st_widget_get_style:
 * @actor: a #StWidget
 *
 * Get the current inline style string. See st_widget_set_style().
 *
 * Returns: The inline style string, or %NULL. The string is owned by the
 * #StWidget and should not be modified or freed.
 */
const gchar*
st_widget_get_style (StWidget *actor)
{
  g_return_val_if_fail (ST_IS_WIDGET (actor), NULL);

  return actor->priv->inline_style;
}

static void
st_widget_name_notify (StWidget   *widget,
                       GParamSpec *pspec,
                       gpointer    data)
{
  st_widget_style_changed (widget);
}

static void
st_widget_init (StWidget *actor)
{
  StWidgetPrivate *priv;

  actor->priv = priv = ST_WIDGET_GET_PRIVATE (actor);
  priv->is_stylable = TRUE;

  /* connect style changed */
  g_signal_connect (actor, "notify::name", G_CALLBACK (st_widget_name_notify), NULL);
}

static void
st_widget_recompute_style (StWidget    *widget,
                           StThemeNode *old_theme_node)
{
  StThemeNode *new_theme_node = st_widget_get_theme_node (widget);

  if (!old_theme_node ||
      !st_theme_node_geometry_equal (old_theme_node, new_theme_node))
    clutter_actor_queue_relayout ((ClutterActor *) widget);

  g_signal_emit (widget, signals[STYLE_CHANGED], 0);
  widget->priv->is_style_dirty = FALSE;
}

/**
 * st_widget_ensure_style:
 * @widget: A #StWidget
 *
 * Ensures that @widget has read its style information.
 *
 */
void
st_widget_ensure_style (StWidget *widget)
{
  g_return_if_fail (ST_IS_WIDGET (widget));

  if (widget->priv->is_style_dirty)
    st_widget_recompute_style (widget, NULL);
}

static StTextDirection default_direction = ST_TEXT_DIRECTION_LTR;

StTextDirection
st_widget_get_default_direction (void)
{
  return default_direction;
}

void
st_widget_set_default_direction (StTextDirection dir)
{
  g_return_if_fail (dir != ST_TEXT_DIRECTION_NONE);

  default_direction = dir;
}

StTextDirection
st_widget_get_direction (StWidget *self)
{
  g_return_val_if_fail (ST_IS_WIDGET (self), ST_TEXT_DIRECTION_LTR);

  if (self->priv->direction != ST_TEXT_DIRECTION_NONE)
    return self->priv->direction;
  else
    return default_direction;
}

void
st_widget_set_direction (StWidget *self, StTextDirection dir)
{
  g_return_if_fail (ST_IS_WIDGET (self));
  self->priv->direction = dir;
}

/**
 * st_widget_get_border_image:
 * @actor: A #StWidget
 *
 * Get the texture used as the border image. This is set using the
 * "border-image" CSS property. This function should normally only be used
 * by subclasses.
 *
 * Returns: (transfer none): #ClutterActor
 */
ClutterActor *
st_widget_get_border_image (StWidget *actor)
{
  StWidgetPrivate *priv = ST_WIDGET (actor)->priv;
  return priv->border_image;
}

/**
 * st_widget_get_background_image:
 * @actor: A #StWidget
 *
 * Get the texture used as the background image. This is set using the
 * "background-image" CSS property. This function should normally only be used
 * by subclasses.
 *
 * Returns: (transfer none): a #ClutterActor
 */
ClutterActor *
st_widget_get_background_image (StWidget *actor)
{
  StWidgetPrivate *priv = ST_WIDGET (actor)->priv;
  return priv->background_image;
}

/**
 * st_widget_set_has_tooltip:
 * @widget: A #StWidget
 * @has_tooltip: #TRUE if the widget should display a tooltip
 *
 * Enables tooltip support on the #StWidget.
 *
 * Note that setting has-tooltip to #TRUE will cause the widget to be set
 * reactive. If you no longer need tooltip support and do not need the widget
 * to be reactive, you need to set ClutterActor::reactive to FALSE.
 *
 */
void
st_widget_set_has_tooltip (StWidget *widget,
                           gboolean  has_tooltip)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (widget));

  priv = widget->priv;

  priv->has_tooltip = has_tooltip;

  if (has_tooltip)
    {
      clutter_actor_set_reactive ((ClutterActor*) widget, TRUE);

      if (!priv->tooltip)
        {
          priv->tooltip = g_object_new (ST_TYPE_TOOLTIP, NULL);
          clutter_actor_set_parent ((ClutterActor *) priv->tooltip,
                                    (ClutterActor *) widget);
        }
    }
  else
    {
      if (priv->tooltip)
        {
          clutter_actor_unparent (CLUTTER_ACTOR (priv->tooltip));
          priv->tooltip = NULL;
        }
    }
}

/**
 * st_widget_get_has_tooltip:
 * @widget: A #StWidget
 *
 * Returns the current value of the has-tooltip property. See
 * st_tooltip_set_has_tooltip() for more information.
 *
 * Returns: current value of has-tooltip on @widget
 */
gboolean
st_widget_get_has_tooltip (StWidget *widget)
{
  g_return_val_if_fail (ST_IS_WIDGET (widget), FALSE);

  return widget->priv->has_tooltip;
}

/**
 * st_widget_set_tooltip_text:
 * @widget: A #StWidget
 * @text: text to set as the tooltip
 *
 * Set the tooltip text of the widget. This will set StWidget::has-tooltip to
 * #TRUE. A value of #NULL will unset the tooltip and set has-tooltip to #FALSE.
 *
 */
void
st_widget_set_tooltip_text (StWidget    *widget,
                            const gchar *text)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (widget));

  priv = widget->priv;

  if (text == NULL)
    st_widget_set_has_tooltip (widget, FALSE);
  else
    st_widget_set_has_tooltip (widget, TRUE);

  st_tooltip_set_label (priv->tooltip, text);
}

/**
 * st_widget_get_tooltip_text:
 * @widget: A #StWidget
 *
 * Get the current tooltip string
 *
 * Returns: The current tooltip string, owned by the #StWidget
 */
const gchar*
st_widget_get_tooltip_text (StWidget *widget)
{
  StWidgetPrivate *priv;

  g_return_val_if_fail (ST_IS_WIDGET (widget), NULL);
  priv = widget->priv;

  if (!priv->has_tooltip)
    return NULL;

  return st_tooltip_get_label (widget->priv->tooltip);
}

/**
 * st_widget_show_tooltip:
 * @widget: A #StWidget
 *
 * Show the tooltip for @widget
 *
 */
void
st_widget_show_tooltip (StWidget *widget)
{
  gfloat x, y, width, height;
  ClutterGeometry area;

  g_return_if_fail (ST_IS_WIDGET (widget));

  /* XXX not necceary, but first allocate transform is wrong */

  clutter_actor_get_transformed_position ((ClutterActor*) widget,
                                          &x, &y);

  clutter_actor_get_size ((ClutterActor*) widget, &width, &height);

  area.x = x;
  area.y = y;
  area.width = width;
  area.height = height;


  if (widget->priv->tooltip)
    {
      st_tooltip_set_tip_area (widget->priv->tooltip, &area);
      st_tooltip_show (widget->priv->tooltip);
    }
}

/**
 * st_widget_hide_tooltip:
 * @widget: A #StWidget
 *
 * Hide the tooltip for @widget
 *
 */
void
st_widget_hide_tooltip (StWidget *widget)
{
  g_return_if_fail (ST_IS_WIDGET (widget));

  if (widget->priv->tooltip)
    st_tooltip_hide (widget->priv->tooltip);
}

/**
 * st_widget_draw_background:
 * @widget: a #StWidget
 *
 * Invokes #StWidget::draw_background() using the default background
 * image and/or color from the @widget style
 *
 * This function should be used by subclasses of #StWidget that override
 * the paint() virtual function and cannot chain up
 */
void
st_widget_draw_background (StWidget *self)
{
  StWidgetPrivate *priv;
  StWidgetClass *klass;

  g_return_if_fail (ST_IS_WIDGET (self));

  priv = self->priv;

  klass = ST_WIDGET_GET_CLASS (self);
  klass->draw_background (ST_WIDGET (self));
}
