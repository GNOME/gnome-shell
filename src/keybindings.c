/* Metacity Keybindings */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include "keybindings.h"
#include "workspace.h"
#include "errors.h"
#include "ui.h"
#include "frame.h"
#include "place.h"

#include <X11/keysym.h>

/* Plainly we'll want some more configurable keybinding system
 * eventually.
 */

typedef void (* MetaKeyHandler) (MetaDisplay *display,
                                 MetaWindow  *window,
                                 XEvent      *event,
                                 gpointer     data);

static void handle_activate_workspace (MetaDisplay *display,
                                       MetaWindow  *window,
                                       XEvent      *event,
                                       gpointer     data);
static void handle_activate_menu      (MetaDisplay *display,
                                       MetaWindow  *window,
                                       XEvent      *event,
                                       gpointer     data);
static void handle_tab_forward        (MetaDisplay *display,
                                       MetaWindow  *window,
                                       XEvent      *event,
                                       gpointer     data);
static void handle_tab_backward       (MetaDisplay *display,
                                       MetaWindow  *window,
                                       XEvent      *event,
                                       gpointer     data);
static void handle_focus_previous     (MetaDisplay *display,
                                       MetaWindow  *window,
                                       XEvent      *event,
                                       gpointer     data);
static void handle_workspace_left     (MetaDisplay *display,
                                       MetaWindow  *window,
                                       XEvent      *event,
                                       gpointer     data);
static void handle_workspace_right     (MetaDisplay *display,
                                        MetaWindow  *window,
                                        XEvent      *event,
                                        gpointer     data);

static gboolean process_keyboard_move_grab (MetaDisplay *display,
                                            MetaWindow  *window,
                                            XEvent      *event,
                                            KeySym       keysym);

static gboolean process_tab_grab           (MetaDisplay *display,
                                            MetaWindow  *window,
                                            XEvent      *event,
                                            KeySym       keysym);

typedef struct _MetaKeyBinding MetaKeyBinding;

struct _MetaKeyBinding
{
  KeySym keysym;
  gulong mask;
  int event_type;
  MetaKeyHandler handler;
  gpointer data;
  int keycode;
};

#define IGNORED_MODIFIERS (LockMask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)
#define INTERESTING_MODIFIERS (~IGNORED_MODIFIERS)

static MetaKeyBinding screen_bindings[] = {
  { XK_F1, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (0), 0 },
  { XK_F2, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (1), 0 },
  { XK_F3, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (2), 0 },
  { XK_F4, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (3), 0 },
  { XK_F5, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (4), 0 },
  { XK_F6, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (5), 0 },
  { XK_1, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (0), 0 },
  { XK_2, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (1), 0 },
  { XK_3, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (2), 0 },
  { XK_4, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (3), 0 },
  { XK_5, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (4), 0 },
  { XK_6, Mod1Mask, KeyPress, handle_activate_workspace, GINT_TO_POINTER (5), 0 },
  { XK_Tab, Mod1Mask, KeyPress, handle_tab_forward, NULL, 0 },
  { XK_ISO_Left_Tab, ShiftMask | Mod1Mask, KeyPress, handle_tab_backward, NULL, 0 },
  { XK_Tab, ShiftMask | Mod1Mask, KeyPress, handle_tab_backward, NULL, 0 },
  { XK_Escape, Mod1Mask, KeyPress, handle_focus_previous, NULL, 0 },
  { XK_Left, Mod1Mask, KeyPress, handle_workspace_left, NULL, 0 },
  { XK_Right, Mod1Mask, KeyPress, handle_workspace_right, NULL, 0 },
  { None, 0, 0, NULL, NULL, 0 }
};

static MetaKeyBinding window_bindings[] = {
  { XK_space, Mod1Mask, KeyPress, handle_activate_menu, NULL, 0 },
  { XK_Tab, Mod1Mask, KeyPress, handle_tab_forward, NULL, 0 },
  { XK_ISO_Left_Tab, ShiftMask | Mod1Mask, KeyPress, handle_tab_backward, NULL, 0 },
  { XK_Tab, ShiftMask | Mod1Mask, KeyPress, handle_tab_backward, NULL, 0 },
  { XK_Escape, Mod1Mask, KeyPress, handle_focus_previous, NULL, 0 },
  { None, 0, 0, NULL, NULL, 0 }
};

