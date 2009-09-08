/*
 * nbtk-widget.c: Base class for Nbtk actors
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

#include "nbtk-widget.h"

#include "nbtk-marshal.h"
#include "nbtk-private.h"
#include "nbtk-stylable.h"
#include "nbtk-texture-cache.h"
#include "nbtk-texture-frame.h"
#include "nbtk-tooltip.h"

typedef ccss_border_image_t             NbtkBorderImage;

/*
 * Forward declaration for sake of NbtkWidgetChild
 */
struct _NbtkWidgetPrivate
{
  NbtkPadding border;
  NbtkPadding padding;

  NbtkStyle *style;
  gchar *pseudo_class;
  gchar *style_class;

  ClutterActor *border_image;
  ClutterActor *background_image;
  ClutterColor *bg_color;

  gboolean is_stylable : 1;
  gboolean has_tooltip : 1;
  gboolean is_style_dirty : 1;

  NbtkTooltip *tooltip;
};

/**
 * SECTION:nbtk-widget
 * @short_description: Base class for stylable actors
 *
 * #NbtkWidget is a simple abstract class on top of #ClutterActor. It
 * provides basic themeing properties.
 *
 * Actors in the Nbtk library should subclass #NbtkWidget if they plan
 * to obey to a certain #NbtkStyle.
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

static void nbtk_stylable_iface_init (NbtkStylableIface *iface);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (NbtkWidget, nbtk_widget, CLUTTER_TYPE_ACTOR,
                                  G_IMPLEMENT_INTERFACE (NBTK_TYPE_STYLABLE,
                                                         nbtk_stylable_iface_init));

#define NBTK_WIDGET_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NBTK_TYPE_WIDGET, NbtkWidgetPrivate))

static void
nbtk_widget_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  NbtkWidget *actor = NBTK_WIDGET (gobject);

  switch (prop_id)
    {
    case PROP_STYLE:
      nbtk_stylable_set_style (NBTK_STYLABLE (actor),
                               g_value_get_object (value));
      break;

    case PROP_PSEUDO_CLASS:
      nbtk_widget_set_style_pseudo_class (actor, g_value_get_string (value));
      break;

    case PROP_STYLE_CLASS:
      nbtk_widget_set_style_class_name (actor, g_value_get_string (value));
      break;

    case PROP_STYLABLE:
      if (actor->priv->is_stylable != g_value_get_boolean (value))
        {
          actor->priv->is_stylable = g_value_get_boolean (value);
          clutter_actor_queue_relayout ((ClutterActor *)gobject);
        }
      break;

    case PROP_HAS_TOOLTIP:
      nbtk_widget_set_has_tooltip (actor, g_value_get_boolean (value));
      break;

    case PROP_TOOLTIP_TEXT:
      nbtk_widget_set_tooltip_text (actor, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_widget_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  NbtkWidget *actor = NBTK_WIDGET (gobject);
  NbtkWidgetPrivate *priv = actor->priv;

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
      g_value_set_string (value, nbtk_tooltip_get_label (priv->tooltip));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_widget_dispose (GObject *gobject)
{
  NbtkWidget *actor = NBTK_WIDGET (gobject);
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;

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

  G_OBJECT_CLASS (nbtk_widget_parent_class)->dispose (gobject);
}

static void
nbtk_widget_finalize (GObject *gobject)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (gobject)->priv;

  g_free (priv->style_class);
  g_free (priv->pseudo_class);

  G_OBJECT_CLASS (nbtk_widget_parent_class)->finalize (gobject);
}

static void
nbtk_widget_allocate (ClutterActor          *actor,
                      const ClutterActorBox *box,
                      ClutterAllocationFlags flags)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;
  ClutterActorClass *klass;
  ClutterGeometry area;
  ClutterVertex in_v, out_v;

  klass = CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class);
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

      nbtk_tooltip_set_tip_area (priv->tooltip, &area);
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
          new_h = (int) ((h / w) * ((gfloat) box_w));
          new_w = (int) ((w / h) * ((gfloat) box_h));

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
          frame_box.x1 = (int) (((box->x2 - box->x1) / 2) - (w / 2));
          frame_box.y1 = (int) (((box->y2 - box->y1) / 2) - (h / 2));
          frame_box.x2 = frame_box.x1 + w;
          frame_box.y2 = frame_box.y1 + h;
        }

      clutter_actor_allocate (CLUTTER_ACTOR (priv->background_image),
                              &frame_box,
                              flags);
    }
}

static void
nbtk_widget_real_draw_background (NbtkWidget         *self,
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
nbtk_widget_paint (ClutterActor *self)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (self)->priv;
  NbtkWidgetClass *klass = NBTK_WIDGET_GET_CLASS (self);

  klass->draw_background (NBTK_WIDGET (self),
                          priv->border_image,
                          priv->bg_color);

  if (priv->background_image != NULL)
    clutter_actor_paint (priv->background_image);
}

static void
nbtk_widget_parent_set (ClutterActor *widget,
                        ClutterActor *old_parent)
{
  ClutterActorClass *parent_class;
  ClutterActor *new_parent;

  parent_class = CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class);
  if (parent_class->parent_set)
    parent_class->parent_set (widget, old_parent);

  new_parent = clutter_actor_get_parent (widget);

  /* don't send the style changed signal if we no longer have a parent actor */
  if (new_parent)
    nbtk_stylable_changed ((NbtkStylable*) widget);
}

