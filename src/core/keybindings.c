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

#define _GNU_SOURCE
#define _SVID_SOURCE /* for putenv() */

#include <config.h>
#include "keybindings-private.h"
#include "workspace-private.h"
#include "errors.h"
#include "edge-resistance.h"
#include "ui.h"
#include "frame-private.h"
#include "place.h"
#include "prefs.h"
#include "effects.h"
#include "util.h"

#include <X11/keysym.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

static gboolean all_bindings_disabled = FALSE;

/* Prototypes for handlers */
#define keybind(name, handler, param, flags, stroke, description) \
static void \
handler (MetaDisplay    *display,\
         MetaScreen     *screen,\
         MetaWindow     *window,\
         XEvent         *event,\
         MetaKeyBinding *binding,               \
         gpointer        user_data);
#include "all-keybindings.h"
#undef keybind

/* These can't be bound to anything, but they are used to handle
 * various other events.  TODO: Possibly we should include them as event
 * handler functions and have some kind of flag to say they're unbindable.
 */

static void handle_workspace_switch  (MetaDisplay    *display,
                                      MetaScreen     *screen,
                                      MetaWindow     *window,
                                      XEvent         *event,
                                      MetaKeyBinding *binding,
                                      gpointer        dummy);

static gboolean process_mouse_move_resize_grab (MetaDisplay *display,
                                                MetaScreen  *screen,
                                                MetaWindow  *window,
                                                XEvent      *event,
                                                KeySym       keysym);

static gboolean process_keyboard_move_grab (MetaDisplay *display,
                                            MetaScreen  *screen,
                                            MetaWindow  *window,
                                            XEvent      *event,
                                            KeySym       keysym);

static gboolean process_keyboard_resize_grab (MetaDisplay *display,
                                              MetaScreen  *screen,
                                              MetaWindow  *window,
                                              XEvent      *event,
                                              KeySym       keysym);

static gboolean process_tab_grab           (MetaDisplay *display,
                                            MetaScreen  *screen,
                                            XEvent      *event,
                                            KeySym       keysym);

static gboolean process_workspace_switch_grab (MetaDisplay *display,
                                               MetaScreen  *screen,
                                               XEvent      *event,
                                               KeySym       keysym);

static void regrab_key_bindings         (MetaDisplay *display);


#define keybind(name, handler, param, flags, stroke, description) \
  { #name, handler, handler, param, flags, NULL, NULL },
static MetaKeyHandler key_handlers[] = {
#include "all-keybindings.h"
  { NULL, NULL, NULL, 0, 0, NULL, NULL }
};
#undef keybind

static void
reload_keymap (MetaDisplay *display)
{
  if (display->keymap)
    meta_XFree (display->keymap);

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

static void
reload_keycodes (MetaDisplay *display)
{
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Reloading keycodes for binding tables\n");
  
  if (display->overlay_key_combo.keysym 
      && display->overlay_key_combo.keycode == 0)
    {
      display->overlay_key_combo.keycode = XKeysymToKeycode (
          display->xdisplay, display->overlay_key_combo.keysym);
    }
  
  if (display->key_bindings)
    {
      int i;
      
      i = 0;
      while (i < display->n_key_bindings)
        {
          if (display->key_bindings[i].keycode == 0)
              display->key_bindings[i].keycode = XKeysymToKeycode (
                      display->xdisplay, display->key_bindings[i].keysym);
          
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
count_bindings (const MetaKeyPref *prefs,
                int                n_prefs)
{
  int i;
  int count;

  count = 0;
  i = 0;
  while (i < n_prefs)
    {
      GSList *tmp = prefs[i].bindings;

      while (tmp)
        {
          MetaKeyCombo *combo = tmp->data;

          if (combo && (combo->keysym != None || combo->keycode != 0))
            {
              count += 1;

              if (prefs[i].add_shift &&
                  (combo->modifiers & META_VIRTUAL_SHIFT_MASK) == 0)
                count += 1;
            }

          tmp = tmp->next;
        }
      
      ++i;
    }

  return count;
}

/* FIXME: replace this with a temporary hash */
static MetaKeyHandler*
find_handler (MetaKeyHandler *handlers,
              const char           *name)
{
  MetaKeyHandler *iter;

  iter = handlers;
  while (iter->name)
    {
      if (strcmp (iter->name, name) == 0)
        return iter;

      ++iter;
    }

  return NULL;
}

static void
rebuild_binding_table (MetaDisplay        *display,
                       MetaKeyBinding    **bindings_p,
                       int                *n_bindings_p,
                       const MetaKeyPref  *prefs,
                       int                 n_prefs)
{
  int n_bindings;
  int src, dest;
  
  n_bindings = count_bindings (prefs, n_prefs);
  g_free (*bindings_p);
  *bindings_p = g_new0 (MetaKeyBinding, n_bindings);

  src = 0;
  dest = 0;
  while (src < n_prefs)
    {
      GSList *tmp = prefs[src].bindings;

      while (tmp)
        {
          MetaKeyCombo *combo = tmp->data;

          if (combo && (combo->keysym != None || combo->keycode != 0))
            {
              MetaKeyHandler *handler = find_handler (key_handlers, prefs[src].name);

              (*bindings_p)[dest].name = prefs[src].name;
              (*bindings_p)[dest].handler = handler;
              (*bindings_p)[dest].keysym = combo->keysym;
              (*bindings_p)[dest].keycode = combo->keycode;
              (*bindings_p)[dest].modifiers = combo->modifiers;
              (*bindings_p)[dest].mask = 0;
          
              ++dest;

              if (prefs[src].add_shift &&
                  (combo->modifiers & META_VIRTUAL_SHIFT_MASK) == 0)
                {
                  meta_topic (META_DEBUG_KEYBINDINGS,
                              "Binding %s also needs Shift grabbed\n",
                               prefs[src].name);
              
                  (*bindings_p)[dest].name = prefs[src].name;
                  (*bindings_p)[dest].handler = handler;
                  (*bindings_p)[dest].keysym = combo->keysym;
                  (*bindings_p)[dest].keycode = combo->keycode;
                  (*bindings_p)[dest].modifiers = combo->modifiers |
                    META_VIRTUAL_SHIFT_MASK;
                  (*bindings_p)[dest].mask = 0;
              
                  ++dest;
                }
            }
            
          tmp = tmp->next;
        }
      
      ++src;
    }

  g_assert (dest == n_bindings);
  
  *n_bindings_p = dest;

  meta_topic (META_DEBUG_KEYBINDINGS,
              " %d bindings in table\n",
              *n_bindings_p);
}

static void
rebuild_key_binding_table (MetaDisplay *display)
{
  const MetaKeyPref *prefs;
  int n_prefs;
  
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Rebuilding key binding table from preferences\n");
  
  meta_prefs_get_key_bindings (&prefs, &n_prefs);
  rebuild_binding_table (display,
                         &display->key_bindings,
                         &display->n_key_bindings,
                         prefs, n_prefs);
}

static void
rebuild_special_bindings (MetaDisplay *display)
{
  MetaKeyCombo combo;
  
  meta_prefs_get_overlay_binding (&combo);

  if (combo.keysym != None || combo.keycode != 0)
    {
      display->overlay_key_combo = combo;
    }
}

static void
regrab_key_bindings (MetaDisplay *display)
{
  GSList *tmp;
  GSList *windows;

  meta_error_trap_push (display); /* for efficiency push outer trap */
  
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;

      meta_screen_ungrab_keys (screen);
      meta_screen_grab_keys (screen);

      tmp = tmp->next;
    }

  windows = meta_display_list_windows (display);
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;
      
      meta_window_ungrab_keys (w);
      meta_window_grab_keys (w);
      
      tmp = tmp->next;
    }
  meta_error_trap_pop (display, FALSE);

  g_slist_free (windows);
}

static MetaKeyBindingAction
display_get_keybinding_action (MetaDisplay  *display,
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
          return meta_prefs_get_keybinding_action (display->key_bindings[i].name);
        }
      
      --i;
    }

  return META_KEYBINDING_ACTION_NONE;
}

void
meta_display_process_mapping_event (MetaDisplay *display,
                                    XEvent      *event)
{ 
  if (event->xmapping.request == MappingModifier)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Received MappingModifier event, will reload modmap and redo keybindings\n");

      reload_modmap (display);

      reload_modifiers (display);
      
      regrab_key_bindings (display);
    }
  else if (event->xmapping.request == MappingKeyboard)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Received MappingKeyboard event, will reload keycodes and redo keybindings\n");

      reload_keymap (display);
      reload_modmap (display);
      
      reload_keycodes (display);

      regrab_key_bindings (display);
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
      rebuild_key_binding_table (display);
      rebuild_special_bindings (display);
      reload_keycodes (display);
      reload_modifiers (display);
      regrab_key_bindings (display);
      break;
    default:
      break;
    }
}


