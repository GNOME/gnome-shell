/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-button.c: Plain button actor
 *
 * Copyright 2007 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
 * Copyright 2009, 2010 Red Hat, Inc.
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

/**
 * StButton:
 *
 * Button widget
 *
 * A button widget with support for either a text label or icon, toggle mode
 * and transitions effects between states.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>
#include <clutter/clutter-pango.h>

#include "st-button.h"

#include "st-icon.h"
#include "st-enum-types.h"
#include "st-texture-cache.h"
#include "st-private.h"

#include <st/st-widget-accessible.h>

enum
{
  PROP_0,

  PROP_LABEL,
  PROP_ICON_NAME,
  PROP_BUTTON_MASK,
  PROP_TOGGLE_MODE,
  PROP_CHECKED,
  PROP_PRESSED,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

enum
{
  CLICKED,

  LAST_SIGNAL
};

typedef struct _StButtonPrivate       StButtonPrivate;

struct _StButtonPrivate
{
  gchar *text;

  ClutterClickGesture *click_gesture;

  gboolean key_pressed;

  guint  button_mask : 3;
  guint  is_toggle   : 1;

  guint  is_checked  : 1;
};

static guint button_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (StButton, st_button, ST_TYPE_BIN);

G_DECLARE_FINAL_TYPE (StButtonAccessible,
                      st_button_accessible,
                      ST, BUTTON_ACCESSIBLE,
                      StWidgetAccessible)

static void
st_button_update_label_style (StButton *button)
{
  ClutterActor *label;

  label = st_bin_get_child (ST_BIN (button));

  /* check the child is really a label */
  if (!CLUTTER_IS_TEXT (label))
    return;

  _st_set_text_from_style (CLUTTER_TEXT (label), st_widget_get_theme_node (ST_WIDGET (button)));
}

static void
st_button_style_changed (StWidget *widget)
{
  StButton *button = ST_BUTTON (widget);

  ST_WIDGET_CLASS (st_button_parent_class)->style_changed (widget);

  /* update the label styling */
  st_button_update_label_style (button);
}

static void
handle_clicked (StButton     *button,
                unsigned int  clicked_button)
{
  StButtonPrivate *priv = st_button_get_instance_private (button);

  if (priv->is_toggle)
    st_button_set_checked (button, !priv->is_checked);

  g_signal_emit (button, button_signals[CLICKED], 0, clicked_button);
}

static void
button_click_gesture_recognize_cb (ClutterPressGesture *press_gesture,
                                   StButton            *button)
{
  StButtonPrivate *priv = st_button_get_instance_private (button);
  unsigned int clicked_button = clutter_press_gesture_get_button (press_gesture);

  if ((priv->button_mask & ST_BUTTON_MASK_FROM_BUTTON (clicked_button)) == 0)
    return;

  handle_clicked (button, clicked_button);
}

static void
button_click_gesture_notify_pressed_cb (GObject    *gobject,
                                        GParamSpec *pspec,
                                        gpointer    data)
{
  ClutterPressGesture *press_gesture = CLUTTER_PRESS_GESTURE (gobject);
  StWidget *widget = ST_WIDGET (data);
  gboolean pressed;

  pressed = clutter_press_gesture_get_pressed (press_gesture);

  if (pressed)
    st_widget_add_style_pseudo_class (widget, "active");
  else
    st_widget_remove_style_pseudo_class (widget, "active");

  g_object_notify (G_OBJECT (widget), "pressed");
}

static gboolean
st_button_key_press (ClutterActor *actor,
                     ClutterEvent *event)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);
  uint32_t keyval;

  if (priv->button_mask & ST_BUTTON_ONE)
    {
      keyval = clutter_event_get_key_symbol (event);

      if (keyval == CLUTTER_KEY_space ||
          keyval == CLUTTER_KEY_Return ||
          keyval == CLUTTER_KEY_KP_Enter ||
          keyval == CLUTTER_KEY_ISO_Enter)
        {
          priv->key_pressed = TRUE;
          st_widget_add_style_pseudo_class (ST_WIDGET (actor), "active");

          return CLUTTER_EVENT_STOP;
        }
    }

  return CLUTTER_ACTOR_CLASS (st_button_parent_class)->key_press_event (actor, event);
}

