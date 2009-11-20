/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CLUTTER_BACKEND_X11_H__
#define __CLUTTER_BACKEND_X11_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-backend.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "clutter-x11.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_X11                (clutter_backend_x11_get_type ())
#define CLUTTER_BACKEND_X11(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_X11, ClutterBackendX11))
#define CLUTTER_IS_BACKEND_X11(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_X11))
#define CLUTTER_BACKEND_X11_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_X11, ClutterBackendX11Class))
#define CLUTTER_IS_BACKEND_X11_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_X11))
#define CLUTTER_BACKEND_X11_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_X11, ClutterBackendX11Class))

typedef struct _ClutterBackendX11       ClutterBackendX11;
typedef struct _ClutterBackendX11Class  ClutterBackendX11Class;

typedef struct _ClutterX11EventFilter
{
  ClutterX11FilterFunc func;
  gpointer             data;

} ClutterX11EventFilter;

struct _ClutterBackendX11
{
  ClutterBackend parent_instance;

  Display *xdpy;
  Window   xwin_root;
  Screen  *xscreen;
  int      xscreen_num;
  gchar   *display_name;

  /* event source */
  GSource *event_source;
  GSList  *event_filters;

  /* props */
  Atom atom_NET_WM_PID;
  Atom atom_NET_WM_PING;
  Atom atom_NET_WM_STATE;
  Atom atom_NET_WM_STATE_FULLSCREEN;
  Atom atom_NET_WM_USER_TIME;
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom_XEMBED;
  Atom atom_XEMBED_INFO;
  Atom atom_NET_WM_NAME;
  Atom atom_UTF8_STRING;

  int event_types[CLUTTER_X11_XINPUT_LAST_EVENT];
  gboolean have_xinput;

  Time last_event_time;
};

struct _ClutterBackendX11Class
{
  ClutterBackendClass parent_class;

  /*
   * To support foreign stage windows the we need a way to ask for an
   * XVisualInfo that may be used by toolkits to create an XWindow, and this
   * may need to be handled differently for different backends.
   */
  XVisualInfo *(* get_visual_info) (ClutterBackendX11 *backend);
};

void   _clutter_backend_x11_events_init (ClutterBackend *backend);
void   _clutter_backend_x11_events_uninit (ClutterBackend *backend);

GType clutter_backend_x11_get_type (void) G_GNUC_CONST;

/* Private to glx/eglx backends */
gboolean
clutter_backend_x11_pre_parse (ClutterBackend  *backend,
                               GError         **error);

gboolean
clutter_backend_x11_post_parse (ClutterBackend  *backend,
                                GError         **error);

gboolean
clutter_backend_x11_init_stage (ClutterBackend  *backend,
                                GError         **error);

ClutterActor *
clutter_backend_x11_get_stage (ClutterBackend *backend);

void
clutter_backend_x11_add_options (ClutterBackend *backend,
                                 GOptionGroup   *group);

ClutterFeatureFlags
clutter_backend_x11_get_features (ClutterBackend *backend);

XVisualInfo *
clutter_backend_x11_get_visual_info (ClutterBackendX11 *backend_x11);

ClutterInputDevice *
_clutter_x11_get_device_for_xid (XID id);

void
_clutter_x11_select_events (Window xwin);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_X11_H__ */
