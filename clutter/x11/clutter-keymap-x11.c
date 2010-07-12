/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corp.
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-keymap-x11.h"

#include "clutter-debug.h"
#include "clutter-private.h"

#include <X11/Xatom.h>

#ifdef HAVE_XINPUT
#include <X11/extensions/XInput.h>
#endif

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

typedef struct _ClutterKeymapX11Class   ClutterKeymapX11Class;

struct _ClutterKeymapX11
{
  GObject parent_instance;

  ClutterBackend *backend;
};

struct _ClutterKeymapX11Class
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,

  PROP_BACKEND
};

G_DEFINE_TYPE (ClutterKeymapX11, clutter_keymap_x11, G_TYPE_OBJECT);

static void
clutter_keymap_x11_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterKeymapX11 *keymap = CLUTTER_KEYMAP_X11 (gobject);

  switch (prop_id)
    {
    case PROP_BACKEND:
      keymap->backend = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_keymap_x11_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_keymap_x11_parent_class)->finalize (gobject);
}

static void
clutter_keymap_x11_class_init (ClutterKeymapX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_keymap_x11_set_property;
  gobject_class->finalize = clutter_keymap_x11_finalize;

  pspec = g_param_spec_object ("backend",
                               "Backend",
                               "The Clutter backend",
                               CLUTTER_TYPE_BACKEND,
                               CLUTTER_PARAM_WRITABLE |
                               G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_BACKEND, pspec);
}

static void
clutter_keymap_x11_init (ClutterKeymapX11 *keymap)
{
}

gint
_clutter_keymap_x11_get_key_group (ClutterModifierType state)
{
#ifdef HAVE_XKB
  return XkbGroupForCoreState (state);
#else
  return 0;
#endif /* HAVE_XKB */
}
