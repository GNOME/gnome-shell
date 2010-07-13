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
#include "clutter-backend-x11.h"

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

  gint min_keycode;
  gint max_keycode;

  ClutterModifierType modmap[8];

  ClutterModifierType num_lock_mask;

#ifdef HAVE_XKB
  XkbDescPtr xkb_desc;
#endif

  guint caps_lock_state : 1;
  guint num_lock_state  : 1;
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

#ifdef HAVE_XKB

/* code adapted from gdk/x11/gdkkeys-x11.c - update_modmap */
static void
update_modmap (Display          *display,
               ClutterKeymapX11 *keymap_x11)
{
  static struct {
    const gchar *name;
    Atom atom;
    ClutterModifierType mask;
  } vmods[] = {
    { "Meta",  0, CLUTTER_META_MASK  },
    { "Super", 0, CLUTTER_SUPER_MASK },
    { "Hyper", 0, CLUTTER_HYPER_MASK },
    { NULL, 0, 0 }
  };

  int i, j, k;

  if (vmods[0].atom == 0)
    for (i = 0; vmods[i].name; i++)
      vmods[i].atom = XInternAtom (display, vmods[i].name, FALSE);

  for (i = 0; i < 8; i++)
    keymap_x11->modmap[i] = 1 << i;

  for (i = 0; i < XkbNumVirtualMods; i++)
    {
      for (j = 0; vmods[j].atom; j++)
        {
          if (keymap_x11->xkb_desc->names->vmods[i] == vmods[j].atom)
            {
              for (k = 0; k < 8; k++)
                {
                  if (keymap_x11->xkb_desc->server->vmods[i] & (1 << k))
                    keymap_x11->modmap[k] |= vmods[j].mask;
                }
            }
        }
    }
}

static XkbDescPtr
get_xkb (ClutterKeymapX11 *keymap_x11)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (keymap_x11->backend);

  if (keymap_x11->max_keycode == 0)
    XDisplayKeycodes (backend_x11->xdpy,
                      &keymap_x11->min_keycode,
                      &keymap_x11->max_keycode);

  if (keymap_x11->xkb_desc == NULL)
    {
      int flags = XkbKeySymsMask
                | XkbKeyTypesMask
                | XkbModifierMapMask
                | XkbVirtualModsMask;

      keymap_x11->xkb_desc = XkbGetMap (backend_x11->xdpy, flags, XkbUseCoreKbd);
      if (G_UNLIKELY (keymap_x11->xkb_desc == NULL))
        {
          g_error ("Failed to get the keymap from XKB");
          return NULL;
        }

      flags = XkbGroupNamesMask | XkbVirtualModNamesMask;
      XkbGetNames (backend_x11->xdpy, flags, keymap_x11->xkb_desc);

      update_modmap (backend_x11->xdpy, keymap_x11);
    }

  if (keymap_x11->num_lock_mask == 0)
    keymap_x11->num_lock_mask = XkbKeysymToModifiers (backend_x11->xdpy,
                                                      XK_Num_Lock);

  return keymap_x11->xkb_desc;
}
#endif /* HAVE_XKB */

#ifdef HAVE_XKB
static void
update_locked_mods (ClutterKeymapX11 *keymap_x11,
                    gint              locked_mods)
{
  gboolean old_caps_lock_state, old_num_lock_state;

  old_caps_lock_state = keymap_x11->caps_lock_state;
  old_num_lock_state  = keymap_x11->num_lock_state;

  keymap_x11->caps_lock_state = (locked_mods & CLUTTER_LOCK_MASK) != 0;
  keymap_x11->num_lock_state  = (locked_mods & keymap_x11->num_lock_mask) != 0;

  CLUTTER_NOTE (BACKEND, "Locks state changed - Num: %s, Caps: %s",
                keymap_x11->num_lock_state ? "set" : "unset",
                keymap_x11->caps_lock_state ? "set" : "unset");

#if 0
  /* Add signal to ClutterBackend? */
  if ((keymap_x11->caps_lock_state != old_caps_lock_state) ||
      (keymap_x11->num_lock_state != old_num_lock_state))
    g_signal_emit_by_name (keymap_x11->backend, "key-lock-changed");
#endif
}

static ClutterX11FilterReturn
xkb_filter (XEvent       *xevent,
            ClutterEvent *event,
            gpointer      data)
{
  ClutterBackendX11 *backend_x11 = data;
  ClutterKeymapX11 *keymap_x11 = backend_x11->keymap;

  g_assert (keymap_x11 != NULL);

  if (!backend_x11->use_xkb)
    return CLUTTER_X11_FILTER_CONTINUE;

  if (xevent->type == backend_x11->xkb_event_base)
    {
      XkbEvent *xkb_event = (XkbEvent *) xevent;

      CLUTTER_NOTE (BACKEND, "Received XKB event [%d]",
                    xkb_event->any.xkb_type);

      switch (xkb_event->any.xkb_type)
        {
        case XkbStateNotify:
          update_locked_mods (keymap_x11, xkb_event->state.locked_mods);
          break;

        default:
          break;
        }
    }

  return CLUTTER_X11_FILTER_CONTINUE;
}
#endif /* HAVE_XKB */

