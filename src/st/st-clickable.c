/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:st-clickable
 * @short_description: A bin with methods and properties useful for implementing buttons
 *
 * A #StBin subclass which translates lower-level Clutter button events
 * into higher level properties which are useful for implementing "button-like"
 * actors.
 */

#include "st-clickable.h"

#include "st-private.h"

G_DEFINE_TYPE (StClickable, st_clickable, ST_TYPE_BIN);

struct _StClickablePrivate {
  gboolean active;
  gboolean held;
  gboolean pressed;

  guint initiating_button;
};

/* Signals */
enum
{
  CLICKED,
  LAST_SIGNAL
};

enum {
  PROP_0,

  PROP_ACTIVE,
  PROP_HELD,
  PROP_PRESSED
};

static guint st_clickable_signals [LAST_SIGNAL] = { 0 };

static void
sync_pseudo_class (StClickable *self)
{
  if (self->priv->pressed || self->priv->active)
    st_widget_add_style_pseudo_class (ST_WIDGET (self), "pressed");
  else
    st_widget_remove_style_pseudo_class (ST_WIDGET (self), "pressed");
}

static void
set_active (StClickable  *self,
            gboolean      active)
{
  if (self->priv->active == active)
    return;
  self->priv->active = active;
  sync_pseudo_class (self);
  g_object_notify (G_OBJECT (self), "active");
}

static void
set_pressed (StClickable  *self,
             gboolean      pressed)
{
  if (self->priv->pressed == pressed)
    return;
  self->priv->pressed = pressed;
  sync_pseudo_class (self);
  g_object_notify (G_OBJECT (self), "pressed");
}

static gboolean
st_clickable_enter_event (ClutterActor         *actor,
                          ClutterCrossingEvent *event)
{
  StClickable *self = ST_CLICKABLE (actor);
  gboolean result;

  g_object_freeze_notify (G_OBJECT (actor));

  result = CLUTTER_ACTOR_CLASS (st_clickable_parent_class)->enter_event (actor, event);

  /* We can't just assume get_hover() is TRUE; see st_widget_enter(). */
  set_pressed (self, self->priv->held && st_widget_get_hover (ST_WIDGET (actor)));

  g_object_thaw_notify (G_OBJECT (actor));

  return result;
}

static gboolean
st_clickable_leave_event (ClutterActor         *actor,
                              ClutterCrossingEvent *event)
{
  StClickable *self = ST_CLICKABLE (actor);
  gboolean result;

  g_object_freeze_notify (G_OBJECT (actor));

  result = CLUTTER_ACTOR_CLASS (st_clickable_parent_class)->leave_event (actor, event);

  /* As above, we can't just assume get_hover() is FALSE. */
  set_pressed (self, self->priv->held && st_widget_get_hover (ST_WIDGET (actor)));

  g_object_thaw_notify (G_OBJECT (actor));

  return result;
}

static gboolean
st_clickable_button_press_event (ClutterActor       *actor,
                                 ClutterButtonEvent *event)
{
  StClickable *self = ST_CLICKABLE (actor);

  if (event->click_count != 1)
    return FALSE;

  if (self->priv->held)
    return TRUE;

  if (!clutter_actor_contains (actor, event->source))
    return FALSE;

  self->priv->held = TRUE;
  self->priv->initiating_button = event->button;
  clutter_grab_pointer (CLUTTER_ACTOR (self));

  set_pressed (self, TRUE);

  return TRUE;
}

static gboolean
st_clickable_button_release_event (ClutterActor       *actor,
                                   ClutterButtonEvent *event)
{
  StClickable *self = ST_CLICKABLE (actor);

  if (event->button != self->priv->initiating_button || event->click_count != 1)
    return FALSE;

  if (!self->priv->held)
    return TRUE;

  self->priv->held = FALSE;
  clutter_ungrab_pointer ();

  if (!clutter_actor_contains (actor, event->source))
    return FALSE;

  set_pressed (self, FALSE);

  g_signal_emit (G_OBJECT (self), st_clickable_signals[CLICKED], 0, event);

  return TRUE;
}

