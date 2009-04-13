/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-alttab.h"
#include "shell-global.h"
#include "shell-wm.h"
#include <mutter-plugin.h>

/* Our MetaAltTabHandler implementation; ideally we would implement
 * this directly from JavaScript, but for now we can't. So we register
 * this glue class as our MetaAltTabHandler and then when mutter
 * creates one, we pass it on to ShellWM, which emits a signal to hand
 * it off to javascript code, which then connects to the signals on
 * this object.
 */

static void shell_alt_tab_handler_interface_init (MetaAltTabHandlerInterface *handler_iface);

G_DEFINE_TYPE_WITH_CODE (ShellAltTabHandler, shell_alt_tab_handler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (META_TYPE_ALT_TAB_HANDLER,
                                                shell_alt_tab_handler_interface_init))

/* Signals */
enum
{
  WINDOW_ADDED,
  SHOW,
  DESTROY,

  LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0 };

enum 
{
  PROP_SELECTED = 1,
  PROP_SCREEN,
  PROP_IMMEDIATE
};

static void
shell_alt_tab_handler_init (ShellAltTabHandler *sth)
{
  sth->windows = g_ptr_array_new ();
  sth->selected = -1;
}

static void
shell_alt_tab_handler_constructed (GObject *object)
{
  ShellGlobal *global = shell_global_get ();
  ShellWM *wm;

  g_object_get (G_OBJECT (global), "window-manager", &wm, NULL);
  _shell_wm_begin_alt_tab (wm, SHELL_ALT_TAB_HANDLER (object));
  g_object_unref (wm);
}

static void
shell_alt_tab_handler_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ShellAltTabHandler *sth = SHELL_ALT_TAB_HANDLER (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      /* We don't care */
      break;
    case PROP_IMMEDIATE:
      sth->immediate_mode = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_alt_tab_handler_get_property (GObject      *object,
                                    guint         prop_id,
                                    GValue       *value,
                                    GParamSpec   *pspec)
{
  ShellAltTabHandler *sth = SHELL_ALT_TAB_HANDLER (object);

  switch (prop_id)
    {
    case PROP_SELECTED:
      g_value_set_int (value, sth->selected);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_alt_tab_handler_finalize (GObject *object)
{
  ShellAltTabHandler *sth = SHELL_ALT_TAB_HANDLER (object);

  g_ptr_array_free (sth->windows, FALSE);

  G_OBJECT_CLASS (shell_alt_tab_handler_parent_class)->finalize (object);
}

static void
shell_alt_tab_handler_add_window (MetaAltTabHandler *handler,
				  MetaWindow        *window)
{
  ShellAltTabHandler *sth = SHELL_ALT_TAB_HANDLER (handler);

  g_ptr_array_add (sth->windows, window);
  g_signal_emit (handler, signals[WINDOW_ADDED], 0,
                 meta_window_get_compositor_private (window));
}

static void
shell_alt_tab_handler_show (MetaAltTabHandler *handler,
                            MetaWindow        *initial_selection)
{
  ShellAltTabHandler *sth = SHELL_ALT_TAB_HANDLER (handler);
  int i;

  sth->selected = -1;
  for (i = 0; i < sth->windows->len; i++)
    {
      if (sth->windows->pdata[i] == (gpointer)initial_selection)
        {
          sth->selected = i;
          break;
        }
    }

  g_signal_emit (handler, signals[SHOW], 0, sth->selected);
}

static void
shell_alt_tab_handler_destroy (MetaAltTabHandler *handler)
{
  g_signal_emit (handler, signals[DESTROY], 0);
}

static void
shell_alt_tab_handler_forward (MetaAltTabHandler *handler)
{
  ShellAltTabHandler *sth = SHELL_ALT_TAB_HANDLER (handler);

  sth->selected = (sth->selected + 1) % sth->windows->len;
  g_object_notify (G_OBJECT (handler), "selected");
}

static void
shell_alt_tab_handler_backward (MetaAltTabHandler *handler)
{
  ShellAltTabHandler *sth = SHELL_ALT_TAB_HANDLER (handler);

  sth->selected = (sth->selected - 1) % sth->windows->len;
  g_object_notify (G_OBJECT (handler), "selected");
}

static MetaWindow *
shell_alt_tab_handler_get_selected (MetaAltTabHandler *handler)
{
  ShellAltTabHandler *sth = SHELL_ALT_TAB_HANDLER (handler);

  if (sth->selected > -1)
    return sth->windows->pdata[sth->selected];
  else
    return NULL;
}

static void
shell_alt_tab_handler_class_init (ShellAltTabHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = shell_alt_tab_handler_constructed;
  object_class->set_property = shell_alt_tab_handler_set_property;
  object_class->get_property = shell_alt_tab_handler_get_property;
  object_class->finalize     = shell_alt_tab_handler_finalize;

  g_object_class_override_property (object_class, PROP_SCREEN, "screen");
  g_object_class_override_property (object_class, PROP_IMMEDIATE, "immediate");
  g_object_class_install_property (object_class,
                                   PROP_SELECTED,
                                   g_param_spec_int ("selected",
                                                     "Selected",
                                                     "Selected window",
                                                     -1, G_MAXINT, -1,
                                                     G_PARAM_READABLE));


  signals[WINDOW_ADDED] = g_signal_new ("window-added",
                                        G_TYPE_FROM_CLASS (klass),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE, 1,
                                        MUTTER_TYPE_COMP_WINDOW);
  signals[SHOW] = g_signal_new ("show",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                0,
                                NULL, NULL,
                                g_cclosure_marshal_VOID__INT,
                                G_TYPE_NONE, 1,
                                G_TYPE_INT);
  signals[DESTROY] = g_signal_new ("destroy",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL,
                                   g_cclosure_marshal_VOID__VOID,
                                   G_TYPE_NONE, 0);
}

static void
shell_alt_tab_handler_interface_init (MetaAltTabHandlerInterface *handler_iface)
{
  handler_iface->add_window   = shell_alt_tab_handler_add_window;
  handler_iface->show         = shell_alt_tab_handler_show;
  handler_iface->destroy      = shell_alt_tab_handler_destroy;
  handler_iface->forward      = shell_alt_tab_handler_forward;
  handler_iface->backward     = shell_alt_tab_handler_backward;
  handler_iface->get_selected = shell_alt_tab_handler_get_selected;
}