static void
nbtk_widget_map (ClutterActor *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;

  CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->map (actor);

  nbtk_widget_ensure_style ((NbtkWidget*) actor);

  if (priv->border_image)
    clutter_actor_map (priv->border_image);

  if (priv->background_image)
    clutter_actor_map (priv->background_image);

  if (priv->tooltip)
    clutter_actor_map ((ClutterActor *) priv->tooltip);
}

static void
nbtk_widget_unmap (ClutterActor *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;

  CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->unmap (actor);

  if (priv->border_image)
    clutter_actor_unmap (priv->border_image);

  if (priv->background_image)
    clutter_actor_unmap (priv->background_image);

  if (priv->tooltip)
    clutter_actor_unmap ((ClutterActor *) priv->tooltip);
}

static void
nbtk_widget_style_changed (NbtkStylable *self)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (self)->priv;
  NbtkBorderImage *border_image = NULL;
  NbtkTextureCache *texture_cache;
  ClutterTexture *texture;
  gchar *bg_file = NULL;
  NbtkPadding *padding = NULL;
  gboolean relayout_needed = FALSE;
  gboolean has_changed = FALSE;
  ClutterColor *color;

  /* application has request this widget is not stylable */
  if (!priv->is_stylable)
    return;

  /* cache these values for use in the paint function */
  nbtk_stylable_get (self,
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
      g_boxed_free (NBTK_TYPE_PADDING, padding);
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

  texture_cache = nbtk_texture_cache_get_default ();

  /* Check if the URL is actually present, not garbage in the property */
  if (border_image && border_image->uri)
    {
      gint border_left, border_right, border_top, border_bottom;
      gint width, height;

      /* `border-image' takes precedence over `background-image'.
       * Firefox lets the background-image shine thru when border-image has
       * alpha an channel, maybe that would be an option for the future. */
      texture = nbtk_texture_cache_get_texture (texture_cache,
                                                border_image->uri,
                                                FALSE);

      clutter_texture_get_base_size (CLUTTER_TEXTURE (texture),
                                     &width, &height);

      border_left = ccss_position_get_size (&border_image->left, width);
      border_top = ccss_position_get_size (&border_image->top, height);
      border_right = ccss_position_get_size (&border_image->right, width);
      border_bottom = ccss_position_get_size (&border_image->bottom, height);

      priv->border_image = nbtk_texture_frame_new (texture,
                                                   border_top,
                                                   border_right,
                                                   border_bottom,
                                                   border_left);
      clutter_actor_set_parent (priv->border_image, CLUTTER_ACTOR (self));
      g_boxed_free (NBTK_TYPE_BORDER_IMAGE, border_image);

      has_changed = TRUE;
      relayout_needed = TRUE;
    }

  if (bg_file != NULL &&
      strcmp (bg_file, "none"))
    {
      texture = nbtk_texture_cache_get_texture (texture_cache,
                                                bg_file,
                                                FALSE);
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
nbtk_widget_stylable_child_notify (ClutterActor *actor,
                                   gpointer      user_data)
{
  if (NBTK_IS_STYLABLE (actor))
    nbtk_stylable_changed ((NbtkStylable*) actor);
}

static void
nbtk_widget_stylable_changed (NbtkStylable *stylable)
{

  NBTK_WIDGET (stylable)->priv->is_style_dirty = TRUE;

  /* update the style only if we are mapped */
  if (!CLUTTER_ACTOR_IS_MAPPED ((ClutterActor *) stylable))
    return;

  g_signal_emit_by_name (stylable, "style-changed", 0);


  if (CLUTTER_IS_CONTAINER (stylable))
    {
      /* notify our children that their parent stylable has changed */
      clutter_container_foreach ((ClutterContainer *) stylable,
                                 nbtk_widget_stylable_child_notify,
                                 NULL);
    }
}

static gboolean
nbtk_widget_enter (ClutterActor         *actor,
                   ClutterCrossingEvent *event)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;


  if (priv->has_tooltip)
      nbtk_widget_show_tooltip ((NbtkWidget*) actor);

  if (CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->enter_event)
    return CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->enter_event (actor, event);
  else
    return FALSE;
}

static gboolean
nbtk_widget_leave (ClutterActor         *actor,
                   ClutterCrossingEvent *event)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;

  if (priv->has_tooltip)
      nbtk_tooltip_hide (priv->tooltip);

  if (CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->leave_event)
    return CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->leave_event (actor, event);
  else
    return FALSE;
}

