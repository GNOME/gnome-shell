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

enum
{
  PROP_0,

  PROP_CLUTTER_TEXT,
  PROP_TEXT
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
    case PROP_TEXT:
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
    case PROP_CLUTTER_TEXT:
      g_value_set_object (value, priv->label);
      break;

    case PROP_TEXT:
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
  StLabelPrivate *priv;
  StThemeNode *theme_node;
  ClutterColor color;
  const PangoFontDescription *font;
  gchar *font_string;

  priv = ST_LABEL (self)->priv;
  theme_node = st_widget_get_theme_node (self);
  st_theme_node_get_foreground_color (theme_node, &color);
  clutter_text_set_color (CLUTTER_TEXT (priv->label), &color);

  font = st_theme_node_get_font (theme_node);
  font_string = pango_font_description_to_string (font);
  clutter_text_set_font_name (CLUTTER_TEXT (priv->label), font_string);
  g_free (font_string);

  ST_WIDGET_CLASS (st_label_parent_class)->style_changed (self);
}

static void
st_label_get_preferred_width (ClutterActor *actor,
                              gfloat        for_height,
                              gfloat       *min_width_p,
                              gfloat       *natural_width_p)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));

  st_theme_node_adjust_for_height (theme_node, &for_height);

  clutter_actor_get_preferred_width (priv->label, for_height,
                                     min_width_p,
                                     natural_width_p);

  st_theme_node_adjust_preferred_width (theme_node, min_width_p, natural_width_p);
}

static void
st_label_get_preferred_height (ClutterActor *actor,
                               gfloat        for_width,
                               gfloat       *min_height_p,
                               gfloat       *natural_height_p)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));

  st_theme_node_adjust_for_width (theme_node, &for_width);

  clutter_actor_get_preferred_height (priv->label, for_width,
                                      min_height_p,
                                      natural_height_p);

  st_theme_node_adjust_preferred_height (theme_node, min_height_p, natural_height_p);
}

static void
st_label_allocate (ClutterActor          *actor,
                   const ClutterActorBox *box,
                   ClutterAllocationFlags flags)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  ClutterActorClass *parent_class;
  ClutterActorBox content_box;

  st_theme_node_get_content_box (theme_node, box, &content_box);

  parent_class = CLUTTER_ACTOR_CLASS (st_label_parent_class);
  parent_class->allocate (actor, box, flags);

  clutter_actor_allocate (priv->label, &content_box, flags);
}

static void
st_label_dispose (GObject   *actor)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;

  if (priv->label)
    {
      clutter_actor_destroy (priv->label);
      priv->label = NULL;
    }

  G_OBJECT_CLASS (st_label_parent_class)->dispose (G_OBJECT (actor));
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
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (StLabelPrivate));

  gobject_class->set_property = st_label_set_property;
  gobject_class->get_property = st_label_get_property;
  gobject_class->dispose = st_label_dispose;

  actor_class->paint = st_label_paint;
  actor_class->allocate = st_label_allocate;
  actor_class->get_preferred_width = st_label_get_preferred_width;
  actor_class->get_preferred_height = st_label_get_preferred_height;
  actor_class->map = st_label_map;
  actor_class->unmap = st_label_unmap;

  widget_class->style_changed = st_label_style_changed;

  pspec = g_param_spec_object ("clutter-text",
			       "Clutter Text",
			       "Internal ClutterText actor",
			       CLUTTER_TYPE_TEXT,
			       G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_CLUTTER_TEXT, pspec);

  pspec = g_param_spec_string ("text",
                               "Text",
                               "Text of the label",
                               NULL, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TEXT, pspec);

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
