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

#include <clutter/clutter.h>
#include <ccss/ccss.h>

#include "st-widget.h"

#include "st-marshal.h"
#include "st-private.h"
#include "st-stylable.h"
#include "st-texture-cache.h"
#include "st-texture-frame.h"
#include "st-tooltip.h"

typedef ccss_border_image_t StBorderImage;

/*
 * Forward declaration for sake of StWidgetChild
 */
struct _StWidgetPrivate
{
  StPadding     border;
  StPadding     padding;

  StStyle      *style;
  gchar        *pseudo_class;
  gchar        *style_class;

  ClutterActor *border_image;
  ClutterActor *background_image;
  ClutterColor *bg_color;

  gboolean      is_stylable : 1;
  gboolean      has_tooltip : 1;
  gboolean      is_style_dirty : 1;

  StTooltip    *tooltip;
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

  PROP_STYLE,
  PROP_PSEUDO_CLASS,
  PROP_STYLE_CLASS,

  PROP_STYLABLE,

  PROP_HAS_TOOLTIP,
  PROP_TOOLTIP_TEXT
};

static void st_stylable_iface_init (StStylableIface *iface);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (StWidget, st_widget, CLUTTER_TYPE_ACTOR,
                                  G_IMPLEMENT_INTERFACE (ST_TYPE_STYLABLE,
                                                         st_stylable_iface_init));

#define ST_WIDGET_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ST_TYPE_WIDGET, StWidgetPrivate))

static void
st_widget_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  StWidget *actor = ST_WIDGET (gobject);

  switch (prop_id)
    {
    case PROP_STYLE:
      st_stylable_set_style (ST_STYLABLE (actor),
                             g_value_get_object (value));
      break;

    case PROP_PSEUDO_CLASS:
      st_widget_set_style_pseudo_class (actor, g_value_get_string (value));
      break;

    case PROP_STYLE_CLASS:
      st_widget_set_style_class_name (actor, g_value_get_string (value));
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
    case PROP_STYLE:
      g_value_set_object (value, priv->style);
      break;

    case PROP_PSEUDO_CLASS:
      g_value_set_string (value, priv->pseudo_class);
      break;

    case PROP_STYLE_CLASS:
      g_value_set_string (value, priv->style_class);
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

  if (priv->style)
    {
      g_object_unref (priv->style);
      priv->style = NULL;
    }

  if (priv->border_image)
    {
      clutter_actor_unparent (priv->border_image);
      priv->border_image = NULL;
    }

  if (priv->bg_color)
    {
      clutter_color_free (priv->bg_color);
      priv->bg_color = NULL;
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
  ClutterActorClass *klass;
  ClutterGeometry area;
  ClutterVertex in_v, out_v;

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



  if (priv->border_image)
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

      clutter_actor_allocate (CLUTTER_ACTOR (priv->background_image),
                              &frame_box,
                              flags);
    }
}

static void
st_widget_real_draw_background (StWidget           *self,
                                ClutterActor       *background,
                                const ClutterColor *color)
{
  /* Default implementation just draws the background
   * colour and the image on top
   */
  if (color && color->alpha != 0)
    {
      ClutterActor *actor = CLUTTER_ACTOR (self);
      ClutterActorBox allocation = { 0, };
      ClutterColor bg_color = *color;
      gfloat w, h;

      bg_color.alpha = clutter_actor_get_paint_opacity (actor)
                       * bg_color.alpha
                       / 255;

      clutter_actor_get_allocation_box (actor, &allocation);

      w = allocation.x2 - allocation.x1;
      h = allocation.y2 - allocation.y1;

      cogl_set_source_color4ub (bg_color.red,
                                bg_color.green,
                                bg_color.blue,
                                bg_color.alpha);
      cogl_rectangle (0, 0, w, h);
    }

  if (background)
    clutter_actor_paint (background);
}

static void
st_widget_paint (ClutterActor *self)
{
  StWidgetPrivate *priv = ST_WIDGET (self)->priv;
  StWidgetClass *klass = ST_WIDGET_GET_CLASS (self);

  klass->draw_background (ST_WIDGET (self),
                          priv->border_image,
                          priv->bg_color);

  if (priv->background_image != NULL)
    clutter_actor_paint (priv->background_image);
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
    st_stylable_changed ((StStylable*) widget);
}

