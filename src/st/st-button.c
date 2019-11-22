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

#include "st-enum-types.h"
#include "st-texture-cache.h"
#include "st-private.h"

#include <st/st-widget-accessible.h>

enum
{
  PROP_0,

  PROP_LABEL,
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

  ClutterInputDevice *device;
  ClutterEventSequence *press_sequence;

  guint  button_mask : 3;
  guint  is_toggle   : 1;

  guint  pressed     : 3;
  guint  grabbed     : 3;
  guint  is_checked  : 1;

  gint   spacing;
};

static guint button_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (StButton, st_button, ST_TYPE_BIN);

static GType st_button_accessible_get_type (void) G_GNUC_CONST;

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
  StButtonPrivate *priv = st_button_get_instance_private (button);
  StButtonClass *button_class = ST_BUTTON_GET_CLASS (button);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (button));
  double spacing;

  ST_WIDGET_CLASS (st_button_parent_class)->style_changed (widget);

  spacing = 6;
  st_theme_node_lookup_length (theme_node, "border-spacing", FALSE, &spacing);
  priv->spacing = (int)(0.5 + spacing);

  /* update the label styling */
  st_button_update_label_style (button);

  /* run a transition if applicable */
  if (button_class->transition)
    {
      button_class->transition (button);
    }
}

static void
st_button_press (StButton             *button,
                 ClutterInputDevice   *device,
                 StButtonMask          mask,
                 ClutterEventSequence *sequence)
{
  StButtonPrivate *priv = st_button_get_instance_private (button);
  gboolean active_changed = priv->pressed == 0 || sequence;

  if (active_changed)
    st_widget_add_style_pseudo_class (ST_WIDGET (button), "active");

  priv->pressed |= mask;
  priv->press_sequence = sequence;
  priv->device = device;

  if (active_changed)
    g_object_notify_by_pspec (G_OBJECT (button), props[PROP_PRESSED]);
}

static void
st_button_release (StButton             *button,
                   ClutterInputDevice   *device,
                   StButtonMask          mask,
                   int                   clicked_button,
                   ClutterEventSequence *sequence)
{
  StButtonPrivate *priv = st_button_get_instance_private (button);

  if ((device && priv->device != device) ||
      (sequence && priv->press_sequence != sequence))
    return;
  else if (!sequence)
    {
      priv->pressed &= ~mask;

      if (priv->pressed != 0)
        return;
    }

  priv->press_sequence = NULL;
  priv->device = NULL;
  st_widget_remove_style_pseudo_class (ST_WIDGET (button), "active");
  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_PRESSED]);

  if (clicked_button || sequence)
    {
      if (priv->is_toggle)
        st_button_set_checked (button, !priv->is_checked);

      g_signal_emit (button, button_signals[CLICKED], 0, clicked_button);
    }
}

static gboolean
st_button_button_press (ClutterActor       *actor,
                        ClutterButtonEvent *event)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);
  StButtonMask mask = ST_BUTTON_MASK_FROM_BUTTON (event->button);
  ClutterInputDevice *device = clutter_event_get_device ((ClutterEvent*) event);

  if (priv->press_sequence)
    return CLUTTER_EVENT_PROPAGATE;

  if (priv->button_mask & mask)
    {
      if (priv->grabbed == 0)
        clutter_input_device_grab (device, actor);

      priv->grabbed |= mask;
      st_button_press (button, device, mask, NULL);

      return TRUE;
    }

  return FALSE;
}

static gboolean
st_button_button_release (ClutterActor       *actor,
                          ClutterButtonEvent *event)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);
  StButtonMask mask = ST_BUTTON_MASK_FROM_BUTTON (event->button);
  ClutterInputDevice *device = clutter_event_get_device ((ClutterEvent*) event);

  if (priv->button_mask & mask)
    {
      gboolean is_click;

      is_click = priv->grabbed && clutter_actor_contains (actor, event->source);
      st_button_release (button, device, mask, is_click ? event->button : 0, NULL);

      priv->grabbed &= ~mask;
      if (priv->grabbed == 0)
        clutter_input_device_ungrab (device);

      return TRUE;
    }

  return FALSE;
}