static gboolean
st_button_key_release (ClutterActor *actor,
                       ClutterEvent *event)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);
  uint32_t keyval;

  if (priv->button_mask & ST_BUTTON_ONE)
    {
      keyval = clutter_event_get_key_symbol (event);

      if (keyval == CLUTTER_KEY_space ||
          keyval == CLUTTER_KEY_Return ||
          keyval == CLUTTER_KEY_KP_Enter ||
          keyval == CLUTTER_KEY_ISO_Enter)
        {
          if (priv->key_pressed)
            {
              handle_clicked (button, ST_BUTTON_ONE);
              st_widget_remove_style_pseudo_class (ST_WIDGET (actor), "active");
              priv->key_pressed = FALSE;
            }

          return CLUTTER_EVENT_STOP;
        }
    }

  return CLUTTER_EVENT_PROPAGATE;
}

static void
st_button_key_focus_out (ClutterActor *actor)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);

  /* If we lose focus between a key press and release, undo the press */
  if (priv->key_pressed)
    {
      st_widget_remove_style_pseudo_class (ST_WIDGET (actor), "active");
      priv->key_pressed = FALSE;
    }

  CLUTTER_ACTOR_CLASS (st_button_parent_class)->key_focus_out (actor);
}

static void
st_button_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  StButton *button = ST_BUTTON (gobject);

  switch (prop_id)
    {
    case PROP_LABEL:
      st_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_ICON_NAME:
      st_button_set_icon_name (button, g_value_get_string (value));
      break;
    case PROP_BUTTON_MASK:
      st_button_set_button_mask (button, g_value_get_flags (value));
      break;
    case PROP_TOGGLE_MODE:
      st_button_set_toggle_mode (button, g_value_get_boolean (value));
      break;
    case PROP_CHECKED:
      st_button_set_checked (button, g_value_get_boolean (value));
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
  StButtonPrivate *priv = st_button_get_instance_private (ST_BUTTON (gobject));

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, priv->text);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, st_button_get_icon_name (ST_BUTTON (gobject)));
      break;
    case PROP_BUTTON_MASK:
      g_value_set_flags (value, priv->button_mask);
      break;
    case PROP_TOGGLE_MODE:
      g_value_set_boolean (value, priv->is_toggle);
      break;
    case PROP_CHECKED:
      g_value_set_boolean (value, priv->is_checked);
      break;
    case PROP_PRESSED:
      g_value_set_boolean (value, st_button_get_pressed (ST_BUTTON (gobject)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_button_finalize (GObject *gobject)
{
  StButtonPrivate *priv = st_button_get_instance_private (ST_BUTTON (gobject));

  g_free (priv->text);

  G_OBJECT_CLASS (st_button_parent_class)->finalize (gobject);
}

static void
st_button_class_init (StButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  gobject_class->set_property = st_button_set_property;
  gobject_class->get_property = st_button_get_property;
  gobject_class->finalize = st_button_finalize;

  actor_class->get_accessible_type = st_button_accessible_get_type;
  actor_class->key_press_event = st_button_key_press;
  actor_class->key_release_event = st_button_key_release;
  actor_class->key_focus_out = st_button_key_focus_out;

  widget_class->style_changed = st_button_style_changed;

  /**
   * StButton:label:
   *
   * The label of the #StButton.
   */
  props[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
                         NULL,
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StButton:icon-name:
   *
   * The icon name of the #StButton.
   */
  props[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StButton:button-mask:
   *
   * Which buttons will trigger the #StButton::clicked signal.
   */
  props[PROP_BUTTON_MASK] =
    g_param_spec_flags ("button-mask", NULL, NULL,
                        ST_TYPE_BUTTON_MASK, ST_BUTTON_ONE,
                        ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StButton:toggle-mode:
   *
   * Whether the #StButton is operating in toggle mode (on/off).
   */
  props[PROP_TOGGLE_MODE] =
    g_param_spec_boolean ("toggle-mode", NULL, NULL,
                          FALSE,
                          ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StButton:checked:
   *
   * If #StButton:toggle-mode is %TRUE, indicates if the #StButton is toggled
   * "on" or "off".
   *
   * When the value is %TRUE, the #StButton will have the `checked` CSS
   * pseudo-class set.
   */
  props[PROP_CHECKED] =
    g_param_spec_boolean ("checked", NULL, NULL,
                          FALSE,
                          ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StButton:pressed:
   *
   * In contrast to #StButton:checked, this property indicates whether the
   * #StButton is being actively pressed, rather than just in the "on" state.
   */
  props[PROP_PRESSED] =
    g_param_spec_boolean ("pressed", NULL, NULL,
                          FALSE,
                          ST_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, props);


  /**
   * StButton::clicked:
   * @button: the object that received the signal
   * @clicked_button: the mouse button that was used
   *
   * Emitted when the user activates the button, either with a mouse press and
   * release or with the keyboard.
   */
  button_signals[CLICKED] =
    g_signal_new ("clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StButtonClass, clicked),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);
}

static void
st_button_init (StButton *button)
{
  StButtonPrivate *priv = st_button_get_instance_private (button);

  priv->button_mask = ST_BUTTON_ONE;
  priv->click_gesture = CLUTTER_CLICK_GESTURE (clutter_click_gesture_new ());
  clutter_press_gesture_set_cancel_threshold (CLUTTER_PRESS_GESTURE (priv->click_gesture), -1);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (priv->click_gesture), "StButton click gesture");

  g_signal_connect (priv->click_gesture, "recognize",
                    G_CALLBACK (button_click_gesture_recognize_cb), button);
  g_signal_connect (priv->click_gesture, "notify::pressed",
                    G_CALLBACK (button_click_gesture_notify_pressed_cb), button);

  clutter_actor_add_action (CLUTTER_ACTOR (button), CLUTTER_ACTION (priv->click_gesture));

  clutter_actor_set_reactive (CLUTTER_ACTOR (button), TRUE);
  st_widget_set_track_hover (ST_WIDGET (button), TRUE);
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
 * Get the text displayed on the button. If the label is empty, an empty string
 * will be returned instead of %NULL.
 *
 * Returns: (transfer none): the text for the button
 */
const gchar *
st_button_get_label (StButton *button)
{
  g_return_val_if_fail (ST_IS_BUTTON (button), NULL);

  return ((StButtonPrivate *)st_button_get_instance_private (button))->text;
}

/**
 * st_button_set_label:
 * @button: a #Stbutton
 * @text: (nullable): text to set the label to
 *
 * Sets the text displayed on the button.
 */
void
st_button_set_label (StButton    *button,
                     const gchar *text)
{
  StButtonPrivate *priv;
  ClutterActor *label;

  g_return_if_fail (ST_IS_BUTTON (button));

  priv = st_button_get_instance_private (button);

  if (g_strcmp0 (priv->text, text) == 0)
    return;

  g_free (priv->text);

  if (text)
    priv->text = g_strdup (text);
  else
    priv->text = g_strdup ("");

  label = st_bin_get_child (ST_BIN (button));

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
                            "x-align", CLUTTER_ACTOR_ALIGN_CENTER,
                            "y-align", CLUTTER_ACTOR_ALIGN_CENTER,
                            NULL);
      st_bin_set_child (ST_BIN (button), label);
    }

  /* Fake a style change so that we reset the style properties on the label */
  st_widget_style_changed (ST_WIDGET (button));

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_LABEL]);
}

/**
 * st_button_get_icon_name:
 * @button: a #StButton
 *
 * Get the icon name of the button. If the button isn't showing an icon,
 * the return value will be %NULL.
 *
 * Returns: (transfer none) (nullable): the icon name of the button
 */
const char *
st_button_get_icon_name (StButton *button)
{
  ClutterActor *icon;

  g_return_val_if_fail (ST_IS_BUTTON (button), NULL);

  icon = st_bin_get_child (ST_BIN (button));
  if (ST_IS_ICON (icon))
    return st_icon_get_icon_name (ST_ICON (icon));
  return NULL;
}

/**
 * st_button_set_icon_name:
 * @button: a #Stbutton
 * @icon_name: an icon name
 *
 * Adds an `StIcon` with the given icon name as a child.
 *
 * If @button already contains a child actor, that child will
 * be removed and replaced with the icon.
 */
void
st_button_set_icon_name (StButton   *button,
                         const char *icon_name)
{
  ClutterActor *icon;

  g_return_if_fail (ST_IS_BUTTON (button));
  g_return_if_fail (icon_name != NULL);

  icon = st_bin_get_child (ST_BIN (button));

  if (ST_IS_ICON (icon))
    {
      if (g_strcmp0 (st_icon_get_icon_name (ST_ICON (icon)), icon_name) == 0)
        return;

      st_icon_set_icon_name (ST_ICON (icon), icon_name);
    }
  else
    {
      icon = g_object_new (ST_TYPE_ICON,
                           "icon-name", icon_name,
                           "x-align", CLUTTER_ACTOR_ALIGN_CENTER,
                           "y-align", CLUTTER_ACTOR_ALIGN_CENTER,
                           NULL);
      st_bin_set_child (ST_BIN (button), icon);
    }

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_ICON_NAME]);
}

/**
 * st_button_get_button_mask:
 * @button: a #StButton
 *
 * Gets the mask of mouse buttons that @button emits the
 * #StButton::clicked signal for.
 *
 * Returns: the mask of mouse buttons that @button emits the
 * #StButton::clicked signal for.
 */
StButtonMask
st_button_get_button_mask (StButton *button)
{
  g_return_val_if_fail (ST_IS_BUTTON (button), 0);

  return ((StButtonPrivate *)st_button_get_instance_private (button))->button_mask;
}

/**
 * st_button_set_button_mask:
 * @button: a #Stbutton
 * @mask: the mask of mouse buttons that @button responds to
 *
 * Sets which mouse buttons @button emits #StButton::clicked for.
 */
void
st_button_set_button_mask (StButton     *button,
                           StButtonMask  mask)
{
  StButtonPrivate *priv;

  g_return_if_fail (ST_IS_BUTTON (button));

  priv = st_button_get_instance_private (button);

  if (priv->button_mask == mask)
    return;

  priv->button_mask = mask;

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_BUTTON_MASK]);
}

