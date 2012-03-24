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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/**
 * SECTION:keybindings
 * @Title: MetaKeybinding
 * @Short_Description: Key bindings
 */

#define _GNU_SOURCE
#define _SVID_SOURCE /* for putenv() */

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
#include <meta/util.h>

#include <X11/keysym.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#define SCHEMA_COMMON_KEYBINDINGS "org.gnome.desktop.wm.keybindings"
#define SCHEMA_MUTTER_KEYBINDINGS "org.gnome.mutter.keybindings"

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

static void handle_workspace_switch  (MetaDisplay    *display,
                                      MetaScreen     *screen,
                                      MetaWindow     *window,
                                      XIDeviceEvent  *event,
                                      MetaKeyBinding *binding,
                                      gpointer        dummy);

static gboolean process_mouse_move_resize_grab (MetaDisplay   *display,
                                                MetaScreen    *screen,
                                                MetaWindow    *window,
                                                XIDeviceEvent *event,
                                                KeySym         keysym);

static gboolean process_keyboard_move_grab (MetaDisplay   *display,
                                            MetaScreen    *screen,
                                            MetaWindow    *window,
                                            XIDeviceEvent *event,
                                            KeySym         keysym);

static gboolean process_keyboard_resize_grab (MetaDisplay   *display,
                                              MetaScreen    *screen,
                                              MetaWindow    *window,
                                              XIDeviceEvent *event,
                                              KeySym         keysym);

static gboolean process_tab_grab           (MetaDisplay   *display,
                                            MetaScreen    *screen,
                                            XIDeviceEvent *event,
                                            KeySym         keysym);

static gboolean process_workspace_switch_grab (MetaDisplay   *display,
                                               MetaScreen    *screen,
                                               XIDeviceEvent *event,
                                               KeySym         keysym);

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

static void
reload_modmap (MetaDisplay *display)
{
  XModifierKeymap *modmap;
  int map_size;
  int i;

  if (display->modmap)
    XFreeModifiermap (display->modmap);

  modmap = XGetModifierMapping (display->xdisplay);
  display->modmap = modmap;

  display->ignored_modifier_mask = 0;

  /* Multiple bits may get set in each of these */
  display->num_lock_mask = 0;
  display->scroll_lock_mask = 0;
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
                  const char *str;

                  str = XKeysymToString (syms[j]);
                  meta_topic (META_DEBUG_KEYBINDINGS,
                              "Keysym %s bound to modifier 0x%x\n",
                              str ? str : "none",
                              (1 << ( i / modmap->max_keypermod)));
                }

              if (syms[j] == XK_Num_Lock)
                {
                  /* Mod1Mask is 1 << 3 for example, i.e. the
                   * fourth modifier, i / keyspermod is the modifier
                   * index
                   */

                  display->num_lock_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XK_Scroll_Lock)
                {
                  display->scroll_lock_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XK_Super_L ||
                       syms[j] == XK_Super_R)
                {
                  display->super_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XK_Hyper_L ||
                       syms[j] == XK_Hyper_R)
                {
                  display->hyper_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XK_Meta_L ||
                       syms[j] == XK_Meta_R)
                {
                  display->meta_mask |= (1 << ( i / modmap->max_keypermod));
                }

              ++j;
            }
        }

      ++i;
    }

  display->ignored_modifier_mask = (display->num_lock_mask |
                                    display->scroll_lock_mask |
                                    LockMask);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Ignoring modmask 0x%x num lock 0x%x scroll lock 0x%x hyper 0x%x super 0x%x meta 0x%x\n",
              display->ignored_modifier_mask,
              display->num_lock_mask,
              display->scroll_lock_mask,
              display->hyper_mask,
              display->super_mask,
              display->meta_mask);
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

  n_keycodes = retval->len;
  *keycodes = (int*) g_array_free (retval, n_keycodes == 0 ? TRUE : FALSE);

  return n_keycodes;
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

  n_keycodes = get_keycodes_for_keysym (display, XK_ISO_Next_Group, &keycodes);

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
          combos[i].keysym = XK_ISO_Next_Group;
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
          combos[i].keysym = XK_ISO_Next_Group;
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
          combos[i].keysym = XK_ISO_Next_Group;
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
          combos[i].keysym = XK_ISO_Next_Group;
          combos[i].keycode = keycodes[i];
          combos[i].modifiers = ShiftMask;

          combos[i + n_keycodes].keysym = XK_ISO_Next_Group;
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
          combos[i].keysym = XK_ISO_Next_Group;
          combos[i].keycode = keycodes[i];
          combos[i].modifiers = Mod1Mask;

          combos[i + n_keycodes].keysym = XK_ISO_Next_Group;
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
          combos[i].keysym = XK_ISO_Next_Group;
          combos[i].keycode = keycodes[i];
          combos[i].modifiers = Mod1Mask;

          combos[i + n_keycodes].keysym = XK_ISO_Next_Group;
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

static guint
keysym_to_keycode (MetaDisplay *display,
                   guint        keysym)
{
  if (keysym == META_KEY_ABOVE_TAB)
    return meta_display_get_above_tab_keycode (display);
  else
    return XKeysymToKeycode (display->xdisplay, keysym);
}

static void
reload_keycodes (MetaDisplay *display)
{
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Reloading keycodes for binding tables\n");

  if (display->overlay_key_combo.keysym != 0)
    {
      display->overlay_key_combo.keycode =
        keysym_to_keycode (display, display->overlay_key_combo.keysym);
    }
  else
    {
      display->overlay_key_combo.keycode = 0;
    }

  reload_iso_next_group_combos (display);

  if (display->key_bindings)
    {
      int i;

      i = 0;
      while (i < display->n_key_bindings)
        {
          if (display->key_bindings[i].keysym != 0)
            {
              display->key_bindings[i].keycode =
                keysym_to_keycode (display, display->key_bindings[i].keysym);
            }

          ++i;
        }
    }
}

static void
reload_modifiers (MetaDisplay *display)
{
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Reloading keycodes for binding tables\n");

  if (display->key_bindings)
    {
      int i;

      i = 0;
      while (i < display->n_key_bindings)
        {
          meta_display_devirtualize_modifiers (display,
                                               display->key_bindings[i].modifiers,
                                               &display->key_bindings[i].mask);

          meta_topic (META_DEBUG_KEYBINDINGS,
                      " Devirtualized mods 0x%x -> 0x%x (%s)\n",
                      display->key_bindings[i].modifiers,
                      display->key_bindings[i].mask,
                      display->key_bindings[i].name);

          ++i;
        }
    }
}


static int
count_bindings (GList *prefs)
{
  GList *p;
  int count;

  count = 0;
  p = prefs;
  while (p)
    {
      MetaKeyPref *pref = (MetaKeyPref*)p->data;
      GSList *tmp = pref->bindings;

      while (tmp)
        {
          MetaKeyCombo *combo = tmp->data;

          if (combo && (combo->keysym != None || combo->keycode != 0))
            {
              count += 1;

              if (pref->add_shift &&
                  (combo->modifiers & META_VIRTUAL_SHIFT_MASK) == 0)
                count += 1;
            }

          tmp = tmp->next;
        }

      p = p->next;
    }

  return count;
}

