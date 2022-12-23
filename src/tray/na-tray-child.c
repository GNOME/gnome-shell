/* na-tray-child.c
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003-2006 Vincent Untz
 * Copyright (C) 2008 Red Hat, Inc.
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
 */

#include <config.h>
#include <string.h>

#include "na-tray-child.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

struct _NaTrayChild
{
  NaXembed parent_instance;
  guint parent_relative_bg : 1;
};

G_DEFINE_TYPE (NaTrayChild, na_tray_child, NA_TYPE_XEMBED)

static void
na_tray_child_init (NaTrayChild *child)
{
}

static void
na_tray_child_class_init (NaTrayChildClass *klass)
{
}

NaTrayChild *
na_tray_child_new (MetaX11Display *x11_display,
                   Window          icon_window)
{
  XWindowAttributes window_attributes;
  Display *xdisplay;
  NaTrayChild *child;
  int result;

  g_return_val_if_fail (META_IS_X11_DISPLAY (x11_display), NULL);
  g_return_val_if_fail (icon_window != None, NULL);

  xdisplay = meta_x11_display_get_xdisplay (x11_display);

  /* We need to determine the visual of the window we are embedding and create
   * the socket in the same visual.
   */

  meta_x11_error_trap_push (x11_display);
  result = XGetWindowAttributes (xdisplay, icon_window,
                                 &window_attributes);
  meta_x11_error_trap_pop (x11_display);

  if (!result) /* Window already gone */
    return NULL;

  child = g_object_new (NA_TYPE_TRAY_CHILD,
                        "x11-display", x11_display,
                        NULL);

  return child;
}

char *
na_tray_child_get_title (NaTrayChild *child)
{
  char *retval = NULL;
  MetaX11Display *x11_display;
  Display *xdisplay;
  Atom utf8_string, atom, type;
  int result;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *val;

  g_return_val_if_fail (NA_IS_TRAY_CHILD (child), NULL);

  x11_display = na_xembed_get_x11_display (NA_XEMBED (child));
  xdisplay = meta_x11_display_get_xdisplay (x11_display);

  utf8_string = XInternAtom (xdisplay, "UTF8_STRING", False);
  atom = XInternAtom (xdisplay, "_NET_WM_NAME", False);

  meta_x11_error_trap_push (x11_display);

  result = XGetWindowProperty (xdisplay,
                               na_xembed_get_plug_window (NA_XEMBED (child)),
                               atom,
                               0, G_MAXLONG,
                               False, utf8_string,
                               &type, &format, &nitems,
                               &bytes_after, (guchar **)&val);

  if (meta_x11_error_trap_pop_with_return (x11_display) || result != Success)
    return NULL;

  if (type != utf8_string ||
      format != 8 ||
      nitems == 0)
    {
      if (val)
        XFree (val);
      return NULL;
    }

  if (!g_utf8_validate (val, nitems, NULL))
    {
      XFree (val);
      return NULL;
    }

  retval = g_strndup (val, nitems);

  XFree (val);

  return retval;
}

/* from libwnck/xutils.c, comes as LGPLv2+ */
static char *
latin1_to_utf8 (const char *latin1)
{
  GString *str;
  const char *p;

  str = g_string_new (NULL);

  p = latin1;
  while (*p)
    {
      g_string_append_unichar (str, (gunichar) *p);
      ++p;
    }

  return g_string_free (str, FALSE);
}

/* derived from libwnck/xutils.c, comes as LGPLv2+ */
static void
_get_wmclass (MetaX11Display  *x11_display,
              Window           xwindow,
              char           **res_class,
              char           **res_name)
{
  XClassHint ch;
  Display *xdisplay;

  ch.res_name = NULL;
  ch.res_class = NULL;

  xdisplay = meta_x11_display_get_xdisplay (x11_display);

  meta_x11_error_trap_push (x11_display);
  XGetClassHint (xdisplay, xwindow, &ch);
  meta_x11_error_trap_pop (x11_display);

  if (res_class)
    *res_class = NULL;

  if (res_name)
    *res_name = NULL;

  if (ch.res_name)
    {
      if (res_name)
        *res_name = latin1_to_utf8 (ch.res_name);

      XFree (ch.res_name);
    }

  if (ch.res_class)
    {
      if (res_class)
        *res_class = latin1_to_utf8 (ch.res_class);

      XFree (ch.res_class);
    }
}

/**
 * na_tray_child_get_wm_class;
 * @child: a #NaTrayChild
 * @res_name: return location for a string containing the application name of
 * @child, or %NULL
 * @res_class: return location for a string containing the application class of
 * @child, or %NULL
 *
 * Fetches the resource associated with @child.
 */
void
na_tray_child_get_wm_class (NaTrayChild  *child,
                            char        **res_name,
                            char        **res_class)
{
  MetaX11Display *x11_display;

  g_return_if_fail (NA_IS_TRAY_CHILD (child));

  x11_display = na_xembed_get_x11_display (NA_XEMBED (child));

  _get_wmclass (x11_display,
                na_xembed_get_plug_window (NA_XEMBED (child)),
                res_class,
                res_name);
}

