/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter Keybindings */
/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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
 */

/**
 * SECTION:keybindings
 * @Title: MetaKeybinding
 * @Short_Description: Key bindings
 */

#define _GNU_SOURCE

#include <config.h>
#include "keybindings-private.h"
#include "workspace-private.h"
#include <meta/compositor.h>
#include <meta/errors.h>
#include "edge-resistance.h"
#include "ui.h"
#include "frame.h"
#include "place.h"
#include "screen-private.h"
#include <meta/prefs.h>
#include "util-private.h"
#include "meta-accel-parse.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <xkbcommon/xkbcommon.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#include "wayland/meta-wayland.h"
#include "meta-backend.h"

#define SCHEMA_COMMON_KEYBINDINGS "org.gnome.desktop.wm.keybindings"
#define SCHEMA_MUTTER_KEYBINDINGS "org.gnome.mutter.keybindings"
#define SCHEMA_MUTTER_WAYLAND_KEYBINDINGS "org.gnome.mutter.wayland.keybindings"

static gboolean add_builtin_keybinding (MetaDisplay          *display,
                                        const char           *name,
                                        GSettings            *settings,
                                        MetaKeyBindingFlags   flags,
                                        MetaKeyBindingAction  action,
                                        MetaKeyHandlerFunc    handler,
                                        int                   handler_arg);

static void
meta_key_binding_free (MetaKeyBinding *binding)
{
  g_slice_free (MetaKeyBinding, binding);
}

static MetaKeyBinding *
meta_key_binding_copy (MetaKeyBinding *binding)
{
  return g_slice_dup (MetaKeyBinding, binding);
}

G_DEFINE_BOXED_TYPE(MetaKeyBinding,
                    meta_key_binding,
                    meta_key_binding_copy,
                    meta_key_binding_free)

const char *
meta_key_binding_get_name (MetaKeyBinding *binding)
{
  return binding->name;
}

MetaVirtualModifier
meta_key_binding_get_modifiers (MetaKeyBinding *binding)
{
  return binding->modifiers;
}

guint
meta_key_binding_get_mask (MetaKeyBinding *binding)
{
  return binding->mask;
}

gboolean
meta_key_binding_is_builtin (MetaKeyBinding *binding)
{
  return binding->handler->flags & META_KEY_BINDING_BUILTIN;
}

/* These can't be bound to anything, but they are used to handle
 * various other events.  TODO: Possibly we should include them as event
 * handler functions and have some kind of flag to say they're unbindable.
 */

static gboolean process_mouse_move_resize_grab (MetaDisplay     *display,
                                                MetaScreen      *screen,
                                                MetaWindow      *window,
                                                ClutterKeyEvent *event);

static gboolean process_keyboard_move_grab (MetaDisplay     *display,
                                            MetaScreen      *screen,
                                            MetaWindow      *window,
                                            ClutterKeyEvent *event);

static gboolean process_keyboard_resize_grab (MetaDisplay     *display,
                                              MetaScreen      *screen,
                                              MetaWindow      *window,
                                              ClutterKeyEvent *event);

static void grab_key_bindings           (MetaDisplay *display);
static void ungrab_key_bindings         (MetaDisplay *display);

static GHashTable *key_handlers;
static GHashTable *external_grabs;

#define HANDLER(name) g_hash_table_lookup (key_handlers, (name))

static void
key_handler_free (MetaKeyHandler *handler)
{
  g_free (handler->name);
  if (handler->user_data_free_func && handler->user_data)
    handler->user_data_free_func (handler->user_data);
  g_free (handler);
}

typedef struct _MetaKeyGrab MetaKeyGrab;
struct _MetaKeyGrab {
  char *name;
  guint action;
  MetaKeyCombo *combo;
};

static void
meta_key_grab_free (MetaKeyGrab *grab)
{
  g_free (grab->name);
  g_free (grab->combo);
  g_free (grab);
}

static guint32
key_binding_key (guint32 keycode,
                 guint32 mask)
{
  /* On X, keycodes are only 8 bits while libxkbcommon supports 32 bit
     keycodes, but since we're using the same XKB keymaps that X uses,
     we won't find keycodes bigger than 8 bits in practice. The bits
     that mutter cares about in the modifier mask are also all in the
     lower 8 bits both on X and clutter key events. This means that we
     can use a 32 bit integer to safely concatenate both keycode and
     mask and thus making it easy to use them as an index in a
     GHashTable. */
  guint32 key = keycode & 0xffff;
  return (key << 16) | (mask & 0xffff);
}


static void
reload_keymap (MetaDisplay *display)
{
  if (display->keymap)
    meta_XFree (display->keymap);

  /* This is expensive to compute, so we'll lazily load if and when we first
   * need it */
  display->above_tab_keycode = 0;

  display->keymap = XGetKeyboardMapping (display->xdisplay,
                                         display->min_keycode,
                                         display->max_keycode -
                                         display->min_keycode + 1,
                                         &display->keysyms_per_keycode);
}

static const char *
keysym_name (xkb_keysym_t keysym)
{
  static char name[32] = "";
  xkb_keysym_get_name (keysym, name, sizeof (name));
  return name;
}

static void
reload_modmap (MetaDisplay *display)
{
  XModifierKeymap *modmap;
  int map_size;
  int i;
  int num_lock_mask = 0;
  int scroll_lock_mask = 0;

  modmap = XGetModifierMapping (display->xdisplay);
  display->ignored_modifier_mask = 0;

  /* Multiple bits may get set in each of these */
  display->meta_mask = 0;
  display->hyper_mask = 0;
  display->super_mask = 0;

  /* there are 8 modifiers, and the first 3 are shift, shift lock,
   * and control
   */
  map_size = 8 * modmap->max_keypermod;
  i = 3 * modmap->max_keypermod;
  while (i < map_size)
    {
      /* get the key code at this point in the map,
       * see if its keysym is one we're interested in
       */
      int keycode = modmap->modifiermap[i];

      if (keycode >= display->min_keycode &&
          keycode <= display->max_keycode)
        {
          int j = 0;
          KeySym *syms = display->keymap +
            (keycode - display->min_keycode) * display->keysyms_per_keycode;

          while (j < display->keysyms_per_keycode)
            {
              if (syms[j] != 0)
                {
                  meta_topic (META_DEBUG_KEYBINDINGS,
                              "Keysym %s bound to modifier 0x%x\n",
                              keysym_name (syms[j]),
                              (1 << ( i / modmap->max_keypermod)));
                }

              if (syms[j] == XKB_KEY_Num_Lock)
                {
                  /* Mod1Mask is 1 << 3 for example, i.e. the
                   * fourth modifier, i / keyspermod is the modifier
                   * index
                   */

                  num_lock_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XKB_KEY_Scroll_Lock)
                {
                  scroll_lock_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XKB_KEY_Super_L ||
                       syms[j] == XKB_KEY_Super_R)
                {
                  display->super_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XKB_KEY_Hyper_L ||
                       syms[j] == XKB_KEY_Hyper_R)
                {
                  display->hyper_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XKB_KEY_Meta_L ||
                       syms[j] == XKB_KEY_Meta_R)
                {
                  display->meta_mask |= (1 << ( i / modmap->max_keypermod));
                }

              ++j;
            }
        }

      ++i;
    }

  display->ignored_modifier_mask = (num_lock_mask |
                                    scroll_lock_mask |
                                    LockMask);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Ignoring modmask 0x%x num lock 0x%x scroll lock 0x%x hyper 0x%x super 0x%x meta 0x%x\n",
              display->ignored_modifier_mask,
              num_lock_mask,
              scroll_lock_mask,
              display->hyper_mask,
              display->super_mask,
              display->meta_mask);

  XFreeModifiermap (modmap);
}

/* Original code from gdk_x11_keymap_get_entries_for_keyval() in
 * gdkkeys-x11.c */
static int
get_keycodes_for_keysym (MetaDisplay  *display,
                         int           keysym,
                         int         **keycodes)
{
  GArray *retval;
  int n_keycodes;
  int keycode;

  retval = g_array_new (FALSE, FALSE, sizeof (int));

  /* Special-case: Fake mutter keysym */
  if (keysym == META_KEY_ABOVE_TAB)
    {
      keycode = meta_display_get_above_tab_keycode (display);
      g_array_append_val (retval, keycode);
      goto out;
    }

  keycode = display->min_keycode;
  while (keycode <= display->max_keycode)
    {
      const KeySym *syms = display->keymap + (keycode - display->min_keycode) * display->keysyms_per_keycode;
      int i = 0;

      while (i < display->keysyms_per_keycode)
        {
          if (syms[i] == (unsigned int)keysym)
            g_array_append_val (retval, keycode);

          ++i;
        }

      ++keycode;
    }

 out:
  n_keycodes = retval->len;
  *keycodes = (int*) g_array_free (retval, n_keycodes == 0 ? TRUE : FALSE);
  return n_keycodes;
}

static guint
get_first_keycode_for_keysym (MetaDisplay *display,
                              guint        keysym)
{
  int *keycodes;
  int n_keycodes;
  int keycode;

  n_keycodes = get_keycodes_for_keysym (display, keysym, &keycodes);

  if (n_keycodes > 0)
    keycode = keycodes[0];
  else
    keycode = 0;

  g_free (keycodes);
  return keycode;
}

static void
reload_iso_next_group_combos (MetaDisplay *display)
{
  const char *iso_next_group_option;
  MetaKeyCombo *combos;
  int *keycodes;
  int n_keycodes;
  int n_combos;
  int i;

  g_clear_pointer (&display->iso_next_group_combos, g_free);
  display->n_iso_next_group_combos = 0;

  iso_next_group_option = meta_prefs_get_iso_next_group_option ();
  if (iso_next_group_option == NULL)
    return;

  n_keycodes = get_keycodes_for_keysym (display, XKB_KEY_ISO_Next_Group, &keycodes);

  if (g_str_equal (iso_next_group_option, "toggle") ||
      g_str_equal (iso_next_group_option, "lalt_toggle") ||
      g_str_equal (iso_next_group_option, "lwin_toggle") ||
      g_str_equal (iso_next_group_option, "rwin_toggle") ||
      g_str_equal (iso_next_group_option, "lshift_toggle") ||
      g_str_equal (iso_next_group_option, "rshift_toggle") ||
      g_str_equal (iso_next_group_option, "lctrl_toggle") ||
      g_str_equal (iso_next_group_option, "rctrl_toggle") ||
      g_str_equal (iso_next_group_option, "sclk_toggle") ||
      g_str_equal (iso_next_group_option, "menu_toggle") ||
      g_str_equal (iso_next_group_option, "caps_toggle"))
    {
      n_combos = n_keycodes;
      combos = g_new (MetaKeyCombo, n_combos);

      for (i = 0; i < n_keycodes; ++i)
        {
          combos[i].keysym = XKB_KEY_ISO_Next_Group;
          combos[i].keycode = keycodes[i];
          combos[i].modifiers = 0;
        }
    }
  else if (g_str_equal (iso_next_group_option, "shift_caps_toggle") ||
           g_str_equal (iso_next_group_option, "shifts_toggle"))
    {
      n_combos = n_keycodes;
      combos = g_new (MetaKeyCombo, n_combos);

      for (i = 0; i < n_keycodes; ++i)
        {
          combos[i].keysym = XKB_KEY_ISO_Next_Group;
          combos[i].keycode = keycodes[i];
          combos[i].modifiers = ShiftMask;
        }
    }
  else if (g_str_equal (iso_next_group_option, "alt_caps_toggle") ||
           g_str_equal (iso_next_group_option, "alt_space_toggle"))
    {
      n_combos = n_keycodes;
      combos = g_new (MetaKeyCombo, n_combos);

      for (i = 0; i < n_keycodes; ++i)
        {
          combos[i].keysym = XKB_KEY_ISO_Next_Group;
          combos[i].keycode = keycodes[i];
          combos[i].modifiers = Mod1Mask;
        }
    }
  else if (g_str_equal (iso_next_group_option, "ctrl_shift_toggle") ||
           g_str_equal (iso_next_group_option, "lctrl_lshift_toggle") ||
           g_str_equal (iso_next_group_option, "rctrl_rshift_toggle"))
    {
      n_combos = n_keycodes * 2;
      combos = g_new (MetaKeyCombo, n_combos);

      for (i = 0; i < n_keycodes; ++i)
        {
          combos[i].keysym = XKB_KEY_ISO_Next_Group;
          combos[i].keycode = keycodes[i];
          combos[i].modifiers = ShiftMask;

          combos[i + n_keycodes].keysym = XKB_KEY_ISO_Next_Group;
          combos[i + n_keycodes].keycode = keycodes[i];
          combos[i + n_keycodes].modifiers = ControlMask;
        }
    }
  else if (g_str_equal (iso_next_group_option, "ctrl_alt_toggle"))
    {
      n_combos = n_keycodes * 2;
      combos = g_new (MetaKeyCombo, n_combos);

      for (i = 0; i < n_keycodes; ++i)
        {
          combos[i].keysym = XKB_KEY_ISO_Next_Group;
          combos[i].keycode = keycodes[i];
          combos[i].modifiers = Mod1Mask;

          combos[i + n_keycodes].keysym = XKB_KEY_ISO_Next_Group;
          combos[i + n_keycodes].keycode = keycodes[i];
          combos[i + n_keycodes].modifiers = ControlMask;
        }
    }
  else if (g_str_equal (iso_next_group_option, "alt_shift_toggle") ||
           g_str_equal (iso_next_group_option, "lalt_lshift_toggle"))
    {
      n_combos = n_keycodes * 2;
      combos = g_new (MetaKeyCombo, n_combos);

      for (i = 0; i < n_keycodes; ++i)
        {
          combos[i].keysym = XKB_KEY_ISO_Next_Group;
          combos[i].keycode = keycodes[i];
          combos[i].modifiers = Mod1Mask;

          combos[i + n_keycodes].keysym = XKB_KEY_ISO_Next_Group;
          combos[i + n_keycodes].keycode = keycodes[i];
          combos[i + n_keycodes].modifiers = ShiftMask;
        }
    }
  else
    {
      n_combos = 0;
      combos = NULL;
    }

  g_free (keycodes);

  display->n_iso_next_group_combos = n_combos;
  display->iso_next_group_combos = combos;
}