static void
rebuild_binding_table (MetaDisplay     *display,
                       MetaKeyBinding **bindings_p,
                       int             *n_bindings_p,
                       GList           *prefs,
                       GList           *grabs)
{
  GList *p, *g;
  int n_bindings;
  int i;

  n_bindings = count_bindings (prefs) + g_list_length (grabs);
  g_free (*bindings_p);
  *bindings_p = g_new0 (MetaKeyBinding, n_bindings);

  i = 0;
  p = prefs;
  while (p)
    {
      MetaKeyPref *pref = (MetaKeyPref*)p->data;
      GSList *tmp = pref->bindings;

      while (tmp)
        {
          MetaKeyCombo *combo = tmp->data;

          if (combo && (combo->keysym != None || combo->keycode != 0))
            {
              MetaKeyHandler *handler = HANDLER (pref->name);

              (*bindings_p)[i].name = pref->name;
              (*bindings_p)[i].handler = handler;
              (*bindings_p)[i].keysym = combo->keysym;
              (*bindings_p)[i].keycode = combo->keycode;
              (*bindings_p)[i].modifiers = combo->modifiers;
              (*bindings_p)[i].mask = 0;

              ++i;

              if (pref->add_shift &&
                  (combo->modifiers & META_VIRTUAL_SHIFT_MASK) == 0)
                {
                  meta_topic (META_DEBUG_KEYBINDINGS,
                              "Binding %s also needs Shift grabbed\n",
                              pref->name);

                  (*bindings_p)[i].name = pref->name;
                  (*bindings_p)[i].handler = handler;
                  (*bindings_p)[i].keysym = combo->keysym;
                  (*bindings_p)[i].keycode = combo->keycode;
                  (*bindings_p)[i].modifiers = combo->modifiers |
                    META_VIRTUAL_SHIFT_MASK;
                  (*bindings_p)[i].mask = 0;

                  ++i;
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

          (*bindings_p)[i].name = grab->name;
          (*bindings_p)[i].handler = handler;
          (*bindings_p)[i].keysym = grab->combo->keysym;
          (*bindings_p)[i].keycode = grab->combo->keycode;
          (*bindings_p)[i].modifiers = grab->combo->modifiers;
          (*bindings_p)[i].mask = 0;

          ++i;
        }

      g = g->next;
    }

  g_assert (i == n_bindings);

  *n_bindings_p = i;

  meta_topic (META_DEBUG_KEYBINDINGS,
              " %d bindings in table\n",
              *n_bindings_p);
}

static void
rebuild_key_binding_table (MetaDisplay *display)
{
  GList *prefs, *grabs;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Rebuilding key binding table from preferences\n");

  prefs = meta_prefs_get_keybindings ();
  grabs = g_hash_table_get_values (external_grabs);
  rebuild_binding_table (display,
                         &display->key_bindings,
                         &display->n_key_bindings,
                         prefs, grabs);
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

  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;

      meta_screen_ungrab_keys (screen);

      tmp = tmp->next;
    }

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

  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;

      meta_screen_grab_keys (screen);

      tmp = tmp->next;
    }

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
                        unsigned int  keysym,
                        unsigned int  keycode,
                        unsigned long mask)
{
  int i;

  i = display->n_key_bindings - 1;
  while (i >= 0)
    {
      if (display->key_bindings[i].keysym == keysym &&
          display->key_bindings[i].keycode == keycode &&
          display->key_bindings[i].mask == mask)
        {
          return &display->key_bindings[i];
        }

      --i;
    }

  return NULL;
}

static guint
next_dynamic_keybinding_action () {
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
  KeySym keysym;

  /* This is much more vague than the MetaDisplay::overlay-key signal,
   * which is only emitted if the overlay-key is the only key pressed;
   * as this method is primarily intended for plugins to allow processing
   * of mutter keybindings while holding a grab, the overlay-key-only-pressed
   * tracking is left to the plugin here.
   */
  if (keycode == (unsigned int)display->overlay_key_combo.keycode)
    return META_KEYBINDING_ACTION_OVERLAY_KEY;

  keysym = XKeycodeToKeysym (display->xdisplay, keycode, 0);
  mask = mask & 0xff & ~display->ignored_modifier_mask;
  binding = display_get_keybinding (display, keysym, keycode, mask);

  if (!binding && keycode == meta_display_get_above_tab_keycode (display))
    binding = display_get_keybinding (display, META_KEY_ABOVE_TAB, keycode, mask);

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

  if (display->modmap)
    XFreeModifiermap (display->modmap);
  g_free (display->key_bindings);
}

