/* Metacity compositing manager */

/* 
 * Copyright (C) 2003, 2004 Red Hat, Inc.
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
#include "window.h"
#include "frame.h"

#ifdef HAVE_COMPOSITE_EXTENSIONS
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>

#if COMPOSITE_MAJOR > 0 || COMPOSITE_MINOR >= 2
#define HAVE_NAME_WINDOW_PIXMAP 1
#endif

#endif /* HAVE_COMPOSITE_EXTENSIONS */

#define SHADOW_OFFSET 3
#define FRAME_INTERVAL_MILLISECONDS ((int)(1000.0/40.0))

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

#if HAVE_NAME_WINDOW_PIXMAP
  Pixmap          pixmap;
#endif
  
  unsigned int managed : 1;
  unsigned int damaged : 1;
  unsigned int viewable : 1;

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
  
  GHashTable *window_hash;

  guint repair_idle;
  guint repair_timeout;
  
  guint enabled : 1;
  guint have_composite : 1;
  guint have_damage : 1;
  guint have_fixes : 1;
#ifdef HAVE_NAME_WINDOW_PIXMAP
  guint have_name_window_pixmap : 1;
#endif
};

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
meta_compositor_window_free (MetaCompositorWindow *cwindow)
{
  g_assert (cwindow->damage != None);
  
  meta_error_trap_push (cwindow->compositor->display);
  /* This seems to cause an error if the window
   * is destroyed?
   */
  XDamageDestroy (cwindow->compositor->display->xdisplay,
                  cwindow->damage);

#if HAVE_NAME_WINDOW_PIXMAP
  /* Free our window pixmap name */
  if (cwindow->pixmap != None)
    XFreePixmap (cwindow->compositor->display->xdisplay,
                 cwindow->pixmap);
#endif
  meta_error_trap_pop (cwindow->compositor->display, FALSE);
  
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
    {
      int composite_major, composite_minor;
      
      compositor->have_composite = TRUE;

#if HAVE_NAME_WINDOW_PIXMAP
      XCompositeQueryVersion (display->xdisplay,
                              &composite_major, &composite_minor);

#if 0
      if (composite_major > 0 || composite_minor >= 2)
	compositor->have_name_window_pixmap = TRUE;
#endif
#endif
    }

  meta_topic (META_DEBUG_COMPOSITOR, "Composite extension event base %d error base %d\n",
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

  meta_topic (META_DEBUG_COMPOSITOR, "Damage extension event base %d error base %d\n",
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

  meta_topic (META_DEBUG_COMPOSITOR, "Fixes extension event base %d error base %d\n",
              compositor->fixes_event_base,
              compositor->fixes_error_base);
  
  if (!(compositor->have_composite &&
        compositor->have_fixes &&
        compositor->have_damage &&
        META_DISPLAY_HAS_RENDER (compositor->display)))
    {
      meta_topic (META_DEBUG_COMPOSITOR, "Failed to find all extensions needed for compositing manager, disabling compositing manager\n");
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
  if (compositor->repair_idle || compositor->repair_timeout)
    meta_topic (META_DEBUG_COMPOSITOR, "Damage idle removed\n");
  
  if (compositor->repair_idle != 0)
    {
      g_source_remove (compositor->repair_idle);
      compositor->repair_idle = 0;
    }

  if (compositor->repair_timeout != 0)
    {
      g_source_remove (compositor->repair_timeout);
      compositor->repair_timeout = 0;
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

  r.width += SHADOW_OFFSET;
  r.height += SHADOW_OFFSET;
  
  return XFixesCreateRegion (cwindow->compositor->display->xdisplay, &r, 1);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
window_get_paint_bounds (MetaCompositorWindow *cwindow,
                         int *x,
                         int *y,
                         int *w,
                         int *h)
{
#ifdef HAVE_NAME_WINDOW_PIXMAP
  if (cwindow->pixmap != None)
    {
      *x = cwindow->x;
      *y = cwindow->y;
      *w = cwindow->width + cwindow->border_width * 2;
      *h = cwindow->height + cwindow->border_width * 2;
    }
  else
#endif
    {
      *x = cwindow->x + cwindow->border_width;
      *y = cwindow->y + cwindow->border_width;
      *w = cwindow->width;
      *h = cwindow->height;
    }
}

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
  GC gc;

  meta_topic (META_DEBUG_COMPOSITOR, "Repainting screen %d root 0x%lx\n",
              screen->number, screen->xroot);

  /* meta_display_grab (screen->display); */
  
  xdisplay = screen->display->xdisplay;
  
  if (damage_region == None)
    {
      XRectangle  r;

      r.x = 0;
      r.y = 0;
      r.width = screen->rect.width;
      r.height = screen->rect.height;
      
      region = XFixesCreateRegion (xdisplay, &r, 1);
    }
  else
    {
      region = XFixesCreateRegion (xdisplay, NULL, 0);
      
      XFixesCopyRegion (compositor->display->xdisplay,
                        region,
                        damage_region);
    }

  buffer_pixmap = XCreatePixmap (xdisplay, screen->xroot,
                                 screen->rect.width,
                                 screen->rect.height,
                                 DefaultDepth (xdisplay,
                                               screen->number));

  gc = XCreateGC (xdisplay, buffer_pixmap, 0, NULL);
  XSetForeground (xdisplay, gc, WhitePixel (xdisplay, screen->number));
  XFixesSetGCClipRegion (xdisplay, gc, 0, 0, region);
  XFillRectangle (xdisplay, buffer_pixmap, gc, 0, 0,
                  screen->rect.width, screen->rect.height);
  
  format = XRenderFindVisualFormat (xdisplay,
                                    DefaultVisual (xdisplay,
                                                   screen->number));
  
  buffer_picture = XRenderCreatePicture (xdisplay,
                                         buffer_pixmap,
                                         format,
                                         0, 0);

  /* set clip */          
  XFixesSetPictureClipRegion (xdisplay,
                              buffer_picture, 0, 0,
                              region);

  /* draw windows from bottom to top */
  
  meta_error_trap_push (compositor->display);
  tmp = g_list_last (screen->compositor_windows);
  while (tmp != NULL)
    {
      MetaCompositorWindow *cwindow = tmp->data;
      XRenderColor shadow_color;
      MetaWindow *window;

      shadow_color.red = 0;
      shadow_color.green = 0;
      shadow_color.blue = 0;
      shadow_color.alpha = 0x90c0;

      if (cwindow->picture == None) /* InputOnly */
        goto next;

      if (!cwindow->viewable)
        goto next; /* not viewable */
      
      if (cwindow->last_painted_extents)
        XFixesDestroyRegion (xdisplay,
                             cwindow->last_painted_extents);

      cwindow->last_painted_extents = window_extents (cwindow);
      
      /* XFixesSubtractRegion (dpy, region, region, 0, 0, w->borderSize, 0, 0); */

      meta_topic (META_DEBUG_COMPOSITOR, "  Compositing window 0x%lx %d,%d %dx%d\n",
                  cwindow->xwindow,
                  cwindow->x, cwindow->y,
                  cwindow->width, cwindow->height);
      
      window = meta_display_lookup_x_window (compositor->display,
                                             cwindow->xwindow);
      if (window != NULL &&
          window == compositor->display->grab_window &&
          (meta_grab_op_is_resizing (compositor->display->grab_op) ||
           meta_grab_op_is_moving (compositor->display->grab_op)))
        {
          /* Draw window transparent while resizing */
          int x, y, w, h;

          window_get_paint_bounds (cwindow, &x, &y, &w, &h);
          
          XRenderComposite (xdisplay,
                            PictOpOver, /* PictOpOver for alpha, PictOpSrc without */
                            cwindow->picture,
                            screen->trans_picture,
                            buffer_picture,
                            0, 0, 0, 0,
                            x, y, w, h);
        }
      else
        {
          /* Draw window normally */
          int x, y, w, h;
          
          window_get_paint_bounds (cwindow, &x, &y, &w, &h);
          
          /* superlame drop shadow */
          XRenderFillRectangle (xdisplay, PictOpOver,
                                buffer_picture,
                                &shadow_color,
                                cwindow->x + SHADOW_OFFSET,
                                cwindow->y + SHADOW_OFFSET,
                                cwindow->width, cwindow->height);

          XRenderComposite (xdisplay,
                            PictOpSrc, /* PictOpOver for alpha, PictOpSrc without */
                            cwindow->picture,
                            None,
                            buffer_picture,
                            0, 0, 0, 0,
                            x, y, w, h);
        }

    next:
      tmp = tmp->prev;
    }
  meta_error_trap_pop (compositor->display, FALSE);

  /* Copy buffer to root window */
  meta_topic (META_DEBUG_COMPOSITOR, "Copying buffer to root window 0x%lx picture 0x%lx\n",
              screen->xroot, screen->root_picture);

#if 1
  XFixesSetPictureClipRegion (xdisplay,
                              screen->root_picture,
                              0, 0, region);
#endif
  
  /* XFixesSetPictureClipRegion (xdisplay, buffer_picture, 0, 0, None); */
  XRenderComposite (xdisplay, PictOpSrc, buffer_picture, None,
                    screen->root_picture,
                    0, 0, 0, 0, 0, 0,
                    screen->rect.width, screen->rect.height);
  
  XFixesDestroyRegion (xdisplay, region);
  XFreePixmap (xdisplay, buffer_pixmap);
  XRenderFreePicture (xdisplay, buffer_picture);
  XFreeGC (xdisplay, gc);

  /* meta_display_ungrab (screen->display); */

  XSync (screen->display->xdisplay, False);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
do_repair (MetaCompositor *compositor)
{
  GSList *tmp;
  
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

  remove_repair_idle (compositor);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static gboolean
repair_idle_func (void *data)
{
  MetaCompositor *compositor = data;

  compositor->repair_idle = 0;
  do_repair (compositor);
  
  return FALSE;
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */


#ifdef HAVE_COMPOSITE_EXTENSIONS
static gboolean
repair_timeout_func (void *data)
{
  MetaCompositor *compositor = data;

  compositor->repair_timeout = 0;
  do_repair (compositor);
  
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

  compositor->repair_idle = g_idle_add_full (META_PRIORITY_COMPOSITE,
                                             repair_idle_func, compositor, NULL);
  compositor->repair_timeout = g_timeout_add (FRAME_INTERVAL_MILLISECONDS,
                                              repair_timeout_func, compositor);

  meta_topic (META_DEBUG_COMPOSITOR, "Damage idle queued\n");
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
      XFixesUnionRegion (compositor->display->xdisplay,
                         screen->damage_region,
                         region, screen->damage_region);
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

  meta_topic (META_DEBUG_COMPOSITOR,
              "(Merging damage region)\n");
  
  XFixesUnionRegion (compositor->display->xdisplay,
                     screen->damage_region,
                     region, screen->damage_region);

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

  /* translate region to screen; can error if window of damage is
   * destroyed
   */
  meta_error_trap_push (compositor->display);
  XDamageSubtract (compositor->display->xdisplay,
                   cwindow->damage, None, region);
  meta_error_trap_pop (compositor->display, FALSE);

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

  g_assert (screen != NULL);

  if (screen->xroot != event->event)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "ConfigureNotify for 0x%lx received by 0x%lx not using\n",
                  event->window, event->event);
      return; /* ignore ConfigureNotify not received on root window */
    }

  meta_topic (META_DEBUG_COMPOSITOR,
              "ConfigureNotify for 0x%lx received by 0x%lx  %d,%d %dx%d\n",
              event->window, event->event,
              event->x, event->y, event->width, event->height);
  
  if (cwindow->last_painted_extents)
    {
      merge_and_destroy_damage_region (compositor,
                                       screen,
                                       cwindow->last_painted_extents);
      cwindow->last_painted_extents = None;
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


#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_expose (MetaCompositor     *compositor,
                XExposeEvent       *event)
{
  XserverRegion region;
  MetaScreen *screen;
  XRectangle r;

  screen = meta_display_screen_for_root (compositor->display,
                                         event->window);

  if (screen == NULL || screen->root_picture == None)
    return;

  r.x = 0;
  r.y = 0;
  r.width = screen->rect.width;
  r.height = screen->rect.height;
  region = XFixesCreateRegion (compositor->display->xdisplay, &r, 1);
  
  merge_and_destroy_damage_region (compositor, screen, region);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_map (MetaCompositor     *compositor,
             XMapEvent          *event)
{
  MetaCompositorWindow *cwindow;
  MetaScreen *screen;

  /* See if window was mapped as child of root */
  screen = meta_display_screen_for_root (compositor->display,
                                         event->event);

  if (screen == NULL || screen->root_picture == None)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "MapNotify received on non-root 0x%lx for 0x%lx\n",
                  event->event, event->window);
      return; /* MapNotify wasn't for a child of the root */
    }
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->window);
  if (cwindow == NULL)
    {
      XWindowAttributes attrs;
      
      meta_error_trap_push_with_return (compositor->display);
      
      XGetWindowAttributes (compositor->display->xdisplay,
                            event->window, &attrs);
      
      if (meta_error_trap_pop_with_return (compositor->display, TRUE) != Success)
        {
          meta_topic (META_DEBUG_COMPOSITOR, "Failed to get attributes for window 0x%lx\n",
                      event->window);
        }
      else
        {
          meta_topic (META_DEBUG_COMPOSITOR,
                      "Map window 0x%lx\n", event->window);
          meta_compositor_add_window (compositor,
                                      event->window, &attrs);
        }
    }
  else
    {
      cwindow->viewable = TRUE;
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_unmap (MetaCompositor     *compositor,
               XUnmapEvent        *event)
{
  MetaCompositorWindow *cwindow;
  MetaScreen *screen;

  /* See if window was unmapped as child of root */
  screen = meta_display_screen_for_root (compositor->display,
                                         event->event);

  if (screen == NULL || screen->root_picture == None)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "UnmapNotify received on non-root 0x%lx for 0x%lx\n",
                  event->event, event->window);
      return; /* UnmapNotify wasn't for a child of the root */
    }
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->window);
  if (cwindow != NULL)
    {
      cwindow->viewable = FALSE;

      if (cwindow->last_painted_extents)
        {
          merge_and_destroy_damage_region (compositor,
                                           screen,
                                           cwindow->last_painted_extents);
          cwindow->last_painted_extents = None;
        }
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_create (MetaCompositor     *compositor,
                XCreateWindowEvent *event)
{
  MetaScreen *screen;
  XWindowAttributes attrs;
      
  screen = meta_display_screen_for_root (compositor->display,
                                         event->parent);

  if (screen == NULL || screen->root_picture == None)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "CreateNotify received on non-root 0x%lx for 0x%lx\n",
                  event->parent, event->window);
      return;
    }
      
  meta_error_trap_push_with_return (compositor->display);
  
  XGetWindowAttributes (compositor->display->xdisplay,
                        event->window, &attrs);
  
  if (meta_error_trap_pop_with_return (compositor->display, TRUE) != Success)
    {
      meta_topic (META_DEBUG_COMPOSITOR, "Failed to get attributes for window 0x%lx\n",
                  event->window);
    }
  else
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "Create window 0x%lx, adding\n", event->window);
      meta_compositor_add_window (compositor,
                                  event->window, &attrs);
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_destroy (MetaCompositor      *compositor,
                 XDestroyWindowEvent *event)
{
  MetaScreen *screen;

  screen = meta_display_screen_for_root (compositor->display,
                                         event->event);

  if (screen == NULL || screen->root_picture == None)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "DestroyNotify received on non-root 0x%lx for 0x%lx\n",
                  event->event, event->window);
      return;
    }

  meta_topic (META_DEBUG_COMPOSITOR,
              "Destroy window 0x%lx\n", event->window);
  meta_compositor_remove_window (compositor, event->window);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */


#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_reparent (MetaCompositor      *compositor,
                  XReparentEvent      *event)
{
  /* Reparent from one screen to another doesn't happen now, but
   * it's been suggested as a future extension
   */
  MetaScreen *event_screen;
  MetaScreen *parent_screen;
  MetaCompositorWindow *cwindow;
  XWindowAttributes attrs;
  
  event_screen = meta_display_screen_for_root (compositor->display,
                                               event->event);

  if (event_screen == NULL || event_screen->root_picture == None)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "ReparentNotify received on non-root 0x%lx for 0x%lx\n",
                  event->event, event->window);
      return;
    }

  meta_topic (META_DEBUG_COMPOSITOR,
              "Reparent window 0x%lx new parent 0x%lx received on 0x%lx\n",
              event->window, event->parent, event->event);

  parent_screen = meta_display_screen_for_root (compositor->display,
                                                event->parent);
  
  if (parent_screen == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "ReparentNotify 0x%lx to a non-screen or unmanaged screen 0x%lx\n",
                  event->window, event->parent);
      meta_compositor_remove_window (compositor, event->window);
      return;
    }
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->window);
  if (cwindow != NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "Window reparented to new screen at %d,%d\n",
                  event->x, event->y);
      cwindow->x = event->x;
      cwindow->y = event->y;
      return;
    }

  meta_error_trap_push_with_return (compositor->display);
  
  XGetWindowAttributes (compositor->display->xdisplay,
                        event->window, &attrs);
  
  if (meta_error_trap_pop_with_return (compositor->display, TRUE) != Success)
    {
      meta_topic (META_DEBUG_COMPOSITOR, "Failed to get attributes for window 0x%lx\n",
                  event->window);
    }
  else
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "Reparent window 0x%lx into screen 0x%lx, adding\n",
                  event->window, event->parent);
      meta_compositor_add_window (compositor,
                                  event->window, &attrs);
    }
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

  /* FIXME support CirculateNotify */
  
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
  else if (event->type == Expose)
    {
      process_expose (compositor,
                      (XExposeEvent*) event);
    }
  else if (event->type == UnmapNotify)
    {
      process_unmap (compositor,
                     (XUnmapEvent*) event);
    }
  else if (event->type == MapNotify)
    {
      process_map (compositor,
                   (XMapEvent*) event);
    }
  else if (event->type == ReparentNotify)
    {
      process_reparent (compositor,
                        (XReparentEvent*) event);
    }
  else if (event->type == CreateNotify)
    {
      process_create (compositor,
                      (XCreateWindowEvent*) event);
    }
  else if (event->type == DestroyNotify)
    {
      process_destroy (compositor,
                       (XDestroyWindowEvent*) event);
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
  XserverRegion region;
  
  if (!compositor->enabled)
    return; /* no extension */

  screen = meta_screen_for_x_screen (attrs->screen);
  g_assert (screen != NULL);
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &xwindow);

  if (cwindow != NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "Window 0x%lx already added\n", xwindow);
      return;
    }

  meta_topic (META_DEBUG_COMPOSITOR,
              "Adding window 0x%lx (%s) to compositor\n",
              xwindow,
              attrs->map_state == IsViewable ?
              "mapped" : "unmapped");
  
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