static void
clutter_keymap_x11_constructed (GObject *gobject)
{
  ClutterKeymapX11 *keymap_x11 = CLUTTER_KEYMAP_X11 (gobject);
  ClutterBackendX11 *backend_x11;

  g_assert (keymap_x11->backend != NULL);
  backend_x11 = CLUTTER_BACKEND_X11 (keymap_x11->backend);

#if HAVE_XKB
  {
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;

    if (XkbLibraryVersion (&xkb_major, &xkb_minor))
      {
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;

        if (XkbQueryExtension (backend_x11->xdpy,
                               NULL, &backend_x11->xkb_event_base, NULL,
                               &xkb_major, &xkb_minor))
          {
            Bool detectable_autorepeat_supported;

            backend_x11->use_xkb = TRUE;

            XkbSelectEvents (backend_x11->xdpy,
                             XkbUseCoreKbd,
                             XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask,
                             XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask);

            XkbSelectEventDetails (backend_x11->xdpy,
                                   XkbUseCoreKbd, XkbStateNotify,
                                   XkbAllStateComponentsMask,
                                   XkbGroupLockMask|XkbModifierLockMask);

            clutter_x11_add_filter (xkb_filter, backend_x11);

            /* enable XKB autorepeat */
            XkbSetDetectableAutoRepeat (backend_x11->xdpy,
                                        True,
                                        &detectable_autorepeat_supported);

            backend_x11->have_xkb_autorepeat = detectable_autorepeat_supported;

            CLUTTER_NOTE (BACKEND, "Detectable autorepeat: %s",
                          backend_x11->have_xkb_autorepeat ? "supported"
                                                           : "not supported");
          }
      }
  }
#endif /* HAVE_XKB */
}

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
  ClutterKeymapX11 *keymap;

  keymap = CLUTTER_KEYMAP_X11 (gobject);

#ifdef HAVE_XKB
  if (keymap->xkb_desc != NULL)
    XkbFreeKeyboard (keymap->xkb_desc, XkbAllComponentsMask, True);
#endif

  G_OBJECT_CLASS (clutter_keymap_x11_parent_class)->finalize (gobject);
}

static void
clutter_keymap_x11_class_init (ClutterKeymapX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->constructed = clutter_keymap_x11_constructed;
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
_clutter_keymap_x11_get_key_group (ClutterKeymapX11    *keymap,
                                   ClutterModifierType  state)
{
#ifdef HAVE_XKB
  return XkbGroupForCoreState (state);
#else
  return 0;
#endif /* HAVE_XKB */
}

gboolean
_clutter_keymap_x11_get_num_lock_state (ClutterKeymapX11 *keymap)
{
  g_return_val_if_fail (CLUTTER_IS_KEYMAP_X11 (keymap), FALSE);

  return keymap->num_lock_state;
}

gboolean
_clutter_keymap_x11_get_caps_lock_state (ClutterKeymapX11 *keymap)
{
  g_return_val_if_fail (CLUTTER_IS_KEYMAP_X11 (keymap), FALSE);

  return keymap->caps_lock_state;
}

gint
_clutter_keymap_x11_translate_key_state (ClutterKeymapX11    *keymap,
                                         guint                hardware_keycode,
                                         ClutterModifierType  modifier_state,
                                         ClutterModifierType *mods_p)
{
  ClutterBackendX11 *backend_x11;
  ClutterModifierType unconsumed_modifiers = 0;
  gint retval;

  g_return_val_if_fail (CLUTTER_IS_KEYMAP_X11 (keymap), 0);

  backend_x11 = CLUTTER_BACKEND_X11 (keymap->backend);

#ifdef HAVE_XKB
  if (backend_x11->use_xkb)
    {
      XkbDescRec *xkb = get_xkb (keymap);
      KeySym tmp_keysym;

      if (XkbTranslateKeyCode (xkb, hardware_keycode, modifier_state,
                               &unconsumed_modifiers,
                               &tmp_keysym))
        {
          retval = tmp_keysym;
        }
      else
        retval = 0;
    }
  else
#endif /* HAVE_XKB */
    retval = XKeycodeToKeysym (backend_x11->xdpy, hardware_keycode, 0);

  if (mods_p)
    *mods_p = unconsumed_modifiers;

  return retval;
}

gboolean
_clutter_keymap_x11_get_is_modifier (ClutterKeymapX11 *keymap,
                                     guint             keycode)
{
  g_return_val_if_fail (CLUTTER_IS_KEYMAP_X11 (keymap), FALSE);

  if (keycode < keymap->min_keycode || keycode > keymap->max_keycode)
    return FALSE;

#ifdef HAVE_XKB
  if (CLUTTER_BACKEND_X11 (keymap->backend)->use_xkb)
    {
      XkbDescRec *xkb = get_xkb (keymap);

      if (xkb->map->modmap && xkb->map->modmap[keycode] != 0)
        return TRUE;
    }
#endif /* HAVE_XKB */

  return FALSE;
}
