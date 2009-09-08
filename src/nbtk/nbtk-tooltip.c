/*
 * nbtk-tooltip.c: Plain tooltip actor
 *
 * Copyright 2008, 2009 Intel Corporation
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
 * Written by: Thomas Wood <thomas@linux.intel.com>
 *
 */

/**
 * SECTION:nbtk-tooltip
 * @short_description: A tooltip widget
 *
 * #NbtkTooltip implements a single tooltip. It should not normally be created
 * by the application but by the widget implementing tooltip capabilities, for
 * example, #nbtk_button_set_tooltip().
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>

#include "nbtk-tooltip.h"

#include "nbtk-widget.h"
#include "nbtk-stylable.h"
#include "nbtk-private.h"

enum
{
  PROP_0,

  PROP_LABEL,
  PROP_TIP_AREA
};

#define NBTK_TOOLTIP_GET_PRIVATE(obj)    \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NBTK_TYPE_TOOLTIP, NbtkTooltipPrivate))

struct _NbtkTooltipPrivate
{
  ClutterActor *label;

  gfloat        arrow_offset;
  gboolean      actor_below;

  ClutterGeometry *tip_area;
};

G_DEFINE_TYPE (NbtkTooltip, nbtk_tooltip, NBTK_TYPE_WIDGET);

static void
nbtk_tooltip_set_property (GObject      *gobject,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  NbtkTooltip *tooltip = NBTK_TOOLTIP (gobject);

  switch (prop_id)
    {
    case PROP_LABEL:
      nbtk_tooltip_set_label (tooltip, g_value_get_string (value));
      break;

    case PROP_TIP_AREA:
      nbtk_tooltip_set_tip_area (tooltip, g_value_get_boxed (value));

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_tooltip_get_property (GObject    *gobject,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  NbtkTooltipPrivate *priv = NBTK_TOOLTIP (gobject)->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, clutter_text_get_text (CLUTTER_TEXT (priv->label)));
      break;

    case PROP_TIP_AREA:
      g_value_set_boxed (value, priv->tip_area);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_tooltip_style_changed (NbtkWidget *self)
{
  ClutterColor *color = NULL;
  NbtkTooltipPrivate *priv;
  gchar *font_name;
  gchar *font_string;
  gint font_size;

  priv = NBTK_TOOLTIP (self)->priv;

  nbtk_stylable_get (NBTK_STYLABLE (self),
                     "color", &color,
                     "font-family", &font_name,
                     "font-size", &font_size,
                     NULL);

  if (color)
    {
      clutter_text_set_color (CLUTTER_TEXT (priv->label), color);
      clutter_color_free (color);
    }

  if (font_name || font_size)
    {
      if (font_name && font_size)
        {
          font_string = g_strdup_printf ("%s %dpx", font_name, font_size);
          g_free (font_name);
        }
      else
        if (font_size)
          font_string = g_strdup_printf ("%dpx", font_size);
        else
          font_string = font_name;

      clutter_text_set_font_name (CLUTTER_TEXT (priv->label), font_string);

      g_free (font_string);
    }

}

static void
nbtk_tooltip_get_preferred_width (ClutterActor *self,
                                  gfloat       for_height,
                                  gfloat      *min_width_p,
                                  gfloat      *natural_width_p)
{
  NbtkTooltipPrivate *priv = NBTK_TOOLTIP (self)->priv;
  gfloat min_label_w, natural_label_w;
  gfloat label_height, arrow_height;
  ClutterActor *arrow_image;
  NbtkPadding padding;

  nbtk_widget_get_padding (NBTK_WIDGET (self), &padding);

  arrow_image = nbtk_widget_get_background_image (NBTK_WIDGET (self));
  if (arrow_image)
    {
      clutter_actor_get_preferred_height (arrow_image,
                                          -1,
                                          NULL,
                                          &arrow_height);
    }
  else
    {
      arrow_height = 0;
    }

  if (for_height > -1)
    {
      label_height = for_height - arrow_height - padding.top - padding.bottom;
    }
  else
    {
      label_height = -1;
    }

  if (priv->label)
    {
      clutter_actor_get_preferred_width (priv->label,
                                         label_height,
                                         &min_label_w,
                                         &natural_label_w);
    }
  else
    {
      min_label_w = 0;
      natural_label_w = 0;
    }


  if (min_width_p)
    {
      *min_width_p = padding.left + padding.right + min_label_w;
    }

  if (natural_width_p)
    {
      *natural_width_p = padding.left + padding.right + natural_label_w;
    }
}

static void
nbtk_tooltip_get_preferred_height (ClutterActor *self,
                                   gfloat        for_width,
                                   gfloat       *min_height_p,
                                   gfloat       *natural_height_p)
{
  NbtkTooltipPrivate *priv = NBTK_TOOLTIP (self)->priv;
  gfloat arrow_height;
  gfloat min_label_h, natural_label_h;
  gfloat label_width;
  ClutterActor *arrow_image;
  NbtkPadding padding;

  arrow_image = nbtk_widget_get_background_image (NBTK_WIDGET (self));

  if (arrow_image && !priv->actor_below)
    {
      clutter_actor_get_preferred_height (arrow_image,
                                          -1,
                                          NULL,
                                          &arrow_height);
    }
  else
    {
      arrow_height = 0;
    }
  nbtk_widget_get_padding (NBTK_WIDGET (self), &padding);

  if (for_width > -1)
    {
      label_width = for_width - padding.left - padding.right;
    }
  else
    {
      label_width = -1;
    }

  if (priv->label)
    {
      clutter_actor_get_preferred_height (priv->label,
                                          label_width,
                                          &min_label_h,
                                          &natural_label_h);
    }
  else
    {
      min_label_h = 0;
      natural_label_h = 0;
    }

  if (min_height_p)
    {
      *min_height_p = padding.top + padding.bottom
                      + arrow_height + min_label_h;
    }

  if (natural_height_p)
    {
      *natural_height_p = padding.top + padding.bottom
                          + arrow_height + natural_label_h;
    }
}

static void
nbtk_tooltip_allocate (ClutterActor          *self,
                       const ClutterActorBox *box,
                       ClutterAllocationFlags flags)
{
  NbtkTooltipPrivate *priv = NBTK_TOOLTIP (self)->priv;
  ClutterActorBox child_box, arrow_box;
  gfloat arrow_height, arrow_width;
  ClutterActor *border_image, *arrow_image;
  NbtkPadding padding;

  CLUTTER_ACTOR_CLASS (nbtk_tooltip_parent_class)->allocate (self,
                                                             box,
                                                             flags);

  nbtk_widget_get_padding (NBTK_WIDGET (self), &padding);

  arrow_image = nbtk_widget_get_background_image (NBTK_WIDGET (self));

  if (arrow_image && !priv->actor_below)
    {
      clutter_actor_get_preferred_height (arrow_image, -1, NULL, &arrow_height);
      clutter_actor_get_preferred_width (arrow_image, -1, NULL, &arrow_width);

      arrow_box.x1 = (float) (priv->arrow_offset) - (int) (arrow_width / 2);
      arrow_box.y1 = 0;
      arrow_box.x2 = arrow_box.x1 + arrow_width;
      arrow_box.y2 = arrow_box.y1 + arrow_height;

      clutter_actor_allocate (arrow_image, &arrow_box, flags);
    }
  else
    {
      arrow_height = 0;
      arrow_width = 0;
    }

  child_box.x1 = child_box.y1 = 0;
  child_box.x2 = (box->x2 - box->x1);
  child_box.y2 = (box->y2 - box->y1);

  /* remove the space that is used by the arrow */
  child_box.y1 += arrow_height;

  border_image = nbtk_widget_get_border_image (NBTK_WIDGET (self));
  if (border_image)
    clutter_actor_allocate (border_image, &child_box, flags);

  if (priv->label)
    {
      /* now remove the padding */
      child_box.y1 += padding.top;
      child_box.x1 += padding.left;
      child_box.x2 -= padding.right;
      child_box.y2 -= padding.bottom;

      clutter_actor_allocate (priv->label, &child_box, flags);
    }
}


