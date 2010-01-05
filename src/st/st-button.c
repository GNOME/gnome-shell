/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-button.c: Plain button actor
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
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 *             Thomas Wood <thomas@linux.intel.com>
 *
 */

/**
 * SECTION:st-button
 * @short_description: Button widget
 *
 * A button widget with support for either a text label or icon, toggle mode
 * and transitions effects between states.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>

#include "st-button.h"

#include "st-marshal.h"
#include "st-texture-frame.h"
#include "st-texture-cache.h"
#include "st-private.h"

enum
{
  PROP_0,

  PROP_LABEL,
  PROP_TOGGLE_MODE,
  PROP_CHECKED,
  PROP_TRANSITION_DURATION
};

enum
{
  CLICKED,

  LAST_SIGNAL
};

#define ST_BUTTON_GET_PRIVATE(obj)    \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ST_TYPE_BUTTON, StButtonPrivate))

struct _StButtonPrivate
{
  gchar            *text;

  ClutterActor     *old_bg;
  gboolean          old_bg_parented; /* TRUE if we have adopted old_bg */

  guint8            old_opacity;

  guint             is_pressed : 1;
  guint             is_hover : 1;
  guint             is_checked : 1;
  guint             is_toggle : 1;

  gint              transition_duration;

  ClutterAnimation *animation;

  gint              spacing;
};

static guint button_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (StButton, st_button, ST_TYPE_BIN);

static void
st_button_update_label_style (StButton *button)
{
  ClutterActor *label;

  label = st_bin_get_child ((StBin*) button);

  /* check the child is really a label */
  if (!CLUTTER_IS_TEXT (label))
    return;

  _st_set_text_from_style ((ClutterText*) label, st_widget_get_theme_node (ST_WIDGET (button)));
}

static void
st_button_dispose_old_bg (StButton *button)
{
  StButtonPrivate *priv = button->priv;

  if (priv->old_bg)
    {
      if (priv->old_bg_parented)
        {
          clutter_actor_unparent (priv->old_bg);
          priv->old_bg_parented = FALSE;
        }
      g_object_unref (priv->old_bg);
      priv->old_bg = NULL;
    }
}

static void
st_animation_completed (ClutterAnimation *animation,
                        StButton         *button)
{
  st_button_dispose_old_bg (button);
}

static void
st_button_style_changed (StWidget *widget)
{
  StButton *button = ST_BUTTON (widget);
  StButtonPrivate *priv = button->priv;
  StButtonClass *button_class = ST_BUTTON_GET_CLASS (button);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (button));
  ClutterActor *bg_image;
  double spacing;

  st_button_dispose_old_bg (button);

  bg_image = st_widget_get_border_image ((StWidget*) button);
  if (bg_image)
    button->priv->old_bg = g_object_ref (bg_image);

  ST_WIDGET_CLASS (st_button_parent_class)->style_changed (widget);

  spacing = 6;
  st_theme_node_get_length (theme_node, "border-spacing", FALSE, &spacing);
  priv->spacing = (int)(0.5 + spacing);

  /* update the label styling */
  st_button_update_label_style (button);

  /* run a transition if applicable */
  if (button_class->transition)
    {
      button_class->transition (button, priv->old_bg);
    }
  else
    {
      if (priv->old_bg &&
          (!st_widget_get_style_pseudo_class (widget)))
        {
          ClutterAnimation *animation;
          if (!clutter_actor_get_parent (priv->old_bg))
            {
              clutter_actor_set_parent (priv->old_bg, (ClutterActor*) widget);
              priv->old_bg_parented = TRUE;
            }
          if (priv->transition_duration > 0)
            {
              animation = clutter_actor_animate (priv->old_bg,
                                                 CLUTTER_LINEAR,
                                                 priv->transition_duration,
                                                 "opacity", 0,
                                                 NULL);
              g_signal_connect (animation, "completed",
                                G_CALLBACK (st_animation_completed), button);
            }
          else
            {
              st_button_dispose_old_bg (button);
            }

        }
    }
}

