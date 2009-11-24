/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <string.h>

#include "shell-wm.h"
#include "shell-global.h"
#include "shell-marshal.h"

#include <keybindings.h>

struct _ShellWM {
  GObject parent;

  MutterPlugin *plugin;
  GList *switch_workspace_actors;
};

/* Signals */
enum
{
  MINIMIZE,
  KILL_MINIMIZE,
  MAXIMIZE,
  KILL_MAXIMIZE,
  UNMAXIMIZE,
  KILL_UNMAXIMIZE,
  MAP,
  KILL_MAP,
  DESTROY,
  KILL_DESTROY,
  SWITCH_WORKSPACE,
  KILL_SWITCH_WORKSPACE,

  KEYBINDING,

  LAST_SIGNAL
};

G_DEFINE_TYPE(ShellWM, shell_wm, G_TYPE_OBJECT);

static void shell_wm_set_switch_workspace_actors (ShellWM *wm,
                                                  GList   *actors);

static guint shell_wm_signals [LAST_SIGNAL] = { 0 };

static void
shell_wm_init (ShellWM *wm)
{
}

static void
shell_wm_finalize (GObject *object)
{
  ShellWM *wm = SHELL_WM (object);

  shell_wm_set_switch_workspace_actors (wm, NULL);

  G_OBJECT_CLASS (shell_wm_parent_class)->finalize (object);
}

