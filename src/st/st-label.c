/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-label.c: Plain label actor
 *
 * Copyright 2008,2009 Intel Corporation
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
 *
 * Written by: Thomas Wood <thomas@linux.intel.com>
 *
 */

/**
 * SECTION:st-label
 * @short_description: Widget for displaying text
 *
 * #StLabel is a simple widget for displaying text. It derives from
 * #StWidget to add extra style and placement functionality over
 * #ClutterText. The internal #ClutterText is publicly accessibly to allow
 * applications to set further properties.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>

#include "st-label.h"

#include "st-widget.h"
#include "st-stylable.h"

enum
{
  PROP_0,

  PROP_LABEL
};

#define ST_LABEL_GET_PRIVATE(obj)     (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ST_TYPE_LABEL, StLabelPrivate))

struct _StLabelPrivate
{
  ClutterActor *label;
};

G_DEFINE_TYPE (StLabel, st_label, ST_TYPE_WIDGET);

static void
st_label_set_property (GObject      *gobject,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  StLabel *label = ST_LABEL (gobject);

  switch (prop_id)
    {
    case PROP_LABEL:
      st_label_set_text (label, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_label_get_property (GObject    *gobject,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  StLabelPrivate *priv = ST_LABEL (gobject)->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, clutter_text_get_text (CLUTTER_TEXT (priv->label)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_label_style_changed (StWidget *self)
{
  StLabelPrivate *priv = ST_LABEL (self)->priv;
  ClutterColor *color = NULL;
  gchar *font_name;
  gchar *font_string;
  gint font_size;

  st_stylable_get (ST_STYLABLE (self),
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
        {
          if (font_size)
            font_string = g_strdup_printf ("%dpx", font_size);
          else
            font_string = font_name;
        }

      clutter_text_set_font_name (CLUTTER_TEXT (priv->label), font_string);
      g_free (font_string);
    }

}

static void
st_label_get_preferred_width (ClutterActor *actor,
                              gfloat        for_height,
                              gfloat       *min_width_p,
                              gfloat       *natural_width_p)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  StPadding padding = { 0, };

  st_widget_get_padding (ST_WIDGET (actor), &padding);

  clutter_actor_get_preferred_width (priv->label, for_height,
                                     min_width_p,
                                     natural_width_p);

  if (min_width_p)
    *min_width_p += padding.left + padding.right;

  if (natural_width_p)
    *natural_width_p += padding.left + padding.right;
}

static void
st_label_get_preferred_height (ClutterActor *actor,
                               gfloat        for_width,
                               gfloat       *min_height_p,
                               gfloat       *natural_height_p)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  StPadding padding = { 0, };

  st_widget_get_padding (ST_WIDGET (actor), &padding);

  clutter_actor_get_preferred_height (priv->label, for_width,
                                      min_height_p,
                                      natural_height_p);

  if (min_height_p)
    *min_height_p += padding.top + padding.bottom;

  if (natural_height_p)
    *natural_height_p += padding.top + padding.bottom;
}

static void
st_label_allocate (ClutterActor          *actor,
                   const ClutterActorBox *box,
                   ClutterAllocationFlags flags)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  ClutterActorClass *parent_class;
  ClutterActorBox child_box;
  StPadding padding = { 0, };

  st_widget_get_padding (ST_WIDGET (actor), &padding);

  parent_class = CLUTTER_ACTOR_CLASS (st_label_parent_class);
  parent_class->allocate (actor, box, flags);

  child_box.x1 = padding.left;
  child_box.y1 = padding.top;
  child_box.x2 = box->x2 - box->x1 - padding.right;
  child_box.y2 = box->y2 - box->y1 - padding.bottom;

  clutter_actor_allocate (priv->label, &child_box, flags);
}

static void
st_label_paint (ClutterActor *actor)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  ClutterActorClass *parent_class;

  parent_class = CLUTTER_ACTOR_CLASS (st_label_parent_class);
  parent_class->paint (actor);

  clutter_actor_paint (priv->label);
}

static void
st_label_map (ClutterActor *actor)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;

  CLUTTER_ACTOR_CLASS (st_label_parent_class)->map (actor);

  clutter_actor_map (priv->label);
}

static void
st_label_unmap (ClutterActor *actor)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;

  CLUTTER_ACTOR_CLASS (st_label_parent_class)->unmap (actor);

  clutter_actor_unmap (priv->label);
}

static void
st_label_class_init (StLabelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (StLabelPrivate));

  gobject_class->set_property = st_label_set_property;
  gobject_class->get_property = st_label_get_property;

  actor_class->paint = st_label_paint;
  actor_class->allocate = st_label_allocate;
  actor_class->get_preferred_width = st_label_get_preferred_width;
  actor_class->get_preferred_height = st_label_get_preferred_height;
  actor_class->map = st_label_map;
  actor_class->unmap = st_label_unmap;

  pspec = g_param_spec_string ("text",
                               "Text",
                               "Text of the label",
                               NULL, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LABEL, pspec);

}

static void
st_label_init (StLabel *label)
{
  StLabelPrivate *priv;

  label->priv = priv = ST_LABEL_GET_PRIVATE (label);

  label->priv->label = g_object_new (CLUTTER_TYPE_TEXT,
                                     "ellipsize", PANGO_ELLIPSIZE_END,
                                     NULL);

  clutter_actor_set_parent (priv->label, CLUTTER_ACTOR (label));

  g_signal_connect (label, "style-changed",
                    G_CALLBACK (st_label_style_changed), NULL);
}

/**
 * st_label_new:
 * @text: text to set the label to
 *
 * Create a new #StLabel with the specified label
 *
 * Returns: a new #StLabel
 */
StWidget *
st_label_new (const gchar *text)
{
  if (text == NULL || *text == '\0')
    return g_object_new (ST_TYPE_LABEL, NULL);
  else
    return g_object_new (ST_TYPE_LABEL,
                         "text", text,
                         NULL);
}

/**
 * st_label_get_text:
 * @label: a #StLabel
 *
 * Get the text displayed on the label
 *
 * Returns: the text for the label. This must not be freed by the application
 */
G_CONST_RETURN gchar *
st_label_get_text (StLabel *label)
{
  g_return_val_if_fail (ST_IS_LABEL (label), NULL);

  return clutter_text_get_text (CLUTTER_TEXT (label->priv->label));
}

/**
 * st_label_set_text:
 * @label: a #StLabel
 * @text: text to set the label to
 *
 * Sets the text displayed on the label
 */
void
st_label_set_text (StLabel     *label,
                   const gchar *text)
{
  StLabelPrivate *priv;

  g_return_if_fail (ST_IS_LABEL (label));
  g_return_if_fail (text != NULL);

  priv = label->priv;

  clutter_text_set_text (CLUTTER_TEXT (priv->label), text);

  g_object_notify (G_OBJECT (label), "text");
}

/**
 * st_label_get_clutter_text:
 * @label: a #StLabel
 *
 * Retrieve the internal #ClutterText so that extra parameters can be set
 *
 * Returns: (transfer none): ethe #ClutterText used by #StLabel. The label
 * is owned by the #StLabel and should not be unref'ed by the application.
 */
ClutterActor*
st_label_get_clutter_text (StLabel *label)
{
  g_return_val_if_fail (ST_LABEL (label), NULL);

  return label->priv->label;
}