static const char*
keysym_name (int keysym)
{
  const char *name;

  name = XKeysymToString (keysym);
  if (name == NULL)
    name = "(unknown)";

  return name;
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
        meta_error_trap_push_with_return (display);
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
                meta_warning (_("Some other program is already using the key %s with modifiers %x as a binding\n"), keysym_name (keysym), modmask | ignored_mask);
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

static void
change_binding_keygrabs (MetaKeyBinding *bindings,
                         int             n_bindings,
                         MetaDisplay    *display,
                         Window          xwindow,
                         gboolean        binding_per_window,
                         gboolean        grab)
{
  int i;

  g_assert (n_bindings == 0 || bindings != NULL);

  meta_error_trap_push (display);

  i = 0;
  while (i < n_bindings)
    {
      if (!!binding_per_window ==
          !!(bindings[i].handler->flags & META_KEY_BINDING_PER_WINDOW) &&
          bindings[i].keycode != 0)
        {
          meta_change_keygrab (display, xwindow, grab,
                               bindings[i].keysym,
                               bindings[i].keycode,
                               bindings[i].mask);
        }

      ++i;
    }

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

  change_binding_keygrabs (screen->display->key_bindings,
                           screen->display->n_key_bindings,
                           screen->display, screen->xroot,
                           FALSE, grab);
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
  change_binding_keygrabs (window->display->key_bindings,
                           window->display->n_key_bindings,
                           window->display, xwindow,
                           TRUE, grab);
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
handle_external_grab (MetaDisplay    *display,
                      MetaScreen     *screen,
                      MetaWindow     *window,
                      XIDeviceEvent  *event,
                      MetaKeyBinding *binding,
                      gpointer        user_data)
{
  guint action = meta_display_get_keybinding_action (display,
                                                     binding->keycode,
                                                     binding->mask);
  meta_display_accelerator_activate (display, action, event->deviceid, event->time);
}


guint
meta_display_grab_accelerator (MetaDisplay *display,
                               const char  *accelerator)
{
  MetaKeyGrab *grab;
  guint keysym = 0;
  guint keycode = 0;
  guint mask = 0;
  MetaVirtualModifier modifiers = 0;
  GSList *l;
  int i;

  if (!meta_ui_parse_accelerator (accelerator, &keysym, &keycode, &modifiers))
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Failed to parse accelerator\n");
      meta_warning (_("\"%s\" is not a valid accelerator\n"), accelerator);

      return META_KEYBINDING_ACTION_NONE;
    }

  meta_display_devirtualize_modifiers (display, modifiers, &mask);
  keycode = keysym_to_keycode (display, keysym);

  if (keycode == 0)
    return META_KEYBINDING_ACTION_NONE;

  for (i = 0; i < display->n_key_bindings; i++)
    if (display->key_bindings[i].keycode == keycode &&
        display->key_bindings[i].mask == mask)
      return META_KEYBINDING_ACTION_NONE;

  for (l = display->screens; l; l = l->next)
    {
      MetaScreen *screen = l->data;
      meta_change_keygrab (display, screen->xroot, TRUE, keysym, keycode, mask);
    }

  grab = g_new0 (MetaKeyGrab, 1);
  grab->action = next_dynamic_keybinding_action ();
  grab->name = meta_external_binding_name_for_action (grab->action);
  grab->combo = g_malloc0 (sizeof (MetaKeyCombo));
  grab->combo->keysym = keysym;
  grab->combo->keycode = keycode;
  grab->combo->modifiers = modifiers;

  g_hash_table_insert (external_grabs, grab->name, grab);

  display->n_key_bindings++;
  display->key_bindings = g_renew (MetaKeyBinding,
                                   display->key_bindings,
                                   display->n_key_bindings);

  MetaKeyBinding *binding = &display->key_bindings[display->n_key_bindings - 1];
  binding->name = grab->name;
  binding->handler = HANDLER ("external-grab");
  binding->keysym = grab->combo->keysym;
  binding->keycode = grab->combo->keycode;
  binding->modifiers = grab->combo->modifiers;
  binding->mask = mask;

  return grab->action;
}

gboolean
meta_display_ungrab_accelerator (MetaDisplay *display,
                                 guint        action)
{
  MetaKeyGrab *grab;
  char *key;
  int i;

  g_return_val_if_fail (action != META_KEYBINDING_ACTION_NONE, FALSE);

  key = meta_external_binding_name_for_action (action);
  grab = g_hash_table_lookup (external_grabs, key);
  if (!grab)
    return FALSE;

  for (i = 0; i < display->n_key_bindings; i++)
    if (display->key_bindings[i].keysym == grab->combo->keysym &&
        display->key_bindings[i].keycode == grab->combo->keycode &&
        display->key_bindings[i].modifiers == grab->combo->modifiers)
      {
        GSList *l;
        for (l = display->screens; l; l = l->next)
          {
            MetaScreen *screen = l->data;
            meta_change_keygrab (display, screen->xroot, FALSE,
                                 display->key_bindings[i].keysym,
                                 display->key_bindings[i].keycode,
                                 display->key_bindings[i].mask);
          }

        display->key_bindings[i].keysym = 0;
        display->key_bindings[i].keycode = 0;
        display->key_bindings[i].modifiers = 0;
        display->key_bindings[i].mask = 0;
        break;
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
  meta_error_trap_push_with_return (display);

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
is_modifier (MetaDisplay *display,
             unsigned int keycode)
{
  int i;
  int map_size;
  gboolean retval = FALSE;

  g_assert (display->modmap);

  map_size = 8 * display->modmap->max_keypermod;
  i = 0;
  while (i < map_size)
    {
      if (keycode == display->modmap->modifiermap[i])
        {
          retval = TRUE;
          break;
        }
      ++i;
    }

  return retval;
}

/* Indexes:
 * shift = 0
 * lock = 1
 * control = 2
 * mod1 = 3
 * mod2 = 4
 * mod3 = 5
 * mod4 = 6
 * mod5 = 7
 */

static gboolean
is_specific_modifier (MetaDisplay *display,
                      unsigned int keycode,
                      unsigned int mask)
{
  int i;
  int end;
  gboolean retval = FALSE;
  int mod_index;

  g_assert (display->modmap);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Checking whether code 0x%x is bound to modifier 0x%x\n",
              keycode, mask);

  mod_index = 0;
  mask = mask >> 1;
  while (mask != 0)
    {
      mod_index += 1;
      mask = mask >> 1;
    }

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Modifier has index %d\n", mod_index);

  end = (mod_index + 1) * display->modmap->max_keypermod;
  i = mod_index * display->modmap->max_keypermod;
  while (i < end)
    {
      if (keycode == display->modmap->modifiermap[i])
        {
          retval = TRUE;
          break;
        }
      ++i;
    }

  return retval;
}

static unsigned int
get_primary_modifier (MetaDisplay *display,
                      unsigned int entire_binding_mask)
{
  /* The idea here is to see if the "main" modifier
   * for Alt+Tab has been pressed/released. So if the binding
   * is Alt+Shift+Tab then releasing Alt is the thing that
   * ends the operation. It's pretty random how we order
   * these.
   */
  unsigned int masks[] = { Mod5Mask, Mod4Mask, Mod3Mask,
                           Mod2Mask, Mod1Mask, ControlMask,
                           ShiftMask, LockMask };

  int i;

  i = 0;
  while (i < (int) G_N_ELEMENTS (masks))
    {
      if (entire_binding_mask & masks[i])
        return masks[i];
      ++i;
    }

  return 0;
}

static gboolean
keycode_is_primary_modifier (MetaDisplay *display,
                             unsigned int keycode,
                             unsigned int entire_binding_mask)
{
  unsigned int primary_modifier;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Checking whether code 0x%x is the primary modifier of mask 0x%x\n",
              keycode, entire_binding_mask);

  primary_modifier = get_primary_modifier (display, entire_binding_mask);
  if (primary_modifier != 0)
    return is_specific_modifier (display, keycode, primary_modifier);
  else
    return FALSE;
}

static gboolean
primary_modifier_still_pressed (MetaDisplay *display,
                                unsigned int entire_binding_mask)
{
  unsigned int primary_modifier;
  double x, y, root_x, root_y;
  Window root, child;
  XIButtonState buttons;
  XIModifierState mods;
  XIGroupState group;
  MetaScreen *random_screen;
  Window      random_xwindow;

  primary_modifier = get_primary_modifier (display, entire_binding_mask);

  random_screen = display->screens->data;
  random_xwindow = random_screen->no_focus_window;
  XIQueryPointer (display->xdisplay,
                  META_VIRTUAL_CORE_POINTER_ID,
                  random_xwindow, /* some random window */
                  &root, &child,
                  &root_x, &root_y,
                  &x, &y,
                  &buttons, &mods, &group);
  free (buttons.mask);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Primary modifier 0x%x full grab mask 0x%x current state 0x%x\n",
              primary_modifier, entire_binding_mask, mods.effective);

  if ((mods.effective & primary_modifier) == 0)
    return FALSE;
  else
    return TRUE;
}

static void
invoke_handler (MetaDisplay    *display,
                MetaScreen     *screen,
                MetaKeyHandler *handler,
                MetaWindow     *window,
                XIDeviceEvent  *event,
                MetaKeyBinding *binding)

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

static void
invoke_handler_by_name (MetaDisplay    *display,
                        MetaScreen     *screen,
                        const char     *handler_name,
                        MetaWindow     *window,
                        XIDeviceEvent  *event)
{
  MetaKeyHandler *handler;

  handler = HANDLER (handler_name);
  if (handler)
    invoke_handler (display, screen, handler, window, event, NULL);
}

static gboolean
process_event (MetaKeyBinding       *bindings,
               int                   n_bindings,
               MetaDisplay          *display,
               MetaScreen           *screen,
               MetaWindow           *window,
               XIDeviceEvent        *event,
               gboolean              on_window)
{
  int i;

  /* we used to have release-based bindings but no longer. */
  if (event->evtype == XI_KeyRelease)
    return FALSE;

  /*
   * TODO: This would be better done with a hash table;
   * it doesn't suit to use O(n) for such a common operation.
   */
  for (i=0; i<n_bindings; i++)
    {
      MetaKeyHandler *handler = bindings[i].handler;

      if ((!on_window && handler->flags & META_KEY_BINDING_PER_WINDOW) ||
          bindings[i].keycode != event->detail ||
          ((event->mods.effective & 0xff & ~(display->ignored_modifier_mask)) !=
           bindings[i].mask) ||
          meta_compositor_filter_keybinding (display->compositor, screen, &bindings[i]))
        continue;

      /*
       * window must be non-NULL for on_window to be true,
       * and so also window must be non-NULL if we get here and
       * this is a META_KEY_BINDING_PER_WINDOW binding.
       */

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Binding keycode 0x%x mask 0x%x matches event 0x%x state 0x%x\n",
                  bindings[i].keycode, bindings[i].mask,
                  event->detail, event->mods.effective);

      if (handler == NULL)
        meta_bug ("Binding %s has no handler\n", bindings[i].name);
      else
        meta_topic (META_DEBUG_KEYBINDINGS,
                    "Running handler for %s\n",
                    bindings[i].name);

      /* Global keybindings count as a let-the-terminal-lose-focus
       * due to new window mapping until the user starts
       * interacting with the terminal again.
       */
      display->allow_terminal_deactivation = TRUE;

      invoke_handler (display, screen, handler, window, event, &bindings[i]);

      return TRUE;
    }

  meta_topic (META_DEBUG_KEYBINDINGS,
              "No handler found for this event in this binding table\n");
  return FALSE;
}