static void
nbtk_tooltip_paint (ClutterActor *self)
{
  ClutterActor *border_image, *arrow_image;
  NbtkTooltipPrivate *priv = NBTK_TOOLTIP (self)->priv;

  border_image = nbtk_widget_get_border_image (NBTK_WIDGET (self));
  if (border_image)
    clutter_actor_paint (border_image);

  arrow_image = nbtk_widget_get_background_image (NBTK_WIDGET (self));
  if (arrow_image && !priv->actor_below)
    clutter_actor_paint (arrow_image);

  clutter_actor_paint (priv->label);
}

static void
nbtk_tooltip_map (ClutterActor *self)
{
  NbtkTooltipPrivate *priv = NBTK_TOOLTIP (self)->priv;
  ClutterActor *border_image, *arrow_image;

  CLUTTER_ACTOR_CLASS (nbtk_tooltip_parent_class)->map (self);

  border_image = nbtk_widget_get_border_image (NBTK_WIDGET (self));
  if (border_image)
    clutter_actor_map (border_image);

  arrow_image = nbtk_widget_get_background_image (NBTK_WIDGET (self));
  if (arrow_image)
    clutter_actor_map (arrow_image);

  clutter_actor_map (priv->label);
}

static void
nbtk_tooltip_unmap (ClutterActor *self)
{
  NbtkTooltipPrivate *priv = NBTK_TOOLTIP (self)->priv;
  ClutterActor *border_image, *arrow_image;

  CLUTTER_ACTOR_CLASS (nbtk_tooltip_parent_class)->unmap (self);

  border_image = nbtk_widget_get_border_image (NBTK_WIDGET (self));
  if (border_image)
    clutter_actor_unmap (border_image);

  arrow_image = nbtk_widget_get_background_image (NBTK_WIDGET (self));
  if (arrow_image)
    clutter_actor_unmap (arrow_image);

  clutter_actor_unmap (priv->label);
}