void
meta_display_init_keys (MetaDisplay *display)
{
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

  rebuild_key_binding_table (display);
  rebuild_special_bindings (display);

  reload_keycodes (display);
  reload_modifiers (display);
  
  /* Keys are actually grabbed in meta_screen_grab_keys() */
  
  meta_prefs_add_listener (bindings_changed_callback, display);
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
      if (ignored_mask & ~(display->ignored_modifier_mask))
        {
          /* Not a combination of ignored modifiers
           * (it contains some non-ignored modifiers)
           */
          ++ignored_mask;
          continue;
        }

      if (meta_is_debugging ())
        meta_error_trap_push_with_return (display);
      if (grab)
        XGrabKey (display->xdisplay, keycode,
                  modmask | ignored_mask,
                  xwindow,
                  True,
                  GrabModeAsync, GrabModeSync);
      else
        XUngrabKey (display->xdisplay, keycode,
                    modmask | ignored_mask,
                    xwindow);

      if (meta_is_debugging ())
        {
          int result;
          
          result = meta_error_trap_pop_with_return (display, FALSE);
          
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

  meta_error_trap_pop (display, FALSE);
}

static void
meta_grab_key (MetaDisplay *display,
               Window       xwindow,
               int          keysym,
               unsigned int keycode,
               int          modmask)
{
  meta_change_keygrab (display, xwindow, TRUE, keysym, keycode, modmask);
}

static void
grab_keys (MetaKeyBinding *bindings,
           int             n_bindings,
           MetaDisplay    *display,
           Window          xwindow,
           gboolean        binding_per_window)
{
  int i;

  g_assert (n_bindings == 0 || bindings != NULL);

  meta_error_trap_push (display);
  
  i = 0;
  while (i < n_bindings)
    {
      if (!!binding_per_window ==
          !!(bindings[i].handler->flags & BINDING_PER_WINDOW) &&
          bindings[i].keycode != 0)
        {
          meta_grab_key (display, xwindow,
                         bindings[i].keysym,
                         bindings[i].keycode,
                         bindings[i].mask);
        }
      
      ++i;
    }

  meta_error_trap_pop (display, FALSE);
}

static void
ungrab_all_keys (MetaDisplay *display,
                 Window       xwindow)
{
  if (meta_is_debugging ())
    meta_error_trap_push_with_return (display);
  else
    meta_error_trap_push (display);

  XUngrabKey (display->xdisplay, AnyKey, AnyModifier,
              xwindow);

  if (meta_is_debugging ())
    {
      int result;
      
      result = meta_error_trap_pop_with_return (display, FALSE);
      
      if (result != Success)    
        meta_topic (META_DEBUG_KEYBINDINGS,
                    "Ungrabbing all keys on 0x%lx failed\n", xwindow);
    }
  else
    meta_error_trap_pop (display, FALSE);
}

void
meta_screen_grab_keys (MetaScreen *screen)
{
  MetaDisplay *display = screen->display;  
  if (screen->all_keys_grabbed)
    return;

  if (screen->keys_grabbed)
    return;
  
  if (display->overlay_key_combo.keycode != 0)
    meta_grab_key (display, screen->xroot,
                   display->overlay_key_combo.keysym,
                   display->overlay_key_combo.keycode,
                   display->overlay_key_combo.modifiers);

  grab_keys (screen->display->key_bindings,
             screen->display->n_key_bindings,
             screen->display, screen->xroot,
             FALSE);

  screen->keys_grabbed = TRUE;
}

void
meta_screen_ungrab_keys (MetaScreen  *screen)
{
  if (screen->keys_grabbed)
    {
      ungrab_all_keys (screen->display, screen->xroot);
      screen->keys_grabbed = FALSE;
    }
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
        ungrab_all_keys (window->display, window->xwindow);
      window->keys_grabbed = FALSE;
      return;
    }
  
  if (window->keys_grabbed)
    {
      if (window->frame && !window->grab_on_frame)
        ungrab_all_keys (window->display, window->xwindow);
      else if (window->frame == NULL &&
               window->grab_on_frame)
        ; /* continue to regrab on client window */
      else
        return; /* already all good */
    }
  
  grab_keys (window->display->key_bindings,
             window->display->n_key_bindings,
             window->display,
             window->frame ? window->frame->xwindow : window->xwindow,
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
        ungrab_all_keys (window->display,
                         window->frame->xwindow);
      else if (!window->grab_on_frame)
        ungrab_all_keys (window->display,
                         window->xwindow);

      window->keys_grabbed = FALSE;
    }
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
               guint32      timestamp)
{
  int result;
  int grab_status;
  
  /* Grab the keyboard, so we get key releases and all key
   * presses
   */
  meta_error_trap_push_with_return (display);

  grab_status = XGrabKeyboard (display->xdisplay,
                               xwindow, True,
                               GrabModeAsync, GrabModeAsync,
                               timestamp);
  
  if (grab_status != GrabSuccess)
    {
      meta_error_trap_pop_with_return (display, TRUE);
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "XGrabKeyboard() returned failure status %s time %u\n",
                  grab_status_to_string (grab_status),
                  timestamp);
      return FALSE;
    }
  else
    {
      result = meta_error_trap_pop_with_return (display, TRUE);
      if (result != Success)
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "XGrabKeyboard() resulted in an error\n");
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
  XUngrabKeyboard (display->xdisplay, timestamp);
  meta_error_trap_pop (display, FALSE);
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
  retval = grab_keyboard (screen->display, screen->xroot, timestamp);
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
  
  grabwindow = window->frame ? window->frame->xwindow : window->xwindow;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Grabbing all keys on window %s\n", window->desc);
  retval = grab_keyboard (window->display, grabwindow, timestamp);
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
  int x, y, root_x, root_y;
  Window root, child;
  guint mask;
  MetaScreen *random_screen;
  Window      random_xwindow;
  
  primary_modifier = get_primary_modifier (display, entire_binding_mask);
  
  random_screen = display->screens->data;
  random_xwindow = random_screen->no_focus_window;
  XQueryPointer (display->xdisplay,
                 random_xwindow, /* some random window */
                 &root, &child,
                 &root_x, &root_y,
                 &x, &y,
                 &mask);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Primary modifier 0x%x full grab mask 0x%x current state 0x%x\n",
              primary_modifier, entire_binding_mask, mask);
  
  if ((mask & primary_modifier) == 0)
    return FALSE;
  else
    return TRUE;
}

