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

#include "config.h"

#include "clutter-keymap-x11.h"
#include "clutter-backend-x11.h"

#include "clutter-debug.h"
#include "clutter-event-translator.h"
#include "clutter-private.h"

#include <X11/Xatom.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

typedef struct _ClutterKeymapX11Class   ClutterKeymapX11Class;
typedef struct _DirectionCacheEntry     DirectionCacheEntry;

struct _DirectionCacheEntry
{
  guint serial;
  Atom group_atom;
  PangoDirection direction;
};

struct _ClutterKeymapX11
{
  GObject parent_instance;

  ClutterBackend *backend;

  int min_keycode;
  int max_keycode;

  ClutterModifierType modmap[8];

  ClutterModifierType num_lock_mask;
  ClutterModifierType scroll_lock_mask;

  PangoDirection current_direction;

#ifdef HAVE_XKB
  XkbDescPtr xkb_desc;
  int xkb_event_base;
  guint xkb_map_serial;
  Atom current_group_atom;
  guint current_cache_serial;
  DirectionCacheEntry group_direction_cache[4];
#endif

  guint caps_lock_state : 1;
  guint num_lock_state  : 1;
  guint has_direction   : 1;
};

struct _ClutterKeymapX11Class
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,

  PROP_BACKEND,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

static void clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface);

#define clutter_keymap_x11_get_type     _clutter_keymap_x11_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterKeymapX11, clutter_keymap_x11, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_EVENT_TRANSLATOR,
                                                clutter_event_translator_iface_init));

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
  else if (keymap_x11->xkb_map_serial != backend_x11->keymap_serial)
    {
      int flags = XkbKeySymsMask
                | XkbKeyTypesMask
                | XkbModifierMapMask
                | XkbVirtualModsMask;

      CLUTTER_NOTE (BACKEND, "Updating XKB keymap");

      XkbGetUpdatedMap (backend_x11->xdpy, flags, keymap_x11->xkb_desc);

      flags = XkbGroupNamesMask | XkbVirtualModNamesMask;
      XkbGetNames (backend_x11->xdpy, flags, keymap_x11->xkb_desc);

      update_modmap (backend_x11->xdpy, keymap_x11);

      keymap_x11->xkb_map_serial = backend_x11->keymap_serial;
    }

  if (keymap_x11->num_lock_mask == 0)
    keymap_x11->num_lock_mask = XkbKeysymToModifiers (backend_x11->xdpy,
                                                      XK_Num_Lock);

  if (keymap_x11->scroll_lock_mask == 0)
    keymap_x11->scroll_lock_mask = XkbKeysymToModifiers (backend_x11->xdpy,
                                                         XK_Scroll_Lock);

  return keymap_x11->xkb_desc;
}
#endif /* HAVE_XKB */