#if HAVE_NAME_WINDOW_PIXMAP
  if (compositor->have_name_window_pixmap)
    {
      meta_error_trap_push (compositor->display);
      cwindow->pixmap = XCompositeNameWindowPixmap (compositor->display->xdisplay,
                                                    cwindow->xwindow);
      meta_error_trap_pop (compositor->display, FALSE);
    }
#endif
  
  /* viewable == mapped for the root window, since root can't be unmapped */
  cwindow->viewable = (attrs->map_state == IsViewable);
  /* FIXME this assertion can fail somehow... */
  g_assert (attrs->map_state != IsUnviewable);
  
  pa.subwindow_mode = IncludeInferiors;

  if (attrs->class != InputOnly)
    {      
      format = XRenderFindVisualFormat (compositor->display->xdisplay,
                                        attrs->visual);
      cwindow->picture = XRenderCreatePicture (compositor->display->xdisplay,
#if HAVE_NAME_WINDOW_PIXMAP
                                               cwindow->pixmap != None ?
                                               cwindow->pixmap :
                                               cwindow->xwindow,
#else
                                               cwindow->xwindow,
#endif
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

  /* assume cwindow is at the top of the stack as it was either just
   * created or just reparented to the root window
   */
  screen->compositor_windows = g_list_prepend (screen->compositor_windows,
                                               cwindow);

  /* schedule paint of the new window */
  region = window_extents (cwindow);
  merge_and_destroy_damage_region (compositor, screen, region);
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
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "Window 0x%lx already removed\n", xwindow);
      return;
    }

  meta_topic (META_DEBUG_COMPOSITOR,
              "Removing window 0x%lx (%s) from compositor\n",
              xwindow,
              cwindow->viewable ? "mapped" : "unmapped");
  
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
  XRectangle r;
  XRenderColor c;
  
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
  meta_topic (META_DEBUG_COMPOSITOR, "Subwindows redirected, we are now the compositing manager\n");
  
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

  screen->trans_pixmap = XCreatePixmap (compositor->display->xdisplay,
                                        screen->xroot, 1, 1, 8);

  pa.repeat = True;
  screen->trans_picture =
    XRenderCreatePicture (compositor->display->xdisplay,
                          screen->trans_pixmap,
                          XRenderFindStandardFormat (compositor->display->xdisplay,
                                                     PictStandardA8),
                          CPRepeat,
                          &pa);
  
  c.red = c.green = c.blue = 0;
  c.alpha = 0xc0c0;
  XRenderFillRectangle (compositor->display->xdisplay,
                        PictOpSrc,
                        screen->trans_picture, &c, 0, 0, 1, 1);
  
  /* Damage the whole screen */
  r.x = 0;
  r.y = 0;
  r.width = screen->rect.width;
  r.height = screen->rect.height;

  merge_and_destroy_damage_region (compositor,
                                   screen,
                                   XFixesCreateRegion (compositor->display->xdisplay,
                                                       &r, 1));
  
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
  XRenderFreePicture (screen->display->xdisplay,
                      screen->trans_picture);
  screen->trans_picture = None;
  XFreePixmap (screen->display->xdisplay,
               screen->trans_pixmap);
  screen->trans_pixmap = None;
  
  while (screen->compositor_windows != NULL)
    {
      MetaCompositorWindow *cwindow = screen->compositor_windows->data;

      meta_topic (META_DEBUG_COMPOSITOR,
                  "Unmanage screen for 0x%lx\n", cwindow->xwindow);
      meta_compositor_remove_window (compositor, cwindow->xwindow);
    }
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_damage_window (MetaCompositor *compositor,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  Window xwindow;
  MetaCompositorWindow *cwindow;

  if (!compositor->enabled)
    return;

  if (window->screen->root_picture == None)
    return;
  
  if (window->frame)
    xwindow = window->frame->xwindow;
  else
    xwindow = window->xwindow;

  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &xwindow);
  if (cwindow == NULL)
    return;
  
  merge_and_destroy_damage_region (compositor,
                                   window->screen,
                                   window_extents (cwindow));
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}
