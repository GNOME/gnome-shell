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
#include "uislave.h"
#include "frame.h"

#include <X11/cursorfont.h>
#include <locale.h>
#include <string.h>

static void  ui_slave_func   (MetaUISlave *uislave,
                              MetaMessage *message,
                              gpointer     data);
static char* get_screen_name (MetaDisplay *display,
                              int          number);


MetaScreen*
meta_screen_new (MetaDisplay *display,
                 int          number)
{
  MetaScreen *screen;
  Window xroot;
  Display *xdisplay;
  Cursor cursor;
  XGCValues vals;
  
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
  screen->screen_name = get_screen_name (display, number);
  screen->xscreen = ScreenOfDisplay (xdisplay, number);
  screen->xroot = xroot;  
  screen->pango_context = NULL;
  
  screen->engine = &meta_default_engine;

  meta_screen_init_visual_info (screen);
  meta_screen_init_ui_colors (screen);

  screen->scratch_gc = XCreateGC (screen->display->xdisplay,
                                  screen->xroot,
                                  0,
                                  &vals);
  
  screen->uislave = meta_ui_slave_new (screen->screen_name,
                                       ui_slave_func,
                                       screen);
  
  meta_verbose ("Added screen %d ('%s') root 0x%lx\n",
                screen->number, screen->screen_name, screen->xroot);  
  
  return screen;
}

void
meta_screen_free (MetaScreen *screen)
{
  meta_ui_slave_free (screen->uislave);

  XFreeGC (screen->display->xdisplay,
           screen->scratch_gc);
  
  if (screen->pango_context)
    g_object_unref (G_OBJECT (screen->pango_context));
  g_free (screen->screen_name);
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

static void
ui_slave_func (MetaUISlave *uislave,
               MetaMessage *message,
               gpointer     data)
{
  switch (message->header.message_code)
    {
    case MetaMessageCheckCode:
      meta_verbose ("Received UI slave check message version: %s host alias: %s messages version: %d\n",
                    message->check.metacity_version,
                    message->check.host_alias,
                    message->check.messages_version);

      if (strcmp (message->check.metacity_version, VERSION) != 0 ||
          strcmp (message->check.host_alias, HOST_ALIAS) != 0 ||
          message->check.messages_version != META_MESSAGES_VERSION)
        {
          meta_warning ("metacity-uislave has the wrong version; must use the one compiled with metacity\n");
          meta_ui_slave_disable (uislave);
        }
      
      break;

    default:
      meta_verbose ("Received unhandled message from UI slave: %d\n",
                    message->header.message_code);
      break;
    }
}


static char*
get_screen_name (MetaDisplay *display,
                 int          number)
{
  char *p;
  char *dname;
  char *scr;
  
  /* DisplayString gives us a sort of canonical display,
   * vs. the user-entered name from XDisplayName()
   */
  dname = g_strdup (DisplayString (display->xdisplay));

  /* Change display name to specify this screen.
   */
  p = strrchr (dname, ':');
  if (p)
    {
      p = strchr (p, '.');
      if (p)
        *p = '\0';
    }
  
  scr = g_strdup_printf ("%s.%d", dname, number);

  g_free (dname);

  return scr;
}

static gint
ptrcmp (gconstpointer a, gconstpointer b)
{
  if (a < b)
    return -1;
  else if (a > b)
    return 1;
  else
    return 0;
}

static void
listify_func (gpointer key, gpointer value, gpointer data)
{
  GSList **listp;
  
  listp = data;

  *listp = g_slist_prepend (*listp, value);
}

void
meta_screen_foreach_window (MetaScreen *screen,
                            MetaScreenWindowFunc func,
                            gpointer data)
{
  GSList *winlist;
  GSList *tmp;

  /* If we end up doing this often, just keeping a list
   * of windows might be sensible.
   */
  
  winlist = NULL;
  g_hash_table_foreach (screen->display->window_ids,
                        listify_func,
                        &winlist);
  
  winlist = g_slist_sort (winlist, ptrcmp);
  
  tmp = winlist;
  while (tmp != NULL)
    {
      /* If the next node doesn't contain this window
       * a second time, delete the window.
       */
      if (tmp->next == NULL ||
          (tmp->next && tmp->next->data != tmp->data))
        {
          MetaWindow *window = tmp->data;

          if (window->screen == screen)
            (* func) (screen, window, data);
        }
      
      tmp = tmp->data;
    }
  g_slist_free (winlist);
}

static void
queue_draw (MetaScreen *screen, MetaWindow *window, gpointer data)
{
  if (window->frame)
    meta_frame_queue_draw (window->frame);
}

void
meta_screen_queue_frame_redraws (MetaScreen *screen)
{
  meta_screen_foreach_window (screen, queue_draw, NULL);
}
