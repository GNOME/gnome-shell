/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-menu
 * @short_description: A box which acts like a popup menu
 *
 * A #BigBox subclass which adds methods and signals useful for implementing
 * popup-menu like actors.
 */

#include "config.h"

#include "shell-menu.h"

G_DEFINE_TYPE(ShellMenu, shell_menu, BIG_TYPE_BOX);

struct _ShellMenuPrivate {
  gboolean popped_up;
  gboolean have_grab;
  guint activating_button;

  gboolean released_on_source;
  ClutterActor *source_actor;

  ClutterActor *selected;
};

/* Signals */
enum
{
  UNSELECTED,
  SELECTED,
  ACTIVATE,
  CANCELLED,
  LAST_SIGNAL
};

static guint shell_menu_signals [LAST_SIGNAL] = { 0 };

static gboolean
container_contains (ClutterContainer *container,
                    ClutterActor     *actor)
{
  while (actor != NULL && actor != (ClutterActor*)container)
    {
      actor = clutter_actor_get_parent (actor);
    }
  return actor != NULL;
}

static void
shell_menu_popdown_nosignal (ShellMenu *box)
{
  box->priv->popped_up = FALSE;
  if (box->priv->have_grab)
    clutter_ungrab_pointer ();
  clutter_actor_hide (CLUTTER_ACTOR (box));
}

static void
on_selected_destroy (ClutterActor  *actor,
                     ShellMenu     *box)
{
  box->priv->selected = NULL;
}

static void
set_selected (ShellMenu      *box,
              ClutterActor   *actor)
{
  if (actor == box->priv->selected)
    return;
  if (box->priv->selected)
    {
      g_signal_handlers_disconnect_by_func (box->priv->selected, G_CALLBACK(on_selected_destroy), box);
      g_signal_emit (G_OBJECT (box), shell_menu_signals[UNSELECTED], 0, box->priv->selected);
    }
  box->priv->selected = actor;
  if (box->priv->selected)
    {
      g_signal_connect (box->priv->selected, "destroy", G_CALLBACK(on_selected_destroy), box);
      g_signal_emit (G_OBJECT (box), shell_menu_signals[SELECTED], 0, box->priv->selected);
    }
}

static gboolean
shell_menu_enter_event (ClutterActor         *actor,
                        ClutterCrossingEvent *event)
{
  ShellMenu *box = SHELL_MENU (actor);

  if (!container_contains (CLUTTER_CONTAINER (box), event->source))
    return TRUE;

  if (event->source == (ClutterActor*)box)
    return TRUE;

  if (g_object_get_data (G_OBJECT (event->source), "shell-is-separator"))
    return TRUE;

  set_selected (box, event->source);

  return TRUE;
}

static gboolean
shell_menu_leave_event (ClutterActor         *actor,
                        ClutterCrossingEvent *event)
{
  ShellMenu *box = SHELL_MENU (actor);

  set_selected (box, NULL);

  return TRUE;
}

static gboolean
shell_menu_button_release_event (ClutterActor       *actor,
                                 ClutterButtonEvent *event)
{
  ShellMenu *box = SHELL_MENU (actor);

  /* Until the user releases the button that brought up the menu, we just
   * ignore other button press/releass.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=596371
   */
  if (box->priv->activating_button > 0 && box->priv->activating_button != event->button)
    return FALSE;

  box->priv->activating_button = 0;

  if (box->priv->source_actor && !box->priv->released_on_source)
    {
      if (box->priv->source_actor == event->source ||
          (CLUTTER_IS_CONTAINER (box->priv->source_actor) &&
           container_contains (CLUTTER_CONTAINER (box->priv->source_actor), event->source)))
        {
          /* On the next release, we want to pop down the menu regardless */
          box->priv->released_on_source = TRUE;
          return TRUE;
        }
    }

  shell_menu_popdown_nosignal (box);

  if (!container_contains (CLUTTER_CONTAINER (box), event->source) ||
      box->priv->selected == NULL)
    {
      g_signal_emit (G_OBJECT (box), shell_menu_signals[CANCELLED], 0);
      return FALSE;
    }

  g_signal_emit (G_OBJECT (box), shell_menu_signals[ACTIVATE], 0, box->priv->selected);

  return TRUE;
}