static gboolean
process_overlay_key (MetaDisplay *display,
                     MetaScreen *Screen,
                     XEvent *event,
                     KeySym keysym)
{
  if (event->xkey.keycode != display->overlay_key_combo.keycode)
    {
      display->overlay_key_only_pressed = FALSE;
      return FALSE;
    }

  if (event->xkey.type == KeyPress)
    {
      display->overlay_key_only_pressed = TRUE;
    }
  else if (event->xkey.type == KeyRelease && display->overlay_key_only_pressed)
    {
      display->overlay_key_only_pressed = FALSE;
      meta_display_overlay_key_activate (display);
    }

  return TRUE;
}

/* now called from only one place, may be worth merging */
static gboolean
process_event (MetaKeyBinding       *bindings,
               int                   n_bindings,
               MetaDisplay          *display,
               MetaScreen           *screen,
               MetaWindow           *window,
               XEvent               *event,
               KeySym                keysym,
               gboolean              on_window)
{
  int i;

  /* we used to have release-based bindings but no longer. */
  if (event->type == KeyRelease)
    return FALSE;

  /*
   * TODO: This would be better done with a hash table;
   * it doesn't suit to use O(n) for such a common operation.
   */
  for (i=0; i<n_bindings; i++)
    {
      MetaKeyHandler *handler = bindings[i].handler;

      if ((!on_window && handler->flags & BINDING_PER_WINDOW) ||
          event->type != KeyPress ||
          bindings[i].keycode != event->xkey.keycode ||
          ((event->xkey.state & 0xff & ~(display->ignored_modifier_mask)) !=
           bindings[i].mask))
        continue;
        
      /*
       * window must be non-NULL for on_window to be true,
       * and so also window must be non-NULL if we get here and
       * this is a BINDING_PER_WINDOW binding.
       */

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Binding keycode 0x%x mask 0x%x matches event 0x%x state 0x%x\n",
                  bindings[i].keycode, bindings[i].mask,
                  event->xkey.keycode, event->xkey.state);

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

      if (handler->func)
        (* handler->func) (display, screen,
                           bindings[i].handler->flags & BINDING_PER_WINDOW ?
                           window: NULL,
                           event,
                           &bindings[i],
                           handler->user_data);
      else
        (* handler->default_func) (display, screen,
                           bindings[i].handler->flags & BINDING_PER_WINDOW ?
                           window: NULL,
                           event,
                           &bindings[i],
                           NULL);

      return TRUE;
    }

  meta_topic (META_DEBUG_KEYBINDINGS,
              "No handler found for this event in this binding table\n");
  return FALSE;
}

