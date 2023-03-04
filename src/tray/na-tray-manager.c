/* na-tray-manager.c
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003-2006 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Used to be: eggtraymanager.c
 */

#include <config.h>
#include <string.h>
#include <libintl.h>

#include "na-tray-manager.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

/* Signals */
enum
{
  TRAY_ICON_ADDED,
  TRAY_ICON_REMOVED,
  MESSAGE_SENT,
  MESSAGE_CANCELLED,
  LOST_SELECTION,
  LAST_SIGNAL
};

static guint manager_signals[LAST_SIGNAL];

enum
{
  PROP_0,
  PROP_X11_DISPLAY,
  N_PROPS,
};

static GParamSpec *props[N_PROPS];

typedef struct
{
  long id, len;
  long remaining_len;

  long timeout;
  char *str;
  Window window;
} PendingMessage;

struct _NaTrayManager
{
  GObject parent_instance;

  MetaX11Display *x11_display;

  Atom selection_atom;
  Atom opcode_atom;
  Atom message_data_atom;

  Window window;
  ClutterColor fg;
  ClutterColor error;
  ClutterColor warning;
  ClutterColor success;

  unsigned int event_func_id;

  GList *messages;
  GHashTable *children;
};

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

static void na_tray_manager_finalize     (GObject      *object);
static void na_tray_manager_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec);
static void na_tray_manager_get_property (GObject      *object,
					  guint         prop_id,
					  GValue       *value,
					  GParamSpec   *pspec);

static void na_tray_manager_unmanage (NaTrayManager *manager);

G_DEFINE_TYPE (NaTrayManager, na_tray_manager, G_TYPE_OBJECT)

static void
na_tray_manager_init (NaTrayManager *manager)
{
  manager->window = None;
  manager->children = g_hash_table_new (NULL, NULL);

  manager->fg.red = 0;
  manager->fg.green = 0;
  manager->fg.blue = 0;

  manager->error.red = 0xff;
  manager->error.green = 0;
  manager->error.blue = 0;

  manager->warning.red = 0xff;
  manager->warning.green = 0xff;
  manager->warning.blue = 0;

  manager->success.red = 0;
  manager->success.green = 0xff;
  manager->success.blue = 0;
}