static void
init_bindings (MetaDisplay    *display,
               MetaKeyBinding *bindings)
{
  int i;

  i = 0;
  while (bindings[i].keysym != None)
    {
      bindings[i].keycode = XKeysymToKeycode (display->xdisplay,
                                              bindings[i].keysym);
      
      ++i;
    }
}               

void
meta_display_init_keys (MetaDisplay *display)
{
  init_bindings (display, screen_bindings);
  init_bindings (display, window_bindings);
}

/* Grab/ungrab, ignoring all annoying modifiers like NumLock etc. */
static void
meta_change_keygrab (MetaDisplay *display,
                     Window       xwindow,
                     gboolean     grab,
                     int          keysym,
                     int          keycode,
                     int          modmask)
{
  int result;
  int ignored_mask;

  /* Grab keycode/modmask, together with
   * all combinations of IGNORED_MODIFIERS.
   * X provides no better way to do this.
   */

  /* modmask can't contain any non-interesting modifiers */
  g_return_if_fail ((modmask & INTERESTING_MODIFIERS) == modmask);
  
  ignored_mask = 0;
  while (ignored_mask < IGNORED_MODIFIERS)
    {
      if (ignored_mask & INTERESTING_MODIFIERS)
        {
          /* Not a combination of IGNORED_MODIFIERS
           * (it contains some non-ignored modifiers)
           */
          ++ignored_mask;
          continue;
        }
      
      meta_error_trap_push (display);
      if (grab)
        XGrabKey (display->xdisplay, keycode,
                  modmask | ignored_mask,
                  xwindow,
                  True,
                  GrabModeAsync, GrabModeAsync);
      else
        XUngrabKey (display->xdisplay, keycode,
                    modmask | ignored_mask,
                    xwindow);
      
      result = meta_error_trap_pop (display);

      if (grab && result != Success)
        {
          const char *name;
          
          name = XKeysymToString (keysym);
          if (name == NULL)
            name = "(unknown)";
      
          if (result == BadAccess)
            meta_warning (_("Some other program is already using the key %s with modifiers %x as a binding\n"), name, modmask | ignored_mask);
        }

      ++ignored_mask;
    }
}

static void
meta_grab_key (MetaDisplay *display,
               Window       xwindow,
               int          keysym,
               int          keycode,
               int          modmask)
{
  meta_change_keygrab (display, xwindow, TRUE, keysym, keycode, modmask);
}

static void
meta_ungrab_key (MetaDisplay *display,
                 Window       xwindow,
                 int          keysym,
                 int          keycode,
                 int          modmask)
{
  meta_change_keygrab (display, xwindow, FALSE, keysym, keycode, modmask);
}

static void
grab_keys (MetaKeyBinding *bindings,
           MetaDisplay    *display,
           Window          xwindow)
{
  int i;

  i = 0;
  while (bindings[i].keysym != None)
    {
      if (bindings[i].keycode != 0)
        {
          meta_grab_key (display, xwindow,
                         bindings[i].keysym,
                         bindings[i].keycode,
                         bindings[i].mask);
        }
      
      ++i;
    }
}

static void
ungrab_keys (MetaKeyBinding *bindings,
             MetaDisplay    *display,
             Window          xwindow)
{
  int i;

  i = 0;
  while (bindings[i].keysym != None)
    {
      if (bindings[i].keycode != 0)
        {
          meta_ungrab_key (display, xwindow,
                           bindings[i].keysym,
                           bindings[i].keycode,
                           bindings[i].mask);
        }
      
      ++i;
    }
}

void
meta_screen_grab_keys (MetaScreen  *screen)
{
  grab_keys (screen_bindings, screen->display, screen->xroot);
}

void
meta_screen_ungrab_keys (MetaScreen  *screen)
{
  ungrab_keys (screen_bindings, screen->display, screen->xroot);
}

void
meta_window_grab_keys (MetaWindow  *window)
{
  if (window->all_keys_grabbed)
    return;
  
  if (window->keys_grabbed)
    {
      if (window->frame && !window->grab_on_frame)
        ungrab_keys (window_bindings, window->display,
                     window->xwindow);
      else if (window->frame == NULL &&
               window->grab_on_frame)
        ; /* continue to regrab on client window */
      else
        return; /* already all good */
    }

  /* no keybindings for Emacs ;-) */
  if (window->res_class &&
      g_strcasecmp (window->res_class, "Emacs") == 0)
    return;
  
  grab_keys (window_bindings, window->display,
             window->frame ? window->frame->xwindow : window->xwindow);

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
        ungrab_keys (window_bindings, window->display,
                     window->frame->xwindow);
      else if (!window->grab_on_frame)
        ungrab_keys (window_bindings, window->display,
                     window->xwindow);
    }
}

