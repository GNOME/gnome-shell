/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-button-box
 * @short_description: A box with properties useful for implementing buttons
 *
 * A #BigBox subclass which translates lower-level Clutter button events
 * into higher level properties which are useful for implementing "button-like"
 * actors.
 */

#include "shell-button-box.h"

G_DEFINE_TYPE(ShellButtonBox, shell_button_box, BIG_TYPE_BOX);

struct _ShellButtonBoxPrivate {
  gboolean active;
  gboolean held;
  gboolean hover;
  gboolean pressed;
};

/* Signals */
enum
{
  ACTIVATE,
  LAST_SIGNAL
};

enum {
  PROP_0,

  PROP_ACTIVE,
  PROP_HOVER,
  PROP_PRESSED,
};

static guint shell_button_box_signals [LAST_SIGNAL] = { 0 };

static void
set_active (ShellButtonBox  *box,
            gboolean         active)
{
  if (box->priv->active == active)
    return;
  box->priv->active = active;
  g_object_notify (G_OBJECT (box), "active");
}

static void
set_hover (ShellButtonBox  *box,
           gboolean         hover)
{
  if (box->priv->hover == hover)
    return;
  box->priv->hover = hover;
  g_object_notify (G_OBJECT (box), "hover");
}

static void
set_pressed (ShellButtonBox  *box,
             gboolean         pressed)
{
  if (box->priv->pressed == pressed)
    return;
  box->priv->pressed = pressed;
  g_object_notify (G_OBJECT (box), "pressed");
}

static gboolean
shell_button_box_contains (ShellButtonBox     *box,
                           ClutterActor       *actor)
{
  while (actor != NULL && actor != (ClutterActor*)box)
    {
      actor = clutter_actor_get_parent (actor);
    }
  return actor != NULL;
}

static gboolean
shell_button_box_enter_event (ClutterActor         *actor,
                              ClutterCrossingEvent *event)
{
  ShellButtonBox *box = SHELL_BUTTON_BOX (actor);

  if (shell_button_box_contains (box, event->related))
    return TRUE;
  if (!shell_button_box_contains (box, event->source))
    return TRUE;

  g_object_freeze_notify (G_OBJECT (actor));

  if (box->priv->held)
    set_pressed (box, TRUE);
  set_hover (box, TRUE);

  g_object_thaw_notify (G_OBJECT (actor));

  return TRUE;
}

static gboolean
shell_button_box_leave_event (ClutterActor         *actor,
                              ClutterCrossingEvent *event)
{
  ShellButtonBox *box = SHELL_BUTTON_BOX (actor);

  if (shell_button_box_contains (box, event->related))
    return TRUE;

  set_hover (box, FALSE);
  set_pressed (box, FALSE);

  return TRUE;
}

static gboolean
shell_button_box_button_press_event (ClutterActor       *actor,
                                     ClutterButtonEvent *event)
{
  ShellButtonBox *box = SHELL_BUTTON_BOX (actor);

  if (event->button != 1 || event->click_count != 1)
    return FALSE;

  if (box->priv->held)
    return TRUE;

  if (!shell_button_box_contains (box, event->source))
    return FALSE;

  box->priv->held = TRUE;
  clutter_grab_pointer (CLUTTER_ACTOR (box));

  set_pressed (box, TRUE);

  return TRUE;
}

static gboolean
shell_button_box_button_release_event (ClutterActor       *actor,
                                       ClutterButtonEvent *event)
{
  ShellButtonBox *box = SHELL_BUTTON_BOX (actor);

  if (event->button != 1 || event->click_count != 1)
    return FALSE;

  if (!box->priv->held)
    return TRUE;

  box->priv->held = FALSE;
  clutter_ungrab_pointer ();

  if (!shell_button_box_contains (box, event->source))
    return FALSE;

  set_pressed (box, FALSE);

  g_signal_emit (G_OBJECT (box), shell_button_box_signals[ACTIVATE], 0);

  return TRUE;
}

/**
 * shell_button_box_fake_release:
 * @box:
 *
 * If this button box is holding a pointer grab, this function will
 * will ungrab it, and reset the pressed state.  The effect is
 * similar to if the user had released the mouse button, but without
 * emitting the activate signal.
 *
 * This function is useful if for example you want to do something after the user
 * is holding the mouse button for a given period of time, breaking the
 * grab.
 */
void
shell_button_box_fake_release (ShellButtonBox *box)
{
  if (!box->priv->held)
    return;

  box->priv->held = FALSE;
  clutter_ungrab_pointer ();

  set_pressed (box, FALSE);
}

static void
shell_button_box_set_property(GObject         *object,
                              guint            prop_id,
                              const GValue    *value,
                              GParamSpec      *pspec)
{
  ShellButtonBox *box = SHELL_BUTTON_BOX (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      set_active (box, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_button_box_get_property(GObject         *object,
                              guint            prop_id,
                              GValue          *value,
                              GParamSpec      *pspec)
{
  ShellButtonBox *box = SHELL_BUTTON_BOX (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, box->priv->active);
      break;
    case PROP_PRESSED:
      g_value_set_boolean (value, box->priv->pressed);
      break;
    case PROP_HOVER:
      g_value_set_boolean (value, box->priv->hover);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_button_box_class_init (ShellButtonBoxClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->get_property = shell_button_box_get_property;
  gobject_class->set_property = shell_button_box_set_property;

  actor_class->enter_event = shell_button_box_enter_event;
  actor_class->leave_event = shell_button_box_leave_event;
  actor_class->button_press_event = shell_button_box_button_press_event;
  actor_class->button_release_event = shell_button_box_button_release_event;

  /**
   * ShellButtonBox::activate
   * @box: The #ShellButtonBox
   *
   * This signal is emitted when the button should take the action
   * associated with button click+release.
   */
  shell_button_box_signals[ACTIVATE] =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ShellButtonBox:active
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
   * ShellButtonBox:hover
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
   * ShellButtonBox:pressed
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

  g_type_class_add_private (gobject_class, sizeof (ShellButtonBoxPrivate));
}

static void
shell_button_box_init (ShellButtonBox *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, SHELL_TYPE_BUTTON_BOX,
                                            ShellButtonBoxPrivate);
}