static void
na_tray_manager_class_init (NaTrayManagerClass *klass)
{
  GObjectClass *gobject_class;
  
  gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = na_tray_manager_finalize;
  gobject_class->set_property = na_tray_manager_set_property;
  gobject_class->get_property = na_tray_manager_get_property;

  manager_signals[TRAY_ICON_ADDED] =
    g_signal_new ("tray_icon_added",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  NA_TYPE_TRAY_CHILD);

  manager_signals[TRAY_ICON_REMOVED] =
    g_signal_new ("tray_icon_removed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  NA_TYPE_TRAY_CHILD);
  manager_signals[MESSAGE_SENT] =
    g_signal_new ("message_sent",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 4,
                  NA_TYPE_TRAY_CHILD,
                  G_TYPE_STRING,
                  G_TYPE_LONG,
                  G_TYPE_LONG);
  manager_signals[MESSAGE_CANCELLED] =
    g_signal_new ("message_cancelled",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  NA_TYPE_TRAY_CHILD,
                  G_TYPE_LONG);
  manager_signals[LOST_SELECTION] =
    g_signal_new ("lost_selection",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  props[PROP_X11_DISPLAY] =
    g_param_spec_object ("x11-display",
                         "x11-display",
                         "x11-display",
                         META_TYPE_X11_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, N_PROPS, props);
}

static void
na_tray_manager_finalize (GObject *object)
{
  NaTrayManager *manager;

  manager = NA_TRAY_MANAGER (object);

  na_tray_manager_unmanage (manager);

  g_list_free (manager->messages);
  g_hash_table_destroy (manager->children);

  G_OBJECT_CLASS (na_tray_manager_parent_class)->finalize (object);
}

static void
na_tray_manager_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  NaTrayManager *manager = NA_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_X11_DISPLAY:
      manager->x11_display = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
na_tray_manager_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  NaTrayManager *manager = NA_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_X11_DISPLAY:
      g_value_set_object (value, manager->x11_display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

NaTrayManager *
na_tray_manager_new (MetaX11Display *x11_display)
{
  return g_object_new (NA_TYPE_TRAY_MANAGER,
                       "x11-display", x11_display,
                       NULL);
}

static gboolean
na_tray_manager_plug_removed (NaTrayChild   *tray_child,
                              NaTrayManager *manager)
{
  Window icon_window;

  icon_window = na_xembed_get_plug_window (NA_XEMBED (tray_child));

  g_hash_table_remove (manager->children,
                       GINT_TO_POINTER (icon_window));
  g_signal_emit (manager, manager_signals[TRAY_ICON_REMOVED], 0, tray_child);

  /* This destroys the socket. */
  return FALSE;
}

static void
na_tray_manager_handle_dock_request (NaTrayManager       *manager,
                                     XClientMessageEvent *xevent)
{
  Window icon_window = xevent->data.l[2];
  NaTrayChild *child;

  if (g_hash_table_lookup (manager->children,
                           GINT_TO_POINTER (icon_window)))
    {
      /* We already got this notification earlier, ignore this one */
      return;
    }

  child = na_tray_child_new (manager->x11_display, icon_window);
  if (child == NULL) /* already gone or other error */
    return;

  g_signal_emit (manager, manager_signals[TRAY_ICON_ADDED], 0,
                 child);

  g_signal_connect (child, "plug-removed",
                    G_CALLBACK (na_tray_manager_plug_removed), manager);

  na_xembed_add_id (NA_XEMBED (child), icon_window);

  if (!na_xembed_get_plug_window (NA_XEMBED (child)))
    {
      /* Embedding failed, we won't get a plug-removed signal */
      /* This signal destroys the tray child */
      g_signal_emit (manager, manager_signals[TRAY_ICON_REMOVED], 0, child);
      return;
    }

  g_hash_table_insert (manager->children,
                       GINT_TO_POINTER (icon_window), child);
}

static void
pending_message_free (PendingMessage *message)
{
  g_free (message->str);
  g_free (message);
}

static void
na_tray_manager_handle_message_data (NaTrayManager       *manager,
                                     XClientMessageEvent *xevent)
{
  GList *p;
  int len;

  /* Try to see if we can find the pending message in the list */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *msg = p->data;

      if (xevent->window == msg->window)
	{
	  /* Append the message */
	  len = MIN (msg->remaining_len, 20);

	  memcpy ((msg->str + msg->len - msg->remaining_len),
		  &xevent->data, len);
	  msg->remaining_len -= len;

          if (msg->remaining_len == 0)
            {
              NaTrayChild *child;

              child = g_hash_table_lookup (manager->children,
                                           GINT_TO_POINTER (msg->window));

              if (child)
                {
                  g_signal_emit (manager, manager_signals[MESSAGE_SENT], 0,
                                 child, msg->str, msg->id, msg->timeout);
                }

              pending_message_free (msg);
              manager->messages = g_list_remove_link (manager->messages, p);
              g_list_free_1 (p);
            }

          break;
	}
    }
}

static void
na_tray_manager_handle_begin_message (NaTrayManager       *manager,
                                      XClientMessageEvent *xevent)
{
  NaTrayChild *child;
  GList *p;
  PendingMessage *msg;
  long timeout, len, id;

  child = g_hash_table_lookup (manager->children,
                               GINT_TO_POINTER (xevent->window));
  /* we don't know about this tray icon, so ignore the message */
  if (!child)
    return;

  timeout = xevent->data.l[2];
  len = xevent->data.l[3];
  id = xevent->data.l[4];

  /* Check if the same message is already in the queue and remove it if so */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *pmsg = p->data;

      if (xevent->window == pmsg->window &&
	  id == pmsg->id)
	{
	  /* Hmm, we found it, now remove it */
	  pending_message_free (pmsg);
	  manager->messages = g_list_remove_link (manager->messages, p);
          g_list_free_1 (p);
	  break;
	}
    }

  if (len == 0)
    {
      g_signal_emit (manager, manager_signals[MESSAGE_SENT], 0,
                     child, "", id, timeout);
    }
  else
    {
      /* Now add the new message to the queue */
      msg = g_new0 (PendingMessage, 1);
      msg->window = xevent->window;
      msg->timeout = timeout;
      msg->len = len;
      msg->id = id;
      msg->remaining_len = msg->len;
      msg->str = g_malloc (msg->len + 1);
      msg->str[msg->len] = '\0';
      manager->messages = g_list_prepend (manager->messages, msg);
    }
}

static void
na_tray_manager_handle_cancel_message (NaTrayManager       *manager,
                                       XClientMessageEvent *xevent)
{
  NaTrayChild *child;
  GList *p;
  long id;

  id = xevent->data.l[2];

  /* Check if the message is in the queue and remove it if so */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *msg = p->data;

      if (xevent->window == msg->window &&
	  id == msg->id)
	{
	  pending_message_free (msg);
	  manager->messages = g_list_remove_link (manager->messages, p);
          g_list_free_1 (p);
	  break;
	}
    }

  child = g_hash_table_lookup (manager->children,
                               GINT_TO_POINTER (xevent->window));

  if (child)
    {
      g_signal_emit (manager, manager_signals[MESSAGE_CANCELLED], 0,
                     child, xevent->data.l[2]);
    }
}