static gboolean
process_overlay_key (MetaDisplay *display,
                     MetaScreen *screen,
                     XIDeviceEvent *event,
                     KeySym keysym)
{
  if (display->overlay_key_only_pressed)
    {
      if (event->detail != (int)display->overlay_key_combo.keycode)
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
          if (process_event (display->key_bindings,
                             display->n_key_bindings,
                             display, screen, NULL, event,
                             FALSE))
            {
              /* As normally, after we've handled a global key
               * binding, we unfreeze the keyboard but keep the grab
               * (this is important for something like cycling
               * windows */
              XIAllowEvents (display->xdisplay, event->deviceid,
                             XIAsyncDevice, event->time);
            }
          else
            {
              /* Replay the event so it gets delivered to our
               * per-window key bindings or to the application */
              XIAllowEvents (display->xdisplay, event->deviceid,
                             XIReplayDevice, event->time);
            }
        }
      else if (event->evtype == XI_KeyRelease)
        {
          MetaKeyBinding *binding;

          display->overlay_key_only_pressed = FALSE;
          /* We want to unfreeze events, but keep the grab so that if the user
           * starts typing into the overlay we get all the keys */
          XIAllowEvents (display->xdisplay, event->deviceid,
                         XIAsyncDevice, event->time);

          binding = display_get_keybinding (display,
                                            display->overlay_key_combo.keysym,
                                            display->overlay_key_combo.keycode,
                                            display->grab_mask);
          if (binding &&
              meta_compositor_filter_keybinding (display->compositor, screen, binding))
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
          XIAllowEvents (display->xdisplay, event->deviceid,
                         XIAsyncDevice, event->time);
        }

      return TRUE;
    }
  else if (event->evtype == XI_KeyPress &&
           event->detail == (int)display->overlay_key_combo.keycode)
    {
      display->overlay_key_only_pressed = TRUE;
      /* We keep the keyboard frozen - this allows us to use ReplayKeyboard
       * on the next event if it's not the release of the overlay key */
      XIAllowEvents (display->xdisplay, event->deviceid,
                     XISyncDevice, event->time);

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
process_iso_next_group (MetaDisplay *display,
                        MetaScreen *screen,
                        XIDeviceEvent *event,
                        KeySym keysym)
{
  gboolean activate;
  unsigned int mods;
  int i;

  if (event->evtype != XI_KeyPress)
    return FALSE;

  activate = FALSE;
  mods = (event->mods.effective & 0xff & ~(display->ignored_modifier_mask));

  for (i = 0; i < display->n_iso_next_group_combos; ++i)
    {
      if (event->detail == (int)display->iso_next_group_combos[i].keycode &&
          mods == display->iso_next_group_combos[i].modifiers)
        {
          /* If the signal handler returns TRUE the keyboard will
             remain frozen. It's the signal handler's responsibility
             to unfreeze it. */
          if (!meta_display_modifiers_accelerator_activate (display))
            XIAllowEvents (display->xdisplay, event->deviceid,
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
meta_display_process_key_event (MetaDisplay   *display,
                                MetaWindow    *window,
                                XIDeviceEvent *event)
{
  KeySym keysym;
  gboolean keep_grab;
  gboolean all_keys_grabbed;
  gboolean handled;
  const char *str;
  MetaScreen *screen;

  /* if key event was on root window, we have a shortcut */
  screen = meta_display_screen_for_root (display, event->event);

  /* else round-trip to server */
  if (screen == NULL)
    screen = meta_display_screen_for_xwindow (display, event->event);

  if (screen == NULL)
    return FALSE; /* event window is destroyed */

  /* ignore key events on popup menus and such. */
  if (meta_ui_window_is_widget (screen->ui, event->event))
    return FALSE;

  /* window may be NULL */

  keysym = XKeycodeToKeysym (display->xdisplay, event->detail, 0);

  str = XKeysymToString (keysym);

  /* was topic */
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Processing key %s event, keysym: %s state: 0x%x window: %s\n",
              event->evtype == XI_KeyPress ? "press" : "release",
              str ? str : "none", event->mods.effective,
              window ? window->desc : "(no window)");

  all_keys_grabbed = window ? window->all_keys_grabbed : screen->all_keys_grabbed;
  if (!all_keys_grabbed)
    {
      handled = process_overlay_key (display, screen, event, keysym);
      if (handled)
        return TRUE;

      handled = process_iso_next_group (display, screen, event, keysym);
      if (handled)
        return TRUE;
    }

  XIAllowEvents (display->xdisplay, event->deviceid,
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
      if (window ? (window == display->grab_window) :
          (screen == display->grab_screen))
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
              keep_grab = process_mouse_move_resize_grab (display, screen,
                                                          window, event, keysym);
              break;

            case META_GRAB_OP_KEYBOARD_MOVING:
              meta_topic (META_DEBUG_KEYBINDINGS,
                          "Processing event for keyboard move\n");
              g_assert (window != NULL);
              keep_grab = process_keyboard_move_grab (display, screen,
                                                      window, event, keysym);
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
              keep_grab = process_keyboard_resize_grab (display, screen,
                                                        window, event, keysym);
              break;

            case META_GRAB_OP_KEYBOARD_TABBING_NORMAL:
            case META_GRAB_OP_KEYBOARD_TABBING_DOCK:
            case META_GRAB_OP_KEYBOARD_TABBING_GROUP:
            case META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL:
            case META_GRAB_OP_KEYBOARD_ESCAPING_DOCK:
            case META_GRAB_OP_KEYBOARD_ESCAPING_GROUP:
              meta_topic (META_DEBUG_KEYBINDINGS,
                          "Processing event for keyboard tabbing/cycling\n");
              keep_grab = process_tab_grab (display, screen, event, keysym);
              break;

            case META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING:
              meta_topic (META_DEBUG_KEYBINDINGS,
                          "Processing event for keyboard workspace switching\n");
              keep_grab = process_workspace_switch_grab (display, screen, event, keysym);
              break;

            default:
              break;
            }
        }
      if (!keep_grab)
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Ending grab op %u on key event sym %s\n",
                      display->grab_op, XKeysymToString (keysym));
          meta_display_end_grab_op (display, event->time);
        }

      return TRUE;
    }

  /* Do the normal keybindings */
  return process_event (display->key_bindings,
                        display->n_key_bindings,
                        display, screen, window, event,
                        !all_keys_grabbed && window);
}

static gboolean
process_mouse_move_resize_grab (MetaDisplay   *display,
                                MetaScreen    *screen,
                                MetaWindow    *window,
                                XIDeviceEvent *event,
                                KeySym         keysym)
{
  /* don't care about releases, but eat them, don't end grab */
  if (event->evtype == XI_KeyRelease)
    return TRUE;

  if (keysym == XK_Escape)
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
        meta_window_maximize (window,
                              META_MAXIMIZE_HORIZONTAL |
                              META_MAXIMIZE_VERTICAL);
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
process_keyboard_move_grab (MetaDisplay   *display,
                            MetaScreen    *screen,
                            MetaWindow    *window,
                            XIDeviceEvent *event,
                            KeySym         keysym)
{
  gboolean handled;
  int x, y;
  int incr;
  gboolean smart_snap;

  handled = FALSE;

  /* don't care about releases, but eat them, don't end grab */
  if (event->evtype == XI_KeyRelease)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, event->detail))
    return TRUE;

  meta_window_get_position (window, &x, &y);

  smart_snap = (event->mods.effective & ShiftMask) != 0;