/**
 * st_button_get_toggle_mode:
 * @button: a #StButton
 *
 * Get the toggle mode status of the button.
 *
 * Returns: %TRUE if toggle mode is set, otherwise %FALSE
 */
gboolean
st_button_get_toggle_mode (StButton *button)
{
  g_return_val_if_fail (ST_IS_BUTTON (button), FALSE);

  return ((StButtonPrivate *)st_button_get_instance_private (button))->is_toggle;
}

/**
 * st_button_set_toggle_mode:
 * @button: a #Stbutton
 * @toggle: %TRUE or %FALSE
 *
 * Enables or disables toggle mode for the button. In toggle mode, the checked
 * state will be "toggled" when the user clicks the button.
 */
void
st_button_set_toggle_mode (StButton *button,
                           gboolean  toggle)
{
  StButtonPrivate *priv;

  g_return_if_fail (ST_IS_BUTTON (button));

  priv = st_button_get_instance_private (button);

  if (priv->is_toggle == toggle)
    return;

  priv->is_toggle = toggle;

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_TOGGLE_MODE]);
}

/**
 * st_button_get_checked:
 * @button: a #StButton
 *
 * Get the #StButton:checked property of a #StButton that is in toggle mode.
 *
 * Returns: %TRUE if the button is checked, or %FALSE if not
 */
