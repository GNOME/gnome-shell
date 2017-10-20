/*
 * Copyright (C) 2017 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "x11/window-x11.h"
#include "x11/window-x11-private.h"
#include "wayland/meta-window-xwayland.h"
#include "wayland/meta-wayland.h"

enum
{
  PROP_0,

  PROP_XWAYLAND_MAY_GRAB_KEYBOARD,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

struct _MetaWindowXwayland
{
  MetaWindowX11 parent;

  gboolean xwayland_may_grab_keyboard;
};

struct _MetaWindowXwaylandClass
{
  MetaWindowX11Class parent_class;
};

G_DEFINE_TYPE (MetaWindowXwayland, meta_window_xwayland, META_TYPE_WINDOW_X11)

static void
meta_window_xwayland_init (MetaWindowXwayland *window_xwayland)
{
}

static void
meta_window_xwayland_force_restore_shortcuts (MetaWindow         *window,
                                              ClutterInputDevice *source)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();

  meta_wayland_compositor_restore_shortcuts (compositor, source);
}

static gboolean
meta_window_xwayland_shortcuts_inhibited (MetaWindow         *window,
                                          ClutterInputDevice *source)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();

  return meta_wayland_compositor_is_shortcuts_inhibited (compositor, source);
}

static void
meta_window_xwayland_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MetaWindowXwayland *window = META_WINDOW_XWAYLAND (object);

  switch (prop_id)
    {
    case PROP_XWAYLAND_MAY_GRAB_KEYBOARD:
      g_value_set_boolean (value, window->xwayland_may_grab_keyboard);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_window_xwayland_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MetaWindowXwayland *window = META_WINDOW_XWAYLAND (object);

  switch (prop_id)
    {
    case PROP_XWAYLAND_MAY_GRAB_KEYBOARD:
      window->xwayland_may_grab_keyboard = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_window_xwayland_class_init (MetaWindowXwaylandClass *klass)
{
  MetaWindowClass *window_class = META_WINDOW_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  window_class->force_restore_shortcuts = meta_window_xwayland_force_restore_shortcuts;
  window_class->shortcuts_inhibited = meta_window_xwayland_shortcuts_inhibited;

  gobject_class->get_property = meta_window_xwayland_get_property;
  gobject_class->set_property = meta_window_xwayland_set_property;

  obj_props[PROP_XWAYLAND_MAY_GRAB_KEYBOARD] =
    g_param_spec_boolean ("xwayland-may-grab-keyboard",
                          "Xwayland may use keyboard grabs",
                          "Whether the client may use Xwayland keyboard grabs on this window",
                          FALSE,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}