static void
st_button_real_pressed (StButton *button)
{
  st_widget_set_style_pseudo_class ((StWidget*) button, "active");
}

static void
st_button_real_released (StButton *button)
{
  StButtonPrivate *priv = button->priv;

  if (priv->is_checked)
    st_widget_set_style_pseudo_class ((StWidget*) button, "checked");
  else if (!priv->is_hover)
    st_widget_set_style_pseudo_class ((StWidget*) button, NULL);
  else
    st_widget_set_style_pseudo_class ((StWidget*) button, "hover");

}

static gboolean
st_button_button_press (ClutterActor       *actor,
                        ClutterButtonEvent *event)
{
  st_widget_hide_tooltip (ST_WIDGET (actor));

  if (event->button == 1)
    {
      StButton *button = ST_BUTTON (actor);
      StButtonClass *klass = ST_BUTTON_GET_CLASS (button);

      button->priv->is_pressed = TRUE;

      clutter_grab_pointer (actor);

      if (klass->pressed)
        klass->pressed (button);

      return TRUE;
    }

  return FALSE;
}

static gboolean
st_button_button_release (ClutterActor       *actor,
                          ClutterButtonEvent *event)
{
  if (event->button == 1)
    {
      StButton *button = ST_BUTTON (actor);
      StButtonClass *klass = ST_BUTTON_GET_CLASS (button);

      if (!button->priv->is_pressed)
        return FALSE;

      clutter_ungrab_pointer ();

      if (button->priv->is_toggle)
        {
          st_button_set_checked (button, !button->priv->is_checked);
        }

      button->priv->is_pressed = FALSE;

      if (klass->released)
        klass->released (button);

      g_signal_emit (button, button_signals[CLICKED], 0);

      return TRUE;
    }

  return FALSE;
}

static gboolean
st_button_enter (ClutterActor         *actor,
                 ClutterCrossingEvent *event)
{
  StButton *button = ST_BUTTON (actor);

  if (!button->priv->is_checked)
    st_widget_set_style_pseudo_class ((StWidget*) button, "hover");

  button->priv->is_hover = 1;

  return CLUTTER_ACTOR_CLASS (st_button_parent_class)->enter_event (actor, event);
}

static gboolean
st_button_leave (ClutterActor         *actor,
                 ClutterCrossingEvent *event)
{
  StButton *button = ST_BUTTON (actor);

  button->priv->is_hover = 0;

  if (button->priv->is_pressed)
    {
      StButtonClass *klass = ST_BUTTON_GET_CLASS (button);

      clutter_ungrab_pointer ();

      button->priv->is_pressed = FALSE;

      if (klass->released)
        klass->released (button);
    }

  if (button->priv->is_checked)
    st_widget_set_style_pseudo_class ((StWidget*) button, "checked");
  else
    st_widget_set_style_pseudo_class ((StWidget*) button, NULL);

  return CLUTTER_ACTOR_CLASS (st_button_parent_class)->leave_event (actor, event);
}

static void
st_button_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  StButton *button = ST_BUTTON (gobject);
  StButtonPrivate *priv = ST_BUTTON (gobject)->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      st_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_TOGGLE_MODE:
      st_button_set_toggle_mode (button, g_value_get_boolean (value));
      break;
    case PROP_CHECKED:
      st_button_set_checked (button, g_value_get_boolean (value));
      break;
    case PROP_TRANSITION_DURATION:
      priv->transition_duration = g_value_get_int (value);
      break;


    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_button_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  StButtonPrivate *priv = ST_BUTTON (gobject)->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, priv->text);
      break;
    case PROP_TOGGLE_MODE:
      g_value_set_boolean (value, priv->is_toggle);
      break;
    case PROP_CHECKED:
      g_value_set_boolean (value, priv->is_checked);
      break;
    case PROP_TRANSITION_DURATION:
      g_value_set_int (value, priv->transition_duration);
      break;


    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_button_finalize (GObject *gobject)
{
  StButtonPrivate *priv = ST_BUTTON (gobject)->priv;

  g_free (priv->text);

  G_OBJECT_CLASS (st_button_parent_class)->finalize (gobject);
}