static void
nbtk_tooltip_class_init (NbtkTooltipClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (NbtkTooltipPrivate));

  gobject_class->set_property = nbtk_tooltip_set_property;
  gobject_class->get_property = nbtk_tooltip_get_property;

  actor_class->get_preferred_width = nbtk_tooltip_get_preferred_width;
  actor_class->get_preferred_height = nbtk_tooltip_get_preferred_height;
  actor_class->allocate = nbtk_tooltip_allocate;
  actor_class->paint = nbtk_tooltip_paint;
  actor_class->map = nbtk_tooltip_map;
  actor_class->unmap = nbtk_tooltip_unmap;

  pspec = g_param_spec_string ("label",
                               "Label",
                               "Label of the tooltip",
                               NULL, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LABEL, pspec);

  pspec = g_param_spec_boxed ("tip-area",
                              "Tip Area",
                              "Area on the stage the tooltip applies to",
                              CLUTTER_TYPE_GEOMETRY,
                              NBTK_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TIP_AREA, pspec);
}

static void
nbtk_tooltip_init (NbtkTooltip *tooltip)
{
  tooltip->priv = NBTK_TOOLTIP_GET_PRIVATE (tooltip);

  tooltip->priv->label = g_object_new (CLUTTER_TYPE_TEXT,
                                       "line-alignment", PANGO_ALIGN_CENTER,
                                       "ellipsize", PANGO_ELLIPSIZE_END,
                                       "use-markup", TRUE,
                                       NULL);

  tooltip->priv->tip_area = NULL;

  clutter_actor_set_parent (CLUTTER_ACTOR (tooltip->priv->label),
                            CLUTTER_ACTOR (tooltip));

  g_object_set (tooltip, "show-on-set-parent", FALSE, NULL);

  clutter_actor_set_reactive (CLUTTER_ACTOR (tooltip), FALSE);

  g_signal_connect (tooltip, "style-changed",
                    G_CALLBACK (nbtk_tooltip_style_changed), NULL);
}