#define SMALL_INCREMENT 1
#define NORMAL_INCREMENT 10

  if (smart_snap)
    incr = 1;
  else if (event->mods.effective & ControlMask)
    incr = SMALL_INCREMENT;
  else
    incr = NORMAL_INCREMENT;

  if (keysym == XK_Escape)
    {
      /* End move and restore to original state.  If the window was a
       * maximized window that had been "shaken loose" we need to
       * remaximize it.  In normal cases, we need to do a moveresize
       * now to get the position back to the original.
       */
      if (window->shaken_loose)
        meta_window_maximize (window,
                              META_MAXIMIZE_HORIZONTAL |
                              META_MAXIMIZE_VERTICAL);
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
  switch (keysym)
    {
    case XK_KP_Home:
    case XK_KP_Prior:
    case XK_Up:
    case XK_KP_Up:
      y -= incr;
      handled = TRUE;
      break;
    case XK_KP_End:
    case XK_KP_Next:
    case XK_Down:
    case XK_KP_Down:
      y += incr;
      handled = TRUE;
      break;
    }

  switch (keysym)
    {
    case XK_KP_Home:
    case XK_KP_End:
    case XK_Left:
    case XK_KP_Left:
      x -= incr;
      handled = TRUE;
      break;
    case XK_KP_Prior:
    case XK_KP_Next:
    case XK_Right:
    case XK_KP_Right:
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
process_keyboard_resize_grab_op_change (MetaDisplay   *display,
                                        MetaScreen    *screen,
                                        MetaWindow    *window,
                                        XIDeviceEvent *event,
                                        KeySym         keysym)
{
  gboolean handled;

  handled = FALSE;
  switch (display->grab_op)
    {
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
      switch (keysym)
        {
        case XK_Up:
        case XK_KP_Up:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_N;
          handled = TRUE;
          break;
        case XK_Down:
        case XK_KP_Down:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_S;
          handled = TRUE;
          break;
        case XK_Left:
        case XK_KP_Left:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          handled = TRUE;
          break;
        case XK_Right:
        case XK_KP_Right:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_S:
      switch (keysym)
        {
        case XK_Left:
        case XK_KP_Left:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          handled = TRUE;
          break;
        case XK_Right:
        case XK_KP_Right:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_N:
      switch (keysym)
        {
        case XK_Left:
        case XK_KP_Left:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          handled = TRUE;
          break;
        case XK_Right:
        case XK_KP_Right:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_W:
      switch (keysym)
        {
        case XK_Up:
        case XK_KP_Up:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_N;
          handled = TRUE;
          break;
        case XK_Down:
        case XK_KP_Down:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_S;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_E:
      switch (keysym)
        {
        case XK_Up:
        case XK_KP_Up:
          display->grab_op = META_GRAB_OP_KEYBOARD_RESIZING_N;
          handled = TRUE;
          break;
        case XK_Down:
        case XK_KP_Down:
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
process_keyboard_resize_grab (MetaDisplay   *display,
                              MetaScreen    *screen,
                              MetaWindow    *window,
                              XIDeviceEvent *event,
                              KeySym         keysym)
{
  gboolean handled;
  int height_inc;
  int width_inc;
  int width, height;
  gboolean smart_snap;
  int gravity;

  handled = FALSE;

  /* don't care about releases, but eat them, don't end grab */
  if (event->evtype == XI_KeyRelease)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, event->detail))
    return TRUE;

  if (keysym == XK_Escape)
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

  if (process_keyboard_resize_grab_op_change (display, screen, window,
                                              event, keysym))
    return TRUE;

  width = window->rect.width;
  height = window->rect.height;

  gravity = meta_resize_gravity_from_grab_op (display->grab_op);

  smart_snap = (event->mods.effective & ShiftMask) != 0;

#define SMALL_INCREMENT 1
#define NORMAL_INCREMENT 10

  if (smart_snap)
    {
      height_inc = 1;
      width_inc = 1;
    }
  else if (event->mods.effective & ControlMask)
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

  switch (keysym)
    {
    case XK_Up:
    case XK_KP_Up:
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

    case XK_Down:
    case XK_KP_Down:
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

    case XK_Left:
    case XK_KP_Left:
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

    case XK_Right:
    case XK_KP_Right:
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

static gboolean
end_keyboard_grab (MetaDisplay *display,
		   unsigned int keycode)
{
#ifdef HAVE_XKB
  if (display->xkb_base_event_type > 0)
    {
      unsigned int primary_modifier;
      XkbStateRec state;

      primary_modifier = get_primary_modifier (display, display->grab_mask);

      XkbGetState (display->xdisplay, XkbUseCoreKbd, &state);

      if (!(primary_modifier & state.mods))
	return TRUE;
    }
  else
#endif
    {
      if (keycode_is_primary_modifier (display, keycode, display->grab_mask))
	return TRUE;
    }

  return FALSE;
}

static gboolean
process_tab_grab (MetaDisplay   *display,
                  MetaScreen    *screen,
                  XIDeviceEvent *event,
                  KeySym         keysym)
{
  MetaKeyBinding *binding;
  MetaKeyBindingAction action;
  gboolean popup_not_showing;
  gboolean backward;
  gboolean key_used;
  MetaWindow *prev_window;

  if (screen != display->grab_screen)
    return FALSE;

  binding = display_get_keybinding (display,
                                    keysym,
                                    event->detail,
                                    display->grab_mask);
  if (binding)
    action = meta_prefs_get_keybinding_action (binding->name);
  else
    action = META_KEYBINDING_ACTION_NONE;

  /*
   * If there is no tab_pop up object, i.e., there is some custom handler
   * implementing Alt+Tab & Co., we call this custom handler; we do not
   * mess about with the grab, as that is up to the handler to deal with.
   */
  if (!screen->tab_popup)
    {
      if (event->evtype == XI_KeyRelease)
        {
          if (end_keyboard_grab (display, event->detail))
            {
              invoke_handler_by_name (display, screen, "tab-popup-select", NULL, event);

              /* We return FALSE to end the grab; if the handler ended the grab itself
               * that will be a noop. If the handler didn't end the grab, then it's a
               * safety measure to prevent a stuck grab.
               */
              return FALSE;
            }

          return TRUE;
        }

      switch (action)
        {
        case META_KEYBINDING_ACTION_CYCLE_PANELS:
        case META_KEYBINDING_ACTION_CYCLE_WINDOWS:
        case META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD:
        case META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD:
        case META_KEYBINDING_ACTION_SWITCH_PANELS:
        case META_KEYBINDING_ACTION_SWITCH_WINDOWS:
        case META_KEYBINDING_ACTION_SWITCH_APPLICATIONS:
        case META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD:
        case META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD:
        case META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD:
        case META_KEYBINDING_ACTION_CYCLE_GROUP:
        case META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD:
        case META_KEYBINDING_ACTION_SWITCH_GROUP:
        case META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD:
          /* These are the tab-popup bindings. If a custom Alt-Tab implementation
           * is in effect, we expect it to want to handle all of these as a group
           *
           * If there are some of them that the custom implementation didn't
           * handle, we treat them as "unbound" for the duration - running the
           * normal handlers could get us into trouble.
           */
          if (binding->handler &&
              binding->handler->func &&
              binding->handler->func != binding->handler->default_func)
            {
              invoke_handler (display, screen, binding->handler, NULL, event, binding);
              return TRUE;
            }
          break;
        case META_KEYBINDING_ACTION_NONE:
          {
            /*
             * If this is simply user pressing the Shift key, we do not want
             * to cancel the grab.
             */
            if (is_modifier (display, event->detail))
              return TRUE;
          }

        default:
          break;
        }

      /* Some unhandled key press */
      invoke_handler_by_name (display, screen, "tab-popup-cancel", NULL, event);
      return FALSE;
    }

  if (event->evtype == XI_KeyRelease &&
      end_keyboard_grab (display, event->detail))
    {
      /* We're done, move to the new window. */
      MetaWindow *target_window;

      target_window = meta_screen_tab_popup_get_selected (screen);

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Ending tab operation, primary modifier released\n");

      if (target_window)
        {
          target_window->tab_unminimized = FALSE;

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Activating target window\n");

          meta_topic (META_DEBUG_FOCUS, "Activating %s due to tab popup "
                      "selection and turning mouse_mode off\n",
                      target_window->desc);
          display->mouse_mode = FALSE;
          meta_window_activate (target_window, event->time);

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Ending grab early so we can focus the target window\n");
          meta_display_end_grab_op (display, event->time);

          return TRUE; /* we already ended the grab */
        }

      return FALSE; /* end grab */
    }

  /* don't care about other releases, but eat them, don't end grab */
  if (event->evtype == XI_KeyRelease)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, event->detail))
    return TRUE;

  prev_window = meta_screen_tab_popup_get_selected (screen);

  /* Cancel when alt-Escape is pressed during using alt-Tab, and vice
   * versa.
   */
  switch (action)
    {
    case META_KEYBINDING_ACTION_CYCLE_PANELS:
    case META_KEYBINDING_ACTION_CYCLE_WINDOWS:
    case META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD:
      /* CYCLE_* are traditionally Escape-based actions,
       * and should cancel traditionally Tab-based ones.
       */
      switch (display->grab_op)
        {
        case META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL:
        case META_GRAB_OP_KEYBOARD_ESCAPING_DOCK:
          /* carry on */
          break;
        default:
          return FALSE;
        }
      break;
    case META_KEYBINDING_ACTION_SWITCH_PANELS:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS:
    case META_KEYBINDING_ACTION_SWITCH_APPLICATIONS:
    case META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD:
      /* SWITCH_* are traditionally Tab-based actions,
       * and should cancel traditionally Escape-based ones.
       */
      switch (display->grab_op)
        {
        case META_GRAB_OP_KEYBOARD_TABBING_NORMAL:
        case META_GRAB_OP_KEYBOARD_TABBING_DOCK:
          /* carry on */
          break;
        default:
          /* Also, we must re-lower and re-minimize whatever window
           * we'd previously raised and unminimized.
           */
          meta_stack_set_positions (screen->stack,
                                    screen->display->grab_old_window_stacking);
          if (prev_window && prev_window->tab_unminimized)
            {
              meta_window_minimize (prev_window);
              prev_window->tab_unminimized = FALSE;
            }
          return FALSE;
        }
      break;
    case META_KEYBINDING_ACTION_CYCLE_GROUP:
    case META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_GROUP:
    case META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD:
      switch (display->grab_op)
        {
        case META_GRAB_OP_KEYBOARD_ESCAPING_GROUP:
        case META_GRAB_OP_KEYBOARD_TABBING_GROUP:
          /* carry on */
          break;
        default:
          return FALSE;
        }

      break;
    default:
      break;
    }

  /* !! TO HERE !!
   * alt-f6 during alt-{Tab,Escape} does not end the grab
   * but does change the grab op (and redraws the window,
   * of course).
   * See _{SWITCH,CYCLE}_GROUP.@@@
   */

  popup_not_showing = FALSE;
  key_used = FALSE;
  backward = FALSE;

  switch (action)
    {
    case META_KEYBINDING_ACTION_CYCLE_PANELS:
    case META_KEYBINDING_ACTION_CYCLE_WINDOWS:
    case META_KEYBINDING_ACTION_CYCLE_GROUP:
      popup_not_showing = TRUE;
      key_used = TRUE;
      break;
    case META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD:
    case META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD:
      popup_not_showing = TRUE;
      key_used = TRUE;
      backward = TRUE;
      break;
    case META_KEYBINDING_ACTION_SWITCH_PANELS:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS:
    case META_KEYBINDING_ACTION_SWITCH_APPLICATIONS:
    case META_KEYBINDING_ACTION_SWITCH_GROUP:
      key_used = TRUE;
      break;
    case META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD:
      key_used = TRUE;
      backward = TRUE;
      break;
    default:
      break;
    }

  if (key_used)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Key pressed, moving tab focus in popup\n");

      if (event->mods.effective & ShiftMask)
        backward = !backward;

      if (backward)
        meta_screen_tab_popup_backward (screen);
      else
        meta_screen_tab_popup_forward (screen);

      if (popup_not_showing)
        {
          /* We can't actually change window focus, due to the grab.
           * but raise the window.
           */
          MetaWindow *target_window;

          meta_stack_set_positions (screen->stack,
                                    display->grab_old_window_stacking);

          target_window = meta_screen_tab_popup_get_selected (screen);

          if (prev_window && prev_window->tab_unminimized)
            {
              prev_window->tab_unminimized = FALSE;
              meta_window_minimize (prev_window);
            }

          if (target_window)
            {
              meta_window_raise (target_window);
              target_window->tab_unminimized = target_window->minimized;
              meta_window_unminimize (target_window);
            }
        }
    }
  else
    {
      /* end grab */
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Ending tabbing/cycling, uninteresting key pressed\n");

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Syncing to old stack positions.\n");
      meta_stack_set_positions (screen->stack,
                                screen->display->grab_old_window_stacking);

      if (prev_window && prev_window->tab_unminimized)
        {
          meta_window_minimize (prev_window);
          prev_window->tab_unminimized = FALSE;
        }
    }

  return key_used;
}

static void
handle_switch_to_workspace (MetaDisplay    *display,
                            MetaScreen     *screen,
                            MetaWindow     *event_window,
                            XIDeviceEvent  *event,
                            MetaKeyBinding *binding,
                            gpointer        dummy)
{
  gint which = binding->handler->data;
  MetaWorkspace *workspace;

  if (which < 0)
    {
      /* Negative workspace numbers are directions with respect to the
       * current workspace.  While we could insta-switch here by setting
       * workspace to the result of meta_workspace_get_neighbor(), when
       * people request a workspace switch to the left or right via
       * the keyboard, they actually want a tab popup.  So we should
       * go there instead.
       *
       * Note that we're the only caller of that function, so perhaps
       * we should merge with it.
       */
      handle_workspace_switch (display, screen, event_window, event, binding,
                               dummy);
      return;
    }

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
handle_maximize_vertically (MetaDisplay    *display,
                            MetaScreen     *screen,
                            MetaWindow     *window,
                            XIDeviceEvent  *event,
                            MetaKeyBinding *binding,
                            gpointer        dummy)
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
handle_maximize_horizontally (MetaDisplay    *display,
                              MetaScreen     *screen,
                              MetaWindow     *window,
                              XIDeviceEvent  *event,
                              MetaKeyBinding *binding,
                              gpointer        dummy)
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
handle_always_on_top          (MetaDisplay    *display,
                              MetaScreen     *screen,
                              MetaWindow     *window,
                              XIDeviceEvent  *event,
                              MetaKeyBinding *binding,
                              gpointer        dummy)
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
handle_move_to_corner_nw  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, FALSE, FALSE, dummy);
}

static void
handle_move_to_corner_ne  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, TRUE, FALSE, dummy);
}

static void
handle_move_to_corner_sw  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, FALSE, TRUE, dummy);
}

static void
handle_move_to_corner_se  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, TRUE, TRUE, dummy);
}