static gboolean
st_button_touch_event (ClutterActor      *actor,
                       ClutterTouchEvent *event)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);
  StButtonMask mask = ST_BUTTON_MASK_FROM_BUTTON (1);
  ClutterEventSequence *sequence;
  ClutterInputDevice *device;

  if (priv->pressed != 0)
    return CLUTTER_EVENT_PROPAGATE;
  if ((priv->button_mask & mask) == 0)
    return CLUTTER_EVENT_PROPAGATE;

  device = clutter_event_get_device ((ClutterEvent*) event);
  sequence = clutter_event_get_event_sequence ((ClutterEvent*) event);

  if (event->type == CLUTTER_TOUCH_BEGIN && !priv->press_sequence)
    {
      clutter_input_device_sequence_grab (device, sequence, actor);
      if (!clutter_event_is_pointer_emulated ((ClutterEvent*) event))
        st_button_press (button, device, 0, sequence);
      return CLUTTER_EVENT_STOP;
    }
  else if (event->type == CLUTTER_TOUCH_END &&
           priv->device == device &&
           priv->press_sequence == sequence)
    {
      if (!clutter_event_is_pointer_emulated ((ClutterEvent*) event))
        st_button_release (button, device, mask, 0, sequence);

      clutter_input_device_sequence_ungrab (device, sequence);
      return CLUTTER_EVENT_STOP;
    }
  else if (event->type == CLUTTER_TOUCH_CANCEL)
    {
      st_button_fake_release (button);
    }

  return CLUTTER_EVENT_PROPAGATE;
}

static gboolean
st_button_key_press (ClutterActor    *actor,
                     ClutterKeyEvent *event)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);

  if (priv->button_mask & ST_BUTTON_ONE)
    {
      if (event->keyval == CLUTTER_KEY_space ||
          event->keyval == CLUTTER_KEY_Return ||
          event->keyval == CLUTTER_KEY_KP_Enter ||
          event->keyval == CLUTTER_KEY_ISO_Enter)
        {
          st_button_press (button, NULL, ST_BUTTON_ONE, NULL);
          return TRUE;
        }
    }

  return CLUTTER_ACTOR_CLASS (st_button_parent_class)->key_press_event (actor, event);
}

static gboolean
st_button_key_release (ClutterActor    *actor,
                       ClutterKeyEvent *event)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);

  if (priv->button_mask & ST_BUTTON_ONE)
    {
      if (event->keyval == CLUTTER_KEY_space ||
          event->keyval == CLUTTER_KEY_Return ||
          event->keyval == CLUTTER_KEY_KP_Enter ||
          event->keyval == CLUTTER_KEY_ISO_Enter)
        {
          gboolean is_click;

          is_click = (priv->pressed & ST_BUTTON_ONE);
          st_button_release (button, NULL, ST_BUTTON_ONE, is_click ? 1 : 0, NULL);
          return TRUE;
        }
    }

  return FALSE;
}

static void
st_button_key_focus_out (ClutterActor *actor)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);

  /* If we lose focus between a key press and release, undo the press */
  if ((priv->pressed & ST_BUTTON_ONE) &&
      !(priv->grabbed & ST_BUTTON_ONE))
    st_button_release (button, NULL, ST_BUTTON_ONE, 0, NULL);

  CLUTTER_ACTOR_CLASS (st_button_parent_class)->key_focus_out (actor);
}

static gboolean
st_button_enter (ClutterActor         *actor,
                 ClutterCrossingEvent *event)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);
  gboolean ret;

  ret = CLUTTER_ACTOR_CLASS (st_button_parent_class)->enter_event (actor, event);

  if (priv->grabbed)
    {
      if (st_widget_get_hover (ST_WIDGET (button)))
        st_button_press (button,  priv->device,
                         priv->grabbed, NULL);
      else
        st_button_release (button, priv->device,
                           priv->grabbed, 0, NULL);
    }

  return ret;
}