static void
binding_reload_keycode_foreach (gpointer key,
                                gpointer value,
                                gpointer data)
{
  MetaDisplay *display = data;
  MetaKeyBinding *binding = value;

  if (binding->keysym)
    binding->keycode = get_first_keycode_for_keysym (display, binding->keysym);
}

static void
reload_keycodes (MetaDisplay *display)
{
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Reloading keycodes for binding tables\n");

  if (display->overlay_key_combo.keysym != 0)
    {
      display->overlay_key_combo.keycode =
        get_first_keycode_for_keysym (display, display->overlay_key_combo.keysym);
    }
  else
    {
      display->overlay_key_combo.keycode = 0;
    }

  reload_iso_next_group_combos (display);

  g_hash_table_foreach (display->key_bindings, binding_reload_keycode_foreach, display);
}

static void
binding_reload_modifiers_foreach (gpointer key,
                                  gpointer value,
                                  gpointer data)
{
  MetaDisplay *display = data;
  MetaKeyBinding *binding = value;

  meta_display_devirtualize_modifiers (display,
                                       binding->modifiers,
                                       &binding->mask);
  meta_topic (META_DEBUG_KEYBINDINGS,
              " Devirtualized mods 0x%x -> 0x%x (%s)\n",
              binding->modifiers,
              binding->mask,
              binding->name);
}

static void
reload_modifiers (MetaDisplay *display)
{
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Reloading keycodes for binding tables\n");

  g_hash_table_foreach (display->key_bindings, binding_reload_modifiers_foreach, display);
}

static void
index_binding (MetaDisplay    *display,
               MetaKeyBinding *binding)
{
  guint32 index_key;

  index_key = key_binding_key (binding->keycode, binding->mask);
  g_hash_table_replace (display->key_bindings_index,
                        GINT_TO_POINTER (index_key), binding);
}

static void
binding_index_foreach (gpointer key,
                       gpointer value,
                       gpointer data)
{
  MetaDisplay *display = data;
  MetaKeyBinding *binding = value;

  index_binding (display, binding);
}

static void
rebuild_binding_index (MetaDisplay *display)
{
  g_hash_table_remove_all (display->key_bindings_index);
  g_hash_table_foreach (display->key_bindings, binding_index_foreach, display);
}

static void
rebuild_binding_table (MetaDisplay     *display,
                       GList           *prefs,
                       GList           *grabs)
{
  MetaKeyBinding *b;
  GList *p, *g;

  g_hash_table_remove_all (display->key_bindings);

  p = prefs;
  while (p)
    {
      MetaKeyPref *pref = (MetaKeyPref*)p->data;
      GSList *tmp = pref->combos;

      while (tmp)
        {
          MetaKeyCombo *combo = tmp->data;

          if (combo && (combo->keysym != None || combo->keycode != 0))
            {
              MetaKeyHandler *handler = HANDLER (pref->name);

              b = g_malloc0 (sizeof (MetaKeyBinding));

              b->name = pref->name;
              b->handler = handler;
              b->flags = handler->flags;
              b->keysym = combo->keysym;
              b->keycode = combo->keycode;
              b->modifiers = combo->modifiers;
              b->mask = 0;

              g_hash_table_add (display->key_bindings, b);

              if (pref->add_shift &&
                  (combo->modifiers & META_VIRTUAL_SHIFT_MASK) == 0)
                {
                  meta_topic (META_DEBUG_KEYBINDINGS,
                              "Binding %s also needs Shift grabbed\n",
                              pref->name);

                  b = g_malloc0 (sizeof (MetaKeyBinding));

                  b->name = pref->name;
                  b->handler = handler;
                  b->flags = handler->flags;
                  b->keysym = combo->keysym;
                  b->keycode = combo->keycode;
                  b->modifiers = combo->modifiers | META_VIRTUAL_SHIFT_MASK;
                  b->mask = 0;

                  g_hash_table_add (display->key_bindings, b);
                }
            }

          tmp = tmp->next;
        }

      p = p->next;
    }

  g = grabs;
  while (g)
    {
      MetaKeyGrab *grab = (MetaKeyGrab*)g->data;
      if (grab->combo && (grab->combo->keysym != None || grab->combo->keycode != 0))
        {
          MetaKeyHandler *handler = HANDLER ("external-grab");

          b = g_malloc0 (sizeof (MetaKeyBinding));

          b->name = grab->name;
          b->handler = handler;
          b->flags = handler->flags;
          b->keysym = grab->combo->keysym;
          b->keycode = grab->combo->keycode;
          b->modifiers = grab->combo->modifiers;
          b->mask = 0;

          g_hash_table_add (display->key_bindings, b);
        }

      g = g->next;
    }

  meta_topic (META_DEBUG_KEYBINDINGS,
              " %d bindings in table\n",
              g_hash_table_size (display->key_bindings));
}

static void
rebuild_key_binding_table (MetaDisplay *display)
{
  GList *prefs, *grabs;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Rebuilding key binding table from preferences\n");

  prefs = meta_prefs_get_keybindings ();
  grabs = g_hash_table_get_values (external_grabs);
  rebuild_binding_table (display, prefs, grabs);
  g_list_free (prefs);
  g_list_free (grabs);
}

static void
rebuild_special_bindings (MetaDisplay *display)
{
  MetaKeyCombo combo;

  meta_prefs_get_overlay_binding (&combo);
  display->overlay_key_combo = combo;
}

static void
ungrab_key_bindings (MetaDisplay *display)
{
  GSList *tmp;
  GSList *windows;

  meta_error_trap_push (display); /* for efficiency push outer trap */

  meta_screen_ungrab_keys (display->screen);

  windows = meta_display_list_windows (display, META_LIST_DEFAULT);
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      meta_window_ungrab_keys (w);

      tmp = tmp->next;
    }
  meta_error_trap_pop (display);

  g_slist_free (windows);
}

static void
grab_key_bindings (MetaDisplay *display)
{
  GSList *tmp;
  GSList *windows;

  meta_error_trap_push (display); /* for efficiency push outer trap */

  meta_screen_grab_keys (display->screen);

  windows = meta_display_list_windows (display, META_LIST_DEFAULT);
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      meta_window_grab_keys (w);

      tmp = tmp->next;
    }
  meta_error_trap_pop (display);

  g_slist_free (windows);
}

static MetaKeyBinding *
display_get_keybinding (MetaDisplay  *display,
                        guint32       keycode,
                        guint32       mask)
{
  guint32 key;

  mask = mask & 0xff & ~display->ignored_modifier_mask;
  key = key_binding_key (keycode, mask);

  return g_hash_table_lookup (display->key_bindings_index, GINT_TO_POINTER (key));
}

static guint
next_dynamic_keybinding_action (void)
{
  static guint num_dynamic_bindings = 0;
  return META_KEYBINDING_ACTION_LAST + (++num_dynamic_bindings);
}

static gboolean
add_keybinding_internal (MetaDisplay          *display,
                         const char           *name,
                         GSettings            *settings,
                         MetaKeyBindingFlags   flags,
                         MetaKeyBindingAction  action,
                         MetaKeyHandlerFunc    func,
                         int                   data,
                         gpointer              user_data,
                         GDestroyNotify        free_data)
{
  MetaKeyHandler *handler;

  if (!meta_prefs_add_keybinding (name, settings, action, flags))
    return FALSE;

  handler = g_new0 (MetaKeyHandler, 1);
  handler->name = g_strdup (name);
  handler->func = func;
  handler->default_func = func;
  handler->data = data;
  handler->flags = flags;
  handler->user_data = user_data;
  handler->user_data_free_func = free_data;

  g_hash_table_insert (key_handlers, g_strdup (name), handler);

  return TRUE;
}

static gboolean
add_builtin_keybinding (MetaDisplay          *display,
                        const char           *name,
                        GSettings            *settings,
                        MetaKeyBindingFlags   flags,
                        MetaKeyBindingAction  action,
                        MetaKeyHandlerFunc    handler,
                        int                   handler_arg)
{
  return add_keybinding_internal (display, name, settings,
                                  flags | META_KEY_BINDING_BUILTIN,
                                  action, handler, handler_arg, NULL, NULL);
}

/**
 * meta_display_add_keybinding:
 * @display: a #MetaDisplay
 * @name: the binding's name
 * @settings: the #GSettings object where @name is stored
 * @flags: flags to specify binding details
 * @handler: function to run when the keybinding is invoked
 * @user_data: the data to pass to @handler
 * @free_data: function to free @user_data
 *
 * Add a keybinding at runtime. The key @name in @schema needs to be of
 * type %G_VARIANT_TYPE_STRING_ARRAY, with each string describing a
 * keybinding in the form of "&lt;Control&gt;a" or "&lt;Shift&gt;&lt;Alt&gt;F1". The parser
 * is fairly liberal and allows lower or upper case, and also abbreviations
 * such as "&lt;Ctl&gt;" and "&lt;Ctrl&gt;". If the key is set to the empty list or a
 * list with a single element of either "" or "disabled", the keybinding is
 * disabled.
 * If %META_KEY_BINDING_REVERSES is specified in @flags, the binding
 * may be reversed by holding down the "shift" key; therefore, "&lt;Shift&gt;"
 * cannot be one of the keys used. @handler is expected to check for the
 * "shift" modifier in this case and reverse its action.
 *
 * Use meta_display_remove_keybinding() to remove the binding.
 *
 * Returns: the corresponding keybinding action if the keybinding was
 *          added successfully, otherwise %META_KEYBINDING_ACTION_NONE
 */
guint
meta_display_add_keybinding (MetaDisplay         *display,
                             const char          *name,
                             GSettings           *settings,
                             MetaKeyBindingFlags  flags,
                             MetaKeyHandlerFunc   handler,
                             gpointer             user_data,
                             GDestroyNotify       free_data)
{
  guint new_action = next_dynamic_keybinding_action ();

  if (!add_keybinding_internal (display, name, settings, flags, new_action,
                                handler, 0, user_data, free_data))
    return META_KEYBINDING_ACTION_NONE;

  return new_action;
}

/**
 * meta_display_remove_keybinding:
 * @display: the #MetaDisplay
 * @name: name of the keybinding to remove
 *
 * Remove keybinding @name; the function will fail if @name is not a known
 * keybinding or has not been added with meta_display_add_keybinding().
 *
 * Returns: %TRUE if the binding has been removed sucessfully,
 *          otherwise %FALSE
 */
gboolean
meta_display_remove_keybinding (MetaDisplay *display,
                                const char  *name)
{
  if (!meta_prefs_remove_keybinding (name))
    return FALSE;

  g_hash_table_remove (key_handlers, name);

  return TRUE;
}

/**
 * meta_display_get_keybinding_action:
 * @display: A #MetaDisplay
 * @keycode: Raw keycode
 * @mask: Event mask
 *
 * Get the keybinding action bound to @keycode. Builtin keybindings
 * have a fixed associated #MetaKeyBindingAction, for bindings added
 * dynamically the function will return the keybinding action
 * meta_display_add_keybinding() returns on registration.
 *
 * Returns: The action that should be taken for the given key, or
 * %META_KEYBINDING_ACTION_NONE.
 */
guint
meta_display_get_keybinding_action (MetaDisplay  *display,
                                    unsigned int  keycode,
                                    unsigned long mask)
{
  MetaKeyBinding *binding;

  /* This is much more vague than the MetaDisplay::overlay-key signal,
   * which is only emitted if the overlay-key is the only key pressed;
   * as this method is primarily intended for plugins to allow processing
   * of mutter keybindings while holding a grab, the overlay-key-only-pressed
   * tracking is left to the plugin here.
   */
  if (keycode == (unsigned int)display->overlay_key_combo.keycode)
    return META_KEYBINDING_ACTION_OVERLAY_KEY;

  binding = display_get_keybinding (display, keycode, mask);

  if (binding)
    {
      MetaKeyGrab *grab = g_hash_table_lookup (external_grabs, binding->name);
      if (grab)
        return grab->action;
      else
        return (guint) meta_prefs_get_keybinding_action (binding->name);
    }
  else
    {
      return META_KEYBINDING_ACTION_NONE;
    }
}