static void
nbtk_tooltip_update_position (NbtkTooltip *tooltip)
{
  NbtkTooltipPrivate *priv = tooltip->priv;
  ClutterGeometry *tip_area = tooltip->priv->tip_area;
  gfloat tooltip_w, tooltip_h, tooltip_x, tooltip_y;
  gfloat stage_w, stage_h;
  ClutterActor *stage;

  /* ensure the tooltip with is not fixed size */
  clutter_actor_set_size ((ClutterActor*) tooltip, -1, -1);

  /* if no area set, just position ourselves top left */
  if (!priv->tip_area)
    {
      clutter_actor_set_position ((ClutterActor*) tooltip, 0, 0);
      return;
    }

  /* we need to have a style in case there are padding values to take into
   * account when calculating width/height */
  nbtk_widget_ensure_style ((NbtkWidget *) tooltip);

  /* find out the tooltip's size */
  clutter_actor_get_size ((ClutterActor*) tooltip, &tooltip_w, &tooltip_h);

  /* attempt to place the tooltip */
  tooltip_x = (int) (tip_area->x + (tip_area->width / 2) - (tooltip_w / 2));
  tooltip_y = (int) (tip_area->y + tip_area->height);

  stage = clutter_actor_get_stage ((ClutterActor *) tooltip);
  if (!stage)
    {
      return;
    }
  clutter_actor_get_size (stage, &stage_w, &stage_h);

  /* make sure the tooltip is not off screen vertically */
  if (tooltip_w > stage_w)
    {
      tooltip_x = 0;
      clutter_actor_set_width ((ClutterActor*) tooltip, stage_w);
    }
  else if (tooltip_x < 0)
    {
      tooltip_x = 0;
    }
  else if (tooltip_x + tooltip_w > stage_w)
    {
      tooltip_x = (int) (stage_w) - tooltip_w;
    }

  /* make sure the tooltip is not off screen horizontally */
  if (tooltip_y + tooltip_h > stage_h)
    {
      priv->actor_below = TRUE;

      /* re-query size as may have changed */
      clutter_actor_get_preferred_height ((ClutterActor*) tooltip,
                                          -1, NULL, &tooltip_h);
      tooltip_y = tip_area->y - tooltip_h;
    }
  else
    {
      priv->actor_below = FALSE;
    }

  /* calculate the arrow offset */
  priv->arrow_offset = tip_area->x + tip_area->width / 2 - tooltip_x;

  clutter_actor_set_position ((ClutterActor*) tooltip, tooltip_x, tooltip_y);
}

/**
 * nbtk_tooltip_get_label:
 * @tooltip: a #NbtkTooltip
 *
 * Get the text displayed on the tooltip
 *
 * Returns: the text for the tooltip. This must not be freed by the application
 */
G_CONST_RETURN gchar *
nbtk_tooltip_get_label (NbtkTooltip *tooltip)
{
  g_return_val_if_fail (NBTK_IS_TOOLTIP (tooltip), NULL);

  return clutter_text_get_text (CLUTTER_TEXT (tooltip->priv->label));
}

/**
 * nbtk_tooltip_set_label:
 * @tooltip: a #NbtkTooltip
 * @text: text to set the label to
 *
 * Sets the text displayed on the tooltip
 */
void
nbtk_tooltip_set_label (NbtkTooltip *tooltip,
                        const gchar *text)
{
  NbtkTooltipPrivate *priv;

  g_return_if_fail (NBTK_IS_TOOLTIP (tooltip));

  priv = tooltip->priv;

  clutter_text_set_text (CLUTTER_TEXT (priv->label), text);

  g_object_notify (G_OBJECT (tooltip), "label");
}

/**
 * nbtk_tooltip_show:
 * @tooltip: a #NbtkTooltip
 *
 * Show the tooltip relative to the associated widget.
 */