static gboolean
st_clickable_key_press_event (ClutterActor    *actor,
                              ClutterKeyEvent *event)
{
  StClickable *self = ST_CLICKABLE (actor);

  if (event->keyval == CLUTTER_KEY_space ||
      event->keyval == CLUTTER_KEY_Return)
    {
      set_pressed (self, TRUE);
      return TRUE;
    }
  return FALSE;
}

static gboolean
st_clickable_key_release_event (ClutterActor    *actor,
                                ClutterKeyEvent *event)
{
  StClickable *self = ST_CLICKABLE (actor);

  if (event->keyval != CLUTTER_KEY_space &&
      event->keyval != CLUTTER_KEY_Return)
    return FALSE;

  set_pressed (self, FALSE);

  g_signal_emit (G_OBJECT (self), st_clickable_signals[CLICKED], 0, event);
  return TRUE;
}

/**
 * st_clickable_fake_release:
 * @box:
 *
 * If this widget is holding a pointer grab, this function will
 * will ungrab it, and reset the pressed state.  The effect is
 * similar to if the user had released the mouse button, but without
 * emitting the clicked signal.
 *
 * This function is useful if for example you want to do something after the user
 * is holding the mouse button for a given period of time, breaking the
 * grab.
 */
void
st_clickable_fake_release (StClickable *self)
{
  if (!self->priv->held)
    return;

  self->priv->held = FALSE;
  clutter_ungrab_pointer ();

  set_pressed (self, FALSE);
}

static void
st_clickable_set_property (GObject         *object,
                           guint            prop_id,
                           const GValue    *value,
                           GParamSpec      *pspec)
{
  StClickable *self = ST_CLICKABLE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      set_active (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
st_clickable_get_property (GObject         *object,
                           guint            prop_id,
                           GValue          *value,
                           GParamSpec      *pspec)
{
  StClickable *self = ST_CLICKABLE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->priv->active);
      break;
    case PROP_HELD:
      g_value_set_boolean (value, self->priv->held);
      break;
    case PROP_PRESSED:
      g_value_set_boolean (value, self->priv->pressed);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
st_clickable_class_init (StClickableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->get_property = st_clickable_get_property;
  gobject_class->set_property = st_clickable_set_property;

  actor_class->enter_event = st_clickable_enter_event;
  actor_class->leave_event = st_clickable_leave_event;
  actor_class->button_press_event = st_clickable_button_press_event;
  actor_class->button_release_event = st_clickable_button_release_event;
  actor_class->key_press_event = st_clickable_key_press_event;
  actor_class->key_release_event = st_clickable_key_release_event;

  /**
   * StClickable::clicked
   * @box: The #StClickable
   *
   * This signal is emitted when the button should take the action
   * associated with button click+release.
   */
  st_clickable_signals[CLICKED] =
    g_signal_new ("clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, CLUTTER_TYPE_EVENT);

  /**
   * StClickable:active
   *
   * The property allows the button to be used as a "toggle button"; it's up to the
   * application to update the active property in response to the activate signal;
   * it doesn't happen automatically.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         "Active",
                                                         "Whether the button persistently active",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * StClickable:held
   *
   * This property tracks whether the button has the pointer grabbed,
   * whether or not the pointer is currently hovering over the button.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HELD,
                                   g_param_spec_boolean ("held",
                                                         "Held state",
                                                         "Whether the mouse button is currently pressed",
                                                         FALSE,
                                                         G_PARAM_READABLE));

  /**
   * StClickable:pressed
   *
   * This property tracks whether the button should have a "pressed in"
   * effect.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PRESSED,
                                   g_param_spec_boolean ("pressed",
                                                         "Pressed state",
                                                         "Whether the button is currently pressed",
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_type_class_add_private (gobject_class, sizeof (StClickablePrivate));
}

static void
st_clickable_init (StClickable *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ST_TYPE_CLICKABLE,
                                            StClickablePrivate);
  st_widget_set_track_hover (ST_WIDGET (self), TRUE);
}