static gboolean
st_button_leave (ClutterActor         *actor,
                 ClutterCrossingEvent *event)
{
  StButton *button = ST_BUTTON (actor);
  StButtonPrivate *priv = st_button_get_instance_private (button);
  gboolean ret;

  ret = CLUTTER_ACTOR_CLASS (st_button_parent_class)->leave_event (actor, event);

  if (priv->grabbed)
    {
      if (st_widget_get_hover (ST_WIDGET (button)))
        st_button_press (button, priv->device,
                         priv->grabbed, NULL);
      else
        st_button_release (button, priv->device,
                           priv->grabbed, 0, NULL);
    }

  return ret;
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
      g_value_set_boolean (value, priv->pressed != 0);
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

  actor_class->button_press_event = st_button_button_press;
  actor_class->button_release_event = st_button_button_release;
  actor_class->key_press_event = st_button_key_press;
  actor_class->key_release_event = st_button_key_release;
  actor_class->key_focus_out = st_button_key_focus_out;
  actor_class->enter_event = st_button_enter;
  actor_class->leave_event = st_button_leave;
  actor_class->touch_event = st_button_touch_event;

  widget_class->style_changed = st_button_style_changed;
  widget_class->get_accessible_type = st_button_accessible_get_type;

  props[PROP_LABEL] =
    g_param_spec_string ("label",
                         "Label",
                         "Label of the button",
                         NULL,
                         ST_PARAM_READWRITE);

  props[PROP_BUTTON_MASK] =
    g_param_spec_flags ("button-mask",
                        "Button mask",
                        "Which buttons trigger the 'clicked' signal",
                        ST_TYPE_BUTTON_MASK, ST_BUTTON_ONE,
                        ST_PARAM_READWRITE);

  props[PROP_TOGGLE_MODE] =
    g_param_spec_boolean ("toggle-mode",
                          "Toggle Mode",
                          "Enable or disable toggling",
                          FALSE,
                          ST_PARAM_READWRITE);

  props[PROP_CHECKED] =
    g_param_spec_boolean ("checked",
                          "Checked",
                          "Indicates if a toggle button is \"on\" or \"off\"",
                          FALSE,
                          ST_PARAM_READWRITE);

  props[PROP_PRESSED] =
    g_param_spec_boolean ("pressed",
                          "Pressed",
                          "Indicates if the button is pressed in",
                          FALSE,
                          ST_PARAM_READABLE);

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

  priv->spacing = 6;
  priv->button_mask = ST_BUTTON_ONE;

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
 * Get the text displayed on the button
 *
 * Returns: the text for the button. This must not be freed by the application
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

  priv = st_button_get_instance_private (button);

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
                            "use-markup", TRUE,
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
 * Enables or disables toggle mode for the button. In toggle mode, the active
 * state will be "toggled" when the user clicks the button.
 */
void
st_button_set_toggle_mode (StButton *button,
                           gboolean  toggle)
{
  StButtonPrivate *priv;

  g_return_if_fail (ST_IS_BUTTON (button));

  priv = st_button_get_instance_private (button);
  priv->is_toggle = toggle;

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_TOGGLE_MODE]);
}

/**
 * st_button_get_checked:
 * @button: a #StButton
 *
 * Get the state of the button that is in toggle mode.
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
 * Sets the pressed state of the button. This is only really useful if the
 * button has #toggle-mode mode set to %TRUE.
 */
void
st_button_set_checked (StButton *button,
                       gboolean  checked)
{
  StButtonPrivate *priv;

  g_return_if_fail (ST_IS_BUTTON (button));

  priv = st_button_get_instance_private (button);
  if (priv->is_checked != checked)
    {
      priv->is_checked = checked;

      if (checked)
        st_widget_add_style_pseudo_class (ST_WIDGET (button), "checked");
      else
        st_widget_remove_style_pseudo_class (ST_WIDGET (button), "checked");
    }

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_CHECKED]);
}

/**
 * st_button_fake_release:
 * @button: an #StButton
 *
 * If this widget is holding a pointer grab, this function will
 * will ungrab it, and reset the pressed state.  The effect is
 * similar to if the user had released the mouse button, but without
 * emitting the clicked signal.
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
  if (priv->device && priv->press_sequence)
    {
      clutter_input_device_sequence_ungrab (priv->device,
                                            priv->press_sequence);
    }
  else if (priv->grabbed)
    {
      priv->grabbed = 0;
      clutter_input_device_ungrab (priv->device);
    }

  if (priv->pressed || priv->press_sequence)
    st_button_release (button, priv->device,
                       priv->pressed, 0, NULL);
}

/******************************************************************************/
/*************************** ACCESSIBILITY SUPPORT ****************************/
/******************************************************************************/

#define ST_TYPE_BUTTON_ACCESSIBLE st_button_accessible_get_type ()

#define ST_BUTTON_ACCESSIBLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  ST_TYPE_BUTTON_ACCESSIBLE, StButtonAccessible))

#define ST_IS_BUTTON_ACCESSIBLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  ST_TYPE_BUTTON_ACCESSIBLE))

#define ST_BUTTON_ACCESSIBLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  ST_TYPE_BUTTON_ACCESSIBLE, StButtonAccessibleClass))

#define ST_IS_BUTTON_ACCESSIBLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  ST_TYPE_BUTTON_ACCESSIBLE))

#define ST_BUTTON_ACCESSIBLE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  ST_TYPE_BUTTON_ACCESSIBLE, StButtonAccessibleClass))

typedef struct _StButtonAccessible  StButtonAccessible;
typedef struct _StButtonAccessibleClass  StButtonAccessibleClass;

struct _StButtonAccessible
{
  StWidgetAccessible parent;
};

struct _StButtonAccessibleClass
{
  StWidgetAccessibleClass parent_class;
};

/* AtkObject */
static void          st_button_accessible_initialize (AtkObject *obj,
                                                      gpointer   data);

G_DEFINE_TYPE (StButtonAccessible, st_button_accessible, ST_TYPE_WIDGET_ACCESSIBLE)

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