static void
st_widget_map (ClutterActor *actor)
{
  StWidgetPrivate *priv = ST_WIDGET (actor)->priv;

  CLUTTER_ACTOR_CLASS (st_widget_parent_class)->map (actor);

  st_widget_ensure_style ((StWidget*) actor);

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

  if (priv->border_image)
    clutter_actor_unmap (priv->border_image);

  if (priv->background_image)
    clutter_actor_unmap (priv->background_image);

  if (priv->tooltip)
    clutter_actor_unmap ((ClutterActor *) priv->tooltip);
}

static void
st_widget_style_changed (StStylable *self)
{
  StWidgetPrivate *priv = ST_WIDGET (self)->priv;
  StBorderImage *border_image = NULL;
  StTextureCache *texture_cache;
  ClutterTexture *texture;
  gchar *bg_file = NULL;
  StPadding *padding = NULL;
  gboolean relayout_needed = FALSE;
  gboolean has_changed = FALSE;
  ClutterColor *color;

  /* application has request this widget is not stylable */
  if (!priv->is_stylable)
    return;

  /* cache these values for use in the paint function */
  st_stylable_get (self,
                   "background-color", &color,
                   "background-image", &bg_file,
                   "border-image", &border_image,
                   "padding", &padding,
                   NULL);

  if (color)
    {
      if (priv->bg_color && clutter_color_equal (color, priv->bg_color))
        {
          /* color is the same ... */
          clutter_color_free (color);
        }
      else
        {
          clutter_color_free (priv->bg_color);
          priv->bg_color = color;
          has_changed = TRUE;
        }
    }
  else
  if (priv->bg_color)
    {
      clutter_color_free (priv->bg_color);
      priv->bg_color = NULL;
      has_changed = TRUE;
    }



  if (padding)
    {
      if (priv->padding.top != padding->top ||
          priv->padding.left != padding->left ||
          priv->padding.right != padding->right ||
          priv->padding.bottom != padding->bottom)
        {
          /* Padding changed. Need to relayout. */
          has_changed = TRUE;
          relayout_needed = TRUE;
        }

      priv->padding = *padding;
      g_boxed_free (ST_TYPE_PADDING, padding);
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

  /* Check if the URL is actually present, not garbage in the property */
  if (border_image && border_image->uri)
    {
      gint border_left, border_right, border_top, border_bottom;
      gint width, height;

      /* `border-image' takes precedence over `background-image'.
       * Firefox lets the background-image shine thru when border-image has
       * alpha an channel, maybe that would be an option for the future. */
      texture = st_texture_cache_get_texture (texture_cache,
                                              border_image->uri);

      clutter_texture_get_base_size (CLUTTER_TEXTURE (texture),
                                     &width, &height);

      border_left = ccss_position_get_size (&border_image->left, width);
      border_top = ccss_position_get_size (&border_image->top, height);
      border_right = ccss_position_get_size (&border_image->right, width);
      border_bottom = ccss_position_get_size (&border_image->bottom, height);

      priv->border_image = st_texture_frame_new (texture,
                                                 border_top,
                                                 border_right,
                                                 border_bottom,
                                                 border_left);
      clutter_actor_set_parent (priv->border_image, CLUTTER_ACTOR (self));
      g_boxed_free (ST_TYPE_BORDER_IMAGE, border_image);

      has_changed = TRUE;
      relayout_needed = TRUE;
    }

  if (bg_file != NULL &&
      strcmp (bg_file, "none"))
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
  g_free (bg_file);

  /* If there are any properties above that need to cause a relayout thay
   * should set this flag.
   */
  if (has_changed)
    {
      if (relayout_needed)
        clutter_actor_queue_relayout ((ClutterActor *) self);
      else
        clutter_actor_queue_redraw ((ClutterActor *) self);
    }

  priv->is_style_dirty = FALSE;
}

static void
st_widget_stylable_child_notify (ClutterActor *actor,
                                 gpointer      user_data)
{
  if (ST_IS_STYLABLE (actor))
    st_stylable_changed ((StStylable*) actor);
}

static void
st_widget_stylable_changed (StStylable *stylable)
{

  ST_WIDGET (stylable)->priv->is_style_dirty = TRUE;

  /* update the style only if we are mapped */
  if (!CLUTTER_ACTOR_IS_MAPPED ((ClutterActor *) stylable))
    return;

  g_signal_emit_by_name (stylable, "style-changed", 0);


  if (CLUTTER_IS_CONTAINER (stylable))
    {
      /* notify our children that their parent stylable has changed */
      clutter_container_foreach ((ClutterContainer *) stylable,
                                 st_widget_stylable_child_notify,
                                 NULL);
    }
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

  g_object_class_override_property (gobject_class, PROP_STYLE, "style");

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

}

static StStyle *
st_widget_get_style (StStylable *stylable)
{
  StWidgetPrivate *priv = ST_WIDGET (stylable)->priv;

  return priv->style;
}

static void
st_style_changed_cb (StStyle    *style,
                     StStylable *stylable)
{
  st_stylable_changed (stylable);
}


static void
st_widget_set_style (StStylable *stylable,
                     StStyle    *style)
{
  StWidgetPrivate *priv = ST_WIDGET (stylable)->priv;

  if (priv->style)
    g_object_unref (priv->style);

  priv->style = g_object_ref_sink (style);

  g_signal_connect (priv->style,
                    "changed",
                    G_CALLBACK (st_style_changed_cb),
                    stylable);
}

static StStylable*
st_widget_get_container (StStylable *stylable)
{
  ClutterActor *parent;

  g_return_val_if_fail (ST_IS_WIDGET (stylable), NULL);

  parent = clutter_actor_get_parent (CLUTTER_ACTOR (stylable));

  if (ST_IS_STYLABLE (parent))
    return ST_STYLABLE (parent);
  else
    return NULL;
}

static StStylable*
st_widget_get_base_style (StStylable *stylable)
{
  return NULL;
}

static const gchar*
st_widget_get_style_id (StStylable *stylable)
{
  g_return_val_if_fail (ST_IS_WIDGET (stylable), NULL);

  return clutter_actor_get_name (CLUTTER_ACTOR (stylable));
}

static const gchar*
st_widget_get_style_type (StStylable *stylable)
{
  return G_OBJECT_TYPE_NAME (stylable);
}

static const gchar*
st_widget_get_style_class (StStylable *stylable)
{
  g_return_val_if_fail (ST_IS_WIDGET (stylable), NULL);

  return ST_WIDGET (stylable)->priv->style_class;
}

static const gchar*
st_widget_get_pseudo_class (StStylable *stylable)
{
  g_return_val_if_fail (ST_IS_WIDGET (stylable), NULL);

  return ST_WIDGET (stylable)->priv->pseudo_class;
}

static gboolean
st_widget_get_viewport (StStylable *stylable,
                        gint       *x,
                        gint       *y,
                        gint       *width,
                        gint       *height)
{
  g_return_val_if_fail (ST_IS_WIDGET (stylable), FALSE);

  *x = 0;
  *y = 0;

  *width = clutter_actor_get_width (CLUTTER_ACTOR (stylable));
  *height = clutter_actor_get_height (CLUTTER_ACTOR (stylable));

  return TRUE;
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

  g_return_if_fail (ST_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (style_class, priv->style_class))
    {
      g_free (priv->style_class);
      priv->style_class = g_strdup (style_class);

      st_stylable_changed ((StStylable*) actor);

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
  g_return_val_if_fail (ST_WIDGET (actor), NULL);

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
  g_return_val_if_fail (ST_WIDGET (actor), NULL);

  return actor->priv->pseudo_class;
}

/**
 * st_widget_set_style_pseudo_class:
 * @actor: a #StWidget
 * @pseudo_class: a new pseudo class string
 *
 * Set the style pseudo class
 */
void
st_widget_set_style_pseudo_class (StWidget    *actor,
                                  const gchar *pseudo_class)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (pseudo_class, priv->pseudo_class))
    {
      g_free (priv->pseudo_class);
      priv->pseudo_class = g_strdup (pseudo_class);

      st_stylable_changed ((StStylable*) actor);

      g_object_notify (G_OBJECT (actor), "pseudo-class");
    }
}


