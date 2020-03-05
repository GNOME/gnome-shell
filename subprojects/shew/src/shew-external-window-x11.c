/*
 * Copyright © 2016 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Jonas Ådahl <jadahl@redhat.com>
 */

#include <errno.h>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <stdlib.h>

#include "shew-external-window-x11.h"

static GdkDisplay *x11_display;

struct _ShewExternalWindowX11
{
  ShewExternalWindow parent;

  GdkWindow *foreign_gdk_window;
};

G_DEFINE_TYPE (ShewExternalWindowX11, shew_external_window_x11,
               SHEW_TYPE_EXTERNAL_WINDOW)

static GdkDisplay *
get_x11_display (void)
{
  if (x11_display)
    return x11_display;

  gdk_set_allowed_backends ("x11");
  x11_display = gdk_display_open (NULL);
  gdk_set_allowed_backends (NULL);
  if (!x11_display)
    g_warning ("Failed to open X11 display");

  return x11_display;
}

ShewExternalWindowX11 *
shew_external_window_x11_new (const char *handle_str)
{
  ShewExternalWindowX11 *external_window_x11;
  GdkDisplay *display;
  int xid;
  GdkWindow *foreign_gdk_window = NULL;

  display = get_x11_display ();
  if (!display)
    {
      g_warning ("No X display connection, ignoring X11 parent");
      return NULL;
    }

  errno = 0;
  xid = strtol (handle_str, NULL, 16);
  if (errno != 0)
    {
      g_warning ("Failed to reference external X11 window, invalid XID %s", handle_str);
      return NULL;
    }

#ifdef GDK_WINDOWING_X11
  foreign_gdk_window = gdk_x11_window_foreign_new_for_display (display, xid);
#endif

  if (!foreign_gdk_window)
    {
      g_warning ("Failed to create foreign window for XID %d", xid);
      return NULL;
    }

  external_window_x11 = g_object_new (SHEW_TYPE_EXTERNAL_WINDOW_X11,
                                      "display", display,
                                      NULL);
  external_window_x11->foreign_gdk_window = foreign_gdk_window;

  return external_window_x11;
}

static void
shew_external_window_x11_set_parent_of (ShewExternalWindow *external_window,
                                        GdkWindow      *child_window)
{
  ShewExternalWindowX11 *external_window_x11 =
    SHEW_EXTERNAL_WINDOW_X11 (external_window);

  gdk_window_set_transient_for (child_window,
                                external_window_x11->foreign_gdk_window);
}

static void
shew_external_window_x11_dispose (GObject *object)
{
  ShewExternalWindowX11 *external_window_x11 = SHEW_EXTERNAL_WINDOW_X11 (object);

  g_clear_object (&external_window_x11->foreign_gdk_window);

  G_OBJECT_CLASS (shew_external_window_x11_parent_class)->dispose (object);
}

static void
shew_external_window_x11_init (ShewExternalWindowX11 *external_window_x11)
{
}

static void
shew_external_window_x11_class_init (ShewExternalWindowX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ShewExternalWindowClass *external_window_class = SHEW_EXTERNAL_WINDOW_CLASS (klass);

  object_class->dispose = shew_external_window_x11_dispose;

  external_window_class->set_parent_of = shew_external_window_x11_set_parent_of;
}
