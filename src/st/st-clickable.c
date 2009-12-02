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

G_DEFINE_TYPE (StClickable, st_clickable, ST_TYPE_BIN);

struct _StClickablePrivate {
  gboolean active;
  gboolean held;
  gboolean hover;
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
  PROP_HOVER,
  PROP_PRESSED,
};

static guint st_clickable_signals [LAST_SIGNAL] = { 0 };

static void
sync_pseudo_class (StClickable *self)
{
  st_widget_set_style_pseudo_class (ST_WIDGET (self),
                                    (self->priv->pressed || self->priv->active) ? "pressed" :
                                      (self->priv->hover ? "hover" : NULL));
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
set_hover (StClickable  *self,
           gboolean      hover)
{
  if (self->priv->hover == hover)
    return;
  self->priv->hover = hover;
  sync_pseudo_class (self);
  g_object_notify (G_OBJECT (self), "hover");
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
st_clickable_contains (StClickable     *self,
                       ClutterActor    *actor)
{
  while (actor != NULL && actor != (ClutterActor*)self)
    {
      actor = clutter_actor_get_parent (actor);
    }
  return actor != NULL;
}

static gboolean
st_clickable_enter_event (ClutterActor         *actor,
                          ClutterCrossingEvent *event)
{
  StClickable *self = ST_CLICKABLE (actor);

  if (st_clickable_contains (self, event->related))
    return TRUE;
  if (!st_clickable_contains (self, event->source))
    return TRUE;

  g_object_freeze_notify (G_OBJECT (actor));

  if (self->priv->held)
    set_pressed (self, TRUE);
  set_hover (self, TRUE);

  g_object_thaw_notify (G_OBJECT (actor));

  return TRUE;
}

static gboolean
st_clickable_leave_event (ClutterActor         *actor,
                              ClutterCrossingEvent *event)
{
  StClickable *self = ST_CLICKABLE (actor);

  if (st_clickable_contains (self, event->related))
    return TRUE;

  set_hover (self, FALSE);
  set_pressed (self, FALSE);

  return TRUE;
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

  if (!st_clickable_contains (self, event->source))
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

  if (!st_clickable_contains (self, event->source))
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
    case PROP_PRESSED:
      g_value_set_boolean (value, self->priv->pressed);
      break;
    case PROP_HOVER:
      g_value_set_boolean (value, self->priv->hover);
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
   * StClickable:hover
   *
   * This property tracks whether the mouse is over the button; note this
   * state is independent of whether the button is pressed.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HOVER,
                                   g_param_spec_boolean ("hover",
                                                         "Hovering state",
                                                         "Whether the mouse is over the button",
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
}