gboolean
st_button_get_checked (StButton *button)
{
  g_return_val_if_fail (ST_IS_BUTTON (button), FALSE);

  return ((StButtonPrivate *)st_button_get_instance_private (button))->is_checked;
}

/**
 * st_button_set_checked:
 * @button: a #Stbutton
 * @checked: %TRUE or %FALSE
 *
 * Set the #StButton:checked property of the button. This is only really useful
 * if the button has #StButton:toggle-mode property set to %TRUE.
 */
void
st_button_set_checked (StButton *button,
                       gboolean  checked)
{
  StButtonPrivate *priv;

  g_return_if_fail (ST_IS_BUTTON (button));

  priv = st_button_get_instance_private (button);
  if (priv->is_checked == checked)
    return;

  priv->is_checked = checked;

  if (checked)
    st_widget_add_style_pseudo_class (ST_WIDGET (button), "checked");
  else
    st_widget_remove_style_pseudo_class (ST_WIDGET (button), "checked");

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_CHECKED]);
}

/**
 * st_button_get_pressed:
 * @button: a #StButton
 *
 * Get the #StButton:pressed property of a #StButton
 *
 * Returns: %TRUE if the button is pressed, or %FALSE if not
 */
gboolean
st_button_get_pressed (StButton *button)
{
  StButtonPrivate *priv;

  g_return_val_if_fail (ST_IS_BUTTON (button), FALSE);

  priv = st_button_get_instance_private (button);

  return clutter_press_gesture_get_pressed (CLUTTER_PRESS_GESTURE (priv->click_gesture));
}