void
meta_display_process_mapping_event (MetaDisplay *display,
                                    XEvent      *event)
{
  gboolean keymap_changed = FALSE;
  gboolean modmap_changed = FALSE;

#ifdef HAVE_XKB
  if (event->type == display->xkb_base_event_type)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "XKB mapping changed, will redo keybindings\n");

      keymap_changed = TRUE;
      modmap_changed = TRUE;
    }
  else
#endif
  if (event->xmapping.request == MappingModifier)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Received MappingModifier event, will reload modmap and redo keybindings\n");

      modmap_changed = TRUE;
    }
  else if (event->xmapping.request == MappingKeyboard)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Received MappingKeyboard event, will reload keycodes and redo keybindings\n");

      keymap_changed = TRUE;
    }

  /* Now to do the work itself */

  if (keymap_changed || modmap_changed)
    {
      ungrab_key_bindings (display);

      if (keymap_changed)
        reload_keymap (display);

      /* Deciphering the modmap depends on the loaded keysyms to find out
       * what modifiers is Super and so forth, so we need to reload it
       * even when only the keymap changes */
      reload_modmap (display);

      if (keymap_changed)
        reload_keycodes (display);

      reload_modifiers (display);

      rebuild_binding_index (display);

      grab_key_bindings (display);
    }
}

static void
bindings_changed_callback (MetaPreference pref,
                           void          *data)
{
  MetaDisplay *display;

  display = data;

  switch (pref)
    {
    case META_PREF_KEYBINDINGS:
      ungrab_key_bindings (display);
      rebuild_key_binding_table (display);
      rebuild_special_bindings (display);
      reload_keycodes (display);
      reload_modifiers (display);
      rebuild_binding_index (display);
      grab_key_bindings (display);
      break;
    default:
      break;
    }
}


void
meta_display_shutdown_keys (MetaDisplay *display)
{
  /* Note that display->xdisplay is invalid in this function */

  meta_prefs_remove_listener (bindings_changed_callback, display);

  if (display->keymap)
    meta_XFree (display->keymap);

  g_hash_table_destroy (display->key_bindings_index);
  g_hash_table_destroy (display->key_bindings);
}

/* Grab/ungrab, ignoring all annoying modifiers like NumLock etc. */
static void
meta_change_keygrab (MetaDisplay *display,
                     Window       xwindow,
                     gboolean     grab,
                     int          keysym,
                     unsigned int keycode,
                     int          modmask)
{
  unsigned int ignored_mask;

  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

  XISetMask (mask.mask, XI_KeyPress);
  XISetMask (mask.mask, XI_KeyRelease);

  /* Grab keycode/modmask, together with
   * all combinations of ignored modifiers.
   * X provides no better way to do this.
   */

  meta_topic (META_DEBUG_KEYBINDINGS,
              "%s keybinding %s keycode %d mask 0x%x on 0x%lx\n",
              grab ? "Grabbing" : "Ungrabbing",
              keysym_name (keysym), keycode,
              modmask, xwindow);

  /* efficiency, avoid so many XSync() */
  meta_error_trap_push (display);

  ignored_mask = 0;
  while (ignored_mask <= display->ignored_modifier_mask)
    {
      XIGrabModifiers mods;

      if (ignored_mask & ~(display->ignored_modifier_mask))
        {
          /* Not a combination of ignored modifiers
           * (it contains some non-ignored modifiers)
           */
          ++ignored_mask;
          continue;
        }

      mods = (XIGrabModifiers) { modmask | ignored_mask, 0 };

      if (meta_is_debugging ())
        meta_error_trap_push (display);
      if (grab)
        XIGrabKeycode (display->xdisplay,
                       META_VIRTUAL_CORE_KEYBOARD_ID,
                       keycode, xwindow,
                       XIGrabModeSync, XIGrabModeAsync,
                       False, &mask, 1, &mods);
      else
        XIUngrabKeycode (display->xdisplay,
                         META_VIRTUAL_CORE_KEYBOARD_ID,
                         keycode, xwindow, 1, &mods);

      if (meta_is_debugging ())
        {
          int result;

          result = meta_error_trap_pop_with_return (display);

          if (grab && result != Success)
            {
              if (result == BadAccess)
                meta_warning ("Some other program is already using the key %s with modifiers %x as a binding\n", keysym_name (keysym), modmask | ignored_mask);
              else
                meta_topic (META_DEBUG_KEYBINDINGS,
                            "Failed to grab key %s with modifiers %x\n",
                            keysym_name (keysym), modmask | ignored_mask);
            }
        }

      ++ignored_mask;
    }

  meta_error_trap_pop (display);
}

typedef struct
{
  MetaDisplay *display;
  Window xwindow;
  gboolean binding_per_window;
  gboolean grab;
} ChangeKeygrabData;

static void
change_keygrab_foreach (gpointer key,
                        gpointer value,
                        gpointer user_data)
{
  ChangeKeygrabData *data = user_data;
  MetaKeyBinding *binding = value;

  if (!!data->binding_per_window ==
      !!(binding->flags & META_KEY_BINDING_PER_WINDOW) &&
      binding->keycode != 0)
    {
      meta_change_keygrab (data->display, data->xwindow, data->grab,
                           binding->keysym,
                           binding->keycode,
                           binding->mask);
    }
}

static void
change_binding_keygrabs (MetaDisplay    *display,
                         Window          xwindow,
                         gboolean        binding_per_window,
                         gboolean        grab)
{
  ChangeKeygrabData data;

  data.display = display;
  data.xwindow = xwindow;
  data.binding_per_window = binding_per_window;
  data.grab = grab;

  meta_error_trap_push (display);
  g_hash_table_foreach (display->key_bindings, change_keygrab_foreach, &data);
  meta_error_trap_pop (display);
}

static void
meta_screen_change_keygrabs (MetaScreen *screen,
                             gboolean    grab)
{
  MetaDisplay *display = screen->display;

  if (display->overlay_key_combo.keycode != 0)
    meta_change_keygrab (display, screen->xroot, grab,
                         display->overlay_key_combo.keysym,
                         display->overlay_key_combo.keycode,
                         display->overlay_key_combo.modifiers);

  if (display->iso_next_group_combos)
    {
      int i = 0;
      while (i < display->n_iso_next_group_combos)
        {
          if (display->iso_next_group_combos[i].keycode != 0)
            {
              meta_change_keygrab (display, screen->xroot, grab,
                                   display->iso_next_group_combos[i].keysym,
                                   display->iso_next_group_combos[i].keycode,
                                   display->iso_next_group_combos[i].modifiers);
            }
          ++i;
        }
    }

  change_binding_keygrabs (screen->display, screen->xroot, FALSE, grab);
}

void
meta_screen_grab_keys (MetaScreen *screen)
{
  if (screen->all_keys_grabbed)
    return;

  if (screen->keys_grabbed)
    return;

  meta_screen_change_keygrabs (screen, TRUE);

  screen->keys_grabbed = TRUE;
}

void
meta_screen_ungrab_keys (MetaScreen  *screen)
{
  if (!screen->keys_grabbed)
    return;

  meta_screen_change_keygrabs (screen, FALSE);

  screen->keys_grabbed = FALSE;
}

static void
meta_window_change_keygrabs (MetaWindow *window,
                             Window      xwindow,
                             gboolean    grab)
{
  change_binding_keygrabs (window->display, xwindow, TRUE, grab);
}

void
meta_window_grab_keys (MetaWindow  *window)
{
  if (window->all_keys_grabbed)
    return;

  if (window->type == META_WINDOW_DOCK
      || window->override_redirect)
    {
      if (window->keys_grabbed)
        meta_window_change_keygrabs (window, window->xwindow, FALSE);
      window->keys_grabbed = FALSE;
      return;
    }

  if (window->keys_grabbed)
    {
      if (window->frame && !window->grab_on_frame)
        meta_window_change_keygrabs (window, window->xwindow, FALSE);
      else if (window->frame == NULL &&
               window->grab_on_frame)
        ; /* continue to regrab on client window */
      else
        return; /* already all good */
    }

  meta_window_change_keygrabs (window,
                               meta_window_get_toplevel_xwindow (window),
                               TRUE);

  window->keys_grabbed = TRUE;
  window->grab_on_frame = window->frame != NULL;
}

void
meta_window_ungrab_keys (MetaWindow  *window)
{
  if (window->keys_grabbed)
    {
      if (window->grab_on_frame &&
          window->frame != NULL)
        meta_window_change_keygrabs (window, window->frame->xwindow, FALSE);
      else if (!window->grab_on_frame)
        meta_window_change_keygrabs (window, window->xwindow, FALSE);

      window->keys_grabbed = FALSE;
    }
}

static void
handle_external_grab (MetaDisplay     *display,
                      MetaScreen      *screen,
                      MetaWindow      *window,
                      ClutterKeyEvent *event,
                      MetaKeyBinding  *binding,
                      gpointer         user_data)
{
  guint action = meta_display_get_keybinding_action (display,
                                                     binding->keycode,
                                                     binding->mask);
  meta_display_accelerator_activate (display, action, event);
}


guint
meta_display_grab_accelerator (MetaDisplay *display,
                               const char  *accelerator)
{
  MetaKeyBinding *binding;
  MetaKeyGrab *grab;
  guint keysym = 0;
  guint keycode = 0;
  guint mask = 0;
  MetaVirtualModifier modifiers = 0;

  if (!meta_parse_accelerator (accelerator, &keysym, &keycode, &modifiers))
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Failed to parse accelerator\n");
      meta_warning ("\"%s\" is not a valid accelerator\n", accelerator);

      return META_KEYBINDING_ACTION_NONE;
    }

  meta_display_devirtualize_modifiers (display, modifiers, &mask);
  keycode = get_first_keycode_for_keysym (display, keysym);

  if (keycode == 0)
    return META_KEYBINDING_ACTION_NONE;

  if (display_get_keybinding (display, keycode, mask))
    return META_KEYBINDING_ACTION_NONE;

  meta_change_keygrab (display, display->screen->xroot, TRUE, keysym, keycode, mask);

  grab = g_new0 (MetaKeyGrab, 1);
  grab->action = next_dynamic_keybinding_action ();
  grab->name = meta_external_binding_name_for_action (grab->action);
  grab->combo = g_malloc0 (sizeof (MetaKeyCombo));
  grab->combo->keysym = keysym;
  grab->combo->keycode = keycode;
  grab->combo->modifiers = modifiers;

  g_hash_table_insert (external_grabs, grab->name, grab);

  binding = g_malloc0 (sizeof (MetaKeyBinding));
  binding->name = grab->name;
  binding->handler = HANDLER ("external-grab");
  binding->keysym = grab->combo->keysym;
  binding->keycode = grab->combo->keycode;
  binding->modifiers = grab->combo->modifiers;
  binding->mask = mask;

  g_hash_table_add (display->key_bindings, binding);
  index_binding (display, binding);

  return grab->action;
}

gboolean
meta_display_ungrab_accelerator (MetaDisplay *display,
                                 guint        action)
{
  MetaKeyBinding *binding;
  MetaKeyGrab *grab;
  char *key;
  guint mask = 0;
  guint keycode = 0;

  g_return_val_if_fail (action != META_KEYBINDING_ACTION_NONE, FALSE);

  key = meta_external_binding_name_for_action (action);
  grab = g_hash_table_lookup (external_grabs, key);
  if (!grab)
    return FALSE;

  meta_display_devirtualize_modifiers (display, grab->combo->modifiers, &mask);
  keycode = get_first_keycode_for_keysym (display, grab->combo->keysym);

  binding = display_get_keybinding (display, keycode, mask);
  if (binding)
    {
      guint32 index_key;

      meta_change_keygrab (display, display->screen->xroot, FALSE,
                           binding->keysym,
                           binding->keycode,
                           binding->mask);

      index_key = key_binding_key (binding->keycode, binding->mask);
      g_hash_table_remove (display->key_bindings_index, GINT_TO_POINTER (index_key));

      g_hash_table_remove (display->key_bindings, binding);
    }

  g_hash_table_remove (external_grabs, key);
  g_free (key);

  return TRUE;
}

#ifdef WITH_VERBOSE_MODE
static const char*
grab_status_to_string (int status)
{
  switch (status)
    {
    case AlreadyGrabbed:
      return "AlreadyGrabbed";
    case GrabSuccess:
      return "GrabSuccess";
    case GrabNotViewable:
      return "GrabNotViewable";
    case GrabFrozen:
      return "GrabFrozen";
    case GrabInvalidTime:
      return "GrabInvalidTime";
    default:
      return "(unknown)";
    }
}
#endif /* WITH_VERBOSE_MODE */