static void
st_stylable_iface_init (StStylableIface *iface)
{
  static gboolean is_initialized = FALSE;

  if (!is_initialized)
    {
      GParamSpec *pspec;
      ClutterColor color = { 0x00, 0x00, 0x00, 0xff };
      ClutterColor bg_color = { 0xff, 0xff, 0xff, 0x00 };

      is_initialized = TRUE;

      pspec = clutter_param_spec_color ("background-color",
                                        "Background Color",
                                        "The background color of an actor",
                                        &bg_color,
                                        G_PARAM_READWRITE);
      st_stylable_iface_install_property (iface, ST_TYPE_WIDGET, pspec);

      pspec = clutter_param_spec_color ("color",
                                        "Text Color",
                                        "The color of the text of an actor",
                                        &color,
                                        G_PARAM_READWRITE);
      st_stylable_iface_install_property (iface, ST_TYPE_WIDGET, pspec);

      pspec = g_param_spec_string ("background-image",
                                   "Background Image",
                                   "Background image filename",
                                   NULL,
                                   G_PARAM_READWRITE);
      st_stylable_iface_install_property (iface, ST_TYPE_WIDGET, pspec);

      pspec = g_param_spec_string ("font-family",
                                   "Font Family",
                                   "Name of the font to use",
                                   "Sans",
                                   G_PARAM_READWRITE);
      st_stylable_iface_install_property (iface, ST_TYPE_WIDGET, pspec);

      pspec = g_param_spec_int ("font-size",
                                "Font Size",
                                "Size of the font to use in pixels",
                                0, G_MAXINT, 12,
                                G_PARAM_READWRITE);
      st_stylable_iface_install_property (iface, ST_TYPE_WIDGET, pspec);

      pspec = g_param_spec_boxed ("border-image",
                                  "Border image",
                                  "9-slice image to use for drawing borders and background",
                                  ST_TYPE_BORDER_IMAGE,
                                  G_PARAM_READWRITE);
      st_stylable_iface_install_property (iface, ST_TYPE_WIDGET, pspec);

      pspec = g_param_spec_boxed ("padding",
                                  "Padding",
                                  "Padding between the widget's borders "
                                  "and its content",
                                  ST_TYPE_PADDING,
                                  G_PARAM_READWRITE);
      st_stylable_iface_install_property (iface, ST_TYPE_WIDGET, pspec);

      iface->style_changed = st_widget_style_changed;
      iface->stylable_changed = st_widget_stylable_changed;

      iface->get_style = st_widget_get_style;
      iface->set_style = st_widget_set_style;
      iface->get_base_style = st_widget_get_base_style;
      iface->get_container = st_widget_get_container;
      iface->get_style_id = st_widget_get_style_id;
      iface->get_style_type = st_widget_get_style_type;
      iface->get_style_class = st_widget_get_style_class;
      iface->get_pseudo_class = st_widget_get_pseudo_class;
      /* iface->get_attribute = st_widget_get_attribute; */
      iface->get_viewport = st_widget_get_viewport;
    }
}