/* Handle a key event. May be called recursively: some key events cause
 * grabs to be ended and then need to be processed again in their own
 * right. This cannot cause infinite recursion because we never call
 * ourselves when there wasn't a grab, and we always clear the grab
 * first; the invariant is enforced using an assertion. See #112560.
 * FIXME: We need to prove there are no race conditions here.
 * FIXME: Does it correctly handle alt-Tab being followed by another
 * grabbing keypress without letting go of alt?
 * FIXME: An iterative solution would probably be simpler to understand
 * (and help us solve the other fixmes).
 */
void
meta_display_process_key_event (MetaDisplay *display,
                                MetaWindow  *window,
                                XEvent      *event)
{
  KeySym keysym;
  gboolean keep_grab;
  gboolean all_keys_grabbed;
  gboolean handled;
  const char *str;
  MetaScreen *screen;

  XAllowEvents (display->xdisplay,
                all_bindings_disabled ? ReplayKeyboard : AsyncKeyboard,
                event->xkey.time);
  if (all_bindings_disabled)
    return;

  /* if key event was on root window, we have a shortcut */
  screen = meta_display_screen_for_root (display, event->xkey.window);
  
  /* else round-trip to server */
  if (screen == NULL)
    screen = meta_display_screen_for_xwindow (display,
                                              event->xany.window);

  if (screen == NULL)
    return; /* event window is destroyed */
  
  /* ignore key events on popup menus and such. */
  if (window == NULL &&
      meta_ui_window_is_widget (screen->ui, event->xany.window))
    return;
  
  /* window may be NULL */
  
  keysym = XKeycodeToKeysym (display->xdisplay, event->xkey.keycode, 0);

  str = XKeysymToString (keysym);
  
  /* was topic */
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Processing key %s event, keysym: %s state: 0x%x window: %s\n",
              event->type == KeyPress ? "press" : "release",
              str ? str : "none", event->xkey.state,
              window ? window->desc : "(no window)");

  keep_grab = TRUE;
  all_keys_grabbed = window ? window->all_keys_grabbed : screen->all_keys_grabbed;
  if (all_keys_grabbed)
    {
      if (display->grab_op == META_GRAB_OP_NONE)
        return;
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
          meta_display_end_grab_op (display, event->xkey.time);
          return;
        }
      }
  
  handled = process_overlay_key (display, screen, event, keysym);
  
  /* Do the normal keybindings */
  if (!handled)
    process_event (display->key_bindings,
                   display->n_key_bindings,
                   display, screen, window, event, keysym,
                   !all_keys_grabbed && window);
}

static gboolean
process_mouse_move_resize_grab (MetaDisplay *display,
                                MetaScreen  *screen,
                                MetaWindow  *window,
                                XEvent      *event,
                                KeySym       keysym)
{
  /* don't care about releases, but eat them, don't end grab */
  if (event->type == KeyRelease)
    return TRUE;

  if (keysym == XK_Escape)
    {
      /* End move or resize and restore to original state.  If the
       * window was a maximized window that had been "shaken loose" we
       * need to remaximize it.  In normal cases, we need to do a
       * moveresize now to get the position back to the original.  In
       * wireframe mode, we just need to set grab_was_cancelled to tru
       * to avoid avoid moveresizing to the position of the wireframe.
       */
      if (window->shaken_loose)
        meta_window_maximize (window,
                              META_MAXIMIZE_HORIZONTAL |
                              META_MAXIMIZE_VERTICAL);
      else if (!display->grab_wireframe_active)
        meta_window_move_resize (display->grab_window,
                                 TRUE,
                                 display->grab_initial_window_pos.x,
                                 display->grab_initial_window_pos.y,
                                 display->grab_initial_window_pos.width,
                                 display->grab_initial_window_pos.height);
      else
        display->grab_was_cancelled = TRUE;

      /* End grab */
      return FALSE;
    }

  return TRUE;
}

static gboolean
process_keyboard_move_grab (MetaDisplay *display,
                            MetaScreen  *screen,
                            MetaWindow  *window,
                            XEvent      *event,
                            KeySym       keysym)
{
  gboolean handled;
  int x, y;
  int incr;
  gboolean smart_snap;
  
  handled = FALSE;

  /* don't care about releases, but eat them, don't end grab */
  if (event->type == KeyRelease)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, event->xkey.keycode))
    return TRUE;

  if (display->grab_wireframe_active)
    {
      x = display->grab_wireframe_rect.x;
      y = display->grab_wireframe_rect.y;
    }
  else
    {
      meta_window_get_position (window, &x, &y);
    }

  smart_snap = (event->xkey.state & ShiftMask) != 0;
  
#define SMALL_INCREMENT 1
#define NORMAL_INCREMENT 10

  if (smart_snap)
    incr = 1;
  else if (event->xkey.state & ControlMask)
    incr = SMALL_INCREMENT;
  else
    incr = NORMAL_INCREMENT;

  if (keysym == XK_Escape)
    {
      /* End move and restore to original state.  If the window was a
       * maximized window that had been "shaken loose" we need to
       * remaximize it.  In normal cases, we need to do a moveresize
       * now to get the position back to the original.  In wireframe
       * mode, we just need to set grab_was_cancelled to tru to avoid
       * avoid moveresizing to the position of the wireframe.
       */
      if (window->shaken_loose)
        meta_window_maximize (window,
                              META_MAXIMIZE_HORIZONTAL |
                              META_MAXIMIZE_VERTICAL);
      else if (!display->grab_wireframe_active)
        meta_window_move_resize (display->grab_window,
                                 TRUE,
                                 display->grab_initial_window_pos.x,
                                 display->grab_initial_window_pos.y,
                                 display->grab_initial_window_pos.width,
                                 display->grab_initial_window_pos.height);
      else
        display->grab_was_cancelled = TRUE;
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

      if (display->grab_wireframe_active)
        old_rect = display->grab_wireframe_rect;
      else
        meta_window_get_client_root_coords (window, &old_rect);

      meta_window_edge_resistance_for_move (window, 
                                            old_rect.x,
                                            old_rect.y,
                                            &x,
                                            &y,
                                            NULL,
                                            smart_snap,
                                            TRUE);

      if (display->grab_wireframe_active)
        {
          meta_window_update_wireframe (window, x, y,
                                        display->grab_wireframe_rect.width,
                                        display->grab_wireframe_rect.height);
        }
      else
        {
          meta_window_move (window, TRUE, x, y);
        }
      
      meta_window_update_keyboard_move (window);
    }

  return handled;
}