static void
handle_move_to_side_n     (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, FALSE, TRUE, FALSE, FALSE, dummy);
}

static void
handle_move_to_side_s     (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, FALSE, TRUE, FALSE, TRUE, dummy);
}

static void
handle_move_to_side_e     (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, FALSE, TRUE, FALSE, dummy);
}

static void
handle_move_to_side_w     (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, FALSE, FALSE, FALSE, dummy);
}

static void
handle_move_to_center  (MetaDisplay    *display,
                        MetaScreen     *screen,
                        MetaWindow     *window,
                        XIDeviceEvent  *event,
                        MetaKeyBinding *binding,
                        gpointer        dummy)
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

static gboolean
process_workspace_switch_grab (MetaDisplay   *display,
                               MetaScreen    *screen,
                               XIDeviceEvent *event,
                               KeySym         keysym)
{
  MetaWorkspace *workspace;

  if (screen != display->grab_screen || !screen->ws_popup)
    return FALSE;

  if (event->evtype == XI_KeyRelease &&
      end_keyboard_grab (display, event->detail))
    {
      /* We're done, move to the new workspace. */
      MetaWorkspace *target_workspace;

      target_workspace = meta_screen_workspace_popup_get_selected (screen);

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Ending workspace tab operation, primary modifier released\n");

      if (target_workspace == screen->active_workspace)
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Ending grab so we can focus on the target workspace\n");
          meta_display_end_grab_op (display, event->time);

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Focusing default window on target workspace\n");

          meta_workspace_focus_default_window (target_workspace,
                                               NULL,
                                               event->time);

          return TRUE; /* we already ended the grab */
        }

      /* Workspace switching should have already occurred on KeyPress */
      meta_warning ("target_workspace != active_workspace.  Some other event must have occurred.\n");

      return FALSE; /* end grab */
    }

  /* don't care about other releases, but eat them, don't end grab */
  if (event->evtype == XI_KeyRelease)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, event->detail))
    return TRUE;

  /* select the next workspace in the popup */
  workspace = meta_screen_workspace_popup_get_selected (screen);

  if (workspace)
    {
      MetaWorkspace *target_workspace;
      MetaKeyBindingAction action;

      action = meta_display_get_keybinding_action (display,
                                                   event->detail,
                                                   display->grab_mask);

      switch (action)
        {
        case META_KEYBINDING_ACTION_WORKSPACE_UP:
          target_workspace = meta_workspace_get_neighbor (workspace,
                                                          META_MOTION_UP);
          break;

        case META_KEYBINDING_ACTION_WORKSPACE_DOWN:
          target_workspace = meta_workspace_get_neighbor (workspace,
                                                          META_MOTION_DOWN);
          break;

        case META_KEYBINDING_ACTION_WORKSPACE_LEFT:
          target_workspace = meta_workspace_get_neighbor (workspace,
                                                          META_MOTION_LEFT);
          break;

        case META_KEYBINDING_ACTION_WORKSPACE_RIGHT:
          target_workspace = meta_workspace_get_neighbor (workspace,
                                                          META_MOTION_RIGHT);
          break;

        default:
          target_workspace = NULL;
          break;
        }

      if (target_workspace)
        {
          meta_screen_workspace_popup_select (screen, target_workspace);
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Tab key pressed, moving tab focus in popup\n");

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Activating target workspace\n");

          meta_workspace_activate (target_workspace, event->time);

          return TRUE; /* we already ended the grab */
        }
    }

  /* end grab */
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Ending workspace tabbing & focusing default window; uninteresting key pressed\n");
  workspace = meta_screen_workspace_popup_get_selected (screen);
  meta_workspace_focus_default_window (workspace, NULL, event->time);
  return FALSE;
}

