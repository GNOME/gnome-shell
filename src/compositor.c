/* Metacity compositing manager */

/* 
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2003 Keith Packard
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

#include <config.h>
#include "compositor.h"
#include "screen.h"
#include "errors.h"

#ifdef HAVE_COMPOSITE_EXTENSIONS
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>

#endif /* HAVE_COMPOSITE_EXTENSIONS */

/* Unlike MetaWindow, there's one of these for _all_ toplevel windows,
 * override redirect or not. We also track unmapped windows as
 * otherwise on window map we'd have to determine where the
 * newly-mapped window was in the stack. A MetaCompositorWindow may
 * correspond to a metacity window frame rather than an application
 * window.
 */
typedef struct
{
  Window xwindow;
  
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositor *compositor;

  int x;
  int y;
  int width;
  int height;
  int border_width;
  
  Damage          damage;
  XserverRegion   last_painted_extents;
  
  Picture         picture;
  XserverRegion   border_size;
  
  unsigned int managed : 1;
  unsigned int damaged : 1;

  unsigned int screen_index : 8;
  
#endif  
} MetaCompositorWindow;

struct MetaCompositor
{
  MetaDisplay *display;

  int composite_error_base;
  int composite_event_base;
  int damage_error_base;
  int damage_event_base;
  int fixes_error_base;
  int fixes_event_base;
  int render_error_base;
  int render_event_base;
  
  GHashTable *window_hash;

  guint repair_idle;
  