static gboolean
process_keyboard_resize_grab_op_change (MetaDisplay *display,
                                        MetaScreen  *screen,
                                        MetaWindow  *window,
                                        XEvent      *event,
                                        KeySym       keysym)
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
process_keyboard_resize_grab (MetaDisplay *display,
                              MetaScreen  *screen,
                              MetaWindow  *window,
                              XEvent      *event,
                              KeySym       keysym)
{
  gboolean handled;
  int height_inc;
  int width_inc;
  int width, height;
  gboolean smart_snap;
  int gravity;
  
  handled = FALSE;

  /* don't care about releases, but eat them, don't end grab */
  if (event->type == KeyRelease)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, event->xkey.keycode))
    return TRUE;

  if (keysym == XK_Escape)
    {
      /* End resize and restore to original state.  If not in
       * wireframe mode, we need to do a moveresize now to get the
       * position back to the original.  If we are in wireframe mode,
       * we need to avoid moveresizing to the position of the
       * wireframe.
       */
      if (!display->grab_wireframe_active)
        meta_window_move_resize (display->grab_window,
                                 TRUE,
                                 display->grab_initial_window_pos.x,
                                 display->grab_initial_window_pos.y,
                                 display->grab_initial_window_pos.width,
                                 display->grab_initial_window_pos.height);
      else
        display->grab_was_cancelled = TRUE;

      return FALSE;
    }

  if (process_keyboard_resize_grab_op_change (display, screen, window, 
                                              event, keysym))
    return TRUE;

  if (display->grab_wireframe_active)
    {
      width = display->grab_wireframe_rect.width;
      height = display->grab_wireframe_rect.height;
    }
  else
    {
      width = window->rect.width;
      height = window->rect.height;
    }

  gravity = meta_resize_gravity_from_grab_op (display->grab_op);

  smart_snap = (event->xkey.state & ShiftMask) != 0;
  
#define SMALL_INCREMENT 1
#define NORMAL_INCREMENT 10

  if (smart_snap)
    {
      height_inc = 1;
      width_inc = 1;
    }
  else if (event->xkey.state & ControlMask)
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
      
      if (display->grab_wireframe_active)
        old_rect = display->grab_wireframe_rect;
      else
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

      if (display->grab_wireframe_active)
        {
          MetaRectangle new_position;
          meta_rectangle_resize_with_gravity (&display->grab_wireframe_rect, 
                                              &new_position,
                                              gravity,
                                              width,
                                              height);
          meta_window_update_wireframe (window, 
                                        new_position.x,
                                        new_position.y,
                                        new_position.width, 
                                        new_position.height);
        }
      else
        {
          /* We don't need to update unless the specified width and height
           * are actually different from what we had before.
           */
          if (window->rect.width != width || window->rect.height != height)
            meta_window_resize_with_gravity (window, 
                                             TRUE,
                                             width,
                                             height,
                                             gravity);
        }
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
process_tab_grab (MetaDisplay *display,
                  MetaScreen  *screen,
                  XEvent      *event,
                  KeySym       keysym)
{
  MetaKeyBindingAction action;
  gboolean popup_not_showing;
  gboolean backward;
  gboolean key_used;
  MetaWindow *prev_window;

  if (screen != display->grab_screen)
    return FALSE;

  action = display_get_keybinding_action (display,
                                          keysym,
                                          event->xkey.keycode,
                                          display->grab_mask);

  /*
   * If there is no tab_pop up object, i.e., there is some custom handler
   * implementing Alt+Tab & Co., we call this custom handler; we do not
   * mess about with the grab, as that is up to the handler to deal with.
   */
  if (!screen->tab_handler)
    {
      MetaKeyHandler *handler = NULL;
      const gchar    *handler_name = NULL;

      switch (action)
        {
        case META_KEYBINDING_ACTION_CYCLE_PANELS:
          handler_name = "cycle_group";
          break;
        case META_KEYBINDING_ACTION_CYCLE_WINDOWS:
          handler_name = "cycle_windows";
          break;
        case META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD:
          handler_name = "cycle_panels_backward";
          break;
        case META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD:
          handler_name = "cycle_windows_backward";
          break;
        case META_KEYBINDING_ACTION_SWITCH_PANELS:
          handler_name = "switch_panels";
          break;
        case META_KEYBINDING_ACTION_SWITCH_WINDOWS:
          handler_name = "switch_windows";
          break;
        case META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD:
          handler_name = "switch_panels_backward";
          break;
        case META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD:
          handler_name = "switch_windows_backward";
          break;
        case META_KEYBINDING_ACTION_CYCLE_GROUP:
          handler_name = "cycle_group";
          break;
        case META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD:
          handler_name = "cycle_group_backward";
          break;
        case META_KEYBINDING_ACTION_SWITCH_GROUP:
          handler_name = "switch_group";
          break;
        case META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD:
          handler_name = "switch_group_backward";
          break;
        default:
          /*
           * This is the case when the Alt key is released; we preserve
           * the grab, as it is up to the custom implementaiton to free it
           * (a plugin can catch this in their xevent_filter function).
           */
          return TRUE;
        }

      /*
       * We do not want to actually call the handler, we just want to ensure
       * that if a custom handler is installed, we do not release the grab here.
       * The handler will get called as normal in the process_event() function.
       */
      handler = find_handler (key_handlers, handler_name);

      if (!handler || !handler->func || handler->func == handler->default_func)
        return FALSE;

      return TRUE;
    }

  if (event->type == KeyRelease &&
      end_keyboard_grab (display, event->xkey.keycode))
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
          meta_window_activate (target_window, event->xkey.time);

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Ending grab early so we can focus the target window\n");
          meta_display_end_grab_op (display, event->xkey.time);

          return TRUE; /* we already ended the grab */
        }
      
      return FALSE; /* end grab */
    }
  
  /* don't care about other releases, but eat them, don't end grab */
  if (event->type == KeyRelease)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, event->xkey.keycode))
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
    case META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD:
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
    case META_KEYBINDING_ACTION_SWITCH_GROUP:
      key_used = TRUE;
      break;
    case META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD:
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

      if (event->xkey.state & ShiftMask)
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
                           XEvent         *event,
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
      meta_workspace_activate (workspace, event->xkey.time);
    }
  else
    {
      /* We could offer to create it I suppose */
    }
}