static gboolean
grab_keyboard (MetaDisplay *display,
               Window       xwindow,
               guint32      timestamp,
               int          grab_mode)
{
  int result;
  int grab_status;

  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

  XISetMask (mask.mask, XI_KeyPress);
  XISetMask (mask.mask, XI_KeyRelease);

  /* Grab the keyboard, so we get key releases and all key
   * presses
   */
  meta_error_trap_push (display);

  /* Strictly, we only need to set grab_mode on the keyboard device
   * while the pointer should always be XIGrabModeAsync. Unfortunately
   * there is a bug in the X server, only fixed (link below) in 1.15,
   * which swaps these arguments for keyboard devices. As such, we set
   * both the device and the paired device mode which works around
   * that bug and also works on fixed X servers.
   *
   * http://cgit.freedesktop.org/xorg/xserver/commit/?id=9003399708936481083424b4ff8f18a16b88b7b3
   */
  grab_status = XIGrabDevice (display->xdisplay,
                              META_VIRTUAL_CORE_KEYBOARD_ID,
                              xwindow,
                              timestamp,
                              None,
                              grab_mode, grab_mode,
                              False, /* owner_events */
                              &mask);

  if (grab_status != Success)
    {
      meta_error_trap_pop_with_return (display);
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "XIGrabDevice() returned failure status %s time %u\n",
                  grab_status_to_string (grab_status),
                  timestamp);
      return FALSE;
    }
  else
    {
      result = meta_error_trap_pop_with_return (display);
      if (result != Success)
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "XIGrabDevice() resulted in an error\n");
          return FALSE;
        }
    }

  meta_topic (META_DEBUG_KEYBINDINGS, "Grabbed all keys\n");

  return TRUE;
}

static void
ungrab_keyboard (MetaDisplay *display, guint32 timestamp)
{
  meta_error_trap_push (display);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Ungrabbing keyboard with timestamp %u\n",
              timestamp);
  XIUngrabDevice (display->xdisplay, META_VIRTUAL_CORE_KEYBOARD_ID, timestamp);
  meta_error_trap_pop (display);
}

gboolean
meta_screen_grab_all_keys (MetaScreen *screen, guint32 timestamp)
{
  gboolean retval;

  if (screen->all_keys_grabbed)
    return FALSE;

  if (screen->keys_grabbed)
    meta_screen_ungrab_keys (screen);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Grabbing all keys on RootWindow\n");
  retval = grab_keyboard (screen->display, screen->xroot, timestamp, XIGrabModeAsync);
  if (retval)
    {
      screen->all_keys_grabbed = TRUE;
      g_object_notify (G_OBJECT (screen), "keyboard-grabbed");
    }
  else
    meta_screen_grab_keys (screen);

  return retval;
}

void
meta_screen_ungrab_all_keys (MetaScreen *screen, guint32 timestamp)
{
  if (screen->all_keys_grabbed)
    {
      ungrab_keyboard (screen->display, timestamp);

      screen->all_keys_grabbed = FALSE;
      screen->keys_grabbed = FALSE;

      /* Re-establish our standard bindings */
      meta_screen_grab_keys (screen);
      g_object_notify (G_OBJECT (screen), "keyboard-grabbed");
    }
}

gboolean
meta_window_grab_all_keys (MetaWindow  *window,
                           guint32      timestamp)
{
  Window grabwindow;
  gboolean retval;

  /* We don't need to grab Wayland clients */
  if (window->client_type == META_WINDOW_CLIENT_TYPE_WAYLAND)
    return TRUE;

  if (window->all_keys_grabbed)
    return FALSE;

  if (window->keys_grabbed)
    meta_window_ungrab_keys (window);

  /* Make sure the window is focused, otherwise the grab
   * won't do a lot of good.
   */
  meta_topic (META_DEBUG_FOCUS,
              "Focusing %s because we're grabbing all its keys\n",
              window->desc);
  meta_window_focus (window, timestamp);

  grabwindow = meta_window_get_toplevel_xwindow (window);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Grabbing all keys on window %s\n", window->desc);
  retval = grab_keyboard (window->display, grabwindow, timestamp, XIGrabModeAsync);
  if (retval)
    {
      window->keys_grabbed = FALSE;
      window->all_keys_grabbed = TRUE;
      window->grab_on_frame = window->frame != NULL;
    }

  return retval;
}

void
meta_window_ungrab_all_keys (MetaWindow *window, guint32 timestamp)
{
  if (window->all_keys_grabbed)
    {
      ungrab_keyboard (window->display, timestamp);

      window->grab_on_frame = FALSE;
      window->all_keys_grabbed = FALSE;
      window->keys_grabbed = FALSE;

      /* Re-establish our standard bindings */
      meta_window_grab_keys (window);
    }
}

void
meta_display_freeze_keyboard (MetaDisplay *display, Window window, guint32 timestamp)
{
  grab_keyboard (display, window, timestamp, XIGrabModeSync);
}

void
meta_display_ungrab_keyboard (MetaDisplay *display, guint32 timestamp)
{
  ungrab_keyboard (display, timestamp);
}

void
meta_display_unfreeze_keyboard (MetaDisplay *display, guint32 timestamp)
{
  meta_error_trap_push (display);
  XIAllowEvents (display->xdisplay, META_VIRTUAL_CORE_KEYBOARD_ID,
                 XIAsyncDevice, timestamp);
  /* We shouldn't need to unfreeze the pointer device here, however we
   * have to, due to the workaround we do in grab_keyboard().
   */
  XIAllowEvents (display->xdisplay, META_VIRTUAL_CORE_POINTER_ID,
                 XIAsyncDevice, timestamp);
  meta_error_trap_pop (display);
}

static gboolean
is_modifier (xkb_keysym_t keysym)
{
  switch (keysym)
    {
    case XKB_KEY_Shift_L:
    case XKB_KEY_Shift_R:
    case XKB_KEY_Control_L:
    case XKB_KEY_Control_R:
    case XKB_KEY_Caps_Lock:
    case XKB_KEY_Shift_Lock:
    case XKB_KEY_Meta_L:
    case XKB_KEY_Meta_R:
    case XKB_KEY_Alt_L:
    case XKB_KEY_Alt_R:
    case XKB_KEY_Super_L:
    case XKB_KEY_Super_R:
    case XKB_KEY_Hyper_L:
    case XKB_KEY_Hyper_R:
      return TRUE;
    default:
      return FALSE;
    }
}

static void
invoke_handler (MetaDisplay     *display,
                MetaScreen      *screen,
                MetaKeyHandler  *handler,
                MetaWindow      *window,
                ClutterKeyEvent *event,
                MetaKeyBinding  *binding)
{
  if (handler->func)
    (* handler->func) (display, screen,
                       handler->flags & META_KEY_BINDING_PER_WINDOW ?
                       window : NULL,
                       event,
                       binding,
                       handler->user_data);
  else
    (* handler->default_func) (display, screen,
                               handler->flags & META_KEY_BINDING_PER_WINDOW ?
                               window: NULL,
                               event,
                               binding,
                               NULL);
}

static gboolean
process_event (MetaDisplay          *display,
               MetaScreen           *screen,
               MetaWindow           *window,
               ClutterKeyEvent      *event)
{
  MetaKeyBinding *binding;

  /* we used to have release-based bindings but no longer. */
  if (event->type == CLUTTER_KEY_RELEASE)
    return FALSE;

  binding = display_get_keybinding (display,
                                    event->hardware_keycode,
                                    event->modifier_state);
  if (!binding ||
      (!window && binding->flags & META_KEY_BINDING_PER_WINDOW))
    goto not_found;

  /* If the compositor filtered out the keybindings, that
   * means they don't want the binding to trigger, so we do
   * the same thing as if the binding didn't exist. */
  if (meta_compositor_filter_keybinding (display->compositor, binding))
    goto not_found;

  if (binding->handler == NULL)
    meta_bug ("Binding %s has no handler\n", binding->name);
  else
    meta_topic (META_DEBUG_KEYBINDINGS,
                "Running handler for %s\n",
                binding->name);

  /* Global keybindings count as a let-the-terminal-lose-focus
   * due to new window mapping until the user starts
   * interacting with the terminal again.
   */
  display->allow_terminal_deactivation = TRUE;

  invoke_handler (display, screen, binding->handler, window, event, binding);

  return TRUE;

 not_found:
  meta_topic (META_DEBUG_KEYBINDINGS,
              "No handler found for this event in this binding table\n");
  return FALSE;
}