static void
nbtk_widget_hide (ClutterActor *actor)
{
  NbtkWidget *widget = (NbtkWidget *) actor;

  /* hide the tooltip, if there is one */
  if (widget->priv->tooltip)
    nbtk_tooltip_hide (NBTK_TOOLTIP (widget->priv->tooltip));

  CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->hide (actor);
}



static void
nbtk_widget_class_init (NbtkWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (NbtkWidgetPrivate));

  gobject_class->set_property = nbtk_widget_set_property;
  gobject_class->get_property = nbtk_widget_get_property;
  gobject_class->dispose = nbtk_widget_dispose;
  gobject_class->finalize = nbtk_widget_finalize;

  actor_class->allocate = nbtk_widget_allocate;
  actor_class->paint = nbtk_widget_paint;
  actor_class->parent_set = nbtk_widget_parent_set;
  actor_class->map = nbtk_widget_map;
  actor_class->unmap = nbtk_widget_unmap;

  actor_class->enter_event = nbtk_widget_enter;
  actor_class->leave_event = nbtk_widget_leave;
  actor_class->hide = nbtk_widget_hide;

  klass->draw_background = nbtk_widget_real_draw_background;

  /**
   * NbtkWidget:pseudo-class:
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
                                                        NBTK_PARAM_READWRITE));
  /**
   * NbtkWidget:style-class:
   *
   * The style-class of the actor for use in styling.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STYLE_CLASS,
                                   g_param_spec_string ("style-class",
                                                        "Style Class",
                                                        "Style class for styling",
                                                        "",
                                                        NBTK_PARAM_READWRITE));

  g_object_class_override_property (gobject_class, PROP_STYLE, "style");

  /**
   * NbtkWidget:stylable:
   *
   * Enable or disable styling of the widget
   */
  pspec = g_param_spec_boolean ("stylable",
                                "Stylable",
                                "Whether the table should be styled",
                                TRUE,
                                NBTK_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_STYLABLE,
                                   pspec);

  /**
   * NbtkWidget:has-tooltip:
   *
   * Determines whether the widget has a tooltip. If set to TRUE, causes the
   * widget to monitor enter and leave events (i.e. sets the widget reactive).
   */
  pspec = g_param_spec_boolean ("has-tooltip",
                                "Has Tooltip",
                                "Determines whether the widget has a tooltip",
                                FALSE,
                                NBTK_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_HAS_TOOLTIP,
                                   pspec);


  /**
   * NbtkWidget:tooltip-text:
   *
   * text displayed on the tooltip
   */
  pspec = g_param_spec_string ("tooltip-text",
                               "Tooltip Text",
                               "Text displayed on the tooltip",
                               "",
                               NBTK_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TOOLTIP_TEXT, pspec);

}

static NbtkStyle *
nbtk_widget_get_style (NbtkStylable *stylable)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (stylable)->priv;

  return priv->style;
}

static void
nbtk_style_changed_cb (NbtkStyle    *style,
                       NbtkStylable *stylable)
{
  nbtk_stylable_changed (stylable);
}


static void
nbtk_widget_set_style (NbtkStylable *stylable,
                       NbtkStyle    *style)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (stylable)->priv;

  if (priv->style)
    g_object_unref (priv->style);

  priv->style = g_object_ref_sink (style);

  g_signal_connect (priv->style,
                    "changed",
                    G_CALLBACK (nbtk_style_changed_cb),
                    stylable);
}