static void
na_tray_manager_event_func (MetaX11Display *x11_display,
                            XEvent         *xevent,
                            gpointer        data)
{
  NaTrayManager *manager = data;

  if (xevent->type == ClientMessage &&
      xevent->xany.window == manager->window)
    {
      /* _NET_SYSTEM_TRAY_OPCODE: SYSTEM_TRAY_REQUEST_DOCK */
      if (xevent->xclient.message_type == manager->opcode_atom &&
          xevent->xclient.data.l[1] == SYSTEM_TRAY_REQUEST_DOCK)
        {
          na_tray_manager_handle_dock_request (manager,
                                               (XClientMessageEvent *) xevent);
        }
      /* _NET_SYSTEM_TRAY_OPCODE: SYSTEM_TRAY_BEGIN_MESSAGE */
      else if (xevent->xclient.message_type == manager->opcode_atom &&
               xevent->xclient.data.l[1] == SYSTEM_TRAY_BEGIN_MESSAGE)
        {
          na_tray_manager_handle_begin_message (manager,
                                                (XClientMessageEvent *) xevent);
        }
      /* _NET_SYSTEM_TRAY_OPCODE: SYSTEM_TRAY_CANCEL_MESSAGE */
      else if (xevent->xclient.message_type == manager->opcode_atom &&
               xevent->xclient.data.l[1] == SYSTEM_TRAY_CANCEL_MESSAGE)
        {
          na_tray_manager_handle_cancel_message (manager,
                                                 (XClientMessageEvent *) xevent);
        }
      /* _NET_SYSTEM_TRAY_MESSAGE_DATA */
      else if (xevent->xclient.message_type == manager->message_data_atom)
        {
          na_tray_manager_handle_message_data (manager,
                                               (XClientMessageEvent *) xevent);
        }
    }
  else if (xevent->type == SelectionClear &&
           xevent->xany.window == manager->window)
    {
      g_signal_emit (manager, manager_signals[LOST_SELECTION], 0);
      na_tray_manager_unmanage (manager);
    }
}

static void
na_tray_manager_unmanage (NaTrayManager *manager)
{
  Display *xdisplay;
  NaTrayChild *child;
  GHashTableIter iter;

  if (manager->window == None)
    return;

  xdisplay = meta_x11_display_get_xdisplay (manager->x11_display);

  if (XGetSelectionOwner (xdisplay, manager->selection_atom) == manager->window)
    {
      XSetSelectionOwner (xdisplay,
                          manager->selection_atom,
                          None,
                          CurrentTime);
    }

  meta_x11_display_remove_event_func (manager->x11_display,
                                      manager->event_func_id);
  manager->event_func_id = 0;

  XDestroyWindow (xdisplay, manager->window);
  manager->window = None;

  g_hash_table_iter_init (&iter, manager->children);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &child))
    {
      g_signal_handlers_disconnect_by_func (child,
                                            na_tray_manager_plug_removed,
                                            manager);
      g_hash_table_iter_remove (&iter);
      g_object_unref (child);
    }
}

static void
na_tray_manager_set_visual_property (NaTrayManager *manager)
{
  Display *xdisplay;
  Atom visual_atom;
  XVisualInfo xvisual_info;
  gulong data[1];
  int result;

  g_return_if_fail (manager->window != None);

  /* The visual property is a hint to the tray icons as to what visual they
   * should use for their windows. If the X server has RGBA colormaps, then
   * we tell the tray icons to use a RGBA colormap and we'll composite the
   * icon onto its parents with real transparency. Otherwise, we just tell
   * the icon to use our colormap, and we'll do some hacks with parent
   * relative backgrounds to simulate transparency.
   */
  xdisplay = meta_x11_display_get_xdisplay (manager->x11_display);
  visual_atom = XInternAtom (xdisplay, "_NET_SYSTEM_TRAY_VISUAL", False);

  result = XMatchVisualInfo (xdisplay, DefaultScreen (xdisplay), 32, TrueColor, &xvisual_info);

  if (result == Success)
    data[0] = xvisual_info.visualid;
  else
    data[0] = XVisualIDFromVisual (DefaultVisual (xdisplay, DefaultScreen (xdisplay)));

  XChangeProperty (xdisplay,
                   manager->window,
                   visual_atom,
                   XA_VISUALID, 32,
                   PropModeReplace,
                   (guchar *) &data, 1);
}