void
shell_menu_popup (ShellMenu         *box,
                  guint              button,
                  guint32            activate_time)
{
  if (box->priv->popped_up)
    return;
  box->priv->activating_button = button;
  box->priv->popped_up = TRUE;
  box->priv->have_grab = TRUE;
  box->priv->released_on_source = FALSE;
  clutter_grab_pointer (CLUTTER_ACTOR (box));
}

/**
 * shell_menu_popdown:
 * @box:
 *
 * If the menu is currently active, hide it, emitting the 'cancelled' signal.
 */
void
shell_menu_popdown (ShellMenu *box)
{
  if (!box->priv->popped_up)
    return;
  shell_menu_popdown_nosignal (box);
  g_signal_emit (G_OBJECT (box), shell_menu_signals[CANCELLED], 0);
}

static void
on_source_destroyed (ClutterActor *actor,
                     ShellMenu    *box)
{
  box->priv->source_actor = NULL;
}

/**
 * shell_menu_set_persistent_source:
 * @box:
 * @source: Actor to use as menu origin
 *
 * This function changes the menu behavior on button release.  Normally
 * when the mouse is released anywhere, the menu "pops down"; when this
 * function is called, if the mouse is released over the source actor,
 * the menu stays.
 *
 * The given @source actor must be reactive for this function to work.
 */
void
shell_menu_set_persistent_source (ShellMenu    *box,
                                  ClutterActor *source)
{
  if (box->priv->source_actor)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (box->priv->source_actor),
                                            G_CALLBACK (on_source_destroyed),
                                            box);
    }
  box->priv->source_actor = source;
  if (box->priv->source_actor)
    {
      g_signal_connect (G_OBJECT (box->priv->source_actor),
                        "destroy",
                        G_CALLBACK (on_source_destroyed),
                        box);
    }
}

/**
 * shell_menu_append_separator:
 * @box:
 * @separator: An actor which functions as a menu separator
 * @flags: Packing flags
 *
 * Actors added to the menu with default functions are treated like
 * menu items; this function will add an actor that should instead
 * be treated like a menu separator.  The current practical effect
 * is that the separators will not be selectable.
 */
void
shell_menu_append_separator (ShellMenu         *box,
                             ClutterActor      *separator,
                             BigBoxPackFlags    flags)
{
  g_object_set_data (G_OBJECT (separator), "shell-is-separator", GUINT_TO_POINTER(TRUE));
  big_box_append (BIG_BOX (box), separator, flags);
}

static void
shell_menu_dispose (GObject *gobject)
{
  ShellMenu *self = SHELL_MENU (gobject);

  shell_menu_set_persistent_source (self, NULL);

  G_OBJECT_CLASS (shell_menu_parent_class)->dispose (gobject);
}

static void
shell_menu_class_init (ShellMenuClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->dispose = shell_menu_dispose;

  actor_class->enter_event = shell_menu_enter_event;
  actor_class->leave_event = shell_menu_leave_event;
  actor_class->button_release_event = shell_menu_button_release_event;

  /**
   * ShellMenu::unselected
   * @box: The #ShellMenu
   * @actor: The previously hovered-over menu item
   *
   * This signal is emitted when a menu item transitions to
   * an unselected state.
   */
  shell_menu_signals[UNSELECTED] =
    g_signal_new ("unselected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, CLUTTER_TYPE_ACTOR);

  /**
   * ShellMenu::selected
   * @box: The #ShellMenu
   * @actor: The hovered-over menu item
   *
   * This signal is emitted when a menu item is in a selected state.
   */
  shell_menu_signals[SELECTED] =
    g_signal_new ("selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, CLUTTER_TYPE_ACTOR);

  /**
   * ShellMenu::activate
   * @box: The #ShellMenu
   * @actor: The clicked menu item
   *
   * This signal is emitted when a menu item is selected.
   */
  shell_menu_signals[ACTIVATE] =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, CLUTTER_TYPE_ACTOR);

  /**
   * ShellMenu::cancelled
   * @box: The #ShellMenu
   *
   * This signal is emitted when the menu is closed without an option selected.
   */
  shell_menu_signals[CANCELLED] =
    g_signal_new ("cancelled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (ShellMenuPrivate));
}

static void
shell_menu_init (ShellMenu *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, SHELL_TYPE_MENU,
                                            ShellMenuPrivate);
}
