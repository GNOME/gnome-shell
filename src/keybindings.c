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

#include <X11/keysym.h>

/* Plainly we'll want some more configurable keybinding system
 * eventually.
 */

typedef void (* MetaKeyHandler) (MetaDisplay *display,
                                 XEvent      *event,
                                 gpointer     data);

static void handle_activate_workspace (MetaDisplay *display,
                                     XEvent      *event,
                                     gpointer     data);

typedef struct _MetaKeyBinding MetaKeyBinding;

struct _MetaKeyBinding
{
  KeySym keysym;
  gulong mask;
  MetaKeyHandler handler;
  gpointer data;
  int keycode;
};

#define INTERESTING_MODIFIERS (ShiftMask | ControlMask | Mod1Mask)

static MetaKeyBinding bindings[] = {
  { XK_F1, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (0), 0 },
  { XK_F2, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (1), 0 },
  { XK_F3, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (2), 0 },
  { XK_F4, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (3), 0 },
  { XK_F5, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (4), 0 },
  { XK_F6, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (5), 0 },
  { XK_1, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (0), 0 },
  { XK_2, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (1), 0 },
  { XK_3, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (2), 0 },
  { XK_4, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (3), 0 },
  { XK_5, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (4), 0 },
  { XK_6, Mod1Mask, handle_activate_workspace, GINT_TO_POINTER (5), 0 }
};

void
meta_display_init_keys (MetaDisplay *display)
{
  int i;

  i = 0;
  while (i < G_N_ELEMENTS (bindings))
    {
      bindings[i].keycode = XKeysymToKeycode (display->xdisplay,
                                              bindings[i].keysym);
      
      ++i;
    }
}

void
meta_screen_grab_keys (MetaScreen  *screen)
{
  int i;

  i = 0;
  while (i < G_N_ELEMENTS (bindings))
    {
      if (bindings[i].keycode != 0)
        {
          int result;
          
          meta_error_trap_push (screen->display);
          XGrabKey (screen->display->xdisplay, bindings[i].keycode,
                    bindings[i].mask, screen->xroot, True,
                    GrabModeAsync, GrabModeAsync);
          result = meta_error_trap_pop (screen->display);
          if (result != Success)
            {
              const char *name;

              name = XKeysymToString (bindings[i].keysym);
              if (name == NULL)
                name = "(unknown)";
              
              if (result == BadAccess)
                meta_warning (_("Some other program is already using the key %s as a binding\n"), name);
              else
                meta_bug ("Unexpected error setting up keybindings\n");
            }
        }
      
      ++i;
    }
}

void
meta_screen_ungrab_keys (MetaScreen  *screen)
{
  int i;

  i = 0;
  while (i < G_N_ELEMENTS (bindings))
    {
      if (bindings[i].keycode != 0)
        {
          XUngrabKey (screen->display->xdisplay, bindings[i].keycode,
                      bindings[i].mask, screen->xroot);
        }
      
      ++i;
    }
}

void
meta_display_process_key_press (MetaDisplay *display,
                                XEvent      *event)
{
  KeySym keysym;
  int i;
  
  keysym = XKeycodeToKeysym (display->xdisplay, event->xkey.keycode, 0);

  i = 0;
  while (i < G_N_ELEMENTS (bindings))
    {
      if (bindings[i].keysym == keysym && 
          ((event->xkey.state & INTERESTING_MODIFIERS) ==
           bindings[i].mask))
        {
          (* bindings[i].handler) (display, event, bindings[i].data);
          break;
        }
      
      ++i;
    }
}

static void
handle_activate_workspace (MetaDisplay *display,
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