static void
handle_show_desktop (MetaDisplay    *display,
                     MetaScreen     *screen,
                     MetaWindow     *window,
                     XIDeviceEvent  *event,
                     MetaKeyBinding *binding,
                     gpointer        dummy)
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
handle_panel (MetaDisplay    *display,
              MetaScreen     *screen,
              MetaWindow     *window,
              XIDeviceEvent  *event,
              MetaKeyBinding *binding,
              gpointer        dummy)
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
              "Sending panel message with timestamp %lu, and turning mouse_mode "
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
handle_activate_window_menu (MetaDisplay    *display,
                             MetaScreen     *screen,
                             MetaWindow     *event_window,
                             XIDeviceEvent  *event,
                             MetaKeyBinding *binding,
                             gpointer        dummy)
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

static MetaGrabOp
tab_op_from_tab_type (MetaTabList type)
{
  switch (type)
    {
    case META_TAB_LIST_NORMAL:
      return META_GRAB_OP_KEYBOARD_TABBING_NORMAL;
    case META_TAB_LIST_DOCKS:
      return META_GRAB_OP_KEYBOARD_TABBING_DOCK;
    case META_TAB_LIST_GROUP:
      return META_GRAB_OP_KEYBOARD_TABBING_GROUP;
    case META_TAB_LIST_NORMAL_ALL:
      break;
    }

  g_assert_not_reached ();

  return 0;
}

static MetaGrabOp
cycle_op_from_tab_type (MetaTabList type)
{
  switch (type)
    {
    case META_TAB_LIST_NORMAL:
      return META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL;
    case META_TAB_LIST_DOCKS:
      return META_GRAB_OP_KEYBOARD_ESCAPING_DOCK;
    case META_TAB_LIST_GROUP:
      return META_GRAB_OP_KEYBOARD_ESCAPING_GROUP;
    case META_TAB_LIST_NORMAL_ALL:
      break;
    }

  g_assert_not_reached ();

  return 0;
}

static void
do_choose_window (MetaDisplay    *display,
                  MetaScreen     *screen,
                  MetaWindow     *event_window,
                  XIDeviceEvent  *event,
                  MetaKeyBinding *binding,
                  gboolean        backward,
                  gboolean        show_popup)
{
  MetaTabList type = binding->handler->data;
  MetaWindow *initial_selection;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Tab list = %u show_popup = %d\n", type, show_popup);

  /* reverse direction if shift is down */
  if (event->mods.effective & ShiftMask)
    backward = !backward;

  initial_selection = meta_display_get_tab_next (display,
                                                 type,
                                                 screen,
                                                 screen->active_workspace,
                                                 NULL,
                                                 backward);

  /* Note that focus_window may not be in the tab chain, but it's OK */
  if (initial_selection == NULL)
    initial_selection = meta_display_get_tab_current (display,
                                                      type, screen,
                                                      screen->active_workspace);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Initially selecting window %s\n",
              initial_selection ? initial_selection->desc : "(none)");

  if (initial_selection == NULL)
    return;

  if (binding->mask == 0)
    {
      /* If no modifiers, we can't do the "hold down modifier to keep
       * moving" thing, so we just instaswitch by one window.
       */
      meta_topic (META_DEBUG_FOCUS,
                  "Activating %s and turning off mouse_mode due to "
                  "switch/cycle windows with no modifiers\n",
                  initial_selection->desc);
      display->mouse_mode = FALSE;
      meta_window_activate (initial_selection, event->time);
      return;
    }

  if (meta_prefs_get_no_tab_popup ())
    {
      /* FIXME? Shouldn't this be merged with the previous case? */
      return;
    }

  if (!meta_display_begin_grab_op (display,
                                   screen,
                                   NULL,
                                   show_popup ?
                                   tab_op_from_tab_type (type) :
                                   cycle_op_from_tab_type (type),
                                   FALSE,
                                   FALSE,
                                   0,
                                   binding->mask,
                                   event->time,
                                   0, 0))
    return;

  if (!primary_modifier_still_pressed (display, binding->mask))
    {
      /* This handles a race where modifier might be released before
       * we establish the grab. must end grab prior to trying to focus
       * a window.
       */
      meta_topic (META_DEBUG_FOCUS,
                  "Ending grab, activating %s, and turning off "
                  "mouse_mode due to switch/cycle windows where "
                  "modifier was released prior to grab\n",
                  initial_selection->desc);
      meta_display_end_grab_op (display, event->time);
      display->mouse_mode = FALSE;
      meta_window_activate (initial_selection, event->time);
      return;
    }

  meta_screen_tab_popup_create (screen, type,
                                show_popup ? META_TAB_SHOW_ICON :
                                META_TAB_SHOW_INSTANTLY,
                                initial_selection);

  if (!show_popup)
    {
      meta_window_raise (initial_selection);
      initial_selection->tab_unminimized =
        initial_selection->minimized;
      meta_window_unminimize (initial_selection);
    }
}

static void
handle_switch (MetaDisplay    *display,
               MetaScreen     *screen,
               MetaWindow     *event_window,
               XIDeviceEvent  *event,
               MetaKeyBinding *binding,
               gpointer        dummy)
{
  gint backwards = (binding->handler->flags & META_KEY_BINDING_IS_REVERSED) != 0;

  do_choose_window (display, screen, event_window, event, binding,
                    backwards, TRUE);
}

static void
handle_cycle (MetaDisplay    *display,
              MetaScreen     *screen,
              MetaWindow     *event_window,
              XIDeviceEvent  *event,
              MetaKeyBinding *binding,
              gpointer        dummy)
{
  gint backwards = (binding->handler->flags & META_KEY_BINDING_IS_REVERSED) != 0;

  do_choose_window (display, screen, event_window, event, binding,
                    backwards, FALSE);
}

static void
handle_tab_popup_select (MetaDisplay    *display,
                         MetaScreen     *screen,
                         MetaWindow     *window,
                         XIDeviceEvent  *event,
                         MetaKeyBinding *binding,
                         gpointer        dummy)
{
  /* Stub for custom handlers; no default implementation */
}