static NbtkStylable*
nbtk_widget_get_container (NbtkStylable *stylable)
{
  ClutterActor *parent;

  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), NULL);

  parent = clutter_actor_get_parent (CLUTTER_ACTOR (stylable));

  if (NBTK_IS_STYLABLE (parent))
    return NBTK_STYLABLE (parent);
  else
    return NULL;
}

static NbtkStylable*
nbtk_widget_get_base_style (NbtkStylable *stylable)
{
  return NULL;
}

static const gchar*
nbtk_widget_get_style_id (NbtkStylable *stylable)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), NULL);

  return clutter_actor_get_name (CLUTTER_ACTOR (stylable));
}

static const gchar*
nbtk_widget_get_style_type (NbtkStylable *stylable)
{
  return G_OBJECT_TYPE_NAME (stylable);
}

static const gchar*
nbtk_widget_get_style_class (NbtkStylable *stylable)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), NULL);

  return NBTK_WIDGET (stylable)->priv->style_class;
}

static const gchar*
nbtk_widget_get_pseudo_class (NbtkStylable *stylable)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), NULL);

  return NBTK_WIDGET (stylable)->priv->pseudo_class;
}

static gboolean
nbtk_widget_get_viewport (NbtkStylable *stylable,
                          gint *x,
                          gint *y,
                          gint *width,
                          gint *height)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (stylable), FALSE);

  *x = 0;
  *y = 0;

  *width = clutter_actor_get_width (CLUTTER_ACTOR (stylable));
  *height = clutter_actor_get_height (CLUTTER_ACTOR (stylable));

  return TRUE;
}

/**
 * nbtk_widget_set_style_class_name:
 * @actor: a #NbtkWidget
 * @style_class: a new style class string
 *
 * Set the style class name
 */
void
nbtk_widget_set_style_class_name (NbtkWidget  *actor,
                                  const gchar *style_class)
{
  NbtkWidgetPrivate *priv = actor->priv;

  g_return_if_fail (NBTK_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (style_class, priv->style_class))
    {
      g_free (priv->style_class);
      priv->style_class = g_strdup (style_class);

      nbtk_stylable_changed ((NbtkStylable*) actor);

      g_object_notify (G_OBJECT (actor), "style-class");
    }
}


/**
 * nbtk_widget_get_style_class_name:
 * @actor: a #NbtkWidget
 *
 * Get the current style class name
 *
 * Returns: the class name string. The string is owned by the #NbtkWidget and
 * should not be modified or freed.
 */
const gchar*
nbtk_widget_get_style_class_name (NbtkWidget *actor)
{
  g_return_val_if_fail (NBTK_WIDGET (actor), NULL);

  return actor->priv->style_class;
}

/**
 * nbtk_widget_get_style_pseudo_class:
 * @actor: a #NbtkWidget
 *
 * Get the current style pseudo class
 *
 * Returns: the pseudo class string. The string is owned by the #NbtkWidget and
 * should not be modified or freed.
 */
const gchar*
nbtk_widget_get_style_pseudo_class (NbtkWidget *actor)
{
  g_return_val_if_fail (NBTK_WIDGET (actor), NULL);

  return actor->priv->pseudo_class;
}

/**
 * nbtk_widget_set_style_pseudo_class:
 * @actor: a #NbtkWidget
 * @pseudo_class: a new pseudo class string
 *
 * Set the style pseudo class
 */
void
nbtk_widget_set_style_pseudo_class (NbtkWidget  *actor,
                                    const gchar *pseudo_class)
{
  NbtkWidgetPrivate *priv;

  g_return_if_fail (NBTK_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (pseudo_class, priv->pseudo_class))
    {
      g_free (priv->pseudo_class);
      priv->pseudo_class = g_strdup (pseudo_class);

      nbtk_stylable_changed ((NbtkStylable*) actor);

      g_object_notify (G_OBJECT (actor), "pseudo-class");
    }
}


