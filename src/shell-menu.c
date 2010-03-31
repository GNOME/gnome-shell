/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-menu
 * @short_description: A box which acts like a popup menu
 *
 * A #StBoxLayout subclass which adds methods and signals useful for
 * implementing popup-menu like actors.
 */

#include "config.h"

#include "shell-menu.h"

G_DEFINE_TYPE (ShellMenu, shell_menu, ST_TYPE_BOX_LAYOUT);

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
  ClutterActor *container_actor = CLUTTER_ACTOR (container);

  while (actor != NULL && actor != container_actor)
    {
      actor = clutter_actor_get_parent (actor);
    }
  return actor != NULL;
}

static void
shell_menu_popdown_nosignal (ShellMenu *menu)
{
  menu->priv->popped_up = FALSE;
  if (menu->priv->have_grab)
    clutter_ungrab_pointer ();
  clutter_actor_hide (CLUTTER_ACTOR (menu));
}

static void
on_selected_destroy (ClutterActor  *actor,
                     ShellMenu     *menu)
{
  menu->priv->selected = NULL;
}

static void
set_selected (ShellMenu      *menu,
              ClutterActor   *actor)
{
  if (actor == menu->priv->selected)
    return;
  if (menu->priv->selected)
    {
      g_signal_handlers_disconnect_by_func (menu->priv->selected,
                                            G_CALLBACK (on_selected_destroy),
                                            menu);
      g_signal_emit (G_OBJECT (menu), shell_menu_signals[UNSELECTED], 0,
                     menu->priv->selected);
    }
  menu->priv->selected = actor;
  if (menu->priv->selected)
    {
      g_signal_connect (menu->priv->selected, "destroy",
                        G_CALLBACK (on_selected_destroy), menu);
      g_signal_emit (G_OBJECT (menu), shell_menu_signals[SELECTED], 0,
                     menu->priv->selected);
    }
}

static gboolean
shell_menu_enter_event (ClutterActor         *actor,
                        ClutterCrossingEvent *event)
{
  ShellMenu *menu = SHELL_MENU (actor);

  if (container_contains (CLUTTER_CONTAINER (menu), event->source) &&
      event->source != CLUTTER_ACTOR (menu))
    set_selected (menu, event->source);

  return CLUTTER_ACTOR_CLASS (shell_menu_parent_class)->enter_event (actor, event);
}

static gboolean
shell_menu_leave_event (ClutterActor         *actor,
                        ClutterCrossingEvent *event)
{
  ShellMenu *menu = SHELL_MENU (actor);

  set_selected (menu, NULL);

  return CLUTTER_ACTOR_CLASS (shell_menu_parent_class)->leave_event (actor, event);
}

static gboolean
shell_menu_button_release_event (ClutterActor       *actor,
                                 ClutterButtonEvent *event)
{
  ShellMenu *menu = SHELL_MENU (actor);

  /* Until the user releases the button that brought up the menu, we just
   * ignore other button press/releass.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=596371
   */
  if (menu->priv->activating_button > 0 &&
      menu->priv->activating_button != event->button)
    return FALSE;

  menu->priv->activating_button = 0;

  if (menu->priv->source_actor && !menu->priv->released_on_source)
    {
      if (menu->priv->source_actor == event->source ||
          (CLUTTER_IS_CONTAINER (menu->priv->source_actor) &&
           container_contains (CLUTTER_CONTAINER (menu->priv->source_actor), event->source)))
        {
          /* On the next release, we want to pop down the menu regardless */
          menu->priv->released_on_source = TRUE;
          return TRUE;
        }
    }

  shell_menu_popdown_nosignal (menu);

  if (!container_contains (CLUTTER_CONTAINER (menu), event->source) ||
      menu->priv->selected == NULL)
    {
      g_signal_emit (G_OBJECT (menu), shell_menu_signals[CANCELLED], 0);
      return FALSE;
    }

  g_signal_emit (G_OBJECT (menu), shell_menu_signals[ACTIVATE], 0,
                 menu->priv->selected); 

  return TRUE;
}

void
shell_menu_popup (ShellMenu         *menu,
                  guint              button,
                  guint32            activate_time)
{
  if (menu->priv->popped_up)
    return;
  menu->priv->activating_button = button;
  menu->priv->popped_up = TRUE;
  menu->priv->have_grab = TRUE;
  menu->priv->released_on_source = FALSE;
  clutter_grab_pointer (CLUTTER_ACTOR (menu));
}

/**
 * shell_menu_popdown:
 * @menu:
 *
 * If the menu is currently active, hide it, emitting the 'cancelled' signal.
 */
void
shell_menu_popdown (ShellMenu *menu)
{
  if (!menu->priv->popped_up)
    return;
  shell_menu_popdown_nosignal (menu);
  g_signal_emit (G_OBJECT (menu), shell_menu_signals[CANCELLED], 0);
}

static void
on_source_destroyed (ClutterActor *actor,
                     ShellMenu    *menu)
{
  menu->priv->source_actor = NULL;
}

/**
 * shell_menu_set_persistent_source:
 * @menu:
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
shell_menu_set_persistent_source (ShellMenu    *menu,
                                  ClutterActor *source)
{
  if (menu->priv->source_actor)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (menu->priv->source_actor),
                                            G_CALLBACK (on_source_destroyed),
                                            menu);
    }
  menu->priv->source_actor = source;
  if (menu->priv->source_actor)
    {
      g_signal_connect (G_OBJECT (menu->priv->source_actor),
                        "destroy",
                        G_CALLBACK (on_source_destroyed),
                        menu);
    }
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
   * @menu: The #ShellMenu
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
   * @menu: The #ShellMenu
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
   * @menu: The #ShellMenu
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
   * @menu: The #ShellMenu
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