static void
st_widget_name_notify (StWidget   *widget,
                       GParamSpec *pspec,
                       gpointer    data)
{
  st_stylable_changed ((StStylable*) widget);
}

static void
st_widget_init (StWidget *actor)
{
  StWidgetPrivate *priv;

  actor->priv = priv = ST_WIDGET_GET_PRIVATE (actor);
  priv->is_stylable = TRUE;

  /* connect style changed */
  g_signal_connect (actor, "notify::name", G_CALLBACK (st_widget_name_notify), NULL);

  /* set the default style */
  st_widget_set_style (ST_STYLABLE (actor), st_style_get_default ());

}

static StBorderImage *
st_border_image_copy (const StBorderImage *border_image)
{
  StBorderImage *copy;

  g_return_val_if_fail (border_image != NULL, NULL);

  copy = g_slice_new (StBorderImage);
  *copy = *border_image;

  return copy;
}

static void
st_border_image_free (StBorderImage *border_image)
{
  if (G_LIKELY (border_image))
    g_slice_free (StBorderImage, border_image);
}

GType
st_border_image_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("StBorderImage"),
                                    (GBoxedCopyFunc) st_border_image_copy,
                                    (GBoxedFreeFunc) st_border_image_free);

  return our_type;
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
    {
      g_signal_emit_by_name (widget, "style-changed", 0);
    }
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
 * st_widget_get_padding:
 * @widget: A #StWidget
 * @padding: A pointer to an #StPadding to fill
 *
 * Gets the padding of the widget, set using the "padding" CSS property. This
 * function should normally only be used by subclasses.
 *
 */
void
st_widget_get_padding (StWidget  *widget,
                       StPadding *padding)
{
  g_return_if_fail (ST_IS_WIDGET (widget));
  g_return_if_fail (padding != NULL);

  *padding = widget->priv->padding;
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
  klass->draw_background (ST_WIDGET (self),
                          priv->border_image,
                          priv->bg_color);

}