static void
handle_tab_popup_cancel (MetaDisplay    *display,
                         MetaScreen     *screen,
                         MetaWindow     *window,
                         XIDeviceEvent  *event,
                         MetaKeyBinding *binding,
                         gpointer        dummy)
{
  /* Stub for custom handlers; no default implementation */
}

static void
handle_toggle_fullscreen  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->fullscreen)
    meta_window_unmake_fullscreen (window);
  else if (window->has_fullscreen_func)
    meta_window_make_fullscreen (window);
}

static void
handle_toggle_above       (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->wm_state_above)
    meta_window_unmake_above (window);
  else
    meta_window_make_above (window);
}

static void
handle_toggle_tiled (MetaDisplay    *display,
                     MetaScreen     *screen,
                     MetaWindow     *window,
                     XIDeviceEvent  *event,
                     MetaKeyBinding *binding,
                     gpointer        dummy)
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
        meta_window_maximize (window, META_MAXIMIZE_VERTICAL |
                              META_MAXIMIZE_HORIZONTAL);
      else
        meta_window_unmaximize (window, META_MAXIMIZE_VERTICAL |
                                META_MAXIMIZE_HORIZONTAL);
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
handle_toggle_maximized    (MetaDisplay    *display,
                            MetaScreen     *screen,
                            MetaWindow     *window,
                            XIDeviceEvent  *event,
                            MetaKeyBinding *binding,
                            gpointer        dummy)
{
  if (META_WINDOW_MAXIMIZED (window))
    meta_window_unmaximize (window,
                            META_MAXIMIZE_HORIZONTAL |
                            META_MAXIMIZE_VERTICAL);
  else if (window->has_maximize_func)
    meta_window_maximize (window,
                          META_MAXIMIZE_HORIZONTAL |
                          META_MAXIMIZE_VERTICAL);
}

static void
handle_maximize           (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->has_maximize_func)
    meta_window_maximize (window,
                          META_MAXIMIZE_HORIZONTAL |
                          META_MAXIMIZE_VERTICAL);
}

static void
handle_unmaximize         (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->maximized_vertically || window->maximized_horizontally)
    meta_window_unmaximize (window,
                            META_MAXIMIZE_HORIZONTAL |
                            META_MAXIMIZE_VERTICAL);
}

static void
handle_toggle_shaded      (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->shaded)
    meta_window_unshade (window, event->time);
  else if (window->has_shade_func)
    meta_window_shade (window, event->time);
}

static void
handle_close              (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->has_close_func)
    meta_window_delete (window, event->time);
}

static void
handle_minimize        (MetaDisplay    *display,
                        MetaScreen     *screen,
                        MetaWindow     *window,
                        XIDeviceEvent  *event,
                        MetaKeyBinding *binding,
                        gpointer        dummy)
{
  if (window->has_minimize_func)
    meta_window_minimize (window);
}

static void
handle_begin_move         (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
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
handle_begin_resize       (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
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
handle_toggle_on_all_workspaces (MetaDisplay    *display,
                                 MetaScreen     *screen,
                                 MetaWindow     *window,
                                 XIDeviceEvent  *event,
                                 MetaKeyBinding *binding,
                                 gpointer        dummy)
{
  if (window->on_all_workspaces_requested)
    meta_window_unstick (window);
  else
    meta_window_stick (window);
}

static void
handle_move_to_workspace  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XIDeviceEvent  *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
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
                        XIDeviceEvent  *event,
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
handle_raise_or_lower (MetaDisplay    *display,
                       MetaScreen     *screen,
		       MetaWindow     *window,
		       XIDeviceEvent  *event,
		       MetaKeyBinding *binding,
                       gpointer        dummy)
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
handle_raise (MetaDisplay    *display,
              MetaScreen     *screen,
              MetaWindow     *window,
              XIDeviceEvent  *event,
              MetaKeyBinding *binding,
              gpointer        dummy)
{
  meta_window_raise (window);
}

static void
handle_lower (MetaDisplay    *display,
              MetaScreen     *screen,
              MetaWindow     *window,
              XIDeviceEvent  *event,
              MetaKeyBinding *binding,
              gpointer        dummy)
{
  meta_window_lower (window);
}

static void
handle_workspace_switch  (MetaDisplay    *display,
                          MetaScreen     *screen,
                          MetaWindow     *window,
                          XIDeviceEvent  *event,
                          MetaKeyBinding *binding,
                          gpointer        dummy)
{
  gint motion = binding->handler->data;
  unsigned int grab_mask;
  MetaWorkspace *next;
  gboolean grabbed_before_release;

  g_assert (motion < 0);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Starting tab between workspaces, showing popup\n");

  /* FIXME should we use binding->mask ? */
  grab_mask = event->mods.effective & ~(display->ignored_modifier_mask);

  if (!meta_display_begin_grab_op (display,
                                   screen,
                                   NULL,
                                   META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING,
                                   FALSE,
                                   FALSE,
                                   0,
                                   grab_mask,
                                   event->time,
                                   0, 0))
    return;

  next = meta_workspace_get_neighbor (screen->active_workspace, motion);
  g_assert (next);

  grabbed_before_release = primary_modifier_still_pressed (display, grab_mask);

  meta_topic (META_DEBUG_KEYBINDINGS, "Activating target workspace\n");

  if (!grabbed_before_release)
    {
      /* end the grab right away, modifier possibly released
       * before we could establish the grab and receive the
       * release event. Must end grab before we can switch
       * spaces.
       */
      meta_display_end_grab_op (display, event->time);
    }

  meta_workspace_activate (next, event->time);

  if (grabbed_before_release && !meta_prefs_get_no_tab_popup ())
    meta_screen_workspace_popup_create (screen, next);
}

static void
handle_set_spew_mark (MetaDisplay    *display,
                      MetaScreen     *screen,
                      MetaWindow     *window,
                      XIDeviceEvent  *event,
                      MetaKeyBinding *binding,
                      gpointer        dummy)
{
  meta_verbose ("-- MARK MARK MARK MARK --\n");
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
                          handle_switch_to_workspace, META_MOTION_LEFT);

  add_builtin_keybinding (display,
                          "switch-to-workspace-right",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_RIGHT,
                          handle_switch_to_workspace, META_MOTION_RIGHT);

  add_builtin_keybinding (display,
                          "switch-to-workspace-up",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_UP,
                          handle_switch_to_workspace, META_MOTION_UP);

  add_builtin_keybinding (display,
                          "switch-to-workspace-down",
                          common_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_WORKSPACE_DOWN,
                          handle_switch_to_workspace, META_MOTION_DOWN);


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


  /* These two are special pseudo-bindings that are provided for allowing
   * custom handlers, but will never be bound to a key. While a tab
   * grab is in effect, they are invoked for releasing the primary modifier
   * or pressing some unbound key, respectively.
   */
  add_builtin_keybinding (display,
                          "tab-popup-select",
                          mutter_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_TAB_POPUP_SELECT,
                          handle_tab_popup_select, 0);

  add_builtin_keybinding (display,
                          "tab-popup-cancel",
                          mutter_keybindings,
                          META_KEY_BINDING_NONE,
                          META_KEYBINDING_ACTION_TAB_POPUP_CANCEL,
                          handle_tab_popup_cancel, 0);

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
}

void
meta_display_init_keys (MetaDisplay *display)
{
  MetaKeyHandler *handler;

  /* Keybindings */
  display->keymap = NULL;
  display->keysyms_per_keycode = 0;
  display->modmap = NULL;
  display->min_keycode = 0;
  display->max_keycode = 0;
  display->ignored_modifier_mask = 0;
  display->num_lock_mask = 0;
  display->scroll_lock_mask = 0;
  display->hyper_mask = 0;
  display->super_mask = 0;
  display->meta_mask = 0;
  display->key_bindings = NULL;
  display->n_key_bindings = 0;

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
