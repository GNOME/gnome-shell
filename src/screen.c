/* Metacity X screen handler */

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

#include "screen.h"
#include "util.h"
#include "errors.h"
#include "window.h"
#include "colors.h"

#include <cursorfont.h>
#include <locale.h>
#include <string.h>

MetaScreen*
meta_screen_new (MetaDisplay *display,
                 int          number)
{
  MetaScreen *screen;
  Window xroot;
  Display *xdisplay;
  Cursor cursor;
  
  /* Only display->name, display->xdisplay, and display->error_traps
   * can really be used in this function, since normally screens are
   * created from the MetaDisplay constructor
   */
  
  xdisplay = display->xdisplay;
  
  meta_verbose ("Trying screen %d on display '%s'\n",
                number, display->name);

  xroot = RootWindow (xdisplay, number);

  /* FVWM checks for None here, I don't know if this
   * ever actually happens
   */
  if (xroot == None)
    {
      meta_warning (_("Screen %d on display '%s' is invalid\n"),
                    number, display->name);
      return NULL;
    }

  /* Select our root window events */
  meta_error_trap_push (display);
  XSelectInput (xdisplay,
                xroot,
                SubstructureRedirectMask | SubstructureNotifyMask |
                ColormapChangeMask | PropertyChangeMask |
                LeaveWindowMask | EnterWindowMask |
                ButtonPressMask | ButtonReleaseMask);
  if (meta_error_trap_pop (display) != Success)
    {
      meta_warning (_("Screen %d on display '%s' already has a window manager\n"),
                    number, display->name);
      return NULL;
    }

  cursor = XCreateFontCursor (display->xdisplay, XC_left_ptr);
  XDefineCursor (display->xdisplay, xroot, cursor);
  XFreeCursor (display->xdisplay, cursor);
  
  screen = g_new (MetaScreen, 1);

  screen->display = display;
  screen->number = number;
  screen->xscreen = ScreenOfDisplay (xdisplay, number);
  screen->xroot = xroot;  
  screen->pango_context = NULL;

  screen->engine = &meta_default_engine;
  
  meta_verbose ("Added screen %d on display '%s' root 0x%lx\n",
                screen->number, screen->display->name, screen->xroot);  
  
  return screen;
}

void
meta_screen_free (MetaScreen *screen)
{
  if (screen->pango_context)
    g_object_unref (G_OBJECT (screen->pango_context));
  g_free (screen);
}

void
meta_screen_manage_all_windows (MetaScreen *screen)
{
  Window ignored1, ignored2;
  Window *children;
  unsigned int n_children;
  int i;

  /* Must grab server to avoid obvious race condition */
  meta_display_grab (screen->display);

  meta_error_trap_push (screen->display);
  
  XQueryTree (screen->display->xdisplay,
              screen->xroot,
              &ignored1, &ignored2, &children, &n_children);

  if (meta_error_trap_pop (screen->display))
    {
      meta_display_ungrab (screen->display);
      return;
    }
  
  i = 0;
  while (i < n_children)
    {
      meta_window_new (screen->display, children[i]);

      ++i;
    }

  meta_display_ungrab (screen->display);
  
  if (children)
    XFree (children);
}

static GC
get_gc_func (PangoContext *context, PangoColor *color, GC base_gc)
{
  MetaScreen *screen;
  GC new_gc;
  XGCValues vals;
  int copy_mask = (GCFunction | GCPlaneMask | GCForeground | GCBackground |
                   GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle |
                   GCFillStyle | GCFillRule | GCTile | GCStipple | GCTileStipXOrigin |
                   GCTileStipYOrigin | GCFont | GCSubwindowMode |
                   GCGraphicsExposures | GCClipXOrigin | GCClipYOrigin |
                   GCDashOffset | GCArcMode);

  screen = g_object_get_data (G_OBJECT (context), "meta-screen");
  
  new_gc = XCreateGC (screen->display->xdisplay,
                      screen->xroot,
                      0, 
                      &vals);

  XCopyGC (screen->display->xdisplay, base_gc, copy_mask, new_gc);

  vals.foreground = meta_screen_get_x_pixel (screen, color);
  XChangeGC (screen->display->xdisplay, new_gc, GCForeground, &vals);

  return new_gc;
}

static void
free_gc_func (PangoContext *context, GC gc)
{
  MetaScreen *screen;

  screen = g_object_get_data (G_OBJECT (context), "meta-screen");

  XFreeGC (screen->display->xdisplay, gc);
}

static char*
get_default_language (void)
{
  /* Copied from GTK, Copyright 2001 Red Hat Inc. */
  gchar *lang;
  gchar *p;
  
  lang = g_strdup (setlocale (LC_CTYPE, NULL));
  p = strchr (lang, '.');
  if (p)
    *p = '\0';
  p = strchr (lang, '@');
  if (p)
    *p = '\0';

  return lang;
}

PangoContext*
meta_screen_get_pango_context (MetaScreen *screen,
                               const PangoFontDescription *desc,
                               PangoDirection direction)
{
  if (screen->pango_context == NULL)
    {
      PangoContext *ctx;
      char *lang;
      
      /* Copied from GDK, Copyright 2001 Red Hat, Inc. */
#ifdef HAVE_XFT
      static int use_xft = -1;
      if (use_xft == -1)
        {
          char *val = g_getenv ("META_USE_XFT");
          
          use_xft = val && (atoi (val) != 0);
        }
      
      if (use_xft)
        ctx = pango_xft_get_context (screen->display, screen->number);
      else
#endif /* HAVE_XFT */
        ctx = pango_x_get_context (screen->display->xdisplay);

      g_object_set_data (G_OBJECT (ctx), "meta-screen", screen);

      pango_x_context_set_funcs (ctx, get_gc_func, free_gc_func);
      
      lang = get_default_language ();  
      pango_context_set_lang (ctx, lang);
      g_free (lang);

      /* FIXME these two lines are wrong;
       * we should be storing a context for each direction/desc,
       * so that the args to meta_screen_get_pango_context()
       * are honored.
       */
      pango_context_set_base_dir (ctx, direction);
      pango_context_set_font_description (ctx, desc);
      
      screen->pango_context = ctx;
    }
  
  return screen->pango_context;
}

MetaScreen*
meta_screen_for_x_screen (Screen *xscreen)
{
  MetaDisplay *display;
  
  display = meta_display_for_x_display (DisplayOfScreen (xscreen));

  if (display == NULL)
    return NULL;
  
  return meta_display_screen_for_x_screen (display, xscreen);
}