static void
error_on_command (int         command_index,
                  const char *command,
                  const char *message,
                  int         screen_number,
                  guint32     timestamp)
{
  if (command_index < 0)
    meta_warning ("Error on terminal command \"%s\": %s\n", command, message);  
  else
    meta_warning ("Error on command %d \"%s\": %s\n",
                  command_index, command, message);  

  /*
    mutter-dialog said:
        
    FIXME offer to change the value of the command's gconf key
  */

  if (command && strcmp(command, "")!=0)
    {
      char *text = g_strdup_printf (
                                    /* Displayed when a keybinding which is
                                     * supposed to launch a program fails.
                                     */
                                    _("There was an error running "
                                      "<tt>%s</tt>:\n\n%s"),
                                    command,
                                    message);

      meta_show_dialog ("--error",
                        text,
                        NULL,
                        screen_number,
                        NULL, NULL, 0,
                        NULL, NULL);

      g_free (text);

    }
  else
    {
      meta_show_dialog ("--error",
                        message,
                        NULL,
                        screen_number,
                        NULL, NULL, 0,
                        NULL, NULL);
    }
}

static void
set_display_setup_func (void *data)
{
  const char *screen_name = data;
  char *full;

  full = g_strdup_printf ("DISPLAY=%s", screen_name);

  putenv (full);

  /* do not free full, because putenv is lame */
} 

static gboolean
meta_spawn_command_line_async_on_screen (const gchar *command_line,
                                         MetaScreen  *screen,
                                         GError     **error)
{
  gboolean retval;
  gchar **argv = NULL;

  g_return_val_if_fail (command_line != NULL, FALSE);

  if (!g_shell_parse_argv (command_line,
                           NULL, &argv,
                           error))
    return FALSE;
  
  retval = g_spawn_async (NULL,
                          argv,
                          NULL,
                          G_SPAWN_SEARCH_PATH,
                          set_display_setup_func,
                          screen->screen_name,
                          NULL,
                          error);
  g_strfreev (argv);

  return retval;
}


static void
handle_run_command (MetaDisplay    *display,
                    MetaScreen     *screen,
                    MetaWindow     *window,
                    XEvent         *event,
                    MetaKeyBinding *binding,
                    gpointer        dummy)
{
  gint which = binding->handler->data;
  const char *command;
  GError *err;
  
  command = meta_prefs_get_command (which);

  if (command == NULL)
    {
      char *s;

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "No command %d to run in response to keybinding press\n",
                  which);
      
      s = g_strdup_printf (_("No command %d has been defined.\n"),
                           which + 1);
      error_on_command (which, NULL, s, screen->number, event->xkey.time);
      g_free (s);
      
      return;
    }

  err = NULL;
  if (!meta_spawn_command_line_async_on_screen (command, screen, &err))
    {
      error_on_command (which, command, err->message, screen->number, event->xkey.time);
      
      g_error_free (err);
    }
}


static void
handle_maximize_vertically (MetaDisplay    *display,
                      MetaScreen     *screen,
                      MetaWindow     *window,
                      XEvent         *event,
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
                       XEvent         *event,
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
  MetaRectangle outer;
  int orig_x, orig_y;
  int new_x, new_y;
  int frame_width, frame_height;

  meta_window_get_work_area_all_xineramas (window, &work_area);
  meta_window_get_outer_rect (window, &outer);
  meta_window_get_position (window, &orig_x, &orig_y);

  frame_width = (window->frame ? window->frame->child_x : 0);
  frame_height = (window->frame ? window->frame->child_y : 0);

  if (xchange) {
    new_x = work_area.x + (to_right ?
            (work_area.width + frame_width) - outer.width :
            0);
  } else {
    new_x = orig_x;
  }

  if (ychange) {
    new_y = work_area.y + (to_bottom ?
            (work_area.height + frame_height) - outer.height :
            0);
  } else {
    new_y = orig_y;
  }

  meta_window_move_resize (window,
          FALSE,
          new_x,
          new_y,
          window->rect.width,
          window->rect.height);
}

static void
handle_move_to_corner_nw  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, FALSE, FALSE, dummy);
}

static void
handle_move_to_corner_ne  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, TRUE, FALSE, dummy);
}

static void
handle_move_to_corner_sw  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, FALSE, TRUE, dummy);
}

static void
handle_move_to_corner_se  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, TRUE, TRUE, TRUE, dummy);
}

static void
handle_move_to_side_n     (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, FALSE, TRUE, FALSE, FALSE, dummy);
}

static void
handle_move_to_side_s     (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, FALSE, TRUE, FALSE, TRUE, dummy);
}