static gboolean
process_overlay_key (MetaDisplay *display,
                     MetaScreen *screen,
                     ClutterKeyEvent *event,
                     MetaWindow *window)
{
  if (display->overlay_key_only_pressed)
    {
      if (event->hardware_keycode != (int)display->overlay_key_combo.keycode)
        {
          display->overlay_key_only_pressed = FALSE;

          /* OK, the user hit modifier+key rather than pressing and
           * releasing the ovelay key. We want to handle the key
           * sequence "normally". Unfortunately, using
           * XAllowEvents(..., ReplayKeyboard, ...) doesn't quite
           * work, since global keybindings won't be activated ("this
           * time, however, the function ignores any passive grabs at
           * above (toward the root of) the grab_window of the grab
           * just released.") So, we first explicitly check for one of
           * our global keybindings, and if not found, we then replay
           * the event. Other clients with global grabs will be out of
           * luck.
           */
          if (process_event (display, screen, window, event))
            {
              /* As normally, after we've handled a global key
               * binding, we unfreeze the keyboard but keep the grab
               * (this is important for something like cycling
               * windows */
              XIAllowEvents (display->xdisplay,
                             clutter_input_device_get_device_id (event->device),
                             XIAsyncDevice, event->time);
            }
          else
            {
              /* Replay the event so it gets delivered to our
               * per-window key bindings or to the application */
              XIAllowEvents (display->xdisplay,
                             clutter_input_device_get_device_id (event->device),
                             XIReplayDevice, event->time);
            }
        }
      else if (event->type == CLUTTER_KEY_RELEASE)
        {
          MetaKeyBinding *binding;

          display->overlay_key_only_pressed = FALSE;

          /* We want to unfreeze events, but keep the grab so that if the user
           * starts typing into the overlay we get all the keys */
          XIAllowEvents (display->xdisplay,
                         clutter_input_device_get_device_id (event->device),
                         XIAsyncDevice, event->time);

          binding = display_get_keybinding (display,
                                            display->overlay_key_combo.keycode,
                                            0);
          if (binding &&
              meta_compositor_filter_keybinding (display->compositor, binding))
            return TRUE;
          meta_display_overlay_key_activate (display);
        }
      else
        {
          /* In some rare race condition, mutter might not receive the Super_L
           * KeyRelease event because:
           * - the compositor might end the modal mode and call XIUngrabDevice
           *   while the key is still down
           * - passive grabs are only activated on KeyPress and not KeyRelease.
           *
           * In this case, display->overlay_key_only_pressed might be wrong.
           * Mutter still ought to acknowledge events, otherwise the X server
           * will not send the next events.
           *
           * https://bugzilla.gnome.org/show_bug.cgi?id=666101
           */
          XIAllowEvents (display->xdisplay,
                         clutter_input_device_get_device_id (event->device),
                         XIAsyncDevice, event->time);
        }

      return TRUE;
    }
  else if (event->type == CLUTTER_KEY_PRESS &&
           event->hardware_keycode == (int)display->overlay_key_combo.keycode)
    {
      display->overlay_key_only_pressed = TRUE;
      /* We keep the keyboard frozen - this allows us to use ReplayKeyboard
       * on the next event if it's not the release of the overlay key */
      XIAllowEvents (display->xdisplay,
                     clutter_input_device_get_device_id (event->device),
                     XISyncDevice, event->time);

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
process_iso_next_group (MetaDisplay *display,
                        MetaScreen *screen,
                        ClutterKeyEvent *event)
{
  gboolean activate;
  int i;

  if (event->type == CLUTTER_KEY_RELEASE)
    return FALSE;

  activate = FALSE;

  for (i = 0; i < display->n_iso_next_group_combos; ++i)
    {
      if (event->hardware_keycode == display->iso_next_group_combos[i].keycode &&
          event->modifier_state == (unsigned int)display->iso_next_group_combos[i].modifiers)
        {
          /* If the signal handler returns TRUE the keyboard will
             remain frozen. It's the signal handler's responsibility
             to unfreeze it. */
          if (!meta_display_modifiers_accelerator_activate (display))
            XIAllowEvents (display->xdisplay,
                           clutter_input_device_get_device_id (event->device),
                           XIAsyncDevice, event->time);
          activate = TRUE;
          break;
        }
    }

  return activate;
}

/* Handle a key event. May be called recursively: some key events cause
 * grabs to be ended and then need to be processed again in their own
 * right. This cannot cause infinite recursion because we never call
 * ourselves when there wasn't a grab, and we always clear the grab
 * first; the invariant is enforced using an assertion. See #112560.
 *
 * The return value is whether we handled the key event.
 *
 * FIXME: We need to prove there are no race conditions here.
 * FIXME: Does it correctly handle alt-Tab being followed by another
 * grabbing keypress without letting go of alt?
 * FIXME: An iterative solution would probably be simpler to understand
 * (and help us solve the other fixmes).
 */
gboolean
meta_display_process_key_event (MetaDisplay     *display,
                                MetaWindow      *window,
                                ClutterKeyEvent *event)
{
  gboolean keep_grab;
  gboolean all_keys_grabbed;
  gboolean handled;
  MetaScreen *screen;

  /* window may be NULL */

  screen = display->screen;

  all_keys_grabbed = window ? window->all_keys_grabbed : screen->all_keys_grabbed;
  if (!all_keys_grabbed)
    {
      handled = process_overlay_key (display, screen, event, window);
      if (handled)
        return TRUE;

      handled = process_iso_next_group (display, screen, event);
      if (handled)
        return TRUE;
    }

  XIAllowEvents (display->xdisplay,
                 clutter_input_device_get_device_id (event->device),
                 XIAsyncDevice, event->time);

  keep_grab = TRUE;
  if (all_keys_grabbed)
    {
      if (display->grab_op == META_GRAB_OP_NONE)
        return TRUE;
      /* If we get here we have a global grab, because
       * we're in some special keyboard mode such as window move
       * mode.
       */
      if ((window && window == display->grab_window) || !window)
        {
          switch (display->grab_op)
            {
            case META_GRAB_OP_MOVING:
            case META_GRAB_OP_RESIZING_SE:
            case META_GRAB_OP_RESIZING_S:
            case META_GRAB_OP_RESIZING_SW:
            case META_GRAB_OP_RESIZING_N:
            case META_GRAB_OP_RESIZING_NE:
            case META_GRAB_OP_RESIZING_NW:
            case META_GRAB_OP_RESIZING_W:
            case META_GRAB_OP_RESIZING_E:
              meta_topic (META_DEBUG_KEYBINDINGS,
                          "Processing event for mouse-only move/resize\n");
              g_assert (window != NULL);
              keep_grab = process_mouse_move_resize_grab (display, screen, window, event);
              break;

            case META_GRAB_OP_KEYBOARD_MOVING:
              meta_topic (META_DEBUG_KEYBINDINGS,
                          "Processing event for keyboard move\n");
              g_assert (window != NULL);
              keep_grab = process_keyboard_move_grab (display, screen, window, event);
              break;

            case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
            case META_GRAB_OP_KEYBOARD_RESIZING_S:
            case META_GRAB_OP_KEYBOARD_RESIZING_N:
            case META_GRAB_OP_KEYBOARD_RESIZING_W:
            case META_GRAB_OP_KEYBOARD_RESIZING_E:
            case META_GRAB_OP_KEYBOARD_RESIZING_SE:
            case META_GRAB_OP_KEYBOARD_RESIZING_NE:
            case META_GRAB_OP_KEYBOARD_RESIZING_SW:
            case META_GRAB_OP_KEYBOARD_RESIZING_NW:
              meta_topic (META_DEBUG_KEYBINDINGS,
                          "Processing event for keyboard resize\n");
              g_assert (window != NULL);
              keep_grab = process_keyboard_resize_grab (display, screen, window, event);
              break;

            default:
              break;
            }
        }
      if (!keep_grab)
        meta_display_end_grab_op (display, event->time);

      return TRUE;
    }

  /* Do the normal keybindings */
  return process_event (display, screen, window, event);
}

static gboolean
process_mouse_move_resize_grab (MetaDisplay     *display,
                                MetaScreen      *screen,
                                MetaWindow      *window,
                                ClutterKeyEvent *event)
{
  /* don't care about releases, but eat them, don't end grab */
  if (event->type == CLUTTER_KEY_RELEASE)
    return TRUE;

  if (event->keyval == CLUTTER_KEY_Escape)
    {
      /* Hide the tiling preview if necessary */
      if (window->tile_mode != META_TILE_NONE)
        meta_screen_hide_tile_preview (screen);

      /* Restore the original tile mode */
      window->tile_mode = display->grab_tile_mode;
      window->tile_monitor_number = display->grab_tile_monitor_number;

      /* End move or resize and restore to original state.  If the
       * window was a maximized window that had been "shaken loose" we
       * need to remaximize it.  In normal cases, we need to do a
       * moveresize now to get the position back to the original.
       */
      if (window->shaken_loose || window->tile_mode == META_TILE_MAXIMIZED)
        meta_window_maximize (window, META_MAXIMIZE_BOTH);
      else if (window->tile_mode != META_TILE_NONE)
        meta_window_tile (window);
      else
        meta_window_move_resize (display->grab_window,
                                 TRUE,
                                 display->grab_initial_window_pos.x,
                                 display->grab_initial_window_pos.y,
                                 display->grab_initial_window_pos.width,
                                 display->grab_initial_window_pos.height);

      /* End grab */
      return FALSE;
    }

  return TRUE;
}

static gboolean
process_keyboard_move_grab (MetaDisplay     *display,
                            MetaScreen      *screen,
                            MetaWindow      *window,
                            ClutterKeyEvent *event)
{
  gboolean handled;
  int x, y;
  int incr;
  gboolean smart_snap;

  handled = FALSE;

  /* don't care about releases, but eat them, don't end grab */
  if (event->type == CLUTTER_KEY_RELEASE)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (event->keyval))
    return TRUE;

  meta_window_get_position (window, &x, &y);

  smart_snap = (event->modifier_state & CLUTTER_SHIFT_MASK) != 0;

#define SMALL_INCREMENT 1
#define NORMAL_INCREMENT 10

  if (smart_snap)
    incr = 1;
  else if (event->modifier_state & CLUTTER_CONTROL_MASK)
    incr = SMALL_INCREMENT;
  else
    incr = NORMAL_INCREMENT;

  if (event->keyval == CLUTTER_KEY_Escape)
    {
      /* End move and restore to original state.  If the window was a
       * maximized window that had been "shaken loose" we need to
       * remaximize it.  In normal cases, we need to do a moveresize
       * now to get the position back to the original.
       */
      if (window->shaken_loose)
        meta_window_maximize (window, META_MAXIMIZE_BOTH);
      else
        meta_window_move_resize (display->grab_window,
                                 TRUE,
                                 display->grab_initial_window_pos.x,
                                 display->grab_initial_window_pos.y,
                                 display->grab_initial_window_pos.width,
                                 display->grab_initial_window_pos.height);
    }

  /* When moving by increments, we still snap to edges if the move
   * to the edge is smaller than the increment. This is because
   * Shift + arrow to snap is sort of a hidden feature. This way
   * people using just arrows shouldn't get too frustrated.
   */
  switch (event->keyval)
    {
    case CLUTTER_KEY_KP_Home:
    case CLUTTER_KEY_KP_Prior:
    case CLUTTER_KEY_Up:
    case CLUTTER_KEY_KP_Up:
      y -= incr;
      handled = TRUE;
      break;
    case CLUTTER_KEY_KP_End:
    case CLUTTER_KEY_KP_Next:
    case CLUTTER_KEY_Down:
    case CLUTTER_KEY_KP_Down:
      y += incr;
      handled = TRUE;
      break;
    }

  switch (event->keyval)
    {
    case CLUTTER_KEY_KP_Home:
    case CLUTTER_KEY_KP_End:
    case CLUTTER_KEY_Left:
    case CLUTTER_KEY_KP_Left:
      x -= incr;
      handled = TRUE;
      break;
    case CLUTTER_KEY_KP_Prior:
    case CLUTTER_KEY_KP_Next:
    case CLUTTER_KEY_Right:
    case CLUTTER_KEY_KP_Right:
      x += incr;
      handled = TRUE;
      break;
    }

  if (handled)
    {
      MetaRectangle old_rect;
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Computed new window location %d,%d due to keypress\n",
                  x, y);

      meta_window_get_client_root_coords (window, &old_rect);

      meta_window_edge_resistance_for_move (window,
                                            old_rect.x,
                                            old_rect.y,
                                            &x,
                                            &y,
                                            NULL,
                                            smart_snap,
                                            TRUE);

      meta_window_move (window, TRUE, x, y);
      meta_window_update_keyboard_move (window);
    }

  return handled;
}