/**
 * st_button_fake_release:
 * @button: an #StButton
 *
 * If this widget is holding a pointer grab, this function will
 * will ungrab it, and reset the #StButton:pressed state.  The effect is
 * similar to if the user had released the mouse button, but without
 * emitting the #StButton::clicked signal.
 *
 * This function is useful if for example you want to do something
 * after the user is holding the mouse button for a given period of
 * time, breaking the grab.
 */
void
st_button_fake_release (StButton *button)
{
  StButtonPrivate *priv;

  g_return_if_fail (ST_IS_BUTTON (button));

  priv = st_button_get_instance_private (button);

  clutter_gesture_cancel (CLUTTER_GESTURE (priv->click_gesture));
}

/******************************************************************************/
/*************************** ACCESSIBILITY SUPPORT ****************************/
/******************************************************************************/

#define ST_TYPE_BUTTON_ACCESSIBLE st_button_accessible_get_type ()

typedef struct _StButtonAccessible
{
  StWidgetAccessible parent;
} StButtonAccessible;


/* AtkObject */
static void          st_button_accessible_initialize (AtkObject *obj,
                                                      gpointer   data);

G_DEFINE_FINAL_TYPE (StButtonAccessible, st_button_accessible, ST_TYPE_WIDGET_ACCESSIBLE)

static const gchar *
st_button_accessible_get_name (AtkObject *obj)
{
  StButton *button = NULL;
  const gchar *name = NULL;

  button = ST_BUTTON (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj)));

  if (button == NULL)
    return NULL;

  name = ATK_OBJECT_CLASS (st_button_accessible_parent_class)->get_name (obj);
  if (name != NULL)
    return name;

  return st_button_get_label (button);
}

static void
st_button_accessible_class_init (StButtonAccessibleClass *klass)
{
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  atk_class->initialize = st_button_accessible_initialize;
  atk_class->get_name = st_button_accessible_get_name;
}

static void
st_button_accessible_init (StButtonAccessible *self)
{
  /* initialization done on AtkObject->initialize */
}

static void
st_button_accessible_notify_label_cb (StButton   *button,
                                      GParamSpec *psec,
                                      AtkObject  *accessible)
{
  g_object_notify (G_OBJECT (accessible), "accessible-name");
}

static void
st_button_accessible_compute_role (AtkObject *accessible,
                                   StButton  *button)
{
  atk_object_set_role (accessible, st_button_get_toggle_mode (button)
                       ? ATK_ROLE_TOGGLE_BUTTON : ATK_ROLE_PUSH_BUTTON);
}

static void
st_button_accessible_notify_toggle_mode_cb (StButton   *button,
                                            GParamSpec *psec,
                                            AtkObject  *accessible)
{
  st_button_accessible_compute_role (accessible, button);
}

static void
st_button_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (st_button_accessible_parent_class)->initialize (obj, data);

  st_button_accessible_compute_role (obj, ST_BUTTON (data));

  g_signal_connect (data, "notify::label",
                    G_CALLBACK (st_button_accessible_notify_label_cb), obj);
  g_signal_connect (data, "notify::toggle-mode",
                    G_CALLBACK (st_button_accessible_notify_toggle_mode_cb), obj);
}