static void
handle_move_to_side_e     (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, FALSE, TRUE, FALSE, dummy);
}

static void
handle_move_to_side_w     (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  handle_move_to_corner_backend (display, screen, window, TRUE, FALSE, FALSE, FALSE, dummy);
}

static void
handle_move_to_center  (MetaDisplay    *display,
                        MetaScreen     *screen,
                        MetaWindow     *window,
                        XEvent         *event,
                        MetaKeyBinding *binding,
                        gpointer        dummy)
{
  MetaRectangle work_area;
  MetaRectangle outer;
  int orig_x, orig_y;
  int frame_width, frame_height;

  meta_window_get_work_area_all_xineramas (window, &work_area);
  meta_window_get_outer_rect (window, &outer);
  meta_window_get_position (window, &orig_x, &orig_y);

  frame_width = (window->frame ? window->frame->child_x : 0);
  frame_height = (window->frame ? window->frame->child_y : 0);

  meta_window_move_resize (window,
          TRUE,
          work_area.x + (work_area.width +frame_width -outer.width )/2,
          work_area.y + (work_area.height+frame_height-outer.height)/2,
          window->rect.width,
          window->rect.height);
}

static gboolean
process_workspace_switch_grab (MetaDisplay *display,
                               MetaScreen  *screen,
                               XEvent      *event,
                               KeySym       keysym)
{
  MetaWorkspace *workspace;

  if (screen != display->grab_screen || !screen->ws_popup)
    return FALSE;

  if (event->type == KeyRelease &&
      end_keyboard_grab (display, event->xkey.keycode))
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
          meta_display_end_grab_op (display, event->xkey.time);

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Focusing default window on target workspace\n");

          meta_workspace_focus_default_window (target_workspace, 
                                               NULL,
                                               event->xkey.time);

          return TRUE; /* we already ended the grab */
        }

      /* Workspace switching should have already occurred on KeyPress */
      meta_warning ("target_workspace != active_workspace.  Some other event must have occurred.\n");
      
      return FALSE; /* end grab */
    }
  
  /* don't care about other releases, but eat them, don't end grab */
  if (event->type == KeyRelease)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, event->xkey.keycode))
    return TRUE;

  /* select the next workspace in the popup */
  workspace = meta_screen_workspace_popup_get_selected (screen);
  
  if (workspace)
    {
      MetaWorkspace *target_workspace;
      MetaKeyBindingAction action;

      action = display_get_keybinding_action (display,
                                              keysym,
                                              event->xkey.keycode,
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

          meta_workspace_activate (target_workspace, event->xkey.time);

          return TRUE; /* we already ended the grab */
        }
    }

  /* end grab */
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Ending workspace tabbing & focusing default window; uninteresting key pressed\n");
  workspace = meta_screen_workspace_popup_get_selected (screen);
  meta_workspace_focus_default_window (workspace, NULL, event->xkey.time);
  return FALSE;
}

static void
handle_show_desktop (MetaDisplay    *display,
                       MetaScreen     *screen,
                       MetaWindow     *window,
                       XEvent         *event,
                       MetaKeyBinding *binding,
                       gpointer        dummy)
{
  if (screen->active_workspace->showing_desktop)
    {
      meta_screen_unshow_desktop (screen);
      meta_workspace_focus_default_window (screen->active_workspace, 
                                           NULL,
                                           event->xkey.time);
    }
  else
    meta_screen_show_desktop (screen, event->xkey.time);
}

static void
handle_panel (MetaDisplay    *display,
                         MetaScreen     *screen,
                         MetaWindow     *window,
                         XEvent         *event,
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
  ev.data.l[1] = event->xkey.time;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Sending panel message with timestamp %lu, and turning mouse_mode "
              "off due to keybinding press\n", event->xkey.time);
  display->mouse_mode = FALSE;

  meta_error_trap_push (display);

  /* Release the grab for the panel before sending the event */
  XUngrabKeyboard (display->xdisplay, event->xkey.time);

  XSendEvent (display->xdisplay,
	      screen->xroot,
	      False,
	      StructureNotifyMask,
	      (XEvent*) &ev);

  meta_error_trap_pop (display, FALSE);
}

static void
handle_toggle_recording (MetaDisplay    *display,
                         MetaScreen     *screen,
                         MetaWindow     *window,
                         XEvent         *event,
                         MetaKeyBinding *binding,
                         gpointer        dummy)
{
  g_signal_emit_by_name (screen, "toggle-recording");
}

static void
handle_activate_window_menu (MetaDisplay    *display,
                      MetaScreen     *screen,
                      MetaWindow     *event_window,
                      XEvent         *event,
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
                             event->xkey.time);
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
    }

  g_assert_not_reached ();
  
  return 0;
}

static void
do_choose_window (MetaDisplay    *display,
                  MetaScreen     *screen,
                  MetaWindow     *event_window,
                  XEvent         *event,
                  MetaKeyBinding *binding,
                  gboolean        backward,
                  gboolean        show_popup)
{
  MetaTabList type = binding->handler->data;
  MetaWindow *initial_selection;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Tab list = %u show_popup = %d\n", type, show_popup);
  
  /* reverse direction if shift is down */
  if (event->xkey.state & ShiftMask)
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
      meta_window_activate (initial_selection, event->xkey.time);
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
                                   event->xkey.time,
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
      meta_display_end_grab_op (display, event->xkey.time);
      display->mouse_mode = FALSE;
      meta_window_activate (initial_selection, event->xkey.time);
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
                    XEvent         *event,
                    MetaKeyBinding *binding,
                    gpointer        dummy)
{
  gint backwards = binding->handler->flags & BINDING_IS_REVERSED;

  do_choose_window (display, screen, event_window, event, binding,
                    backwards, TRUE);
}

