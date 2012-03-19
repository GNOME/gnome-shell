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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __CLUTTER_BACKEND_X11_H__
#define __CLUTTER_BACKEND_X11_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "clutter-x11.h"

#include "clutter-backend-private.h"
#include "clutter-keymap-x11.h"

#include "xsettings/xsettings-client.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_X11                (_clutter_backend_x11_get_type ())
#define CLUTTER_BACKEND_X11(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_X11, ClutterBackendX11))
#define CLUTTER_IS_BACKEND_X11(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_X11))
#define CLUTTER_BACKEND_X11_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_X11, ClutterBackendX11Class))
#define CLUTTER_IS_BACKEND_X11_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_X11))
#define CLUTTER_BACKEND_X11_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_X11, ClutterBackendX11Class))

typedef struct _ClutterBackendX11       ClutterBackendX11;
typedef struct _ClutterBackendX11Class  ClutterBackendX11Class;
typedef struct _ClutterEventX11         ClutterEventX11;
typedef struct _ClutterX11EventFilter   ClutterX11EventFilter;

struct _ClutterX11EventFilter
{
  ClutterX11FilterFunc func;
  gpointer             data;

};

struct _ClutterEventX11
{
  /* additional fields for Key events */
  gint key_group;

  guint key_is_modifier : 1;
  guint num_lock_set    : 1;
  guint caps_lock_set   : 1;
};

struct _ClutterBackendX11
{
  ClutterBackend parent_instance;

  Display *xdpy;
  gchar   *display_name;

  Screen  *xscreen;
  int      xscreen_num;
  int      xscreen_width;
  int      xscreen_height;

  Window   xwin_root;

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

  Time last_event_time;

  ClutterDeviceManager *device_manager;
  gboolean has_xinput;
  int xi_minor;

  XSettingsClient *xsettings;
  Window xsettings_xwin;

  ClutterKeymapX11 *keymap;
  gboolean use_xkb;
  gboolean have_xkb_autorepeat;
  guint keymap_serial;
};

struct _ClutterBackendX11Class
{
  ClutterBackendClass parent_class;
};

GType _clutter_backend_x11_get_type (void) G_GNUC_CONST;

void            _clutter_backend_x11_events_init        (ClutterBackend *backend);

GSource *       _clutter_x11_event_source_new   (ClutterBackendX11 *backend_x11);

/* Private to glx/eglx backends */
XVisualInfo *   _clutter_backend_x11_get_visual_info (ClutterBackendX11 *backend_x11);

void            _clutter_x11_select_events (Window xwin);

ClutterEventX11 *       _clutter_event_x11_new          (void);
ClutterEventX11 *       _clutter_event_x11_copy         (ClutterEventX11 *event_x11);
void                    _clutter_event_x11_free         (ClutterEventX11 *event_x11);

gboolean        _clutter_x11_input_device_translate_screen_coord (ClutterInputDevice *device,
                                                                  gint                stage_root_x,
                                                                  gint                stage_root_y,
                                                                  guint               index_,
                                                                  gdouble             value,
                                                                  gdouble            *axis_value);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_X11_H__ */