static void
shell_wm_class_init (ShellWMClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shell_wm_finalize;

  shell_wm_signals[MINIMIZE] =
    g_signal_new ("minimize",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  MUTTER_TYPE_COMP_WINDOW);
  shell_wm_signals[KILL_MINIMIZE] =
    g_signal_new ("kill-minimize",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  MUTTER_TYPE_COMP_WINDOW);
  shell_wm_signals[MAXIMIZE] =
    g_signal_new ("maximize",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _shell_marshal_VOID__OBJECT_INT_INT_INT_INT,
                  G_TYPE_NONE, 5,
                  MUTTER_TYPE_COMP_WINDOW, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
  shell_wm_signals[KILL_MAXIMIZE] =
    g_signal_new ("kill-maximize",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  MUTTER_TYPE_COMP_WINDOW);
  shell_wm_signals[UNMAXIMIZE] =
    g_signal_new ("unmaximize",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _shell_marshal_VOID__OBJECT_INT_INT_INT_INT,
                  G_TYPE_NONE, 1,
                  MUTTER_TYPE_COMP_WINDOW, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
  shell_wm_signals[KILL_UNMAXIMIZE] =
    g_signal_new ("kill-unmaximize",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  MUTTER_TYPE_COMP_WINDOW);
  shell_wm_signals[MAP] =
    g_signal_new ("map",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  MUTTER_TYPE_COMP_WINDOW);
  shell_wm_signals[KILL_MAP] =
    g_signal_new ("kill-map",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  MUTTER_TYPE_COMP_WINDOW);
  shell_wm_signals[DESTROY] =
    g_signal_new ("destroy",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  MUTTER_TYPE_COMP_WINDOW);
  shell_wm_signals[KILL_DESTROY] =
    g_signal_new ("kill-destroy",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  MUTTER_TYPE_COMP_WINDOW);
  shell_wm_signals[SWITCH_WORKSPACE] =
    g_signal_new ("switch-workspace",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  _shell_marshal_VOID__INT_INT_INT,
		  G_TYPE_NONE, 3,
                  G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
  shell_wm_signals[KILL_SWITCH_WORKSPACE] =
    g_signal_new ("kill-switch-workspace",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * ShellWM::keybinding:
   * @shellwm: the #ShellWM
   * @binding: the keybinding name
   * @window: for window keybindings, the #MetaWindow
   * @backwards: for "reversible" keybindings, whether or not
   * the backwards (Shifted) variant was invoked
   *
   * Emitted when a keybinding captured via
   * shell_wm_takeover_keybinding() is invoked. The keybinding name
   * (which has underscores, not hyphens) is also included as the
   * detail of the signal name, so you can connect just specific
   * keybindings.
   */
  shell_wm_signals[KEYBINDING] =
    g_signal_new ("keybinding",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		  0,
		  NULL, NULL,
		  _shell_marshal_VOID__STRING_OBJECT_BOOLEAN,
		  G_TYPE_NONE, 3,
                  G_TYPE_STRING,
                  META_TYPE_WINDOW,
                  G_TYPE_BOOLEAN);
}

void
_shell_wm_switch_workspace (ShellWM      *wm,
                            const GList **actors,
                            gint          from,
                            gint          to,
                            MetaMotionDirection direction)
{
  shell_wm_set_switch_workspace_actors (wm, (GList *)*actors);
  g_signal_emit (wm, shell_wm_signals[SWITCH_WORKSPACE], 0,
                 from, to, direction);
}

/**
 * shell_wm_get_switch_workspace_actors:
 * @wm: the #ShellWM
 *
 * A workaround for a missing feature in gobject-introspection. Returns
 * the list of windows involved in a switch-workspace operation (which
 * cannot be passed directly to the signal handler because there's no
 * way to annotate the element-type of a signal parameter.)
 *
 * Return value: (element-type MutterWindow) (transfer full): the list
 * of windows
 **/
GList *
shell_wm_get_switch_workspace_actors (ShellWM *wm)
{
  GList *l;

  for (l = wm->switch_workspace_actors; l; l = l->next)
    g_object_ref (l->data);
  return g_list_copy (wm->switch_workspace_actors);
}

static void
shell_wm_set_switch_workspace_actors (ShellWM *wm, GList *actors)
{
  const GList *l;

  for (l = wm->switch_workspace_actors; l; l = l->next)
    g_object_unref (l->data);
  g_list_free (wm->switch_workspace_actors);
  wm->switch_workspace_actors = g_list_copy (actors);
  for (l = wm->switch_workspace_actors; l; l = l->next)
    g_object_ref (l->data);
}

/**
 * shell_wm_completed_switch_workspace:
 * @wm: the ShellWM
 *
 * The plugin must call this when it has finished switching the
 * workspace.
 **/
void
shell_wm_completed_switch_workspace (ShellWM *wm)
{
  g_return_if_fail (wm->switch_workspace_actors != NULL);

  /* mutter_plugin_effect_completed() requires us to pass a window,
   * though it doesn't matter *which* window in this case.
   */
  mutter_plugin_effect_completed (wm->plugin,
                                  wm->switch_workspace_actors->data,
                                  MUTTER_PLUGIN_SWITCH_WORKSPACE);
  shell_wm_set_switch_workspace_actors (wm, NULL);
}

/**
 * shell_wm_completed_minimize
 * @wm: the ShellWM
 * @actor: the MutterWindow actor
 *
 * The plugin must call this when it has completed a window minimize effect.
 **/
void
shell_wm_completed_minimize (ShellWM      *wm,
                             MutterWindow *actor)
{
  mutter_plugin_effect_completed (wm->plugin,
                                  actor,
                                  MUTTER_PLUGIN_MINIMIZE);
}

/**
 * shell_wm_completed_maximize
 * @wm: the ShellWM
 * @actor: the MutterWindow actor
 *
 * The plugin must call this when it has completed a window maximize effect.
 **/
void
shell_wm_completed_maximize (ShellWM      *wm,
                             MutterWindow *actor)
{
  mutter_plugin_effect_completed (wm->plugin,
                                  actor,
                                  MUTTER_PLUGIN_MAXIMIZE);
}

/**
 * shell_wm_completed_unmaximize
 * @wm: the ShellWM
 * @actor: the MutterWindow actor
 *
 * The plugin must call this when it has completed a window unmaximize effect.
 **/
void
shell_wm_completed_unmaximize (ShellWM      *wm,
                               MutterWindow *actor)
{
  mutter_plugin_effect_completed (wm->plugin,
                                  actor,
                                  MUTTER_PLUGIN_UNMAXIMIZE);
}

/**
 * shell_wm_completed_map
 * @wm: the ShellWM
 * @actor: the MutterWindow actor
 *
 * The plugin must call this when it has completed a window map effect.
 **/
void
shell_wm_completed_map (ShellWM      *wm,
                        MutterWindow *actor)
{
  mutter_plugin_effect_completed (wm->plugin,
                                  actor,
                                  MUTTER_PLUGIN_MAP);
}

/**
 * shell_wm_completed_destroy
 * @wm: the ShellWM
 * @actor: the MutterWindow actor
 *
 * The plugin must call this when it has completed a window destroy effect.
 **/
void
shell_wm_completed_destroy (ShellWM      *wm,
                            MutterWindow *actor)
{
  mutter_plugin_effect_completed (wm->plugin,
                                  actor,
                                  MUTTER_PLUGIN_DESTROY);
}

void
_shell_wm_kill_effect (ShellWM      *wm,
                       MutterWindow *actor,
                       gulong        events)
{
  if (events & MUTTER_PLUGIN_MINIMIZE)
    g_signal_emit (wm, shell_wm_signals[KILL_MINIMIZE], 0, actor);
  if (events & MUTTER_PLUGIN_MAXIMIZE)
    g_signal_emit (wm, shell_wm_signals[KILL_MAXIMIZE], 0, actor);
  if (events & MUTTER_PLUGIN_UNMAXIMIZE)
    g_signal_emit (wm, shell_wm_signals[KILL_UNMAXIMIZE], 0, actor);
  if (events & MUTTER_PLUGIN_MAP)
    g_signal_emit (wm, shell_wm_signals[KILL_MAP], 0, actor);
  if (events & MUTTER_PLUGIN_DESTROY)
    g_signal_emit (wm, shell_wm_signals[KILL_DESTROY], 0, actor);
  if (events & MUTTER_PLUGIN_SWITCH_WORKSPACE)
    g_signal_emit (wm, shell_wm_signals[KILL_SWITCH_WORKSPACE], 0);
}


void
_shell_wm_minimize (ShellWM      *wm,
                    MutterWindow *actor)
{
  g_signal_emit (wm, shell_wm_signals[MINIMIZE], 0, actor);
}

void
_shell_wm_maximize (ShellWM      *wm,
                    MutterWindow *actor,
                    int           target_x,
                    int           target_y,
                    int           target_width,
                    int           target_height)
{
  g_signal_emit (wm, shell_wm_signals[MAXIMIZE], 0, actor, target_x, target_y, target_width, target_height);
}

void
_shell_wm_unmaximize (ShellWM      *wm,
                      MutterWindow *actor,
                      int           target_x,
                      int           target_y,
                      int           target_width,
                      int           target_height)
{
  g_signal_emit (wm, shell_wm_signals[UNMAXIMIZE], 0, actor, target_x, target_y, target_width, target_height);
}

void
_shell_wm_map (ShellWM      *wm,
               MutterWindow *actor)
{
  g_signal_emit (wm, shell_wm_signals[MAP], 0, actor);
}

void
_shell_wm_destroy (ShellWM      *wm,
                   MutterWindow *actor)
{
  g_signal_emit (wm, shell_wm_signals[DESTROY], 0, actor);
}

/**
 * shell_wm_new:
 * @plugin: the #MutterPlugin
 *
 * Creates a new window management interface by hooking into @plugin.
 *
 * Return value: the new window-management interface
 **/
ShellWM *
shell_wm_new (MutterPlugin *plugin)
{
  ShellWM *wm;

  wm = g_object_new (SHELL_TYPE_WM, NULL);
  wm->plugin = plugin;

  return wm;
}

static void
shell_wm_key_handler (MetaDisplay    *display,
                      MetaScreen     *screen,
                      MetaWindow     *window,
                      XEvent         *event,
                      MetaKeyBinding *binding,
                      gpointer        data)
{
  ShellWM *wm = data;
  gboolean backwards = (event->xkey.state & ShiftMask);

  g_signal_emit (wm, shell_wm_signals[KEYBINDING],
                 g_quark_from_string (binding->name),
                 binding->name, window, backwards);
}

/**
 * shell_wm_takeover_keybinding:
 * @wm: the #ShellWM
 * @binding_name: a mutter keybinding name
 *
 * Tells mutter to forward keypresses for @binding_name to the shell
 * rather than processing them internally. This will cause a
 * #ShellWM::keybinding signal to be emitted when that key is pressed.
 */
void
shell_wm_takeover_keybinding (ShellWM      *wm,
                              const char   *binding_name)
{
  meta_keybindings_set_custom_handler (binding_name,
                                       shell_wm_key_handler,
                                       wm, NULL);
}