static void
st_button_dispose (GObject *gobject)
{
  st_button_dispose_old_bg (ST_BUTTON (gobject));

  G_OBJECT_CLASS (st_button_parent_class)->dispose (gobject);
}

static void
st_button_map (ClutterActor *self)
{
  StButtonPrivate *priv = ST_BUTTON (self)->priv;

  CLUTTER_ACTOR_CLASS (st_button_parent_class)->map (self);

  if (priv->old_bg && priv->old_bg_parented)
    clutter_actor_map (priv->old_bg);
}

static void
st_button_unmap (ClutterActor *self)
{
  StButtonPrivate *priv = ST_BUTTON (self)->priv;

  CLUTTER_ACTOR_CLASS (st_button_parent_class)->unmap (self);

  if (priv->old_bg && priv->old_bg_parented)
    clutter_actor_unmap (priv->old_bg);
}

static void
st_button_draw_background (StWidget         *widget)
{
  StButtonPrivate *priv;

  ST_WIDGET_CLASS (st_button_parent_class)->draw_background (widget);

  priv = ST_BUTTON (widget)->priv;

  if (priv->old_bg && priv->old_bg_parented)
    clutter_actor_paint (priv->old_bg);
}

static void
st_button_class_init (StButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (StButtonPrivate));

  klass->pressed = st_button_real_pressed;
  klass->released = st_button_real_released;

  gobject_class->set_property = st_button_set_property;
  gobject_class->get_property = st_button_get_property;
  gobject_class->dispose = st_button_dispose;
  gobject_class->finalize = st_button_finalize;

  actor_class->button_press_event = st_button_button_press;
  actor_class->button_release_event = st_button_button_release;
  actor_class->enter_event = st_button_enter;
  actor_class->leave_event = st_button_leave;

  actor_class->map = st_button_map;
  actor_class->unmap = st_button_unmap;

  widget_class->draw_background = st_button_draw_background;
  widget_class->style_changed = st_button_style_changed;

  pspec = g_param_spec_string ("label",
                               "Label",
                               "Label of the button",
                               NULL, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LABEL, pspec);

  pspec = g_param_spec_boolean ("toggle-mode",
                                "Toggle Mode",
                                "Enable or disable toggling",
                                FALSE, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TOGGLE_MODE, pspec);

  pspec = g_param_spec_boolean ("checked",
                                "Checked",
                                "Indicates if a toggle button is \"on\""
                                " or \"off\"",
                                FALSE, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHECKED, pspec);

  pspec = g_param_spec_int ("transition-duration",
                            "Transition Duration",
                            "Duration of the state transition effect",
                            0, G_MAXINT, 120, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TRANSITION_DURATION, pspec);


  /**
   * StButton::clicked:
   * @button: the object that received the signal
   *
   * Emitted when the user activates the button, either with a mouse press and
   * release or with the keyboard.
   */

  button_signals[CLICKED] =
    g_signal_new ("clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StButtonClass, clicked),
                  NULL, NULL,
                  _st_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
st_button_init (StButton *button)
{
  button->priv = ST_BUTTON_GET_PRIVATE (button);
  button->priv->transition_duration = 120;
  button->priv->spacing = 6;

  clutter_actor_set_reactive ((ClutterActor *) button, TRUE);
}

/**
 * st_button_new:
 *
 * Create a new button
 *
 * Returns: a new #StButton
 */
StWidget *
st_button_new (void)
{
  return g_object_new (ST_TYPE_BUTTON, NULL);
}

/**
 * st_button_new_with_label:
 * @text: text to set the label to
 *
 * Create a new #StButton with the specified label
 *
 * Returns: a new #StButton
 */
StWidget *
st_button_new_with_label (const gchar *text)
{
  return g_object_new (ST_TYPE_BUTTON, "label", text, NULL);
}

/**
 * st_button_get_label:
 * @button: a #StButton
 *
 * Get the text displayed on the button
 *
 * Returns: the text for the button. This must not be freed by the application
 */
G_CONST_RETURN gchar *
st_button_get_label (StButton *button)
{
  g_return_val_if_fail (ST_IS_BUTTON (button), NULL);

  return button->priv->text;
}

/**
 * st_button_set_label:
 * @button: a #Stbutton
 * @text: text to set the label to
 *
 * Sets the text displayed on the button
 */
void
st_button_set_label (StButton    *button,
                     const gchar *text)
{
  StButtonPrivate *priv;
  ClutterActor *label;

  g_return_if_fail (ST_IS_BUTTON (button));

  priv = button->priv;

  g_free (priv->text);

  if (text)
    priv->text = g_strdup (text);
  else
    priv->text = g_strdup ("");

  label = st_bin_get_child ((StBin*) button);

  if (label && CLUTTER_IS_TEXT (label))
    {
      clutter_text_set_text (CLUTTER_TEXT (label), priv->text);
    }
  else
    {
      label = g_object_new (CLUTTER_TYPE_TEXT,
                            "text", priv->text,
                            "line-alignment", PANGO_ALIGN_CENTER,
                            "ellipsize", PANGO_ELLIPSIZE_END,
                            "use-markup", TRUE,
                            NULL);
      st_bin_set_child ((StBin*) button, label);
    }

  /* Fake a style change so that we reset the style properties on the label */
  st_widget_style_changed (ST_WIDGET (button));

  g_object_notify (G_OBJECT (button), "label");
}

/**
 * st_button_get_toggle_mode:
 * @button: a #StButton
 *
 * Get the toggle mode status of the button.
 *
 * Returns: #TRUE if toggle mode is set, otherwise #FALSE
 */
gboolean
st_button_get_toggle_mode (StButton *button)
{
  g_return_val_if_fail (ST_IS_BUTTON (button), FALSE);

  return button->priv->is_toggle;
}

/**
 * st_button_set_toggle_mode:
 * @button: a #Stbutton
 * @toggle: #TRUE or #FALSE
 *
 * Enables or disables toggle mode for the button. In toggle mode, the active
 * state will be "toggled" when the user clicks the button.
 */
void
st_button_set_toggle_mode (StButton *button,
                           gboolean  toggle)
{
  g_return_if_fail (ST_IS_BUTTON (button));

  button->priv->is_toggle = toggle;

  g_object_notify (G_OBJECT (button), "toggle-mode");
}

/**
 * st_button_get_checked:
 * @button: a #StButton
 *
 * Get the state of the button that is in toggle mode.
 *
 * Returns: #TRUE if the button is checked, or #FALSE if not
 */
gboolean
st_button_get_checked (StButton *button)
{
  g_return_val_if_fail (ST_IS_BUTTON (button), FALSE);

  return button->priv->is_checked;
}

/**
 * st_button_set_checked:
 * @button: a #Stbutton
 * @checked: #TRUE or #FALSE
 *
 * Sets the pressed state of the button. This is only really useful if the
 * button has #toggle-mode mode set to #TRUE.
 */
void
st_button_set_checked (StButton *button,
                       gboolean  checked)
{
  g_return_if_fail (ST_IS_BUTTON (button));

  if (button->priv->is_checked != checked)
    {
      button->priv->is_checked = checked;

      if (checked)
        st_widget_set_style_pseudo_class ((StWidget*) button, "checked");
      else
      if (button->priv->is_hover)
        st_widget_set_style_pseudo_class ((StWidget*) button, "hover");
      else
        st_widget_set_style_pseudo_class ((StWidget*) button, NULL);
    }

  g_object_notify (G_OBJECT (button), "checked");
}
