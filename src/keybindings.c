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

#define INTERESTING_MODIFIERS (ShiftMask | ControlMask | Mod1Mask)

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
          int result;
          
          meta_error_trap_push (display);
          XGrabKey (display->xdisplay, bindings[i].keycode,
                    bindings[i].mask, xwindow, True,
                    GrabModeAsync, GrabModeAsync);
          result = meta_error_trap_pop (display);
          if (result != Success)
            {
              const char *name;

              name = XKeysymToString (bindings[i].keysym);
              if (name == NULL)
                name = "(unknown)";
              
              if (result == BadAccess)
                meta_warning (_("Some other program is already using the key %s as a binding\n"), name);
            }
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
          meta_error_trap_push (display);
          XUngrabKey (display->xdisplay, bindings[i].keycode,
                      bindings[i].mask, xwindow);
          meta_error_trap_pop (display);
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

static void
process_event (MetaKeyBinding *bindings,
               MetaDisplay *display,
               MetaWindow  *window,
               XEvent      *event)
{
  KeySym keysym;
  int i;
  
  keysym = XKeycodeToKeysym (display->xdisplay, event->xkey.keycode, 0);

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
  process_event (screen_bindings, display, window, event);
  process_event (window_bindings, display, window, event);
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
      meta_workspace_activate (workspace);
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
                                            event_window,
                                            FALSE);
        }
    }

  if (window)
    {
      meta_window_raise (window);
      meta_window_focus (window, event->xkey.time);
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
                                            event_window,
                                            TRUE);
        }
    }

  if (window)
    {
      meta_window_raise (window);
      meta_window_focus (window, event->xkey.time);
    }
}

static void
handle_focus_previous (MetaDisplay *display,
                       MetaWindow  *event_window,
                       XEvent      *event,
                       gpointer     data)
{
  MetaWindow *window;

  meta_verbose ("Focus previous window\n");
  
  window = display->prev_focus_window;

  if (window == NULL)
    {
      /* Pick first window in tab order */
      MetaScreen *screen;
      
      screen = meta_display_screen_for_root (display,
                                             event->xkey.root);
      
      /* We get the screen because event_window may be NULL,
       * in which case we can't use event_window->screen
       */
      if (screen)
        {
          window = meta_stack_get_tab_next (screen->stack,
                                            event_window,
                                            TRUE);
        }
    }
  
  if (window)
    {
      meta_window_raise (window);
      meta_window_focus (window, event->xkey.time);
    }
}