#ifdef HAVE_XKB
static void
update_locked_mods (ClutterKeymapX11 *keymap_x11,
                    gint              locked_mods)
{
#if 0
  gboolean old_caps_lock_state, old_num_lock_state;

  old_caps_lock_state = keymap_x11->caps_lock_state;
  old_num_lock_state  = keymap_x11->num_lock_state;
#endif

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
#endif /* HAVE_XKB */

#ifdef HAVE_XKB
/* the code to retrieve the keymap direction and cache it
 * is taken from GDK:
 *      gdk/x11/gdkkeys-x11.c
 */
static PangoDirection
get_direction (XkbDescPtr xkb,
               int        group)
{
  int rtl_minus_ltr = 0; /* total number of RTL keysyms minus LTR ones */
  int code;

  for (code = xkb->min_key_code;
       code <= xkb->max_key_code;
       code += 1)
    {
      int level = 0;
      KeySym sym = XkbKeySymEntry (xkb, code, level, group);
      PangoDirection dir = pango_unichar_direction (clutter_keysym_to_unicode (sym));

      switch (dir)
        {
        case PANGO_DIRECTION_RTL:
          rtl_minus_ltr++;
          break;

        case PANGO_DIRECTION_LTR:
          rtl_minus_ltr--;
          break;

        default:
          break;
        }
    }

  if (rtl_minus_ltr > 0)
    return PANGO_DIRECTION_RTL;

  return PANGO_DIRECTION_LTR;
}

static PangoDirection
get_direction_from_cache (ClutterKeymapX11 *keymap_x11,
                          XkbDescPtr        xkb,
                          int               group)
{
  Atom group_atom = xkb->names->groups[group];
  gboolean cache_hit = FALSE;
  DirectionCacheEntry *cache = keymap_x11->group_direction_cache;
  PangoDirection direction = PANGO_DIRECTION_NEUTRAL;
  int i;

  if (keymap_x11->has_direction)
    {
      /* look up in the cache */
      for (i = 0; i < G_N_ELEMENTS (keymap_x11->group_direction_cache); i++)
        {
          if (cache[i].group_atom == group_atom)
            {
              cache_hit = TRUE;
              cache[i].serial = keymap_x11->current_cache_serial++;
              direction = cache[i].direction;
              group_atom = cache[i].group_atom;
              break;
            }
        }
    }
  else
    {
      /* initialize the cache */
      for (i = 0; i < G_N_ELEMENTS (keymap_x11->group_direction_cache); i++)
        {
          cache[i].group_atom = 0;
          cache[i].direction = PANGO_DIRECTION_NEUTRAL;
          cache[i].serial = keymap_x11->current_cache_serial;
        }

      keymap_x11->current_cache_serial += 1;
    }

  /* insert the new entry in the cache */
  if (!cache_hit)
    {
      int oldest = 0;

      direction = get_direction (xkb, group);

      /* replace the oldest entry */
      for (i = 0; i < G_N_ELEMENTS (keymap_x11->group_direction_cache); i++)
        {
          if (cache[i].serial < cache[oldest].serial)
            oldest = i;
        }

      cache[oldest].group_atom = group_atom;
      cache[oldest].direction = direction;
      cache[oldest].serial = keymap_x11->current_cache_serial++;
    }

  return direction;
}
#endif /* HAVE_XKB */

static void
update_direction (ClutterKeymapX11 *keymap_x11,
                  int               group)
{
#ifdef HAVE_XKB
  XkbDescPtr xkb = get_xkb (keymap_x11);
  Atom group_atom;

  group_atom = xkb->names->groups[group];

  if (!keymap_x11->has_direction || keymap_x11->current_group_atom != group_atom)
    {
      keymap_x11->current_direction = get_direction_from_cache (keymap_x11, xkb, group);
      keymap_x11->current_group_atom = group_atom;
      keymap_x11->has_direction = TRUE;
    }
#endif /* HAVE_XKB */
}

static void
clutter_keymap_x11_constructed (GObject *gobject)
{
  ClutterKeymapX11 *keymap_x11 = CLUTTER_KEYMAP_X11 (gobject);
  ClutterBackendX11 *backend_x11;

  g_assert (keymap_x11->backend != NULL);
  backend_x11 = CLUTTER_BACKEND_X11 (keymap_x11->backend);

#ifdef HAVE_XKB
  {
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;

    if (XkbLibraryVersion (&xkb_major, &xkb_minor))
      {
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;

        if (XkbQueryExtension (backend_x11->xdpy,
                               NULL,
                               &keymap_x11->xkb_event_base,
                               NULL,
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
                                   XkbGroupLockMask | XkbModifierLockMask);

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
  ClutterEventTranslator *translator;

  keymap = CLUTTER_KEYMAP_X11 (gobject);
  translator = CLUTTER_EVENT_TRANSLATOR (keymap);

#ifdef HAVE_XKB
  _clutter_backend_remove_event_translator (keymap->backend, translator);

  if (keymap->xkb_desc != NULL)
    XkbFreeKeyboard (keymap->xkb_desc, XkbAllComponentsMask, True);
#endif

  G_OBJECT_CLASS (clutter_keymap_x11_parent_class)->finalize (gobject);
}

static void
clutter_keymap_x11_class_init (ClutterKeymapX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  obj_props[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         P_("Backend"),
                         P_("The Clutter backend"),
                         CLUTTER_TYPE_BACKEND,
                         CLUTTER_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  gobject_class->constructed = clutter_keymap_x11_constructed;
  gobject_class->set_property = clutter_keymap_x11_set_property;
  gobject_class->finalize = clutter_keymap_x11_finalize;
  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_keymap_x11_init (ClutterKeymapX11 *keymap)
{
  keymap->current_direction = PANGO_DIRECTION_NEUTRAL;
}

static ClutterTranslateReturn
clutter_keymap_x11_translate_event (ClutterEventTranslator *translator,
                                    gpointer                native,
                                    ClutterEvent           *event)
{
  ClutterKeymapX11 *keymap_x11 = CLUTTER_KEYMAP_X11 (translator);
  ClutterBackendX11 *backend_x11;
  ClutterTranslateReturn retval;
  XEvent *xevent;

  backend_x11 = CLUTTER_BACKEND_X11 (keymap_x11->backend);
  if (!backend_x11->use_xkb)
    return CLUTTER_TRANSLATE_CONTINUE;

  xevent = native;

  retval = CLUTTER_TRANSLATE_CONTINUE;

#ifdef HAVE_XKB
  if (xevent->type == keymap_x11->xkb_event_base)
    {
      XkbEvent *xkb_event = (XkbEvent *) xevent;

      switch (xkb_event->any.xkb_type)
        {
        case XkbStateNotify:
          CLUTTER_NOTE (EVENT, "Updating keyboard state");
          update_direction (keymap_x11, XkbStateGroup (&xkb_event->state));
          update_locked_mods (keymap_x11, xkb_event->state.locked_mods);
          retval = CLUTTER_TRANSLATE_REMOVE;
          break;

        case XkbNewKeyboardNotify:
        case XkbMapNotify:
          CLUTTER_NOTE (EVENT, "Updating keyboard mapping");
          XkbRefreshKeyboardMapping (&xkb_event->map);
          backend_x11->keymap_serial += 1;
          retval = CLUTTER_TRANSLATE_REMOVE;
          break;

        default:
          break;
        }
    }
#endif /* HAVE_XKB */

  return retval;
}

static void
clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface)
{
  iface->translate_event = clutter_keymap_x11_translate_event;
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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* XXX - yes, I know that XKeycodeToKeysym() has been deprecated; hopefully,
 * this code will never get run on any decent system that is also able to
 * run Clutter. I just don't want to copy the implementation inside GDK for
 * a fallback path.
 */
static int
translate_keysym (ClutterKeymapX11 *keymap,
                  guint             hardware_keycode)
{
  ClutterBackendX11 *backend_x11;
  gint retval;

  backend_x11 = CLUTTER_BACKEND_X11 (keymap->backend);

  retval = XKeycodeToKeysym (backend_x11->xdpy, hardware_keycode, 0);

  return retval;
}

G_GNUC_END_IGNORE_DEPRECATIONS

gint
_clutter_keymap_x11_translate_key_state (ClutterKeymapX11    *keymap,
                                         guint                hardware_keycode,
                                         ClutterModifierType *modifier_state_p,
                                         ClutterModifierType *mods_p)
{
  ClutterBackendX11 *backend_x11;
  ClutterModifierType unconsumed_modifiers = 0;
  ClutterModifierType modifier_state = *modifier_state_p;
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
    retval = translate_keysym (keymap, hardware_keycode);

  if (mods_p)
    *mods_p = unconsumed_modifiers;

  *modifier_state_p = modifier_state & ~(keymap->num_lock_mask |
                                         keymap->scroll_lock_mask |
                                         LockMask);

  return retval;
}

gboolean
_clutter_keymap_x11_get_is_modifier (ClutterKeymapX11 *keymap,
                                     gint              keycode)
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

PangoDirection
_clutter_keymap_x11_get_direction (ClutterKeymapX11 *keymap)
{
  g_return_val_if_fail (CLUTTER_IS_KEYMAP_X11 (keymap), PANGO_DIRECTION_NEUTRAL);

#ifdef HAVE_XKB
  if (CLUTTER_BACKEND_X11 (keymap->backend)->use_xkb)
    {
      if (!keymap->has_direction)
        {
          Display *xdisplay = CLUTTER_BACKEND_X11 (keymap->backend)->xdpy;
          XkbStateRec state_rec;

          XkbGetState (xdisplay, XkbUseCoreKbd, &state_rec);
          update_direction (keymap, XkbStateGroup (&state_rec));
        }

      return keymap->current_direction;
    }
  else
#endif
    return PANGO_DIRECTION_NEUTRAL;
}