static gboolean
process_keyboard_resize_grab_op_change (MetaDisplay     *display,
                                        MetaScreen      *screen,
                                        MetaWindow      *window,
                                        ClutterKeyEvent *event)
{
  gboolean handled;

  handled = FALSE;
  switch (display->grab_op)
    {
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
      switch (event->keyval)
        {
        case CLUTTER_KEY_Up:
        case CLUTTER_KEY_KP_Up:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_N;
          handled = TRUE;
          break;
        case CLUTTER_KEY_Down:
        case CLUTTER_KEY_KP_Down:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_S;
          handled = TRUE;
          break;
        case CLUTTER_KEY_Left:
        case CLUTTER_KEY_KP_Left:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          handled = TRUE;
          break;
        case CLUTTER_KEY_Right:
        case CLUTTER_KEY_KP_Right:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_S:
      switch (event->keyval)
        {
        case CLUTTER_KEY_Left:
        case CLUTTER_KEY_KP_Left:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          handled = TRUE;
          break;
        case CLUTTER_KEY_Right:
        case CLUTTER_KEY_KP_Right:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_N:
      switch (event->keyval)
        {
        case CLUTTER_KEY_Left:
        case CLUTTER_KEY_KP_Left:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          handled = TRUE;
          break;
        case CLUTTER_KEY_Right:
        case CLUTTER_KEY_KP_Right:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_W:
      switch (event->keyval)
        {
        case CLUTTER_KEY_Up:
        case CLUTTER_KEY_KP_Up:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_N;
          handled = TRUE;
          break;
        case CLUTTER_KEY_Down:
        case CLUTTER_KEY_KP_Down:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_S;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_E:
      switch (event->keyval)
        {
        case CLUTTER_KEY_Up:
        case CLUTTER_KEY_KP_Up:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_N;
          handled = TRUE;
          break;
        case CLUTTER_KEY_Down:
        case CLUTTER_KEY_KP_Down:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_S;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (handled)
    {
      meta_window_update_keyboard_resize (window, TRUE);
      return TRUE;
    }

  return FALSE;
}

static gboolean
process_keyboard_resize_grab (MetaDisplay     *display,
                              MetaScreen      *screen,
                              MetaWindow      *window,
                              ClutterKeyEvent *event)
{
  gboolean handled;
  int height_inc;
  int width_inc;
  int width, height;
  gboolean smart_snap;
  int gravity;

  handled = FALSE;

  /* don't care about releases, but eat them, don't end grab */
  if (event->type == CLUTTER_KEY_RELEASE)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (event->keyval))
    return TRUE;

  if (event->keyval == CLUTTER_KEY_Escape)
    {
      /* End resize and restore to original state. */
      meta_window_move_resize (display->grab_window,
                               TRUE,
                               display->grab_initial_window_pos.x,
                               display->grab_initial_window_pos.y,
                               display->grab_initial_window_pos.width,
                               display->grab_initial_window_pos.height);

      return FALSE;
    }

  if (process_keyboard_resize_grab_op_change (display, screen, window, event))
    return TRUE;

  width = window->rect.width;
  height = window->rect.height;

  gravity = meta_resize_gravity_from_grab_op (display->grab_op);

  smart_snap = (event->modifier_state & CLUTTER_SHIFT_MASK) != 0;

#define SMALL_INCREMENT 1
#define NORMAL_INCREMENT 10

  if (smart_snap)
    {
      height_inc = 1;
      width_inc = 1;
    }
  else if (event->modifier_state & CLUTTER_CONTROL_MASK)
    {
      width_inc = SMALL_INCREMENT;
      height_inc = SMALL_INCREMENT;
    }
  else
    {
      width_inc = NORMAL_INCREMENT;
      height_inc = NORMAL_INCREMENT;
    }

  /* If this is a resize increment window, make the amount we resize
   * the window by match that amount (well, unless snap resizing...)
   */
  if (window->size_hints.width_inc > 1)
    width_inc = window->size_hints.width_inc;
  if (window->size_hints.height_inc > 1)
    height_inc = window->size_hints.height_inc;

  switch (event->keyval)
    {
    case CLUTTER_KEY_Up:
    case CLUTTER_KEY_KP_Up:
      switch (gravity)
        {
        case NorthGravity:
        case NorthWestGravity:
        case NorthEastGravity:
          /* Move bottom edge up */
          height -= height_inc;
          break;

        case SouthGravity:
        case SouthWestGravity:
        case SouthEastGravity:
          /* Move top edge up */
          height += height_inc;
          break;

        case EastGravity:
        case WestGravity:
        case CenterGravity:
          g_assert_not_reached ();
          break;
        }

      handled = TRUE;
      break;

    case CLUTTER_KEY_Down:
    case CLUTTER_KEY_KP_Down:
      switch (gravity)
        {
        case NorthGravity:
        case NorthWestGravity:
        case NorthEastGravity:
          /* Move bottom edge down */
          height += height_inc;
          break;

        case SouthGravity:
        case SouthWestGravity:
        case SouthEastGravity:
          /* Move top edge down */
          height -= height_inc;
          break;

        case EastGravity:
        case WestGravity:
        case CenterGravity:
          g_assert_not_reached ();
          break;
        }

      handled = TRUE;
      break;

    case CLUTTER_KEY_Left:
    case CLUTTER_KEY_KP_Left:
      switch (gravity)
        {
        case EastGravity:
        case SouthEastGravity:
        case NorthEastGravity:
          /* Move left edge left */
          width += width_inc;
          break;

        case WestGravity:
        case SouthWestGravity:
        case NorthWestGravity:
          /* Move right edge left */
          width -= width_inc;
          break;

        case NorthGravity:
        case SouthGravity:
        case CenterGravity:
          g_assert_not_reached ();
          break;
        }

      handled = TRUE;
      break;

    case CLUTTER_KEY_Right:
    case CLUTTER_KEY_KP_Right:
      switch (gravity)
        {
        case EastGravity:
        case SouthEastGravity:
        case NorthEastGravity:
          /* Move left edge right */
          width -= width_inc;
          break;

        case WestGravity:
        case SouthWestGravity:
        case NorthWestGravity:
          /* Move right edge right */
          width += width_inc;
          break;

        case NorthGravity:
        case SouthGravity:
        case CenterGravity:
          g_assert_not_reached ();
          break;
        }

      handled = TRUE;
      break;

    default:
      break;
    }

  /* fixup hack (just paranoia, not sure it's required) */
  if (height < 1)
    height = 1;
  if (width < 1)
    width = 1;

  if (handled)
    {
      MetaRectangle old_rect;
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Computed new window size due to keypress: "
                  "%dx%d, gravity %s\n",
                  width, height, meta_gravity_to_string (gravity));

      old_rect = window->rect;  /* Don't actually care about x,y */

      /* Do any edge resistance/snapping */
      meta_window_edge_resistance_for_resize (window,
                                              old_rect.width,
                                              old_rect.height,
                                              &width,
                                              &height,
                                              gravity,
                                              NULL,
                                              smart_snap,
                                              TRUE);

      /* We don't need to update unless the specified width and height
       * are actually different from what we had before.
       */
      if (window->rect.width != width || window->rect.height != height)
        meta_window_resize_with_gravity (window,
                                         TRUE,
                                         width,
                                         height,
                                         gravity);

      meta_window_update_keyboard_resize (window, FALSE);
    }

  return handled;
}

static void
handle_switch_to_workspace (MetaDisplay     *display,
                            MetaScreen      *screen,
                            MetaWindow      *event_window,
                            ClutterKeyEvent *event,
                            MetaKeyBinding  *binding,
                            gpointer         dummy)
{
  gint which = binding->handler->data;
  MetaWorkspace *workspace;

  workspace = meta_screen_get_workspace_by_index (screen, which);

  if (workspace)
    {
      meta_workspace_activate (workspace, event->time);
    }
  else
    {
      /* We could offer to create it I suppose */
    }
}


static void
handle_maximize_vertically (MetaDisplay     *display,
                            MetaScreen      *screen,
                            MetaWindow      *window,
                            ClutterKeyEvent *event,
                            MetaKeyBinding  *binding,
                            gpointer         dummy)
{
  if (window->has_resize_func)
    {
      if (window->maximized_vertically)
        meta_window_unmaximize (window, META_MAXIMIZE_VERTICAL);
      else
        meta_window_maximize (window, META_MAXIMIZE_VERTICAL);
    }
}

static void
handle_maximize_horizontally (MetaDisplay     *display,
                              MetaScreen      *screen,
                              MetaWindow      *window,
                              ClutterKeyEvent *event,
                              MetaKeyBinding  *binding,
                              gpointer         dummy)
{
  if (window->has_resize_func)
    {
      if (window->maximized_horizontally)
        meta_window_unmaximize (window, META_MAXIMIZE_HORIZONTAL);
      else
        meta_window_maximize (window, META_MAXIMIZE_HORIZONTAL);
    }
}

static void
handle_always_on_top (MetaDisplay     *display,
                      MetaScreen      *screen,
                      MetaWindow      *window,
                      ClutterKeyEvent *event,
                      MetaKeyBinding  *binding,
                      gpointer         dummy)
{
  if (window->wm_state_above == FALSE)
    meta_window_make_above (window);
  else
    meta_window_unmake_above (window);
}

/* Move a window to a corner; to_bottom/to_right are FALSE for the
 * top or left edge, or TRUE for the bottom/right edge.  xchange/ychange
 * are FALSE if that dimension is not to be changed, TRUE otherwise.
 * Together they describe which of the four corners, or four sides,
 * is desired.
 */
static void
handle_move_to_corner_backend (MetaDisplay    *display,
                               MetaScreen     *screen,
                               MetaWindow     *window,
                               gboolean        xchange,
                               gboolean        ychange,
                               gboolean        to_right,
                               gboolean        to_bottom,
                               gpointer        dummy)
{
  MetaRectangle work_area;
  MetaRectangle frame_rect;
  int orig_x, orig_y;
  int new_x, new_y;

  meta_window_get_work_area_all_monitors (window, &work_area);
  meta_window_get_frame_rect (window, &frame_rect);
  meta_window_get_position (window, &orig_x, &orig_y);

  if (xchange) {
    new_x = work_area.x + (to_right ?
                           work_area.width - frame_rect.width :
                           0);
  } else {
    new_x = orig_x;
  }

  if (ychange) {
    new_y = work_area.y + (to_bottom ?
                           work_area.height - frame_rect.height :
                           0);
  } else {
    new_y = orig_y;
  }

  meta_window_move_frame (window,
                          TRUE,
                          new_x,
                          new_y);
}

static void
handle_move_to_corner_nw  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, FALSE, FALSE, dummy);
}

static void
handle_move_to_corner_ne  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, TRUE, FALSE, dummy);
}

static void
handle_move_to_corner_sw  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, FALSE, TRUE, dummy);
}

static void
handle_move_to_corner_se  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, TRUE, TRUE, dummy);
}

static void
handle_move_to_side_n     (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  handle_move_to_corner_backend (display, screen, window, FALSE, TRUE, FALSE, FALSE, dummy);
}

static void
handle_move_to_side_s     (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  handle_move_to_corner_backend (display, screen, window, FALSE, TRUE, FALSE, TRUE, dummy);
}

static void
handle_move_to_side_e     (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, FALSE, TRUE, FALSE, dummy);
}

static void
handle_move_to_side_w     (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, FALSE, FALSE, FALSE, dummy);
}

static void
handle_move_to_center  (MetaDisplay     *display,
                        MetaScreen      *screen,
                        MetaWindow      *window,
                        ClutterKeyEvent *event,
                        MetaKeyBinding  *binding,
                        gpointer         dummy)
{
  MetaRectangle work_area;
  MetaRectangle frame_rect;
  int orig_x, orig_y;
  int frame_width, frame_height;

  meta_window_get_work_area_all_monitors (window, &work_area);
  meta_window_get_frame_rect (window, &frame_rect);
  meta_window_get_position (window, &orig_x, &orig_y);

  frame_width = (window->frame ? window->frame->child_x : 0);
  frame_height = (window->frame ? window->frame->child_y : 0);

  meta_window_move_resize (window,
                           TRUE,
                           work_area.x + (work_area.width +frame_width -frame_rect.width )/2,
                           work_area.y + (work_area.height+frame_height-frame_rect.height)/2,
                           window->rect.width,
                           window->rect.height);
}

static void
handle_show_desktop (MetaDisplay     *display,
                     MetaScreen      *screen,
                     MetaWindow      *window,
                     ClutterKeyEvent *event,
                     MetaKeyBinding  *binding,
                     gpointer         dummy)
{
  if (screen->active_workspace->showing_desktop)
    {
      meta_screen_unshow_desktop (screen);
      meta_workspace_focus_default_window (screen->active_workspace,
                                           NULL,
                                           event->time);
    }
  else
    meta_screen_show_desktop (screen, event->time);
}

static void
handle_panel (MetaDisplay     *display,
              MetaScreen      *screen,
              MetaWindow      *window,
              ClutterKeyEvent *event,
              MetaKeyBinding  *binding,
              gpointer         dummy)
{
  MetaKeyBindingAction action = binding->handler->data;
  Atom action_atom;
  XClientMessageEvent ev;

  action_atom = None;
  switch (action)
    {
      /* FIXME: The numbers are wrong */
    case META_KEYBINDING_ACTION_PANEL_MAIN_MENU:
      action_atom = display->atom__GNOME_PANEL_ACTION_MAIN_MENU;
      break;
    case META_KEYBINDING_ACTION_PANEL_RUN_DIALOG:
      action_atom = display->atom__GNOME_PANEL_ACTION_RUN_DIALOG;
      break;
    default:
      return;
    }

  ev.type = ClientMessage;
  ev.window = screen->xroot;
  ev.message_type = display->atom__GNOME_PANEL_ACTION;
  ev.format = 32;
  ev.data.l[0] = action_atom;
  ev.data.l[1] = event->time;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Sending panel message with timestamp %u, and turning mouse_mode "
              "off due to keybinding press\n", event->time);
  display->mouse_mode = FALSE;

  meta_error_trap_push (display);

  /* Release the grab for the panel before sending the event */
  XUngrabKeyboard (display->xdisplay, event->time);

  XSendEvent (display->xdisplay,
	      screen->xroot,
	      False,
	      StructureNotifyMask,
	      (XEvent*) &ev);

  meta_error_trap_pop (display);
}

static void
handle_activate_window_menu (MetaDisplay     *display,
                             MetaScreen      *screen,
                             MetaWindow      *event_window,
                             ClutterKeyEvent *event,
                             MetaKeyBinding  *binding,
                             gpointer         dummy)
{
  if (display->focus_window)
    {
      int x, y;

      meta_window_get_position (display->focus_window,
                                &x, &y);

      if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
        x += display->focus_window->rect.width;

      meta_window_show_menu (display->focus_window,
                             x, y,
                             0,
                             event->time);
    }
}

static void
do_choose_window (MetaDisplay     *display,
                  MetaScreen      *screen,
                  MetaWindow      *event_window,
                  ClutterKeyEvent *event,
                  MetaKeyBinding  *binding,
                  gboolean         backward)
{
  MetaTabList type = binding->handler->data;
  MetaWindow *initial_selection;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Tab list = %u\n", type);

  /* reverse direction if shift is down */
  if (event->modifier_state & CLUTTER_SHIFT_MASK)
    backward = !backward;

  initial_selection = meta_display_get_tab_next (display,
                                                 type,
                                                 screen->active_workspace,
                                                 NULL,
                                                 backward);

  meta_window_activate (initial_selection, event->time);
}

static void
handle_switch (MetaDisplay     *display,
               MetaScreen      *screen,
               MetaWindow      *event_window,
               ClutterKeyEvent *event,
               MetaKeyBinding  *binding,
               gpointer         dummy)
{
  gint backwards = (binding->handler->flags & META_KEY_BINDING_IS_REVERSED) != 0;
  do_choose_window (display, screen, event_window, event, binding, backwards);
}

static void
handle_cycle (MetaDisplay     *display,
              MetaScreen      *screen,
              MetaWindow      *event_window,
              ClutterKeyEvent *event,
              MetaKeyBinding  *binding,
              gpointer         dummy)
{
  gint backwards = (binding->handler->flags & META_KEY_BINDING_IS_REVERSED) != 0;
  do_choose_window (display, screen, event_window, event, binding, backwards);
}

static void
handle_toggle_fullscreen  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  if (window->fullscreen)
    meta_window_unmake_fullscreen (window);
  else if (window->has_fullscreen_func)
    meta_window_make_fullscreen (window);
}

static void
handle_toggle_above       (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  if (window->wm_state_above)
    meta_window_unmake_above (window);
  else
    meta_window_make_above (window);
}

static void
handle_toggle_tiled (MetaDisplay     *display,
                     MetaScreen      *screen,
                     MetaWindow      *window,
                     ClutterKeyEvent *event,
                     MetaKeyBinding  *binding,
                     gpointer         dummy)
{
  MetaTileMode mode = binding->handler->data;

  if ((META_WINDOW_TILED_LEFT (window) && mode == META_TILE_LEFT) ||
      (META_WINDOW_TILED_RIGHT (window) && mode == META_TILE_RIGHT))
    {
      window->tile_monitor_number = window->saved_maximize ? window->monitor->number
        : -1;
      window->tile_mode = window->saved_maximize ? META_TILE_MAXIMIZED
        : META_TILE_NONE;

      if (window->saved_maximize)
        meta_window_maximize (window, META_MAXIMIZE_BOTH);
      else
        meta_window_unmaximize (window, META_MAXIMIZE_BOTH);
    }
  else if (meta_window_can_tile_side_by_side (window))
    {
      window->tile_monitor_number = window->monitor->number;
      window->tile_mode = mode;
      /* Maximization constraints beat tiling constraints, so if the window
       * is maximized, tiling won't have any effect unless we unmaximize it
       * horizontally first; rather than calling meta_window_unmaximize(),
       * we just set the flag and rely on meta_window_tile() syncing it to
       * save an additional roundtrip.
       */
      window->maximized_horizontally = FALSE;
      meta_window_tile (window);
    }
}