gboolean
meta_window_grab_all_keys (MetaWindow  *window)
{
  int result;
  Window grabwindow;
  
  if (window->all_keys_grabbed)
    return FALSE;
  
  if (window->keys_grabbed)
    meta_window_ungrab_keys (window);

  /* Make sure the window is focused, otherwise the grab
   * won't do a lot of good.
   */
  meta_window_focus (window, CurrentTime);
  
  grabwindow = window->frame ? window->frame->xwindow : window->xwindow;
  
  meta_error_trap_push (window->display);
  XGrabKey (window->display->xdisplay, AnyKey, AnyModifier,
            grabwindow, True,
            GrabModeAsync, GrabModeAsync);
  
  result = meta_error_trap_pop (window->display);
  if (result != Success)
    {
      meta_verbose ("Global key grab failed for window %s\n", window->desc);
      return FALSE;
    }
  else
    {
      /* Also grab the keyboard, so we get key releases and all key
       * presses
       */
       meta_error_trap_push (window->display);
       /* FIXME CurrentTime bogus */
       XGrabKeyboard (window->display->xdisplay,
                      grabwindow, True,
                      GrabModeAsync, GrabModeAsync,
                      CurrentTime);
       
       result = meta_error_trap_pop (window->display);
       if (result != Success)
         {
           meta_verbose ("XGrabKeyboard() failed for window %s\n",
                         window->desc);
           return FALSE;
         }
       
       meta_verbose ("Grabbed all keys on window %s\n", window->desc);
       
       window->keys_grabbed = FALSE;
       window->all_keys_grabbed = TRUE;
       window->grab_on_frame = window->frame != NULL;
       return TRUE;
    }
}

void
meta_window_ungrab_all_keys (MetaWindow  *window)
{
  if (window->all_keys_grabbed)
    {
      Window grabwindow;

      grabwindow = (window->frame && window->grab_on_frame) ?
        window->frame->xwindow : window->xwindow;
      
      meta_error_trap_push (window->display);
      XUngrabKey (window->display->xdisplay,
                  AnyKey, AnyModifier,
                  grabwindow);
      /* FIXME CurrentTime bogus */
      XUngrabKeyboard (window->display->xdisplay, CurrentTime);
      meta_error_trap_pop (window->display);
      
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
  XModifierKeymap *mod_keymap;
  gboolean retval = FALSE;
  
  /* FIXME this is ass-slow, cache the modmap */
  
  mod_keymap = XGetModifierMapping (display->xdisplay);
  
  map_size = 8 * mod_keymap->max_keypermod;
  i = 0;
  while (i < map_size)
    {
      if (keycode == mod_keymap->modifiermap[i])
        {
          retval = TRUE;
          break;
        }
      ++i;
    }
  
  XFreeModifiermap (mod_keymap);
  
  return retval;
}

#define MOD1_INDEX 3 /* shift, lock, control, mod1 */
static gboolean 
is_mod1_key (MetaDisplay *display,
             unsigned int keycode)
{
  int i;
  int end;
  XModifierKeymap *mod_keymap;
  gboolean retval = FALSE;
  
  /* FIXME this is ass-slow, cache the modmap */
  
  mod_keymap = XGetModifierMapping (display->xdisplay);
  
  end = (MOD1_INDEX + 1) * mod_keymap->max_keypermod;
  i = MOD1_INDEX * mod_keymap->max_keypermod;
  while (i < end)
    {
      if (keycode == mod_keymap->modifiermap[i])
        {
          retval = TRUE;
          break;
        }
      ++i;
    }
  
  XFreeModifiermap (mod_keymap);
  
  return retval;
}

static void
process_event (MetaKeyBinding *bindings,
               MetaDisplay *display,
               MetaWindow  *window,
               XEvent      *event,
               KeySym       keysym)
{
  int i;

  i = 0;
  while (bindings[i].keysym != None)
    {
      if (bindings[i].keysym == keysym && 
          ((event->xkey.state & INTERESTING_MODIFIERS) ==
           bindings[i].mask) &&
          bindings[i].event_type == event->type)
        {
          (* bindings[i].handler) (display, window, event, bindings[i].data);
          break;
        }
      
      ++i;
    }
}
                
void
meta_display_process_key_event (MetaDisplay *display,
                                MetaWindow  *window,
                                XEvent      *event)
{
  KeySym keysym;
  gboolean handled;

  /* window may be NULL */
  
  keysym = XKeycodeToKeysym (display->xdisplay, event->xkey.keycode, 0);
  
  meta_verbose ("Processing key %s event, keysym: %s state: 0x%x window: %s\n",
                event->type == KeyPress ? "press" : "release",
                XKeysymToString (keysym), event->xkey.state,
                window ? window->desc : "(no window)");

  if (window == NULL || !window->all_keys_grabbed)
    {
      /* Do the normal keybindings */
      process_event (screen_bindings, display, NULL, event, keysym);

      if (window)
        process_event (window_bindings, display, window, event, keysym);

      return;
    }
  
  if (display->grab_op == META_GRAB_OP_NONE)
    return;    

  /* If we get here we have a global grab, because
   * we're in some special keyboard mode such as window move
   * mode.
   */
  
  handled = FALSE;

  if (window == display->grab_window)
    {
      switch (display->grab_op)
        {
        case META_GRAB_OP_KEYBOARD_MOVING:
          meta_verbose ("Processing event for keyboard move\n");
          handled = process_keyboard_move_grab (display, window, event, keysym);
          break;
        case META_GRAB_OP_KEYBOARD_TABBING:
          meta_verbose ("Processing event for keyboard tabbing\n");
          handled = process_tab_grab (display, window, event, keysym);
          break;
        default:
          break;
        }
    }
  
  /* end grab if a key that isn't used gets pressed */
  if (!handled)
    {
      meta_verbose ("Ending grab op %d on key event sym %s\n",
                    display->grab_op, XKeysymToString (keysym));
      meta_display_end_grab_op (display, event->xkey.time);
    }
}

static gboolean
process_keyboard_move_grab (MetaDisplay *display,
                            MetaWindow  *window,
                            XEvent      *event,
                            KeySym       keysym)
{
  gboolean handled;
  int x, y;
  int incr;
  gboolean smart_snap;
  int edge;
  
  handled = FALSE;

  /* don't care about releases, but eat them, don't end grab */
  if (event->type == KeyRelease)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, event->xkey.keycode))
    return TRUE;

  meta_window_get_position (window, &x, &y);

  smart_snap = (event->xkey.state & ShiftMask) != 0;