void
nbtk_tooltip_show (NbtkTooltip *tooltip)
{
  NbtkTooltipPrivate *priv;
  ClutterActor *parent;
  ClutterActor *stage;
  ClutterActor *self = CLUTTER_ACTOR (tooltip);
  ClutterAnimation *animation;

  /* make sure we're not currently already animating (e.g. hiding) */
  animation = clutter_actor_get_animation (CLUTTER_ACTOR (tooltip));
  if (animation)
    clutter_animation_completed (animation);

  priv = tooltip->priv;
  parent = clutter_actor_get_parent (self);
  stage = clutter_actor_get_stage (self);

  if (!stage)
    {
      g_warning ("NbtkTooltip is not on any stage.");
      return;
    }

  /* make sure we're parented on the stage */
  if (G_UNLIKELY (parent != stage))
    {
      g_object_ref (self);
      clutter_actor_unparent (self);
      clutter_actor_set_parent (self, stage);
      g_object_unref (self);
      parent = stage;
    }

  /* raise the tooltip to the top */
  clutter_container_raise_child (CLUTTER_CONTAINER (stage),
                                 CLUTTER_ACTOR (tooltip),
                                 NULL);

  nbtk_tooltip_update_position (tooltip);

  /* finally show the tooltip... */
  CLUTTER_ACTOR_CLASS (nbtk_tooltip_parent_class)->show (self);

  /* and give it some bounce! */
  g_object_set (G_OBJECT (self),
                "scale-center-x", priv->arrow_offset,
                "scale-center-y", (priv->actor_below) ? clutter_actor_get_height (self) : 0,
                NULL);
  clutter_actor_set_scale (self, 0.0, 0.0);
  clutter_actor_animate (self, CLUTTER_EASE_OUT_ELASTIC,
                         500,
                         "scale-x", 1.0,
                         "scale-y", 1.0,
                         NULL);
}

static void
nbtk_tooltip_hide_complete (ClutterAnimation *animation,
                            ClutterActor     *actor)
{
  CLUTTER_ACTOR_CLASS (nbtk_tooltip_parent_class)->hide (actor);
  g_signal_handlers_disconnect_by_func (actor,
                                        nbtk_tooltip_hide_complete,
                                        actor);
}

/**
 * nbtk_tooltip_hide:
 * @tooltip: a #NbtkTooltip
 *
 * Hide the tooltip
 */
void
nbtk_tooltip_hide (NbtkTooltip *tooltip)
{
  ClutterAnimation *animation;

  g_return_if_fail (NBTK_TOOLTIP (tooltip));

  /* make sure we're not currently already animating (e.g. hiding) */
  animation = clutter_actor_get_animation (CLUTTER_ACTOR (tooltip));
  if (animation)
    clutter_animation_completed (animation);

  g_object_set (G_OBJECT (tooltip),
                "scale-center-x", tooltip->priv->arrow_offset,
                NULL);
  animation =
    clutter_actor_animate (CLUTTER_ACTOR (tooltip), CLUTTER_EASE_IN_SINE,
                           150,
                           "scale-x", 0.0,
                           "scale-y", 0.0,
                           NULL);
  g_signal_connect (animation, "completed",
                    G_CALLBACK (nbtk_tooltip_hide_complete), tooltip);
}

/**
 * nbtk_tooltip_set_tip_area:
 * @tooltip: A #NbtkTooltip
 * @area: A #ClutterGeometry
 *
 * Set the area on the stage that the tooltip applies to.
 */
void
nbtk_tooltip_set_tip_area (NbtkTooltip           *tooltip,
                           const ClutterGeometry *area)
{
  g_return_if_fail (NBTK_IS_TOOLTIP (tooltip));

  if (tooltip->priv->tip_area)
    g_boxed_free (CLUTTER_TYPE_GEOMETRY, tooltip->priv->tip_area);
  tooltip->priv->tip_area = g_boxed_copy (CLUTTER_TYPE_GEOMETRY, area);

  nbtk_tooltip_update_position (tooltip);
}

/**
 * nbtk_tooltip_get_tip_area:
 * @tooltip: A #NbtkTooltip
 *
 * Retrieve the area on the stage that the tooltip currently applies to
 *
 * Returns: the #ClutterGeometry, owned by the tooltip which must not be freed
 * by the application.
 */
G_CONST_RETURN ClutterGeometry*
nbtk_tooltip_get_tip_area (NbtkTooltip *tooltip)
{
  g_return_val_if_fail (NBTK_IS_TOOLTIP (tooltip), NULL);

  return tooltip->priv->tip_area;
}