pid_t
na_tray_child_get_pid (NaTrayChild *child)
{
  MetaX11Display *x11_display;
  Display *xdisplay;
  Atom type;
  int result, format;
  gulong nitems, bytes_after, *val = NULL;
  pid_t pid = 0;

  x11_display = na_xembed_get_x11_display (NA_XEMBED (child));
  xdisplay = meta_x11_display_get_xdisplay (x11_display);

  xdisplay = meta_x11_display_get_xdisplay (x11_display);
  meta_x11_error_trap_push (x11_display);
  result = XGetWindowProperty (xdisplay,
                               na_xembed_get_plug_window (NA_XEMBED (child)),
                               XInternAtom (xdisplay, "_NET_WM_PID", False),
                               0, G_MAXLONG, False, XA_CARDINAL,
                               &type, &format, &nitems,
                               &bytes_after, (guchar **)&val);

  if (!meta_x11_error_trap_pop_with_return (x11_display) &&
      result == Success &&
      type == XA_CARDINAL &&
      nitems == 1)
    pid = *val;

  if (val)
    XFree (val);

  return pid;
}

void
na_tray_child_emulate_event (NaTrayChild *tray_child,
                             ClutterEvent *event)
{
  MetaX11Display *x11_display;
  XKeyEvent xkevent;
  XButtonEvent xbevent;
  XCrossingEvent xcevent;
  Display *xdisplay;
  Window xwindow, xrootwindow;
  ClutterEventType event_type = clutter_event_type (event);
  int width, height;

  g_return_if_fail (event_type == CLUTTER_BUTTON_RELEASE ||
                    event_type == CLUTTER_KEY_PRESS ||
                    event_type == CLUTTER_KEY_RELEASE);

  x11_display = na_xembed_get_x11_display (NA_XEMBED (tray_child));

  xwindow = na_xembed_get_plug_window (NA_XEMBED (tray_child));
  if (xwindow == None)
    {
      g_warning ("shell tray: plug window is gone");
      return;
    }

  na_xembed_get_size (NA_XEMBED (tray_child), &width, &height);

  meta_x11_error_trap_push (x11_display);

  xdisplay = meta_x11_display_get_xdisplay (x11_display);
  xrootwindow = XDefaultRootWindow (xdisplay);

  /* First make the icon believe the pointer is inside it */
  xcevent.type = EnterNotify;
  xcevent.window = xwindow;
  xcevent.root = xrootwindow;
  xcevent.subwindow = None;
  xcevent.time = clutter_event_get_time (event);
  xcevent.x = width / 2;
  xcevent.y = height / 2;
  xcevent.x_root = xcevent.x;
  xcevent.y_root = xcevent.y;
  xcevent.mode = NotifyNormal;
  xcevent.detail = NotifyNonlinear;
  xcevent.same_screen = True;
  XSendEvent (xdisplay, xwindow, False, 0, (XEvent *)&xcevent);

  /* Now do the click */
  if (event_type == CLUTTER_BUTTON_RELEASE)
    {
      xbevent.window = xwindow;
      xbevent.root = xrootwindow;
      xbevent.subwindow = None;
      xbevent.time = xcevent.time;
      xbevent.x = xcevent.x;
      xbevent.y = xcevent.y;
      xbevent.x_root = xcevent.x_root;
      xbevent.y_root = xcevent.y_root;
      xbevent.state = clutter_event_get_state (event);
      xbevent.same_screen = True;
      xbevent.type = ButtonPress;
      xbevent.button = clutter_event_get_button (event);
      XSendEvent (xdisplay, xwindow, False, 0, (XEvent *)&xbevent);

      xbevent.type = ButtonRelease;
      XSendEvent (xdisplay, xwindow, False, 0, (XEvent *)&xbevent);
    }
  else
    {
      xkevent.window = xwindow;
      xkevent.root = xrootwindow;
      xkevent.subwindow = None;
      xkevent.time = xcevent.time;
      xkevent.x = xcevent.x;
      xkevent.y = xcevent.y;
      xkevent.x_root = xcevent.x_root;
      xkevent.y_root = xcevent.y_root;
      xkevent.state = clutter_event_get_state (event);
      xkevent.same_screen = True;
      xkevent.keycode = clutter_event_get_key_code (event);

      xkevent.type = KeyPress;
      XSendEvent (xdisplay, xwindow, False, 0, (XEvent *)&xkevent);

      if (event_type == CLUTTER_KEY_RELEASE)
        {
          /* If the application takes a grab on KeyPress, we don't
           * want to send it a KeyRelease. There's no good way of
           * knowing whether a tray icon will take a grab, so just
           * assume it does, and don't send the KeyRelease. That might
           * make the tracking for key events messed up if it doesn't take
           * a grab, but the tray icon won't get key focus in normal cases,
           * so let's hope this isn't too damaging...
           */
          xkevent.type = KeyRelease;
          XSendEvent (xdisplay, xwindow, False, 0, (XEvent *)&xkevent);
        }
    }

  /* And move the pointer back out */
  xcevent.type = LeaveNotify;
  XSendEvent (xdisplay, xwindow, False, 0, (XEvent *)&xcevent);

  meta_x11_error_trap_pop (x11_display);
}