#define SMALL_INCREMENT 1
#define NORMAL_INCREMENT 10

  if (smart_snap)
    incr = 0;
  else if (event->xkey.state & ControlMask)
    incr = SMALL_INCREMENT;
  else
    incr = NORMAL_INCREMENT;

      /* When moving by increments, we still snap to edges if the move
       * to the edge is smaller than the increment. This is because
       * Shift + arrow to snap is sort of a hidden feature. This way
       * people using just arrows shouldn't get too frustrated.
       */
      
  switch (keysym)
    {
    case XK_Up:
    case XK_KP_Up:
      edge = meta_window_find_next_horizontal_edge (window, FALSE);
      y -= incr;
          
      if (smart_snap || ((edge > y) && ABS (edge - y) < incr))
        y = edge;
          
      handled = TRUE;
      break;
    case XK_Down:
    case XK_KP_Down:
      edge = meta_window_find_next_horizontal_edge (window, TRUE);
      y += incr;

      if (smart_snap || ((edge < y) && ABS (edge - y) < incr))
        y = edge;
          
      handled = TRUE;
      break;
    case XK_Left:
    case XK_KP_Left:
      edge = meta_window_find_next_vertical_edge (window, FALSE);
      x -= incr;
          
      if (smart_snap || ((edge > x) && ABS (edge - x) < incr))
        x = edge;

      handled = TRUE;
      break;
    case XK_Right:
    case XK_KP_Right:
      edge = meta_window_find_next_vertical_edge (window, TRUE);
      x += incr;
      if (smart_snap || ((edge < x) && ABS (edge - x) < incr))
        x = edge;
      handled = TRUE;
      break;

    case XK_Escape:
      /* End move and restore to original position */
      meta_window_move_resize (display->grab_window,
                               TRUE,
                               display->grab_initial_window_pos.x,
                               display->grab_initial_window_pos.y,
                               display->grab_initial_window_pos.width,
                               display->grab_initial_window_pos.height);
      break;
          
    default:
      break;
    }

  if (handled)
    meta_window_move (window, TRUE, x, y);

  return handled;
}