  guint enabled : 1;
  guint have_composite : 1;
  guint have_damage : 1;
  guint have_fixes : 1;
  guint have_render : 1;
};

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
meta_compositor_window_free (MetaCompositorWindow *cwindow)
{
  XDamageDestroy (cwindow->compositor->display->xdisplay,
                  cwindow->damage);
  
  g_free (cwindow);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
free_window_hash_value (void *v)
{
  MetaCompositorWindow *cwindow = v;

  meta_compositor_window_free (cwindow);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

MetaCompositor*
meta_compositor_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositor *compositor;

  compositor = g_new0 (MetaCompositor, 1);

  compositor->display = display;
  
  if (!XCompositeQueryExtension (display->xdisplay,
                                 &compositor->composite_event_base,
                                 &compositor->composite_error_base))
    {
      compositor->composite_event_base = 0;
      compositor->composite_error_base = 0;
    }
  else
    compositor->have_composite = TRUE;

  meta_verbose ("Composite extension event base %d error base %d\n",
                compositor->composite_event_base,
                compositor->composite_error_base);

  if (!XDamageQueryExtension (display->xdisplay,
                              &compositor->damage_event_base,
                              &compositor->damage_error_base))
    {
      compositor->damage_event_base = 0;
      compositor->damage_error_base = 0;
    }
  else
    compositor->have_damage = TRUE;

  meta_verbose ("Damage extension event base %d error base %d\n",
                compositor->damage_event_base,
                compositor->damage_error_base);
  
  if (!XFixesQueryExtension (display->xdisplay,
                             &compositor->fixes_event_base,
                             &compositor->fixes_error_base))
    {
      compositor->fixes_event_base = 0;
      compositor->fixes_error_base = 0;
    }
  else
    compositor->have_fixes = TRUE;

  meta_verbose ("Fixes extension event base %d error base %d\n",
                compositor->fixes_event_base,
                compositor->fixes_error_base);

  if (!XRenderQueryExtension (display->xdisplay,
                              &compositor->render_event_base,
                              &compositor->render_error_base))
    {
      compositor->render_event_base = 0;
      compositor->render_error_base = 0;
    }
  else
    compositor->have_render = TRUE;

  meta_verbose ("Render extension event base %d error base %d\n",
                compositor->render_event_base,
                compositor->render_error_base);
  
  if (!(compositor->have_composite &&
        compositor->have_fixes &&
        compositor->have_render &&
        compositor->have_damage))
    {
      meta_verbose ("Failed to find all extensions needed for compositing manager, disabling compositing manager\n");
      g_assert (!compositor->enabled);
      return compositor;
    }
  
  compositor->window_hash = g_hash_table_new_full (meta_unsigned_long_hash,
                                                   meta_unsigned_long_equal,
                                                   NULL,
                                                   free_window_hash_value);
  
  compositor->enabled = TRUE;
  
  return compositor;
#else /* HAVE_COMPOSITE_EXTENSIONS */
  return (void*) 0xdeadbeef; /* non-NULL value */
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
remove_repair_idle (MetaCompositor *compositor)
{
  if (compositor->repair_idle != 0)
    {
      g_source_remove (compositor->repair_idle);
      compositor->repair_idle = 0;
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

void
meta_compositor_unref (MetaCompositor *compositor)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  /* There isn't really a refcount at the moment since
   * there's no ref()
   */
  remove_repair_idle (compositor);

  if (compositor->window_hash)
    g_hash_table_destroy (compositor->window_hash);
  
  g_free (compositor);
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

#ifdef HAVE_COMPOSITE_EXTENSIONS
static XserverRegion
window_extents (MetaCompositorWindow *cwindow)
{
  XRectangle r;

  r.x = cwindow->x;
  r.y = cwindow->y;
  r.width = cwindow->width;
  r.height = cwindow->height;

  return XFixesCreateRegion (cwindow->compositor->display->xdisplay, &r, 1);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
paint_screen (MetaCompositor *compositor,
              MetaScreen     *screen,
              XserverRegion   damage_region)
{
  XserverRegion region;
  Picture buffer_picture;
  Pixmap buffer_pixmap;
  Display *xdisplay;
  XRenderPictFormat *format;
  GList *tmp;

  xdisplay = screen->display->xdisplay;
  
  if (damage_region == None)
    {
      XRectangle  r;

      r.x = 0;
      r.y = 0;
      r.width = screen->width;
      r.height = screen->height;
      
      region = XFixesCreateRegion (xdisplay, &r, 1);
    }
  else
    {
      region = XFixesCreateRegion (xdisplay, NULL, 0);

      XFixesCopyRegion (compositor->display->xdisplay,
                        region,
                        damage_region);
    }

  buffer_pixmap = XCreatePixmap (xdisplay, None,
                                 screen->width,
                                 screen->height,
                                 DefaultDepth (xdisplay,
                                               screen->number));
  
  format = XRenderFindVisualFormat (xdisplay,
                                    DefaultVisual (xdisplay,
                                                   screen->number));
  
  buffer_picture = XRenderCreatePicture (xdisplay,
                                         buffer_pixmap,
                                         format,
                                         0, 0);

  /* set clip on the root window */
  XFixesSetPictureClipRegion (xdisplay,
                              screen->root_picture, 0, 0, region);

  /* draw windows from bottom to top */
  meta_error_trap_push (compositor->display);
  tmp = g_list_last (screen->compositor_windows);
  while (tmp != NULL)
    {
      MetaCompositorWindow *cwindow = tmp->data;

      if (cwindow->picture == None) /* InputOnly */
        goto next;
      
      if (cwindow->last_painted_extents)
        XFixesDestroyRegion (xdisplay,
                             cwindow->last_painted_extents);

      cwindow->last_painted_extents = window_extents (cwindow);
        
      XFixesSetPictureClipRegion (xdisplay,
                                  buffer_picture, 0, 0,
                                  region);
      
      /* XFixesSubtractRegion (dpy, region, region, 0, 0, w->borderSize, 0, 0); */
      
      XRenderComposite (xdisplay,
                        PictOpSrc, /* PictOpOver for alpha */
                        cwindow->picture,
                        None, buffer_picture,
                        0, 0, 0, 0,
                        cwindow->x + cwindow->border_width,
                        cwindow->y + cwindow->border_width,
                        cwindow->width,
                        cwindow->height);

    next:
      tmp = tmp->prev;
    }
  meta_error_trap_pop (compositor->display, FALSE);

  /* Copy buffer to root window */
  
  XFixesSetPictureClipRegion (xdisplay, buffer_picture, 0, 0, None);
  XRenderComposite (xdisplay, PictOpSrc, buffer_picture, None,
                    screen->root_picture,
                    0, 0, 0, 0, 0, 0,
                    screen->width, screen->height);
  
  XFixesDestroyRegion (xdisplay, region);
  XFreePixmap (xdisplay, buffer_pixmap);
  XRenderFreePicture (xdisplay, buffer_picture);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static gboolean
repair_idle_func (void *data)
{
  GSList *tmp;
  
  MetaCompositor *compositor = data;
  
  tmp = compositor->display->screens;
  while (tmp != NULL)
    {
      MetaScreen *s = tmp->data;

      if (s->damage_region != None)
        {
          paint_screen (compositor, s,
                        s->damage_region);
          XFixesDestroyRegion (s->display->xdisplay,
                               s->damage_region);
          s->damage_region = None;
        }
      
      tmp = tmp->next;
    }

  compositor->repair_idle = 0;
  
  return FALSE;
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static MetaScreen*
meta_compositor_window_get_screen (MetaCompositorWindow *cwindow)
{
  MetaScreen *screen;
  GSList *tmp;
  
  screen = NULL;
  tmp = cwindow->compositor->display->screens;
  while (tmp != NULL)
    {
      MetaScreen *s = tmp->data;

      if (s->number == cwindow->screen_index)
        {
          screen = s;
          break;
        }
      
      tmp = tmp->next;
    }
  g_assert (screen != NULL);

  return screen;
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
ensure_repair_idle (MetaCompositor *compositor)
{
  if (compositor->repair_idle != 0)
    return;
  
  compositor->repair_idle = g_idle_add (repair_idle_func, compositor);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
merge_and_destroy_damage_region (MetaCompositor *compositor,
                                 MetaScreen     *screen,
                                 XserverRegion   region)
{
  if (screen->damage_region != None)
    {
      XFixesCopyRegion (compositor->display->xdisplay,
                        screen->damage_region,
                        region);
      XFixesDestroyRegion (compositor->display->xdisplay,
                           region);
    }
  else
    {
      screen->damage_region = region;
    }

  ensure_repair_idle (compositor);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
merge_damage_region (MetaCompositor *compositor,
                     MetaScreen     *screen,
                     XserverRegion   region)
{
  if (screen->damage_region == None)
    screen->damage_region =
      XFixesCreateRegion (compositor->display->xdisplay, NULL, 0);
  
  XFixesCopyRegion (compositor->display->xdisplay,
                    screen->damage_region,
                    region);

  ensure_repair_idle (compositor);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_damage_notify (MetaCompositor     *compositor,
                       XDamageNotifyEvent *event)
{
  MetaCompositorWindow *cwindow;
  XserverRegion region;
  MetaScreen *screen;
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->drawable);
  if (cwindow == NULL)
    return;

  region = XFixesCreateRegion (compositor->display->xdisplay, NULL, 0);

  /* translate region to screen */
  XDamageSubtract (compositor->display->xdisplay,
                   cwindow->damage, None, region);

  XFixesTranslateRegion (compositor->display->xdisplay,
                         region,
                         cwindow->x,
                         cwindow->y);

  screen = meta_compositor_window_get_screen (cwindow);
  
  merge_and_destroy_damage_region (compositor, screen, region);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_configure_notify (MetaCompositor  *compositor,
                          XConfigureEvent *event)
{
  MetaCompositorWindow *cwindow;
  MetaScreen *screen;
  GList *link;
  Window above;
  XserverRegion region;
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->window);
  if (cwindow == NULL)
    return;

  screen = meta_compositor_window_get_screen (cwindow);

  if (cwindow->last_painted_extents)
    {
      merge_damage_region (compositor,
                           screen,
                           cwindow->last_painted_extents);
    }
  
  cwindow->x = event->x;
  cwindow->y = event->y;
  cwindow->width = event->width;
  cwindow->height = event->height;
  cwindow->border_width = event->border_width;

  link = g_list_find (screen->compositor_windows,
                      cwindow);

  g_assert (link != NULL);

  if (link->next)
    above = ((MetaCompositorWindow*) link->next)->xwindow;
  else
    above = None;
  
  if (above != event->above)
    {
      GList *tmp;
      
      screen->compositor_windows =
        g_list_delete_link (screen->compositor_windows,
                            link);
      link = NULL;

      /* Note that event->above is None if our window is on the bottom */
      tmp = screen->compositor_windows;
      while (tmp != NULL)
        {
          MetaCompositorWindow *t = tmp->data;
          
          if (t->xwindow == event->above)
            {
              /* We are above this window, i.e. earlier in list */
              break;
            }
          
          tmp = tmp->next;
        }

      if (tmp != NULL)
        {
          screen->compositor_windows =
            g_list_insert_before (screen->compositor_windows,
                                  tmp,
                                  cwindow);
        }
      else
        screen->compositor_windows =
          g_list_prepend (screen->compositor_windows,
                          cwindow);
    }

  region = window_extents (cwindow);
  merge_damage_region (compositor,
                       screen,
                       region);
  XFixesDestroyRegion (compositor->display->xdisplay, region);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

void
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (!compositor->enabled)
    return; /* no extension */

  if (event->type == (compositor->damage_event_base + XDamageNotify))
    {
      process_damage_notify (compositor,
                             (XDamageNotifyEvent*) event);
    }
  else if (event->type == ConfigureNotify)
    {
      process_configure_notify (compositor,
                                (XConfigureEvent*) event);
    }
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

/* This is called when metacity does its XQueryTree() on startup
 * and when a new window is mapped.
 */
void
meta_compositor_add_window (MetaCompositor    *compositor,
                            Window             xwindow,
                            XWindowAttributes *attrs)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositorWindow *cwindow;
  MetaScreen *screen;
  Damage damage;
  XRenderPictFormat *format;
  XRenderPictureAttributes pa;
  
  if (!compositor->enabled)
    return; /* no extension */

  screen = meta_screen_for_x_screen (attrs->screen);
  g_assert (screen != NULL);
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &xwindow);

  if (cwindow != NULL)
    return;

  /* Create Damage object to monitor window damage */
  meta_error_trap_push (compositor->display);
  damage = XDamageCreate (compositor->display->xdisplay,
                          xwindow, XDamageReportNonEmpty);
  meta_error_trap_pop (compositor->display, FALSE);

  if (damage == None)
    return;
  
  cwindow = g_new0 (MetaCompositorWindow, 1);

  cwindow->compositor = compositor;
  cwindow->xwindow = xwindow;
  cwindow->screen_index = screen->number;
  cwindow->damage = damage;
  cwindow->x = attrs->x;
  cwindow->y = attrs->y;
  cwindow->width = attrs->width;
  cwindow->height = attrs->height;
  cwindow->border_width = attrs->border_width;

  pa.subwindow_mode = IncludeInferiors;

  if (attrs->class != InputOnly)
    {
      format = XRenderFindVisualFormat (compositor->display->xdisplay,
                                        attrs->visual);
      cwindow->picture = XRenderCreatePicture (compositor->display->xdisplay,
                                               xwindow,
                                               format,
                                               CPSubwindowMode,
                                               &pa);
    }
  else
    {
      cwindow->picture = None;
    }
  
  g_hash_table_insert (compositor->window_hash,
                       &cwindow->xwindow, cwindow);

  /* assume cwindow is at the top of the stack */
  screen->compositor_windows = g_list_prepend (screen->compositor_windows,
                                               cwindow);
  
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_remove_window (MetaCompositor    *compositor,
                               Window             xwindow)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositorWindow *cwindow;
  MetaScreen *screen;
  
  if (!compositor->enabled)
    return; /* no extension */

  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &xwindow);

  if (cwindow == NULL)
    return;
  
  screen = meta_compositor_window_get_screen (cwindow);

  if (cwindow->last_painted_extents)
    {
      merge_and_destroy_damage_region (compositor,
                                       screen,
                                       cwindow->last_painted_extents);
      cwindow->last_painted_extents = None;
    }
  
  screen->compositor_windows = g_list_remove (screen->compositor_windows,
                                              cwindow);
  
  /* Frees cwindow as side effect */
  g_hash_table_remove (compositor->window_hash,
                       &xwindow);
  
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  XRenderPictureAttributes pa;
  
  if (!compositor->enabled)
    return; /* no extension */

  /* FIXME we need to handle root window resize by recreating the
   * root_picture
   */
  
  g_assert (screen->root_picture == None);

  /* FIXME add flag for whether we're composite-managing each
   * screen and detect failure here
   */
  XCompositeRedirectSubwindows (screen->display->xdisplay,
                                screen->xroot,
                                CompositeRedirectManual);
  g_print ("Subwindows redirected\n");
  
  pa.subwindow_mode = IncludeInferiors;
  
  screen->root_picture =
    XRenderCreatePicture (compositor->display->xdisplay,
                          screen->xroot, 
                          XRenderFindVisualFormat (compositor->display->xdisplay,
                                                   DefaultVisual (compositor->display->xdisplay,
                                                                  screen->number)),
                          CPSubwindowMode,
                          &pa);

  g_assert (screen->root_picture != None);
  
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS  
  if (!compositor->enabled)
    return; /* no extension */

  XRenderFreePicture (screen->display->xdisplay,
                      screen->root_picture);
  screen->root_picture = None;
  
  while (screen->compositor_windows != NULL)
    {
      MetaCompositorWindow *cwindow = screen->compositor_windows->data;

      meta_compositor_remove_window (compositor, cwindow->xwindow);
    }
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}