static void
handle_toggle_maximized    (MetaDisplay     *display,
                            MetaScreen      *screen,
                            MetaWindow      *window,
                            ClutterKeyEvent *event,
                            MetaKeyBinding  *binding,
                            gpointer         dummy)
{
  if (META_WINDOW_MAXIMIZED (window))
    meta_window_unmaximize (window, META_MAXIMIZE_BOTH);
  else if (window->has_maximize_func)
    meta_window_maximize (window, META_MAXIMIZE_BOTH);
}

static void
handle_maximize           (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  if (window->has_maximize_func)
    meta_window_maximize (window, META_MAXIMIZE_BOTH);
}

static void
handle_unmaximize         (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  if (window->maximized_vertically || window->maximized_horizontally)
    meta_window_unmaximize (window, META_MAXIMIZE_BOTH);
}

static void
handle_toggle_shaded      (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  if (window->shaded)
    meta_window_unshade (window, event->time);
  else if (window->has_shade_func)
    meta_window_shade (window, event->time);
}

static void
handle_close              (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  if (window->has_close_func)
    meta_window_delete (window, event->time);
}

static void
handle_minimize        (MetaDisplay     *display,
                        MetaScreen      *screen,
                        MetaWindow      *window,
                        ClutterKeyEvent *event,
                        MetaKeyBinding  *binding,
                        gpointer         dummy)
{
  if (window->has_minimize_func)
    meta_window_minimize (window);
}

static void
handle_begin_move         (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  if (window->has_move_func)
    {
      meta_window_begin_grab_op (window,
                                 META_GRAB_OP_KEYBOARD_MOVING,
                                 FALSE,
                                 event->time);
    }
}

static void
handle_begin_resize       (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  if (window->has_resize_func)
    {
      meta_window_begin_grab_op (window,
                                 META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN,
                                 FALSE,
                                 event->time);
    }
}

static void
handle_toggle_on_all_workspaces (MetaDisplay     *display,
                                 MetaScreen      *screen,
                                 MetaWindow      *window,
                                 ClutterKeyEvent *event,
                                 MetaKeyBinding  *binding,
                                 gpointer         dummy)
{
  if (window->on_all_workspaces_requested)
    meta_window_unstick (window);
  else
    meta_window_stick (window);
}

static void
handle_move_to_workspace  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           ClutterKeyEvent *event,
                           MetaKeyBinding  *binding,
                           gpointer         dummy)
{
  gint which = binding->handler->data;
  gboolean flip = (which < 0);
  MetaWorkspace *workspace;

  /* If which is zero or positive, it's a workspace number, and the window
   * should move to the workspace with that number.
   *
   * However, if it's negative, it's a direction with respect to the current
   * position; it's expressed as a member of the MetaMotionDirection enum,
   * all of whose members are negative.  Such a change is called a flip.
   */

  if (window->always_sticky)
    return;

  workspace = NULL;
  if (flip)
    {
      workspace = meta_workspace_get_neighbor (screen->active_workspace,
                                               which);
    }
  else
    {
      workspace = meta_screen_get_workspace_by_index (screen, which);
    }

  if (workspace)
    {
      /* Activate second, so the window is never unmapped */
      meta_window_change_workspace (window, workspace);
      if (flip)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Resetting mouse_mode to FALSE due to "
                      "handle_move_to_workspace() call with flip set.\n");
          meta_display_clear_mouse_mode (workspace->screen->display);
          meta_workspace_activate_with_focus (workspace,
                                              window,
                                              event->time);
        }
    }
  else
    {
      /* We could offer to create it I suppose */
    }
}

static void
handle_move_to_monitor (MetaDisplay    *display,
                        MetaScreen     *screen,
                        MetaWindow     *window,
		        ClutterKeyEvent *event,
                        MetaKeyBinding *binding,
                        gpointer        dummy)
{
  gint which = binding->handler->data;
  const MetaMonitorInfo *current, *new;

  current = meta_screen_get_monitor_for_window (screen, window);
  new = meta_screen_get_monitor_neighbor (screen, current->number, which);

  if (new == NULL)
    return;

  meta_window_move_to_monitor (window, new->number);
}

static void
handle_raise_or_lower (MetaDisplay     *display,
                       MetaScreen      *screen,
		       MetaWindow      *window,
		       ClutterKeyEvent *event,
		       MetaKeyBinding  *binding,
                       gpointer         dummy)
{
  /* Get window at pointer */

  MetaWindow *above = NULL;

  /* Check if top */
  if (meta_stack_get_top (window->screen->stack) == window)
    {
      meta_window_lower (window);
      return;
    }

  /* else check if windows in same layer are intersecting it */

  above = meta_stack_get_above (window->screen->stack, window, TRUE);

  while (above)
    {
      MetaRectangle tmp, win_rect, above_rect;

      if (above->mapped)
        {
          meta_window_get_frame_rect (window, &win_rect);
          meta_window_get_frame_rect (above, &above_rect);

          /* Check if obscured */
          if (meta_rectangle_intersect (&win_rect, &above_rect, &tmp))
            {
              meta_window_raise (window);
              return;
            }
        }

      above = meta_stack_get_above (window->screen->stack, above, TRUE);
    }

  /* window is not obscured */
  meta_window_lower (window);
}

static void
handle_raise (MetaDisplay     *display,
              MetaScreen      *screen,
              MetaWindow      *window,
              ClutterKeyEvent *event,
              MetaKeyBinding  *binding,
              gpointer         dummy)
{
  meta_window_raise (window);
}

static void
handle_lower (MetaDisplay     *display,
              MetaScreen      *screen,
              MetaWindow      *window,
              ClutterKeyEvent *event,
              MetaKeyBinding  *binding,
              gpointer         dummy)
{
  meta_window_lower (window);
}

static void
handle_set_spew_mark (MetaDisplay     *display,
                      MetaScreen      *screen,
                      MetaWindow      *window,
                      ClutterKeyEvent *event,
                      MetaKeyBinding  *binding,
                      gpointer         dummy)
{
  meta_verbose ("-- MARK MARK MARK MARK --\n");
}

static void
handle_switch_vt (MetaDisplay     *display,
                  MetaScreen      *screen,
                  MetaWindow      *window,
                  ClutterKeyEvent *event,
                  MetaKeyBinding  *binding,
                  gpointer         dummy)
{
  gint vt = binding->handler->data;
  GError *error = NULL;

  if (!meta_activate_vt (vt, &error))
    {
      g_warning ("Failed to switch VT: %s", error->message);
      g_error_free (error);
    }
}

/**
 * meta_keybindings_set_custom_handler:
 * @name: The name of the keybinding to set
 * @handler: (allow-none): The new handler function
 * @user_data: User data to pass to the callback
 * @free_data: Will be called when this handler is overridden.
 *
 * Allows users to register a custom handler for a
 * builtin key binding.
 *
 * Returns: %TRUE if the binding known as @name was found,
 * %FALSE otherwise.
 */
gboolean
meta_keybindings_set_custom_handler (const gchar        *name,
                                     MetaKeyHandlerFunc  handler,
                                     gpointer            user_data,
                                     GDestroyNotify      free_data)
{
  MetaKeyHandler *key_handler = HANDLER (name);

  if (!key_handler)
    return FALSE;

  if (key_handler->user_data_free_func && key_handler->user_data)
    key_handler->user_data_free_func (key_handler->user_data);

  key_handler->func = handler;
  key_handler->user_data = user_data;
  key_handler->user_data_free_func = free_data;

  return TRUE;
}

static void
init_builtin_key_bindings (MetaDisplay *display)
{
#define REVERSES_AND_REVERSED (META_KEY_BINDING_REVERSES |      \
                               META_KEY_BINDING_IS_REVERSED)
  GSettings *common_keybindings = g_settings_new (SCHEMA_COMMON_KEYBINDINGS);
  GSettings *mutter_keybindings = g_settings_new (SCHEMA_MUTTER_KEYBINDINGS);
  GSettings *mutter_wayland_keybindings = g_settings_new (SCHEMA_MUTTER_WAYLAND_KEYBINDINGS);

  add_builtin_keybinding (display,
                          "switch-to-workspace-1",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_1,
                          handle_switch_to_workspace, 0);
  add_builtin_keybinding (display,
                          "switch-to-workspace-2",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_2,
                          handle_switch_to_workspace, 1);
  add_builtin_keybinding (display,
                          "switch-to-workspace-3",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_3,
                          handle_switch_to_workspace, 2);
  add_builtin_keybinding (display,
                          "switch-to-workspace-4",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_4,
                          handle_switch_to_workspace, 3);
  add_builtin_keybinding (display,
                          "switch-to-workspace-5",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_5,
                          handle_switch_to_workspace, 4);
  add_builtin_keybinding (display,
                          "switch-to-workspace-6",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_6,
                          handle_switch_to_workspace, 5);
  add_builtin_keybinding (display,
                          "switch-to-workspace-7",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_7,
                          handle_switch_to_workspace, 6);
  add_builtin_keybinding (display,
                          "switch-to-workspace-8",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_8,
                          handle_switch_to_workspace, 7);
  add_builtin_keybinding (display,
                          "switch-to-workspace-9",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_9,
                          handle_switch_to_workspace, 8);
  add_builtin_keybinding (display,
                          "switch-to-workspace-10",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_10,
                          handle_switch_to_workspace, 9);
  add_builtin_keybinding (display,
                          "switch-to-workspace-11",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_11,
                          handle_switch_to_workspace, 10);
  add_builtin_keybinding (display,
                          "switch-to-workspace-12",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_12,
                          handle_switch_to_workspace, 11);

  add_builtin_keybinding (display,
                          "switch-to-workspace-left",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_LEFT,
                          NULL, 0);

  add_builtin_keybinding (display,
                          "switch-to-workspace-right",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_RIGHT,
                          NULL, 0);

  add_builtin_keybinding (display,
                          "switch-to-workspace-up",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_UP,
                          NULL, 0);

  add_builtin_keybinding (display,
                          "switch-to-workspace-down",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_DOWN,
                          NULL, 0);


  /* The ones which have inverses.  These can't be bound to any keystroke
   * containing Shift because Shift will invert their "backward" state.
   *
   * TODO: "NORMAL" and "DOCKS" should be renamed to the same name as their
   * action, for obviousness.
   *
   * TODO: handle_switch and handle_cycle should probably really be the
   * same function checking a bit in the parameter for difference.
   */

  add_builtin_keybinding (display,
                          "switch-group",
                          common_keybindings,
                          META_KEY_BINDING_REVERSES,
                          META_KEYBINDING_ACTION_SWITCH_GROUP,
                          handle_switch, META_TAB_LIST_GROUP);

  add_builtin_keybinding (display,
                          "switch-group-backward",
                          common_keybindings,
                          REVERSES_AND_REVERSED,
                          META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD,
                          handle_switch, META_TAB_LIST_GROUP);

  add_builtin_keybinding (display,
                          "switch-applications",
                          common_keybindings,
                          META_KEY_BINDING_REVERSES,
                          META_KEYBINDING_ACTION_SWITCH_APPLICATIONS,
                          handle_switch, META_TAB_LIST_NORMAL);

  add_builtin_keybinding (display,
                          "switch-applications-backward",
                          common_keybindings,
                          REVERSES_AND_REVERSED,
                          META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD,
                          handle_switch, META_TAB_LIST_NORMAL);

  add_builtin_keybinding (display,
                          "switch-windows",
                          common_keybindings,
                          META_KEY_BINDING_REVERSES,
                          META_KEYBINDING_ACTION_SWITCH_WINDOWS,
                          handle_switch, META_TAB_LIST_NORMAL);

  add_builtin_keybinding (display,
                          "switch-windows-backward",
                          common_keybindings,
                          REVERSES_AND_REVERSED,
                          META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD,
                          handle_switch, META_TAB_LIST_NORMAL);

  add_builtin_keybinding (display,
                          "switch-panels",
                          common_keybindings,
                          META_KEY_BINDING_REVERSES,
                          META_KEYBINDING_ACTION_SWITCH_PANELS,
                          handle_switch, META_TAB_LIST_DOCKS);

  add_builtin_keybinding (display,
                          "switch-panels-backward",
                          common_keybindings,
                          REVERSES_AND_REVERSED,
                          META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD,
                          handle_switch, META_TAB_LIST_DOCKS);

  add_builtin_keybinding (display,
                          "cycle-group",
                          common_keybindings,
                          META_KEY_BINDING_REVERSES,
                          META_KEYBINDING_ACTION_CYCLE_GROUP,
                          handle_cycle, META_TAB_LIST_GROUP);

  add_builtin_keybinding (display,
                          "cycle-group-backward",
                          common_keybindings,
                          REVERSES_AND_REVERSED,
                          META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD,
                          handle_cycle, META_TAB_LIST_GROUP);

  add_builtin_keybinding (display,
                          "cycle-windows",
                          common_keybindings,
                          META_KEY_BINDING_REVERSES,
                          META_KEYBINDING_ACTION_CYCLE_WINDOWS,
                          handle_cycle, META_TAB_LIST_NORMAL);

  add_builtin_keybinding (display,
                          "cycle-windows-backward",
                          common_keybindings,
                          REVERSES_AND_REVERSED,
                          META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD,
                          handle_cycle, META_TAB_LIST_NORMAL);

  add_builtin_keybinding (display,
                          "cycle-panels",
                          common_keybindings,
                          META_KEY_BINDING_REVERSES,
                          META_KEYBINDING_ACTION_CYCLE_PANELS,
                          handle_cycle, META_TAB_LIST_DOCKS);

  add_builtin_keybinding (display,
                          "cycle-panels-backward",
                          common_keybindings,
                          REVERSES_AND_REVERSED,
                          META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD,
                          handle_cycle, META_TAB_LIST_DOCKS);

  /***********************************/

  add_builtin_keybinding (display,
                          "show-desktop",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_SHOW_DESKTOP,
                          handle_show_desktop, 0);

  add_builtin_keybinding (display,
                          "panel-main-menu",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_PANEL_MAIN_MENU,
                          handle_panel, META_KEYBINDING_ACTION_PANEL_MAIN_MENU);

  add_builtin_keybinding (display,
                          "panel-run-dialog",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_PANEL_RUN_DIALOG,
                          handle_panel, META_KEYBINDING_ACTION_PANEL_RUN_DIALOG);

  add_builtin_keybinding (display,
                          "set-spew-mark",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_SET_SPEW_MARK,
                          handle_set_spew_mark, 0);

  if (meta_is_wayland_compositor ())
    {
      add_builtin_keybinding (display,
                              "switch-to-session-1",
                              mutter_wayland_keybindings,
                              META_KEY_BINDING_NONE,
                              META_KEYBINDING_ACTION_NONE,
                              handle_switch_vt, 1);

      add_builtin_keybinding (display,
                              "switch-to-session-2",
                              mutter_wayland_keybindings,
                              META_KEY_BINDING_NONE,
                              META_KEYBINDING_ACTION_NONE,
                              handle_switch_vt, 2);

      add_builtin_keybinding (display,
                              "switch-to-session-3",
                              mutter_wayland_keybindings,
                              META_KEY_BINDING_NONE,
                              META_KEYBINDING_ACTION_NONE,
                              handle_switch_vt, 3);

      add_builtin_keybinding (display,
                              "switch-to-session-4",
                              mutter_wayland_keybindings,
                              META_KEY_BINDING_NONE,
                              META_KEYBINDING_ACTION_NONE,
                              handle_switch_vt, 4);

      add_builtin_keybinding (display,
                              "switch-to-session-5",
                              mutter_wayland_keybindings,
                              META_KEY_BINDING_NONE,
                              META_KEYBINDING_ACTION_NONE,
                              handle_switch_vt, 5);

      add_builtin_keybinding (display,
                              "switch-to-session-6",
                              mutter_wayland_keybindings,
                              META_KEY_BINDING_NONE,
                              META_KEYBINDING_ACTION_NONE,
                              handle_switch_vt, 6);

      add_builtin_keybinding (display,
                              "switch-to-session-7",
                              mutter_wayland_keybindings,
                              META_KEY_BINDING_NONE,
                              META_KEYBINDING_ACTION_NONE,
                              handle_switch_vt, 7);
    }