static gboolean
process_tab_grab (MetaDisplay *display,
                  MetaWindow  *window,
                  XEvent      *event,
                  KeySym       keysym)
{
  MetaScreen *screen;

  window = NULL; /* be sure we don't use this, it's irrelevant */

  screen = display->grab_window->screen;

  g_return_val_if_fail (screen->tab_popup != NULL, FALSE);
  
  if (event->type == KeyRelease &&
      is_mod1_key (display, event->xkey.keycode))
    {
      /* We're done, move to the new window. */
      Window target_xwindow;
      MetaWindow *target_window;

      target_xwindow =
        meta_ui_tab_popup_get_selected (screen->tab_popup);
      target_window =
        meta_display_lookup_x_window (display, target_xwindow);

      meta_verbose ("Ending tab operation, Mod1 released\n");
      
      if (target_window)
        {
          meta_verbose ("Ending grab early so we can focus the target window\n");
          meta_display_end_grab_op (display, event->xkey.time);

          meta_verbose ("Focusing target window\n");
          meta_window_raise (target_window);
          meta_window_focus (target_window, event->xkey.time);

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

  switch (keysym)
    {
    case XK_ISO_Left_Tab:
    case XK_Tab:
      if (event->xkey.state & ShiftMask)
        meta_ui_tab_popup_backward (screen->tab_popup);
      else
        meta_ui_tab_popup_forward (screen->tab_popup);

      /* continue grab */
      meta_verbose ("Tab key pressed, moving tab focus in popup\n");
      return TRUE;
      break;

    default:
      break;
    }

  /* end grab */
  meta_verbose ("Ending tabbing, uninteresting key pressed\n");
  return FALSE;
}

static void
switch_to_workspace (MetaDisplay *display,
                     MetaWorkspace *workspace)
{
  MetaWindow *move_window;

  move_window = NULL;
  if (display->grab_op == META_GRAB_OP_MOVING)
    move_window = display->grab_window;
      
  if (move_window != NULL)
    {
      if (move_window->on_all_workspaces)
        move_window = NULL; /* don't move it after all */

      /* We put the window on the new workspace, flip spaces,
       * then remove from old workspace, so the window
       * never gets unmapped and we maintain the button grab
       * on it.
       */
      if (move_window)
        {
          if (!meta_workspace_contains_window (workspace,
                                               move_window))
            meta_workspace_add_window (workspace, move_window);
        }
    }
      
  meta_workspace_activate (workspace);

  if (move_window)
    {
      /* Lamely rely on prepend */
      g_assert (move_window->workspaces->data == workspace);
                
      while (move_window->workspaces->next) /* while list size > 1 */
        meta_workspace_remove_window (move_window->workspaces->next->data,
                                      move_window);
    }
}

static void
handle_activate_workspace (MetaDisplay *display,
                           MetaWindow  *event_window,
                           XEvent      *event,
                           gpointer     data)
{
  int which;
  MetaWorkspace *workspace;
  
  which = GPOINTER_TO_INT (data);

  workspace = meta_display_get_workspace_by_index (display, which);

  if (workspace)
    {
      switch_to_workspace (display, workspace);
    }
  else
    {
      /* We could offer to create it I suppose */
    }
}

static void
handle_workspace_left (MetaDisplay *display,
                       MetaWindow  *window,
                       XEvent      *event,
                       gpointer     data)
{
  MetaWorkspace *workspace;
  MetaScreen *screen;
  int i;
  
  screen = meta_display_screen_for_root (display,
                                         event->xkey.root);

  if (screen == NULL)
    return;  
  
  i = meta_workspace_index (screen->active_workspace);
  --i;

  workspace = meta_display_get_workspace_by_index (display, i);
  
  if (workspace)
    {
      switch_to_workspace (display, workspace);
    }
  else
    {
      /* We could offer to create it I suppose */
    }
}

static void
handle_workspace_right (MetaDisplay *display,
                        MetaWindow  *window,
                        XEvent      *event,
                        gpointer     data)
{
  MetaWorkspace *workspace;
  MetaScreen *screen;
  int i;
  
  screen = meta_display_screen_for_root (display,
                                         event->xkey.root);

  if (screen == NULL)
    return;  
  
  i = meta_workspace_index (screen->active_workspace);
  ++i;

  workspace = meta_display_get_workspace_by_index (display, i);
  
  if (workspace)
    {
      switch_to_workspace (display, workspace);
    }
  else
    {
      /* We could offer to create it I suppose */
    }
}

static void
handle_activate_menu (MetaDisplay *display,
                      MetaWindow  *event_window,
                      XEvent      *event,
                      gpointer     data)
{
  if (display->focus_window)
    {
      int x, y;

      meta_window_get_position (display->focus_window,
                                &x, &y);
      
      meta_window_show_menu (display->focus_window,
                             x, y,
                             0,
                             event->xkey.time);
    }
}

static void
handle_tab_forward (MetaDisplay *display,
                    MetaWindow  *event_window,
                    XEvent      *event,
                    gpointer     data)
{
  MetaWindow *window;
  
  meta_verbose ("Tab forward\n");
  
  window = NULL;
  
  if (display->focus_window != NULL)
    {
      window = meta_stack_get_tab_next (display->focus_window->screen->stack,
                                        display->focus_window->screen->active_workspace,
                                        display->focus_window,
                                        FALSE);
    }

  if (window == NULL)
    {
      MetaScreen *screen;
      
      screen = meta_display_screen_for_root (display,
                                             event->xkey.root);

      /* We get the screen because event_window may be NULL,
       * in which case we can't use event_window->screen
       */
      if (screen)
        {
          window = meta_stack_get_tab_next (screen->stack,
                                            screen->active_workspace,
                                            NULL,
                                            FALSE);
        }
    }

  if (window)
    {
      meta_verbose ("Starting tab forward, showing popup\n");

      meta_display_begin_grab_op (window->display,
                                  display->focus_window ?
                                  display->focus_window : window,
                                  META_GRAB_OP_KEYBOARD_TABBING,
                                  FALSE,
                                  0, 0,
                                  event->xkey.time,
                                  0, 0);

      meta_ui_tab_popup_select (window->screen->tab_popup,
                                window->xwindow);
      /* only after selecting proper window */
      meta_ui_tab_popup_set_showing (window->screen->tab_popup,
                                     TRUE);
    }
}

static void
handle_tab_backward (MetaDisplay *display,
                     MetaWindow  *event_window,
                     XEvent      *event,
                     gpointer     data)
{
  MetaWindow *window;
  
  meta_verbose ("Tab backward\n");
  
  window = NULL;
  
  if (display->focus_window != NULL)
    {
      window = meta_stack_get_tab_next (display->focus_window->screen->stack,
                                        display->focus_window->screen->active_workspace,
                                        display->focus_window,
                                        TRUE);
    }

  if (window == NULL)
    {
      MetaScreen *screen;
      
      screen = meta_display_screen_for_root (display,
                                             event->xkey.root);

      /* We get the screen because event_window may be NULL,
       * in which case we can't use event_window->screen
       */
      if (screen)
        {
          window = meta_stack_get_tab_next (screen->stack,
                                            screen->active_workspace,
                                            NULL,
                                            TRUE);
        }
    }

  if (window)
    {
      meta_verbose ("Starting tab backward, showing popup\n");
      
      meta_display_begin_grab_op (window->display,
                                  display->focus_window ?
                                  display->focus_window : window,
                                  META_GRAB_OP_KEYBOARD_TABBING,
                                  FALSE,
                                  0, 0,
                                  event->xkey.time,
                                  0, 0);

      meta_ui_tab_popup_select (window->screen->tab_popup,
                                window->xwindow);
      /* only after selecting proper window */
      meta_ui_tab_popup_set_showing (window->screen->tab_popup,
                                     TRUE);
    }
}

static void
handle_focus_previous (MetaDisplay *display,
                       MetaWindow  *event_window,
                       XEvent      *event,
                       gpointer     data)
{
  MetaWindow *window;
  MetaScreen *screen;      
  
  meta_verbose ("Focus previous window\n");

  screen = meta_display_screen_for_root (display,
                                         event->xkey.root);

  if (screen == NULL)
    return;
  
  window = display->prev_focus_window;

  if (window &&
      !meta_workspace_contains_window (screen->active_workspace,
                                       window))
    window = NULL;
  
  if (window == NULL)
    {
      /* Pick first window in tab order */      
      window = meta_stack_get_tab_next (screen->stack,
                                        screen->active_workspace,
                                        NULL,
                                        TRUE);
    }

  if (window &&
      !meta_workspace_contains_window (screen->active_workspace,
                                       window))
    window = NULL;
  
  if (window)
    {
      meta_window_raise (window);
      meta_window_focus (window, event->xkey.time);
    }
}