static void
na_tray_manager_set_colors_property (NaTrayManager *manager)
{
  Display *xdisplay;
  Atom atom;
  gulong data[12];

  g_return_if_fail (manager->window != None);

  xdisplay = meta_x11_display_get_xdisplay (manager->x11_display);
  atom = XInternAtom (xdisplay,  "_NET_SYSTEM_TRAY_COLORS", False);

  data[0] = manager->fg.red * 0x101;
  data[1] = manager->fg.green * 0x101;
  data[2] = manager->fg.blue * 0x101;
  data[3] = manager->error.red * 0x101;
  data[4] = manager->error.green * 0x101;
  data[5] = manager->error.blue * 0x101;
  data[6] = manager->warning.red * 0x101;
  data[7] = manager->warning.green * 0x101;
  data[8] = manager->warning.blue * 0x101;
  data[9] = manager->success.red * 0x101;
  data[10] = manager->success.green * 0x101;
  data[11] = manager->success.blue * 0x101;

  XChangeProperty (xdisplay,
                   manager->window,
                   atom,
                   XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &data, 12);
}

gboolean
na_tray_manager_manage (NaTrayManager *manager)
{
  Display *xdisplay;

  g_return_val_if_fail (NA_IS_TRAY_MANAGER (manager), FALSE);

  xdisplay = meta_x11_display_get_xdisplay (manager->x11_display);

  meta_x11_error_trap_push (manager->x11_display);
  manager->window = XCreateSimpleWindow (xdisplay,
                                         XDefaultRootWindow (xdisplay),
                                         0, 0, 1, 1,
                                         0, 0, 0);
  XSelectInput (xdisplay, manager->window,
                StructureNotifyMask | PropertyChangeMask);

  if (meta_x11_error_trap_pop_with_return (manager->x11_display) ||
      !manager->window)
    return FALSE;

  manager->selection_atom = XInternAtom (xdisplay, "_NET_SYSTEM_TRAY_S0", False);

  na_tray_manager_set_visual_property (manager);
  na_tray_manager_set_colors_property (manager);

  meta_x11_error_trap_push (manager->x11_display);

  XSetSelectionOwner (xdisplay, manager->selection_atom,
		      manager->window, CurrentTime);

  /* Check if we could set the selection owner successfully */
  if (!meta_x11_error_trap_pop_with_return (manager->x11_display))
    {
      XClientMessageEvent xev;

      xev.type = ClientMessage;
      xev.window = XDefaultRootWindow (xdisplay);
      xev.message_type = XInternAtom (xdisplay, "MANAGER", False);

      xev.format = 32;
      xev.data.l[0] = CurrentTime;
      xev.data.l[1] = manager->selection_atom;
      xev.data.l[2] = manager->window;
      xev.data.l[3] = 0; /* manager specific data */
      xev.data.l[4] = 0; /* manager specific data */

      XSendEvent (xdisplay,
                  XDefaultRootWindow (xdisplay),
                  False, StructureNotifyMask, (XEvent *)&xev);

      manager->opcode_atom =
        XInternAtom (xdisplay, "_NET_SYSTEM_TRAY_OPCODE", False);
      manager->message_data_atom =
        XInternAtom (xdisplay, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);

      /* Add an event filter */
      manager->event_func_id =
        meta_x11_display_add_event_func (manager->x11_display,
                                         na_tray_manager_event_func,
                                         manager,
                                         NULL);
      return TRUE;
    }
  else
    {
      XDestroyWindow (xdisplay, manager->window);
      manager->window = None;

      return FALSE;
    }
}

void
na_tray_manager_set_colors (NaTrayManager *manager,
                            ClutterColor  *fg,
                            ClutterColor  *error,
                            ClutterColor  *warning,
                            ClutterColor  *success)
{
  g_return_if_fail (NA_IS_TRAY_MANAGER (manager));

  if (!clutter_color_equal (&manager->fg, fg) ||
      !clutter_color_equal (&manager->error, error) ||
      !clutter_color_equal (&manager->warning, warning) ||
      !clutter_color_equal (&manager->success, success))
    {
      manager->fg = *fg;
      manager->error = *error;
      manager->warning = *warning;
      manager->success = *success;

      na_tray_manager_set_colors_property (manager);
    }
}