#undef REVERSES_AND_REVERSED

  /************************ PER WINDOW BINDINGS ************************/

  /* These take a window as an extra parameter; they have no effect
   * if no window is active.
   */

  add_builtin_keybinding (display,
                          "activate-window-menu",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_ACTIVATE_WINDOW_MENU,
                          handle_activate_window_menu, 0);

  add_builtin_keybinding (display,
                          "toggle-fullscreen",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_TOGGLE_FULLSCREEN,
                          handle_toggle_fullscreen, 0);

  add_builtin_keybinding (display,
                          "toggle-maximized",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_TOGGLE_MAXIMIZED,
                          handle_toggle_maximized, 0);

  add_builtin_keybinding (display,
                          "toggle-tiled-left",
                          mutter_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_TOGGLE_TILED_LEFT,
                          handle_toggle_tiled, META_TILE_LEFT);

  add_builtin_keybinding (display,
                          "toggle-tiled-right",
                          mutter_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_TOGGLE_TILED_RIGHT,
                          handle_toggle_tiled, META_TILE_RIGHT);

  add_builtin_keybinding (display,
                          "toggle-above",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_TOGGLE_ABOVE,
                          handle_toggle_above, 0);

  add_builtin_keybinding (display,
                          "maximize",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MAXIMIZE,
                          handle_maximize, 0);

  add_builtin_keybinding (display,
                          "unmaximize",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_UNMAXIMIZE,
                          handle_unmaximize, 0);

  add_builtin_keybinding (display,
                          "toggle-shaded",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_TOGGLE_SHADED,
                          handle_toggle_shaded, 0);

  add_builtin_keybinding (display,
                          "minimize",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MINIMIZE,
                          handle_minimize, 0);

  add_builtin_keybinding (display,
                          "close",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_CLOSE,
                          handle_close, 0);

  add_builtin_keybinding (display,
                          "begin-move",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_BEGIN_MOVE,
                          handle_begin_move, 0);

  add_builtin_keybinding (display,
                          "begin-resize",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_BEGIN_RESIZE,
                          handle_begin_resize, 0);

  add_builtin_keybinding (display,
                          "toggle-on-all-workspaces",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_TOGGLE_ON_ALL_WORKSPACES,
                          handle_toggle_on_all_workspaces, 0);

  add_builtin_keybinding (display,
                          "move-to-workspace-1",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_1,
                          handle_move_to_workspace, 0);

  add_builtin_keybinding (display,
                          "move-to-workspace-2",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_2,
                          handle_move_to_workspace, 1);

  add_builtin_keybinding (display,
                          "move-to-workspace-3",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_3,
                          handle_move_to_workspace, 2);

  add_builtin_keybinding (display,
                          "move-to-workspace-4",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_4,
                          handle_move_to_workspace, 3);

  add_builtin_keybinding (display,
                          "move-to-workspace-5",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_5,
                          handle_move_to_workspace, 4);

  add_builtin_keybinding (display,
                          "move-to-workspace-6",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_6,
                          handle_move_to_workspace, 5);

  add_builtin_keybinding (display,
                          "move-to-workspace-7",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_7,
                          handle_move_to_workspace, 6);

  add_builtin_keybinding (display,
                          "move-to-workspace-8",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_8,
                          handle_move_to_workspace, 7);

  add_builtin_keybinding (display,
                          "move-to-workspace-9",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_9,
                          handle_move_to_workspace, 8);

  add_builtin_keybinding (display,
                          "move-to-workspace-10",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_10,
                          handle_move_to_workspace, 9);

  add_builtin_keybinding (display,
                          "move-to-workspace-11",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_11,
                          handle_move_to_workspace, 10);

  add_builtin_keybinding (display,
                          "move-to-workspace-12",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_12,
                          handle_move_to_workspace, 11);

  add_builtin_keybinding (display,
                          "move-to-workspace-left",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_LEFT,
                          handle_move_to_workspace, META_MOTION_LEFT);

  add_builtin_keybinding (display,
                          "move-to-workspace-right",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_RIGHT,
                          handle_move_to_workspace, META_MOTION_RIGHT);

  add_builtin_keybinding (display,
                          "move-to-workspace-up",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_UP,
                          handle_move_to_workspace, META_MOTION_UP);

  add_builtin_keybinding (display,
                          "move-to-workspace-down",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_DOWN,
                          handle_move_to_workspace, META_MOTION_DOWN);

  add_builtin_keybinding (display,
                          "move-to-monitor-left",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_MONITOR_LEFT,
                          handle_move_to_monitor, META_SCREEN_LEFT);

  add_builtin_keybinding (display,
                          "move-to-monitor-right",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_MONITOR_RIGHT,
                          handle_move_to_monitor, META_SCREEN_RIGHT);

  add_builtin_keybinding (display,
                          "move-to-monitor-down",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_MONITOR_DOWN,
                          handle_move_to_monitor, META_SCREEN_DOWN);

  add_builtin_keybinding (display,
                          "move-to-monitor-up",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_MONITOR_UP,
                          handle_move_to_monitor, META_SCREEN_UP);

  add_builtin_keybinding (display,
                          "raise-or-lower",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_RAISE_OR_LOWER,
                          handle_raise_or_lower, 0);

  add_builtin_keybinding (display,
                          "raise",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_RAISE,
                          handle_raise, 0);

  add_builtin_keybinding (display,
                          "lower",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_LOWER,
                          handle_lower, 0);

  add_builtin_keybinding (display,
                          "maximize-vertically",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MAXIMIZE_VERTICALLY,
                          handle_maximize_vertically, 0);

  add_builtin_keybinding (display,
                          "maximize-horizontally",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MAXIMIZE_HORIZONTALLY,
                          handle_maximize_horizontally, 0);

  add_builtin_keybinding (display,
                          "always-on-top",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_ALWAYS_ON_TOP,
                          handle_always_on_top, 0);

  add_builtin_keybinding (display,
                          "move-to-corner-nw",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_CORNER_NW,
                          handle_move_to_corner_nw, 0);

  add_builtin_keybinding (display,
                          "move-to-corner-ne",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_CORNER_NE,
                          handle_move_to_corner_ne, 0);

  add_builtin_keybinding (display,
                          "move-to-corner-sw",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_CORNER_SW,
                          handle_move_to_corner_sw, 0);

  add_builtin_keybinding (display,
                          "move-to-corner-se",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_CORNER_SE,
                          handle_move_to_corner_se, 0);

  add_builtin_keybinding (display,
                          "move-to-side-n",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_SIDE_N,
                          handle_move_to_side_n, 0);

  add_builtin_keybinding (display,
                          "move-to-side-s",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_SIDE_S,
                          handle_move_to_side_s, 0);

  add_builtin_keybinding (display,
                          "move-to-side-e",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_SIDE_E,
                          handle_move_to_side_e, 0);

  add_builtin_keybinding (display,
                          "move-to-side-w",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_SIDE_W,
                          handle_move_to_side_w, 0);

  add_builtin_keybinding (display,
                          "move-to-center",
                          common_keybindings,
                          META_KEY_BINDING_PER_WINDOW,
                          META_KEYBINDING_ACTION_MOVE_TO_CENTER,
                          handle_move_to_center, 0);

  g_object_unref (common_keybindings);
  g_object_unref (mutter_keybindings);
  g_object_unref (mutter_wayland_keybindings);
}

void
meta_display_init_keys (MetaDisplay *display)
{
  MetaKeyHandler *handler;

  /* Keybindings */
  display->keymap = NULL;
  display->keysyms_per_keycode = 0;
  display->min_keycode = 0;
  display->max_keycode = 0;
  display->ignored_modifier_mask = 0;
  display->hyper_mask = 0;
  display->super_mask = 0;
  display->meta_mask = 0;

  display->key_bindings = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  display->key_bindings_index = g_hash_table_new (NULL, NULL);

  XDisplayKeycodes (display->xdisplay,
                    &display->min_keycode,
                    &display->max_keycode);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Display has keycode range %d to %d\n",
              display->min_keycode,
              display->max_keycode);

  reload_keymap (display);
  reload_modmap (display);

  key_handlers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                        (GDestroyNotify) key_handler_free);

  handler = g_new0 (MetaKeyHandler, 1);
  handler->name = g_strdup ("overlay-key");
  handler->flags = META_KEY_BINDING_BUILTIN;

  g_hash_table_insert (key_handlers, g_strdup ("overlay-key"), handler);

  handler = g_new0 (MetaKeyHandler, 1);
  handler->name = g_strdup ("iso-next-group");
  handler->flags = META_KEY_BINDING_BUILTIN;

  g_hash_table_insert (key_handlers, g_strdup ("iso-next-group"), handler);

  handler = g_new0 (MetaKeyHandler, 1);
  handler->name = g_strdup ("external-grab");
  handler->func = handle_external_grab;
  handler->default_func = handle_external_grab;

  g_hash_table_insert (key_handlers, g_strdup ("external-grab"), handler);

  external_grabs = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          NULL,
                                          (GDestroyNotify)meta_key_grab_free);

  init_builtin_key_bindings (display);

  rebuild_key_binding_table (display);
  rebuild_special_bindings (display);

  reload_keycodes (display);
  reload_modifiers (display);
  rebuild_binding_index (display);

  /* Keys are actually grabbed in meta_screen_grab_keys() */

  meta_prefs_add_listener (bindings_changed_callback, display);

#ifdef HAVE_XKB
  /* meta_display_init_keys() should have already called XkbQueryExtension() */
  if (display->xkb_base_event_type != -1)
    XkbSelectEvents (display->xdisplay, XkbUseCoreKbd,
                     XkbNewKeyboardNotifyMask | XkbMapNotifyMask,
                     XkbNewKeyboardNotifyMask | XkbMapNotifyMask);
#endif
}