static void
handle_cycle (MetaDisplay    *display,
                    MetaScreen     *screen,
                    MetaWindow     *event_window,
                    XEvent         *event,
                    MetaKeyBinding *binding,
                    gpointer        dummy)
{
  gint backwards = binding->handler->flags & BINDING_IS_REVERSED;

  do_choose_window (display, screen, event_window, event, binding,
                    backwards, FALSE);
}


static void
handle_toggle_fullscreen  (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
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
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->wm_state_above)
    meta_window_unmake_above (window);
  else
    meta_window_make_above (window);
}

static void
handle_toggle_maximized    (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
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
                           XEvent         *event,
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
                           XEvent         *event,
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
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->shaded)
    meta_window_unshade (window, event->xkey.time);
  else if (window->has_shade_func)
    meta_window_shade (window, event->xkey.time);
}

static void
handle_close              (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->has_close_func)
    meta_window_delete (window, event->xkey.time);
}

static void
handle_minimize        (MetaDisplay    *display,
                        MetaScreen     *screen,
                        MetaWindow     *window,
                        XEvent         *event,
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
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->has_move_func)
    {
      meta_window_begin_grab_op (window,
                                 META_GRAB_OP_KEYBOARD_MOVING,
                                 FALSE,
                                 event->xkey.time);
    }
}

static void
handle_begin_resize       (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->has_resize_func)
    {
      meta_window_begin_grab_op (window,
                                 META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN,
                                 FALSE,
                                 event->xkey.time);
    }
}

static void
handle_toggle_on_all_workspaces (MetaDisplay    *display,
                           MetaScreen     *screen,
                           MetaWindow     *window,
                           XEvent         *event,
                           MetaKeyBinding *binding,
                           gpointer        dummy)
{
  if (window->on_all_workspaces)
    meta_window_unstick (window);
  else
    meta_window_stick (window);
}

static void
handle_move_to_workspace  (MetaDisplay    *display,
                              MetaScreen     *screen,
                              MetaWindow     *window,
                              XEvent         *event,
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
          workspace->screen->display->mouse_mode = FALSE;
          meta_workspace_activate_with_focus (workspace,
                                              window,
                                              event->xkey.time);
        }
    }
  else
    {
      /* We could offer to create it I suppose */
    }  
}

static void 
handle_raise_or_lower (MetaDisplay    *display,
                       MetaScreen     *screen,
		       MetaWindow     *window,
		       XEvent         *event,
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
          meta_window_get_outer_rect (window, &win_rect);
          meta_window_get_outer_rect (above, &above_rect);
          
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
              XEvent         *event,
              MetaKeyBinding *binding,
              gpointer        dummy)
{
  meta_window_raise (window);
}

static void
handle_lower (MetaDisplay    *display,
              MetaScreen     *screen,
              MetaWindow     *window,
              XEvent         *event,
              MetaKeyBinding *binding,
              gpointer        dummy)
{
  meta_window_lower (window);
}

static void
handle_workspace_switch  (MetaDisplay    *display,
                          MetaScreen     *screen,
                          MetaWindow     *window,
                          XEvent         *event,
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
  grab_mask = event->xkey.state & ~(display->ignored_modifier_mask);
  
  if (!meta_display_begin_grab_op (display,
                                   screen,
                                   NULL,
                                   META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING,
                                   FALSE,
                                   FALSE,
                                   0,
                                   grab_mask,
                                   event->xkey.time,
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
      meta_display_end_grab_op (display, event->xkey.time);
    }

  meta_workspace_activate (next, event->xkey.time);

  if (grabbed_before_release && !meta_prefs_get_no_tab_popup ())
    meta_screen_workspace_popup_create (screen, next);
}

static void
handle_set_spew_mark (MetaDisplay    *display,
                  MetaScreen     *screen,
                  MetaWindow     *window,
                  XEvent         *event,
                      MetaKeyBinding *binding,
                      gpointer        dummy)
{
  meta_verbose ("-- MARK MARK MARK MARK --\n");
}

void
meta_set_keybindings_disabled (gboolean setting)
{
  all_bindings_disabled = setting;
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Keybindings %s\n", all_bindings_disabled ? "disabled" : "enabled");
}

static void
handle_run_terminal (MetaDisplay    *display,
                     MetaScreen     *screen,
                     MetaWindow     *window,
                     XEvent         *event,
                     MetaKeyBinding *binding,
                     gpointer        dummy)
{
  const char *command;
  GError *err;
  
  command = meta_prefs_get_terminal_command ();

  if (command == NULL)
    {
      char *s;

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "No terminal command to run in response to "
		  "keybinding press\n");
      
      s = g_strdup_printf (_("No terminal command has been defined.\n"));
      error_on_command (-1, NULL, s, screen->number, event->xkey.time);
      g_free (s);
      
      return;
    }

  err = NULL;
  if (!meta_spawn_command_line_async_on_screen (command, screen, &err))
    {
      error_on_command (-1, command, err->message, screen->number,
                        event->xkey.time);
      
      g_error_free (err);
    }
}

gboolean
meta_keybindings_set_custom_handler (const gchar        *name,
                                     MetaKeyHandlerFunc  handler,
                                     gpointer            user_data,
                                     GDestroyNotify      free_data)
{
  MetaKeyHandler *key_handler = find_handler (key_handlers, name);

  if (!key_handler)
    return FALSE;

  if (key_handler->user_data_free_func && key_handler->user_data)
    key_handler->user_data_free_func (key_handler->user_data);

  key_handler->func = handler;
  key_handler->user_data = user_data;
  key_handler->user_data_free_func = free_data;

  return TRUE;
}

void
meta_keybindings_switch_window (MetaDisplay    *display,
                                MetaScreen     *screen,
                                MetaWindow     *event_window,
                                XEvent         *event,
                                MetaKeyBinding *binding)
{
  gint backwards = binding->handler->flags & BINDING_IS_REVERSED;

  do_choose_window (display, screen, event_window, event, binding,
                    backwards, FALSE);
}