static void
nbtk_stylable_iface_init (NbtkStylableIface *iface)
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
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = clutter_param_spec_color ("color",
                                  "Text Color",
                                  "The color of the text of an actor",
                                  &color,
                                  G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_string ("background-image",
                                   "Background Image",
                                   "Background image filename",
                                   NULL,
                                   G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_string ("font-family",
                                   "Font Family",
                                   "Name of the font to use",
                                   "Sans",
                                   G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_int ("font-size",
                                "Font Size",
                                "Size of the font to use in pixels",
                                0, G_MAXINT, 12,
                                G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_boxed ("border-image",
                                  "Border image",
                                  "9-slice image to use for drawing borders and background",
                                  NBTK_TYPE_BORDER_IMAGE,
                                  G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      pspec = g_param_spec_boxed ("padding",
                                  "Padding",
                                  "Padding between the widget's borders "
                                  "and its content",
                                  NBTK_TYPE_PADDING,
                                  G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface, NBTK_TYPE_WIDGET, pspec);

      iface->style_changed = nbtk_widget_style_changed;
      iface->stylable_changed = nbtk_widget_stylable_changed;

      iface->get_style = nbtk_widget_get_style;
      iface->set_style = nbtk_widget_set_style;
      iface->get_base_style = nbtk_widget_get_base_style;
      iface->get_container = nbtk_widget_get_container;
      iface->get_style_id = nbtk_widget_get_style_id;
      iface->get_style_type = nbtk_widget_get_style_type;
      iface->get_style_class = nbtk_widget_get_style_class;
      iface->get_pseudo_class = nbtk_widget_get_pseudo_class;
      /* iface->get_attribute = nbtk_widget_get_attribute; */
      iface->get_viewport = nbtk_widget_get_viewport;
    }
}


static void
nbtk_widget_name_notify (NbtkWidget *widget,
                         GParamSpec *pspec,
                         gpointer data)
{
  nbtk_stylable_changed ((NbtkStylable*) widget);
}

static void
nbtk_widget_init (NbtkWidget *actor)
{
  NbtkWidgetPrivate *priv;

  actor->priv = priv = NBTK_WIDGET_GET_PRIVATE (actor);
  priv->is_stylable = TRUE;

  /* connect style changed */
  g_signal_connect (actor, "notify::name", G_CALLBACK (nbtk_widget_name_notify), NULL);

  /* set the default style */
  nbtk_widget_set_style (NBTK_STYLABLE (actor), nbtk_style_get_default ());

}

static NbtkBorderImage *
nbtk_border_image_copy (const NbtkBorderImage *border_image)
{
  NbtkBorderImage *copy;

  g_return_val_if_fail (border_image != NULL, NULL);

  copy = g_slice_new (NbtkBorderImage);
  *copy = *border_image;

  return copy;
}

static void
nbtk_border_image_free (NbtkBorderImage *border_image)
{
  if (G_LIKELY (border_image))
    g_slice_free (NbtkBorderImage, border_image);
}

GType
nbtk_border_image_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("NbtkBorderImage"),
                                    (GBoxedCopyFunc) nbtk_border_image_copy,
                                    (GBoxedFreeFunc) nbtk_border_image_free);

  return our_type;
}

/**
 * nbtk_widget_ensure_style:
 * @widget: A #NbtkWidget
 *
 * Ensures that @widget has read its style information.
 *
 */
void
nbtk_widget_ensure_style (NbtkWidget *widget)
{
  g_return_if_fail (NBTK_IS_WIDGET (widget));

  if (widget->priv->is_style_dirty)
    {
      g_signal_emit_by_name (widget, "style-changed", 0);
    }
}


/**
 * nbtk_widget_get_border_image:
 * @actor: A #NbtkWidget
 *
 * Get the texture used as the border image. This is set using the
 * "border-image" CSS property. This function should normally only be used
 * by subclasses.
 *
 * Returns: #ClutterActor
 */
ClutterActor *
nbtk_widget_get_border_image (NbtkWidget *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;
  return priv->border_image;
}

/**
 * nbtk_widget_get_background_image:
 * @actor: A #NbtkWidget
 *
 * Get the texture used as the background image. This is set using the
 * "background-image" CSS property. This function should normally only be used
 * by subclasses.
 *
 * Returns: a #ClutterActor
 */
ClutterActor *
nbtk_widget_get_background_image (NbtkWidget *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;
  return priv->background_image;
}

/**
 * nbtk_widget_get_padding:
 * @widget: A #NbtkWidget
 * @padding: A pointer to an #NbtkPadding to fill
 *
 * Gets the padding of the widget, set using the "padding" CSS property. This
 * function should normally only be used by subclasses.
 *
 */
void
nbtk_widget_get_padding (NbtkWidget *widget,
                         NbtkPadding *padding)
{
  g_return_if_fail (NBTK_IS_WIDGET (widget));
  g_return_if_fail (padding != NULL);

  *padding = widget->priv->padding;
}

/**
 * nbtk_widget_set_has_tooltip:
 * @widget: A #NbtkWidget
 * @has_tooltip: #TRUE if the widget should display a tooltip
 *
 * Enables tooltip support on the #NbtkWidget.
 *
 * Note that setting has-tooltip to #TRUE will cause the widget to be set
 * reactive. If you no longer need tooltip support and do not need the widget
 * to be reactive, you need to set ClutterActor::reactive to FALSE.
 *
 */
void
nbtk_widget_set_has_tooltip (NbtkWidget *widget,
                             gboolean    has_tooltip)
{
  NbtkWidgetPrivate *priv;

  g_return_if_fail (NBTK_IS_WIDGET (widget));

  priv = widget->priv;

  priv->has_tooltip = has_tooltip;

  if (has_tooltip)
    {
      clutter_actor_set_reactive ((ClutterActor*) widget, TRUE);

      if (!priv->tooltip)
        {
          priv->tooltip = g_object_new (NBTK_TYPE_TOOLTIP, NULL);
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
 * nbtk_widget_get_has_tooltip:
 * @widget: A #NbtkWidget
 *
 * Returns the current value of the has-tooltip property. See
 * nbtk_tooltip_set_has_tooltip() for more information.
 *
 * Returns: current value of has-tooltip on @widget
 */
gboolean
nbtk_widget_get_has_tooltip (NbtkWidget *widget)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (widget), FALSE);

  return widget->priv->has_tooltip;
}

/**
 * nbtk_widget_set_tooltip_text:
 * @widget: A #NbtkWidget
 * @text: text to set as the tooltip
 *
 * Set the tooltip text of the widget. This will set NbtkWidget::has-tooltip to
 * #TRUE. A value of #NULL will unset the tooltip and set has-tooltip to #FALSE.
 *
 */
void
nbtk_widget_set_tooltip_text (NbtkWidget  *widget,
                              const gchar *text)
{
  NbtkWidgetPrivate *priv;

  g_return_if_fail (NBTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (text == NULL)
    nbtk_widget_set_has_tooltip (widget, FALSE);
  else
    nbtk_widget_set_has_tooltip (widget, TRUE);

  nbtk_tooltip_set_label (priv->tooltip, text);
}

/**
 * nbtk_widget_get_tooltip_text:
 * @widget: A #NbtkWidget
 *
 * Get the current tooltip string
 *
 * Returns: The current tooltip string, owned by the #NbtkWidget
 */
const gchar*
nbtk_widget_get_tooltip_text (NbtkWidget *widget)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (widget), NULL);

  return nbtk_tooltip_get_label (widget->priv->tooltip);
}

/**
 * nbtk_widget_show_tooltip:
 * @widget: A #NbtkWidget
 *
 * Show the tooltip for @widget
 *
 */
void
nbtk_widget_show_tooltip (NbtkWidget *widget)
{
  gfloat x, y, width, height;
  ClutterGeometry area;

  g_return_if_fail (NBTK_IS_WIDGET (widget));

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
      nbtk_tooltip_set_tip_area (widget->priv->tooltip, &area);
      nbtk_tooltip_show (widget->priv->tooltip);
    }
}

/**
 * nbtk_widget_hide_tooltip:
 * @widget: A #NbtkWidget
 *
 * Hide the tooltip for @widget
 *
 */
void
nbtk_widget_hide_tooltip (NbtkWidget *widget)
{
  g_return_if_fail (NBTK_IS_WIDGET (widget));

  if (widget->priv->tooltip)
    nbtk_tooltip_hide (widget->priv->tooltip);
}

/**
 * nbtk_widget_draw_background:
 * @widget: a #NbtkWidget
 *
 * Invokes #NbtkWidget::draw_background() using the default background
 * image and/or color from the @widget style
 *
 * This function should be used by subclasses of #NbtkWidget that override
 * the paint() virtual function and cannot chain up
 */
void
nbtk_widget_draw_background (NbtkWidget *self)
{
  NbtkWidgetPrivate *priv;
  NbtkWidgetClass *klass;

  g_return_if_fail (NBTK_IS_WIDGET (self));

  priv = self->priv;

  klass = NBTK_WIDGET_GET_CLASS (self);
  klass->draw_background (NBTK_WIDGET (self),
                          priv->border_image,
                          priv->bg_color);

}
