/* 
 * Copyright (C) 2003, 2004, 2005, 2006 Red Hat, Inc.
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
#include "workspace.h"

#include <math.h>
#include <stdlib.h>

#ifdef HAVE_COMPOSITE_EXTENSIONS
#include <cm/node.h>
#include <cm/drawable-node.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <cm/ws.h>
#include <cm/wsint.h>

#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#define FRAME_INTERVAL_MILLISECONDS ((int)(1000.0/40.0))

#ifdef HAVE_COMPOSITE_EXTENSIONS
/* Screen specific information */
typedef struct
{
  /* top of stack is first in list */
  GList *compositor_nodes;
  WsWindow *glw;
  int idle_id;
} ScreenInfo;

struct MetaCompositor
{
  MetaDisplay *meta_display;
  
  WsDisplay *display;
  
  GHashTable *window_hash;
  
  guint repair_idle;
  
  guint enabled : 1;
  guint have_composite : 1;
  guint have_damage : 1;
  guint have_fixes : 1;
  guint have_name_window_pixmap : 1;
  guint debug_updates : 1;
  
  GList *ignored_damage;
};
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
free_window_hash_value (void *v)
{
  CmDrawableNode *drawable_node = v;
  
  g_object_unref (G_OBJECT (drawable_node));
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

static WsDisplay *compositor_display;

MetaCompositor*
meta_compositor_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositor *compositor;
  
  compositor = g_new0 (MetaCompositor, 1);
  
  if (!compositor_display)
    compositor_display = ws_display_new (NULL);
  compositor->display = compositor_display;
  
  ws_display_set_synchronize (compositor_display,
			      getenv ("METACITY_SYNC") != NULL);
  
  ws_display_init_test (compositor->display);
  ws_display_set_ignore_grabs (compositor->display, TRUE);
  
  compositor->meta_display = display;
  
  compositor->window_hash = g_hash_table_new_full (
						   meta_unsigned_long_hash,
						   meta_unsigned_long_equal,
						   NULL,
						   free_window_hash_value);
  
  compositor->enabled = TRUE;
  
  return compositor;
#else /* HAVE_COMPOSITE_EXTENSIONS */
  return NULL;
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_set_debug_updates (MetaCompositor *compositor,
				   gboolean	   debug_updates)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  compositor->debug_updates = !!debug_updates;
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
remove_repair_idle (MetaCompositor *compositor)
{
  if (compositor->repair_idle)
    {
      meta_topic (META_DEBUG_COMPOSITOR, "Damage idle removed\n");
      
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
static void
draw_windows (MetaScreen *screen,
	      GList      *list)
{
  CmNode *node;
  
  if (!list)
    return;
  
  node = list->data;
  
  draw_windows (screen, list->next);
  
#if 0
  g_print ("rendering: %p\n", node);
#endif
  
  cm_node_render (node);
}

static MetaScreen *
node_get_screen (Display *dpy,
		 CmDrawableNode *node)
{
  /* FIXME: we should probably have a reverse mapping
   * from nodes to screens
   */
  
  Screen *screen = XDefaultScreenOfDisplay (dpy);
  return meta_screen_for_x_screen (screen);
}

static void
handle_restacking (MetaCompositor *compositor,
		   CmDrawableNode *node,
		   CmDrawableNode *above)
{
  GList *window_link, *above_link;
  MetaScreen *screen;
  ScreenInfo *scr_info;
  
  screen = node_get_screen (compositor->meta_display->xdisplay, node);
  scr_info = screen->compositor_data;
  
  window_link = g_list_find (scr_info->compositor_nodes, node);
  above_link  = g_list_find (scr_info->compositor_nodes, above);
  
  if (!window_link || !above_link)
    return;
  
  if (window_link == above_link)
    {
      /* This can happen if the topmost window is raised above
       * the GL window
       */
      return;
    }
  
#if 0
  g_print ("restacking\n");
#endif
  
  if (window_link->next != above_link)
    {
      ScreenInfo *scr_info = screen->compositor_data;
      
      scr_info->compositor_nodes =
	g_list_delete_link (scr_info->compositor_nodes, window_link);
      scr_info->compositor_nodes =
	g_list_insert_before (scr_info->compositor_nodes, above_link, node);
    }
}

static void
process_configure_notify (MetaCompositor  *compositor,
                          XConfigureEvent *event)
{
  WsWindow *above_window;
  CmDrawableNode *node = g_hash_table_lookup (compositor->window_hash,
					      &event->window);
  CmDrawableNode *above_node;
  MetaScreen *screen;
  ScreenInfo *scr_info;
  
#if 0
  g_print ("processing configure\n");
#endif
  
  if (!node)
    return;
  
#if 0
  g_print ("we do now have a node\n");
#endif
  
  screen = node_get_screen (compositor->meta_display->xdisplay, node);
  scr_info = screen->compositor_data;
  
  above_window = ws_window_lookup (WS_RESOURCE (node->drawable)->display,
				   event->above);
  
  if (above_window == scr_info->glw)
    {
      above_node = scr_info->compositor_nodes->data;
    }
  else
    {
      above_node = g_hash_table_lookup (compositor->window_hash,
					&event->above);
    }
  
  handle_restacking (compositor, node, above_node);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */


#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_expose (MetaCompositor     *compositor,
                XExposeEvent       *event)
{
  /* FIXME: queue repaint */
}

#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void queue_repaint (CmDrawableNode *node, gpointer data);

typedef struct
{
  CmDrawableNode *node;
  GTimer	   *timer;
} FadeInfo;

#define FADE_TIME 0.3

static gboolean
fade_in (gpointer data)
{
  FadeInfo *info = data;
  gdouble elapsed = g_timer_elapsed (info->timer, NULL);
  gdouble alpha;
  
  if (elapsed > FADE_TIME)
    alpha = 1.0;
  else
    alpha = elapsed / FADE_TIME;
  
  cm_drawable_node_set_alpha (info->node, alpha);
  
  if (elapsed >= FADE_TIME)
    {
      g_object_unref (info->node);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

static gboolean
fade_out (gpointer data)
{
  FadeInfo *info = data;
  gdouble elapsed = g_timer_elapsed (info->timer, NULL);
  gdouble alpha;
  
  if (elapsed > FADE_TIME)
    alpha = 0.0;
  else
    alpha = 1 - (elapsed / FADE_TIME);
  
  cm_drawable_node_set_alpha (info->node, alpha);
  
#if 0
  g_print ("fade out: %f\n", alpha);
#endif
  
  if (elapsed >= FADE_TIME)
    {
      cm_drawable_node_set_viewable (info->node, FALSE);
      
      g_object_unref (info->node);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}
#endif

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_map (MetaCompositor     *compositor,
             XMapEvent          *event)
{
  CmDrawableNode *node;
  MetaScreen *screen;
  
  /* FIXME: do we sometimes get mapnotifies for windows that are
   * not (direct) children of the root?
   */
  
  /* See if window was mapped as child of root */
  screen = meta_display_screen_for_root (compositor->meta_display,
					 event->event);
  
  if (screen == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
		  "MapNotify received on non-root 0x%lx for 0x%lx\n",
		  event->event, event->window);
      return; /* MapNotify wasn't for a child of the root */
    }
  
#if 0
  g_print ("processing map for %lx\n", event->window);
#endif
  
  node = g_hash_table_lookup (compositor->window_hash,
			      &event->window);
  if (node == NULL)
    {
      XWindowAttributes attrs;
      
      meta_error_trap_push_with_return (compositor->meta_display);
      
      XGetWindowAttributes (compositor->meta_display->xdisplay,
			    event->window, &attrs);
      
      if (meta_error_trap_pop_with_return (compositor->meta_display, TRUE) != Success)
        {
	  meta_topic (META_DEBUG_COMPOSITOR, "Failed to get attributes for window 0x%lx\n",
		      event->window);
        }
      else
	{
	  meta_compositor_add_window (compositor,
				      event->window, &attrs);
        }
    }
  else
    {
      cm_drawable_node_update_pixmap (node);
      
      cm_drawable_node_set_alpha (node, 1.0);
      
      FadeInfo *info = g_new (FadeInfo, 1);
      
      info->node = g_object_ref (node);
      info->timer = g_timer_new ();
      
      cm_drawable_node_set_viewable (node, TRUE);
    }
  
  queue_repaint (node, screen);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_unmap (MetaCompositor     *compositor,
               XUnmapEvent        *event)
{
  CmDrawableNode *node;
  MetaScreen *screen;
  
  /* See if window was unmapped as child of root */
  screen = meta_display_screen_for_root (compositor->meta_display,
					 event->event);
  
  if (screen == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
		  "UnmapNotify received on non-root 0x%lx for 0x%lx\n",
		  event->event, event->window);
      return; /* UnmapNotify wasn't for a child of the root */
    }
  
#if 0
  g_print ("processing unmap on %lx\n", event->window);
#endif
  
  node = g_hash_table_lookup (compositor->window_hash,
			      &event->window);
  if (node != NULL)
    {
      FadeInfo *info = g_new (FadeInfo, 1);
      
      info->node = g_object_ref (node);
      info->timer = g_timer_new ();
      
      g_idle_add (fade_out, info);
    }
  
  queue_repaint (node, screen);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_create (MetaCompositor     *compositor,
                XCreateWindowEvent *event)
{
  MetaScreen *screen;
  XWindowAttributes attrs;
  
  screen = meta_display_screen_for_root (compositor->meta_display,
					 event->parent);
  
  if (screen == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
		  "CreateNotify received on non-root 0x%lx for 0x%lx\n",
		  event->parent, event->window);
      return;
    }
  
  meta_error_trap_push_with_return (compositor->meta_display);
  
  XGetWindowAttributes (compositor->meta_display->xdisplay,
			event->window, &attrs);
  
  if (meta_error_trap_pop_with_return (compositor->meta_display, TRUE) != Success)
    {
      meta_topic (META_DEBUG_COMPOSITOR, "Failed to get attributes for window 0x%lx\n",
		  event->window);
    }
  else
    {
#if 0
      g_print (//META_DEBUG_COMPOSITOR,
	       "Create window 0x%lx, adding\n", event->window);
#endif
      
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
  
  screen = meta_display_screen_for_root (compositor->meta_display,
					 event->event);
  
  if (screen == NULL)
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
  CmDrawableNode *node;
  XWindowAttributes attrs;
  
  event_screen = meta_display_screen_for_root (compositor->meta_display,
					       event->event);
  
  if (event_screen == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
		  "ReparentNotify received on non-root 0x%lx for 0x%lx\n",
		  event->event, event->window);
      return;
    }
  
#if 0
  g_print (//META_DEBUG_COMPOSITOR,
	   "Reparent window 0x%lx new parent 0x%lx received on 0x%lx\n",
	   event->window, event->parent, event->event);
#endif
  
  parent_screen = meta_display_screen_for_root (compositor->meta_display,
						event->parent);
  
  if (parent_screen == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
		  "ReparentNotify 0x%lx to a non-screen or unmanaged screen 0x%lx\n",
		  event->window, event->parent);
      meta_compositor_remove_window (compositor, event->window);
      return;
    }
  
  node = g_hash_table_lookup (compositor->window_hash,
			      &event->window);
  
  meta_error_trap_push_with_return (compositor->meta_display);
  
  XGetWindowAttributes (compositor->meta_display->xdisplay,
			event->window, &attrs);
  
  if (meta_error_trap_pop_with_return (compositor->meta_display, TRUE) != Success)
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
  
  if (event->type == ConfigureNotify)
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

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
wavy (double time,
      double in_x, double in_y,
      double *out_x, double *out_y,
      gpointer data)
{
  static int m;
  time = time * 5;
  double dx = 0.0025 * sin (time + 35 * in_y);
  double dy = 0.0025 * cos (time + 35 * in_x);
  
  *out_x = in_x + dx;
  *out_y = in_y + dy;
  
  m++;
}

static void
update_frame_counter (void)
{
#define BUFSIZE 128
  static GTimer *timer;
  static double buffer [BUFSIZE];
  static int next = 0;
  
  if (!timer)
    timer = g_timer_new ();
  
  buffer[next++] = g_timer_elapsed (timer, NULL);
  
  if (next == BUFSIZE)
    {
      int i;
      double total;
      
      next = 0;
      
      total = 0.0;
      for (i = 1; i < BUFSIZE; ++i)
	total += buffer[i] - buffer[i - 1];
      
      g_print ("frames per second: %f\n", 1 / (total / (BUFSIZE - 1)));
    }
}

static gboolean
update (gpointer data)
{
  MetaScreen *screen = data;
  ScreenInfo *scr_info = screen->compositor_data;
  WsWindow *gl_window = scr_info->glw;
  
  glViewport (0, 0, screen->rect.width, screen->rect.height);
  
#if 0
  glColor4f (1.0, 1.0, 1.0, 1.0);
  glBegin (GL_QUADS);
  glVertex2f (0.0, 0.0);
  glVertex2f (1600.0, 0.0);
  glVertex2f (1600.0, 1200.0);
  glVertex2f (0.0, 1200.0);
  glEnd ();
#endif
  
#if 0
#endif
#if 0
  glClear (GL_DEPTH_BUFFER_BIT);
#endif
  
#if 0
  glColor4f (1.0, 0.0, 0.0, 1.0);
  
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_DEPTH_TEST);
  
  glBegin (GL_QUADS);
  
  glVertex2f (0.2, 0.2);
  glVertex2f (0.2, 0.4);
  glVertex2f (0.4, 0.4);
  glVertex2f (0.4, 0.2);
  
  glEnd ();
#endif
  
#if 0
  glClearColor (0.0, 0.0, 0.0, 0.0);
  glClear (GL_COLOR_BUFFER_BIT);
#endif
  
#if 0
  glEnable (GL_TEXTURE_2D);
#endif
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_DEPTH_TEST);
  ws_window_raise (gl_window);
  
  draw_windows (screen, scr_info->compositor_nodes);
  
  /* FIXME: we should probably grab the server around the raise/swap
   */
  
#if 0
  ws_display_grab (ws_drawable_get_display ((WsDrawable *)gl_window));
#endif
  
  ws_window_gl_swap_buffers (gl_window);
  glFinish();
  
  update_frame_counter ();
  
#if 0
  ws_display_ungrab (ws_drawable_get_display ((WsDrawable *)gl_window));
#endif
  
  scr_info->idle_id = 0;
  
  return FALSE;
}

static void
queue_repaint (CmDrawableNode *node, gpointer data)
{
  MetaScreen *screen = data;
  ScreenInfo *scr_info = screen->compositor_data;
  
#if 0
  g_print ("metacity queueing repaint for %p\n", node);
#endif
  
  if (!scr_info)
    {
      /* compositor has been turned off */
      return;
    }
  
  if (!scr_info->idle_id)
    {
      scr_info->idle_id = g_idle_add (update, screen);
#if 0
      g_print ("done\n");
#endif
    }
  else
    {
#if 0
      g_print ("one was queued already\n");
#endif
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
dump_stacking_order (GList *nodes)
{
  GList *list;
  
  for (list = nodes; list != NULL; list = list->next)
    {
      CmDrawableNode *node = list->data;
      
      g_print ("%lx, ", WS_RESOURCE_XID (node->drawable));
    }
  g_print ("\n");
}
#endif

/* This is called when metacity does its XQueryTree() on startup
 * and when a new window is mapped.
 */
void
meta_compositor_add_window (MetaCompositor    *compositor,
                            Window             xwindow,
                            XWindowAttributes *attrs)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  CmDrawableNode *node;
  MetaScreen *screen;
  WsDrawable *drawable;
  ScreenInfo *scr_info;
  
  if (!compositor->enabled)
    return; /* no extension */
  
  screen = meta_screen_for_x_screen (attrs->screen);
  g_assert (screen != NULL);
  
  node = g_hash_table_lookup (compositor->window_hash,
			      &xwindow);
  
  g_print ("adding %lx\n", xwindow);
  
  if (node != NULL)
    {
      g_print ("window %lx already added\n", xwindow);
      meta_topic (META_DEBUG_COMPOSITOR,
		  "Window 0x%lx already added\n", xwindow);
      return;
    }
  
  ws_display_begin_error_trap (compositor->display);
  
  drawable = (WsDrawable *)ws_window_lookup (compositor->display, xwindow);
  
  scr_info = screen->compositor_data;
  
  ws_display_end_error_trap (compositor->display);
  
  if (!drawable)
    return;
  
  g_assert (scr_info);
  
  ws_display_begin_error_trap (compositor->display);
  
  if (ws_window_query_input_only ((WsWindow *)drawable) ||
      drawable == (WsDrawable *)scr_info->glw)
    {
      ws_display_end_error_trap (compositor->display);
      return;
    }
  
  ws_display_end_error_trap (compositor->display);
  
  node = cm_drawable_node_new (drawable);
  
  cm_drawable_node_set_damage_func (node, queue_repaint, screen);
#if 0
  drawable_node_set_deformation_func (node, wavy, NULL);
#endif
  
  /* FIXME: we should probably just store xid's directly */
  g_hash_table_insert (compositor->window_hash,
		       &(WS_RESOURCE (node->drawable)->xid), node);
  
  /* assume cwindow is at the top of the stack as it was either just
   * created or just reparented to the root window
   */
  scr_info->compositor_nodes = g_list_prepend (scr_info->compositor_nodes,
					       node);
  
  dump_stacking_order (scr_info->compositor_nodes);
  
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}
void
meta_compositor_remove_window (MetaCompositor    *compositor,
                               Window             xwindow)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  CmDrawableNode *node;
  MetaScreen *screen;
  ScreenInfo *scr_info;
  
  if (!compositor->enabled)
    return; /* no extension */
  
  node = g_hash_table_lookup (compositor->window_hash,
			      &xwindow);
  
  if (node == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
		  "Window 0x%lx already removed\n", xwindow);
      return;
    }
  
  screen = node_get_screen (compositor->meta_display->xdisplay, node);
  scr_info = screen->compositor_data;
  
  scr_info->compositor_nodes = g_list_remove (scr_info->compositor_nodes,
					      node);
  
  /* Frees node as side effect */
  g_hash_table_remove (compositor->window_hash,
		       &xwindow);
  
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  ScreenInfo *scr_info = g_new0 (ScreenInfo, 1);
  
  WsScreen *ws_screen =
    ws_display_get_screen_from_number (compositor->display, screen->number);
  WsWindow *root = ws_screen_get_root_window (ws_screen);
  WsRegion *region;
  Window current_cm_sn_owner;
  WsWindow *new_cm_sn_owner;
  Display *xdisplay;
  Atom cm_sn_atom;
  char buf[128];
  
  scr_info->glw = ws_screen_get_gl_window (ws_screen);
  scr_info->compositor_nodes = NULL;
  scr_info->idle_id = 0;
  
  g_print ("setting compositor_data for screen %p to %p\n", screen, scr_info);
  screen->compositor_data = scr_info;
  
  ws_display_init_composite (compositor->display);
  ws_display_init_damage (compositor->display);
  ws_display_init_fixes (compositor->display);
  
  ws_window_redirect_subwindows (root);
  ws_window_set_override_redirect (scr_info->glw, TRUE);
  ws_window_unredirect (scr_info->glw);
  
  region = ws_region_new (compositor->display);
  ws_window_set_input_shape (scr_info->glw, region);
  g_object_unref (G_OBJECT (region));
  
  xdisplay = WS_RESOURCE_XDISPLAY (ws_screen);
  snprintf(buf, sizeof(buf), "CM_S%d", screen->number);
  cm_sn_atom = XInternAtom (xdisplay, buf, False);
  current_cm_sn_owner = XGetSelectionOwner (xdisplay, cm_sn_atom);
  
  if (current_cm_sn_owner != None)
    {
      meta_warning (_("Screen %d on display \"%s\" already has a compositing manager\n"),
		    screen->number, ",madgh");
    }
  
  new_cm_sn_owner = ws_screen_get_root_window (ws_screen);
  
  XSetSelectionOwner (xdisplay, cm_sn_atom, WS_RESOURCE_XID (new_cm_sn_owner),
		      CurrentTime);
  
  ws_window_map (scr_info->glw);
  
  ws_display_sync (compositor->display);
  
#if 0
  children = ws_window_list_children (root);
#endif
  
#endif
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS  
  ScreenInfo *scr_info = screen->compositor_data;
  WsScreen *ws_screen =
    ws_display_get_screen_from_number (compositor->display, screen->number);
  WsWindow *root = ws_screen_get_root_window (ws_screen);
  
  if (!compositor->enabled)
    return; /* no extension */
  
  while (scr_info->compositor_nodes != NULL)
    {
      CmDrawableNode *node = scr_info->compositor_nodes->data;
      
      meta_compositor_remove_window (compositor,
				     WS_RESOURCE (node->drawable)->xid);
    }
  
  ws_window_raise (scr_info->glw);
  
  ws_window_unredirect_subwindows (root);
  ws_window_unmap (scr_info->glw);
  
  screen->compositor_data = NULL;
  
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

#ifdef HAVE_COMPOSITE_EXTENSIONS  
static CmDrawableNode *
window_to_node (MetaCompositor *compositor,
		MetaWindow *window)
{
  Window xwindow;
  CmDrawableNode *node;
  
  if (window->frame)
    xwindow = window->frame->xwindow;
  else
    xwindow = window->xwindow;
  
  node = g_hash_table_lookup (compositor->window_hash,
			      &xwindow);
  
  return node;
}
#endif

typedef struct
{
  double x;
  double y;
  double width;
  double height;
} DoubleRect;

#if 0
static gdouble
interpolate (gdouble t, gdouble begin, gdouble end, double power)
{
  return (begin + (end - begin) * pow (t, power));
}
#endif

#if 0
static gboolean
stop_minimize (gpointer data)
{
  MiniInfo *info = data;
  
  g_source_remove (info->repaint_id);
  
  cm_drawable_node_set_deformation_func (info->node, NULL, NULL);
  
  if (info->finished_func)
    info->finished_func (info->finished_data);
  
  g_free (info);
  
  return FALSE;
}
#endif

#if 0
static void
minimize_deformation (gdouble time,
		      double in_x,
		      double in_y,
		      double *out_x,
		      double *out_y,
		      gpointer data)
{
#define MINIMIZE_TIME 0.5
  MiniInfo *info = data;
  gdouble elapsed;
  gdouble pos;
  
  if (info->start_time == -1)
    info->start_time = time;
  
  elapsed = time - info->start_time;
  pos = elapsed / MINIMIZE_TIME;
  
  *out_x = interpolate (pos, in_x, info->target.x + info->target.width * ((in_x - info->start.x)  / info->start.width), 10 * in_y);
  *out_y = interpolate (pos, in_y, info->target.y + info->target.height * ((in_y - info->start.y)  / info->start.height), 1.0);
  
  if (elapsed > MINIMIZE_TIME)
    {
      g_assert (info->node);
      if (!info->idle_id)
	info->idle_id = g_idle_add (stop_minimize, info);
    }
}
#endif

static gdouble
interpolate (gdouble t, gdouble begin, gdouble end, double power)
{
  return (begin + (end - begin) * pow (t, power));
}

static void
interpolate_rectangle (gdouble		t,
		       WsRectangle *	from,
		       WsRectangle *	to,
		       WsRectangle *	result)
{
  if (!result)
    return;
  
  result->x = interpolate (t, from->x, to->x, 1);
  result->y = interpolate (t, from->y, to->y, 1);
  result->width = interpolate (t, from->width, to->width, 1);
  result->height = interpolate (t, from->height, to->height, 1);
}

#define MINIMIZE_STYLE 3

#ifndef HAVE_COMPOSITE_EXTENSIONS
#undef MINIMIZE_STYLE
#define MINIMIZE_STYLE 0
#endif

#if MINIMIZE_STYLE == 0

void
meta_compositor_minimize (MetaCompositor           *compositor,
			  MetaWindow               *window,
			  int                       x,
			  int                       y,
			  int                       width,
			  int                       height,
			  MetaMinimizeFinishedFunc  finished,
			  gpointer                  data)
{
}

#elif MINIMIZE_STYLE == 1

typedef struct
{
  CmDrawableNode *node;
  GTimer *timer;
  
  MetaCompositor *compositor;
  ScreenInfo *scr_info;
  
  MetaMinimizeFinishedFunc finished_func;
  gpointer		     finished_data;
  
  gdouble	aspect_ratio;
  
  WsRectangle current_geometry;
  WsRectangle target_geometry;
  gdouble	 current_alpha;
  gdouble	 target_alpha;
  
  int		button_x;
  int		button_y;
  int		button_width;
  int		button_height;
  
  /* FIXME: maybe would be simpler if all of this was an array */
  gboolean phase_1_started;
  gboolean phase_2_started;
  gboolean phase_3_started;
  gboolean phase_4_started;
  gboolean phase_5_started;
} MiniInfo;

static void
set_geometry (MiniInfo *info, gdouble elapsed)
{
  WsRectangle rect;
  
  interpolate_rectangle (elapsed, &info->current_geometry, &info->target_geometry, &rect);
  
  g_print ("y: %d %d  (%f  => %d)\n", info->current_geometry.y, info->target_geometry.y,
	   elapsed, rect.y);
  
  g_print ("setting: %d %d %d %d\n", rect.x, rect.y, rect.width, rect.height);
  
  cm_drawable_node_set_geometry (info->node,
				 rect.x, rect.y,
				 rect.width, rect.height);
}

static int
center (gdouble what, gdouble in)
{
  return (in - what) / 2.0 + 0.5;
}

static void
run_phase_1 (MiniInfo *info, gdouble elapsed)
{
  if (!info->phase_1_started)
    {
      GList *next;
      g_print ("starting phase 1\n");
      info->phase_1_started = TRUE;
      
      info->current_geometry.x = info->node->real_x;
      info->current_geometry.y = info->node->real_y;
      info->current_geometry.width = info->node->real_width;
      info->current_geometry.height = info->node->real_height;
      
      info->target_geometry.height = info->button_height;
      info->target_geometry.width = info->button_height * info->aspect_ratio;
      info->target_geometry.x = info->button_x + center (info->target_geometry.width, info->button_width);
      info->target_geometry.y = info->node->real_y + center (info->button_height, info->node->real_height);
      
      handle_restacking (info->compositor, info->node,
			 info->scr_info->compositor_nodes->data);
    }
  
  set_geometry (info, elapsed);
}

static void
run_phase_2 (MiniInfo *info, gdouble elapsed)
{
#define WOBBLE_FACTOR 3
  
  if (!info->phase_2_started)
    {
      WsRectangle cur = info->target_geometry;
      
      g_print ("starting phase 2\n");
      
      info->phase_2_started = TRUE;
      
      info->current_geometry = cur;
      
      info->target_geometry.x = cur.x + center (WOBBLE_FACTOR * cur.width, cur.width);
      info->target_geometry.y = cur.y + center (WOBBLE_FACTOR * cur.height, cur.height);
      info->target_geometry.width = cur.width * WOBBLE_FACTOR;
      info->target_geometry.height = cur.height * WOBBLE_FACTOR;
    }
  
  set_geometry (info, elapsed);
}

static void
run_phase_3 (MiniInfo *info, gdouble elapsed)
{
  if (!info->phase_3_started)
    {
      WsRectangle cur = info->target_geometry;
      
      g_print ("starting phase 3\n");
      info->phase_3_started = TRUE;
      
      info->current_geometry = cur;
      
      info->target_geometry.height = info->button_height;
      info->target_geometry.width = info->button_height * info->aspect_ratio;
      info->target_geometry.x = info->button_x + center (info->target_geometry.width, info->button_width);
      info->target_geometry.y = info->node->real_y + center (info->button_height, info->node->real_height);
    }
  
  set_geometry (info, elapsed);
}

static void
run_phase_4 (MiniInfo *info, gdouble elapsed)
{
  if (!info->phase_4_started)
    {
      WsRectangle cur = info->target_geometry;
      
      g_print ("starting phase 4\n");
      info->phase_4_started = TRUE;
      
      info->current_geometry = cur;
      
      info->target_geometry.height = info->button_height;
      info->target_geometry.width = info->button_height * info->aspect_ratio;
      info->target_geometry.x = cur.x;
      g_print ("button y: %d\n", info->button_y);
      info->target_geometry.y = info->button_y;
    }
  
  set_geometry (info, elapsed);
}

static void
run_phase_5 (MiniInfo *info, gdouble elapsed)
{
  if (!info->phase_5_started)
    {
      WsRectangle cur = info->target_geometry;
      
      g_print ("starting phase 5\n");
      info->phase_5_started = TRUE;
      
      info->current_geometry = cur;
      info->target_geometry.x = info->button_x;
      info->target_geometry.y = info->button_y;
      info->target_geometry.width = info->button_width;
      info->target_geometry.height = info->button_height;
    }
  
  set_geometry (info, elapsed);
  
  cm_drawable_node_set_alpha (info->node, 1 - elapsed);
}

static gboolean
run_animation_01 (gpointer data)
{
  MiniInfo *info = data;
  gdouble elapsed;
  
  elapsed = g_timer_elapsed (info->timer, NULL);
  
#define PHASE_0		0.0
#define PHASE_1		0.225		/* scale to size of button */
#define PHASE_2		0.325		/* scale up a little */
#define PHASE_3		0.425		/* scale back a little */
#define PHASE_4		0.650		/* move to button */
#define PHASE_5		1.0		/* fade out */
  
  if (elapsed < PHASE_1)
    {
      /* phase one */
      run_phase_1 (info, (elapsed - PHASE_0)/(PHASE_1 - PHASE_0));
    }
  else if (elapsed < PHASE_2)
    {
      /* phase two */
      run_phase_2 (info, (elapsed - PHASE_1)/(PHASE_2 - PHASE_1));
    }
  else if (elapsed < PHASE_3)
    {
      /* phase three */
      run_phase_3 (info, (elapsed - PHASE_2)/(PHASE_3 - PHASE_2));
    }
  else if (elapsed < PHASE_4)
    {
      /* phase four */
      run_phase_4 (info, (elapsed - PHASE_3)/(PHASE_4 - PHASE_3));
    }
  else if (elapsed < PHASE_5)
    {
      /* phase five */
      run_phase_5 (info, (elapsed - PHASE_4)/(PHASE_5 - PHASE_4));
    }
  else 
    {
      cm_drawable_node_set_viewable (info->node, FALSE);
      
      cm_drawable_node_unset_geometry (info->node);
      
      cm_drawable_node_set_alpha (info->node, 1.0);
      
      if (info->finished_func)
	info->finished_func (info->finished_data);
      
      return FALSE;
    }
  
  return TRUE;
}

void
meta_compositor_minimize (MetaCompositor           *compositor,
			  MetaWindow               *window,
			  int                       x,
			  int                       y,
			  int                       width,
			  int                       height,
			  MetaMinimizeFinishedFunc  finished,
			  gpointer                  data)
{
  MiniInfo *info = g_new (MiniInfo, 1);
  CmDrawableNode *node = window_to_node (compositor, window);
  WsRectangle start;
  MetaScreen *screen = window->screen;
  
  info->node = node;
  info->timer = g_timer_new ();
  
  info->finished_func = finished;
  info->finished_data = data;
  
  info->phase_1_started = FALSE;
  info->phase_2_started = FALSE;
  info->phase_3_started = FALSE;
  info->phase_4_started = FALSE;
  info->phase_5_started = FALSE;
  
  info->button_x = x;
  info->button_y = y;
  info->button_width = width;
  info->button_height = height;
  
  info->compositor = compositor;
  info->scr_info = screen->compositor_data;
  
#if 0
  cm_drawable_node_set_deformation_func (node, minimize_deformation, info);
#endif
  
  info->aspect_ratio = 1.3;
  
  g_idle_add (run_animation_01, info);
}

void
meta_compositor_unminimize (MetaCompositor           *compositor,
			    MetaWindow               *window,
			    int                       x,
			    int                       y,
			    int                       width,
			    int                       height,
			    MetaMinimizeFinishedFunc  finished,
			    gpointer                  data)
{
  finished(data);
}

#elif MINIMIZE_STYLE == 2

#if 0
static gboolean
do_minimize_animation (gpointer data)
{
  MiniInfo *info = data;
  double elapsed;
  gboolean done = FALSE;
  
#define FADE_TIME 0.5
  
  elapsed = g_timer_elapsed (info->timer, NULL);
  elapsed = elapsed / FADE_TIME;
  
  if (elapsed >= 1.0)
    {
      elapsed = 1.0;
      done = TRUE;
    }
  
  g_print ("%f\n", elapsed);
  
  cm_drawable_node_set_geometry (info->node,
				 info->node->real_x + interpolate (elapsed, 0, info->node->real_width / 2, 1),
				 info->node->real_y + interpolate (elapsed, 0, info->node->real_height / 2, 1),
				 interpolate (elapsed, info->node->real_width, 0, 1),
				 interpolate (elapsed, info->node->real_height, 0, 1));
  
  if (done)
    return FALSE;
  
#if 0
  g_print ("inter: %f %f %f\n", 0, 735, interpolate (0.0, 735.0, 0.5, 1.0));
  
  g_print ("inter x .5: %f (%d %d)\n", info->node->real_x + interpolate (0, info->node->real_width / 2, .5, 1), 0, info->node->real_width);
#endif
  
  cm_drawable_node_set_alpha (info->node, 1 - elapsed);
  
  if (done)
    {
    }
  else
    {
      return TRUE;
    }
  
#if 0
  queue_repaint (info->node,
		 node_get_screen (info->window->display->xdisplay,
				  info->node));
#endif
}
#endif

#elif MINIMIZE_STYLE == 3


typedef struct XYPair Point;
typedef struct XYPair Vector;
typedef struct Spring Spring;
typedef struct Object Object;
typedef struct Model Model;

struct XYPair {
  double x, y;
};

#define GRID_WIDTH  4
#define GRID_HEIGHT 4

#define MODEL_MAX_OBJECTS (GRID_WIDTH * GRID_HEIGHT)
#define MODEL_MAX_SPRINGS (MODEL_MAX_OBJECTS * 2)

#define DEFAULT_SPRING_K  5.0
#define DEFAULT_FRICTION  1.4

struct Spring {
  Object *a;
  Object *b;
  /* Spring position at rest, from a to b:
     offset = b.position - a.position
  */
  Vector offset;
};

struct Object {
  Vector force;
  
  Point position;
  Vector velocity;
  
  double mass;
  double theta;
  
  int immobile;
};

struct Model {
  int num_objects;
  Object objects[MODEL_MAX_OBJECTS];
  
  int num_springs;
  Spring springs[MODEL_MAX_SPRINGS];
  
  Object *anchor_object;
  Vector anchor_offset;
  
  double friction;/* Friction constant */
  double k;/* Spring constant */
  
  double last_time;
  double steps;
};


typedef struct
{
  CmDrawableNode *node;
  GTimer *timer;
  gboolean expand;
  
  MetaCompositor *compositor;
  ScreenInfo *scr_info;
  MetaRectangle rect;
  
  MetaAnimationFinishedFunc finished_func;
  gpointer		      finished_data;
  
  Model	model;
  
  int		button_x;
  int		button_y;
  int		button_width;
  int		button_height;
} MiniInfo;

static void
object_init (Object *object,
	     double position_x, double position_y,
	     double velocity_x, double velocity_y, double mass)
{
  object->position.x = position_x;
  object->position.y = position_y;
  
  object->velocity.x = velocity_x;
  object->velocity.y = velocity_y;
  
  object->mass = mass;
  
  object->force.x = 0;
  object->force.y = 0;
  
  object->immobile = 0;
}

static void
spring_init (Spring *spring,
	     Object *object_a, Object *object_b,
	     double offset_x, double offset_y)
{
  spring->a = object_a;
  spring->b = object_b;
  spring->offset.x = offset_x;
  spring->offset.y = offset_y;
}

static void
model_add_spring (Model *model,
		  Object *object_a, Object *object_b,
		  double offset_x, double offset_y)
{
  Spring *spring;
  
  g_assert (model->num_springs < MODEL_MAX_SPRINGS);
  
  spring = &model->springs[model->num_springs];
  model->num_springs++;
  
  spring_init (spring, object_a, object_b, offset_x, offset_y);
}

static void
model_init_grid (Model *model, MetaRectangle *rect, gboolean expand)
{
  int x, y, i, v_x, v_y;
  int hpad, vpad;
  
  model->num_objects = MODEL_MAX_OBJECTS;
  
  model->num_springs = 0;
  
  i = 0;
  if (expand) {
    hpad = rect->width / 3;
    vpad = rect->height / 3;
  }
  else {
    hpad = rect->width / 6;
    vpad = rect->height / 6;
  }
  
  for (y = 0; y < GRID_HEIGHT; y++)
    for (x = 0; x < GRID_WIDTH; x++) {
      
      v_x = random() % 40 - 20;
      v_y = random() % 40 - 20;
      
      if (expand)
	object_init (&model->objects[i],
		     rect->x + x * rect->width / 6 + rect->width / 4,
		     rect->y + y * rect->height / 6 + rect->height / 4,
		     v_x, v_y, 20);
      else
	object_init (&model->objects[i],
		     rect->x + x * rect->width / 3,
		     rect->y + y * rect->height / 3,
		     v_x, v_y, 20);
      
      
      if (x > 0)
	model_add_spring (model,
			  &model->objects[i - 1],
			  &model->objects[i],
			  hpad, 0);
      
      if (y > 0)
	model_add_spring (model,
			  &model->objects[i - GRID_WIDTH],
			  &model->objects[i],
			  0, vpad);
      
      i++;
    }
}

static void
model_init (Model *model, MetaRectangle *rect, gboolean expand)
{
  model->anchor_object = NULL;
  
  model->k        = DEFAULT_SPRING_K;
  model->friction = DEFAULT_FRICTION;
  
  model_init_grid (model, rect, expand);
  model->steps = 0;
  model->last_time = 0;
}

static void
object_apply_force (Object *object, double fx, double fy)
{
  object->force.x += fx;
  object->force.y += fy;
}

/* The model here can be understood as a rigid body of the spring's
 * rest shape, centered on the vector between the two object
 * positions. This rigid body is then connected by linear-force
 * springs to each object. This model does degnerate into a simple
 * spring for linear displacements, and does something reasonable for
 * rotation.
 *
 * There are other possibilities for handling the rotation of the
 * spring, and it might be interesting to explore something which has
 * better length-preserving properties. For example, with the current
 * model, an initial 180 degree rotation of the spring results in the
 * spring collapsing down to 0 size before expanding back to it's
 * natural size again.
 */

static void
spring_exert_forces (Spring *spring, double k)
{
  Vector da, db;
  Vector a, b;
  
  a = spring->a->position;
  b = spring->b->position;
  
  /* A nice vector diagram would likely help here, but my ASCII-art
   * skills aren't up to the task. Here's how to make your own
   * diagram:
   *
   * Draw a and b, and the vector AB from a to b
   * Find the center of AB
   * Draw spring->offset so that its center point is on the center of AB
   * Draw da from a to the initial point of spring->offset
   * Draw db from b to the final point of spring->offset
   *
   * The math below should be easy to verify from the diagram.
   */
  
  da.x = 0.5 * (b.x - a.x - spring->offset.x);
  da.y = 0.5 * (b.y - a.y - spring->offset.y);
  
  db.x = 0.5 * (a.x - b.x + spring->offset.x);
  db.y = 0.5 * (a.y - b.y + spring->offset.y);
  
  object_apply_force (spring->a, k *da.x, k * da.y);
  
  object_apply_force (spring->b, k * db.x, k * db.y);
}

static void
model_step_object (Model *model, Object *object)
{
  Vector acceleration;
  
  object->theta += 0.05;
  
  /* Slow down due to friction. */
  object->force.x -= model->friction * object->velocity.x;
  object->force.y -= model->friction * object->velocity.y;
  
  acceleration.x = object->force.x / object->mass;
  acceleration.y = object->force.y / object->mass;
  
  if (object->immobile) {
    object->velocity.x = 0;
    object->velocity.y = 0;
  } else {
    object->velocity.x += acceleration.x;
    object->velocity.y += acceleration.y;
    
    object->position.x += object->velocity.x;
    object->position.y += object->velocity.y;
  }
  
  object->force.x = 0.0;
  object->force.y = 0.0;
}

static void
model_step (Model *model)
{
  int i;
  
  for (i = 0; i < model->num_springs; i++)
    spring_exert_forces (&model->springs[i], model->k);
  
  for (i = 0; i < model->num_objects; i++)
    model_step_object (model, &model->objects[i]);
}

#define WOBBLE_TIME 1.0

static gboolean
run_animation (gpointer data)
{
  MiniInfo *info = data;
  gdouble t, blend;
  CmPoint points[4][4];
  int i, j, steps, target_x, target_y;
  
  t = g_timer_elapsed (info->timer, NULL);
  
  info->model.steps += (t - info->model.last_time) / 0.03;
  info->model.last_time = t;
  steps = floor(info->model.steps);
  info->model.steps -= steps;
  
  for (i = 0; i < steps; i++)
    model_step (&info->model);
  
  if (info->expand)
    blend = t / WOBBLE_TIME;
  else
    blend = 0;
  
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++) {
      target_x = info->node->real_x + i * info->node->real_width / 3;
      target_y = info->node->real_y + j * info->node->real_height / 3;
      
      points[j][i].x =
	(1 - blend) * info->model.objects[j * 4 + i].position.x +
	blend * target_x;
      points[j][i].y =
	(1 - blend) * info->model.objects[j * 4 + i].position.y +
	blend * target_y;
    }
  
  cm_drawable_node_set_patch (info->node, points);
  if (info->expand)
    cm_drawable_node_set_alpha (info->node, t / WOBBLE_TIME);
  else
    cm_drawable_node_set_alpha (info->node, 1.0 - t / WOBBLE_TIME);
  
  if (t > WOBBLE_TIME) {
    cm_drawable_node_set_viewable (info->node, info->expand);
    cm_drawable_node_unset_geometry (info->node);
    cm_drawable_node_set_alpha (info->node, 1.0);
    
    if (info->finished_func)
      info->finished_func (info->finished_data);
    return FALSE;
  }
  else {
    return TRUE;
  }
}

void
meta_compositor_minimize (MetaCompositor            *compositor,
			  MetaWindow                *window,
			  int                        x,
			  int                        y,
			  int                        width,
			  int                        height,
			  MetaAnimationFinishedFunc  finished,
			  gpointer                   data)
{
  MiniInfo *info = g_new (MiniInfo, 1);
  CmDrawableNode *node = window_to_node (compositor, window);
  MetaScreen *screen = window->screen;
  
  info->node = node;
  info->timer = g_timer_new ();
  
  info->finished_func = finished;
  info->finished_data = data;
  
  info->rect = window->user_rect;
  
  model_init (&info->model, &info->rect, FALSE);
  
  info->expand = FALSE;
  info->button_x = x;
  info->button_y = y;
  info->button_width = width;
  info->button_height = height;
  
  info->compositor = compositor;
  info->scr_info = screen->compositor_data;
  
  g_idle_add (run_animation, info);
}

void
meta_compositor_unminimize (MetaCompositor            *compositor,
			    MetaWindow                *window,
			    int                        x,
			    int                        y,
			    int                        width,
			    int                        height,
			    MetaAnimationFinishedFunc  finished,
			    gpointer                   data)
{
  MiniInfo *info = g_new (MiniInfo, 1);
  CmDrawableNode *node = window_to_node (compositor, window);
  MetaScreen *screen = window->screen;
  
  info->node = node;
  info->timer = g_timer_new ();
  
  info->finished_func = finished;
  info->finished_data = data;
  
  info->rect = window->user_rect;
  
  model_init (&info->model, &info->rect, TRUE);
  
  info->expand = TRUE;
  info->button_x = x;
  info->button_y = y;
  info->button_width = width;
  info->button_height = height;
  
  info->compositor = compositor;
  info->scr_info = screen->compositor_data;
  
  g_idle_add (run_animation, info);
}

void
meta_compositor_set_updates (MetaCompositor *compositor,
			     MetaWindow *window,
			     gboolean updates)
{
  CmDrawableNode *node = window_to_node (compositor, window);
  
  if (node)
    {
      g_print ("turning updates %s\n", updates? "on" : "off");
      cm_drawable_node_set_updates (node, updates);
      
      update (window->screen);
    }
}

#endif

#ifdef HAVE_COMPOSITE_EXTENSIONS

#define BALLOON_TIME 2

typedef struct
{
  CmDrawableNode *node;
  MetaAnimationFinishedFunc finished;
  gpointer finished_data;
  GTimer *timer;
} BalloonInfo;

static gboolean
blow_up (gpointer data)
{
  BalloonInfo *info = data;
  gdouble elapsed = g_timer_elapsed (info->timer, NULL) / BALLOON_TIME;
  CmPoint points[4][4];
  int i, j;
  
  if (elapsed > BALLOON_TIME)
    {
      cm_drawable_node_set_viewable (info->node, FALSE);
      return FALSE;
    }
  
  for (i = 0; i < 4; ++i)
    {
      for (j = 0; j < 4; ++j)
	{
	  points[i][j].x = info->node->real_x + j;
	  points[i][j].y = info->node->real_y + i;
	}
    }
  
  cm_drawable_node_set_patch (info->node, points);
  
  return TRUE;
}

void
meta_compositor_delete_window (MetaCompositor *compositor, 
			       MetaWindow *window,
			       MetaAnimationFinishedFunc finished,
			       gpointer data)
{
  CmDrawableNode *node;
  BalloonInfo *info = g_new (BalloonInfo, 1);
  
  node = window_to_node (compositor, window);
  
  if (!node)
    {
      finished (data);
      return;
    }
  
  info->finished = finished;
  info->finished_data = data;
  info->timer = g_timer_new ();
  g_idle_add (blow_up, info);
}



void
meta_compositor_destroy (MetaCompositor *compositor)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS 
  GSList *list;
  
#if 0
  /* FIXME */
  ws_display_free (compositor->display);
#endif
  
  g_hash_table_destroy (compositor->window_hash);
  
  g_free (compositor);
#endif
}

#endif
