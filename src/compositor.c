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
#include "workspace.h"

#include <math.h>

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

MetaCompositor*
meta_compositor_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositor *compositor;
  
  compositor = g_new0 (MetaCompositor, 1);
  
  compositor->display = ws_display_new (NULL);
  
  ws_display_init_test (compositor->display);
  ws_display_set_ignore_grabs (compositor->display, TRUE);
  
  compositor->meta_display = display;
  
  compositor->window_hash = g_hash_table_new_full (meta_unsigned_long_hash,
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
      /* This can happen if the topmost window is raise above
       * the GL window
       */
      return;
    }
  
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
  
  if (!node)
    return;
  
  screen = node_get_screen (compositor->meta_display->xdisplay, node);
  scr_info = screen->compositor_data;
  
  above_window = ws_window_lookup (node->drawable->display, event->above);
  
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
static void
process_map (MetaCompositor     *compositor,
             XMapEvent          *event)
{
  CmDrawableNode *node;
  MetaScreen *screen;
  
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
      cm_drawable_node_set_viewable (node, TRUE);
    }
  
  /* We don't actually need to invalidate anything, because we will
   * get damage events as the server fills the background and the client
   * draws the window
   */
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
  
  node = g_hash_table_lookup (compositor->window_hash,
			      &event->window);
  if (node != NULL)
    {
      cm_drawable_node_set_viewable (node, FALSE);
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
  
  meta_topic (META_DEBUG_COMPOSITOR,
	      "Reparent window 0x%lx new parent 0x%lx received on 0x%lx\n",
	      event->window, event->parent, event->event);
  
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

static gboolean
update (gpointer data)
{
  MetaScreen *screen = data;
  ScreenInfo *scr_info = screen->compositor_data;
  WsWindow *gl_window = scr_info->glw;
  
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  gluOrtho2D (0, 1.0, 0.0, 1.0);
  
  glClearColor (0.0, 0.5, 0.5, 0.0);
  glClear (GL_COLOR_BUFFER_BIT);
  
  glColor4f (1.0, 0.0, 0.0, 1.0);
  
  glDisable (GL_TEXTURE_2D);
  
  glBegin (GL_QUADS);
  
  glVertex2f (0.2, 0.2);
  glVertex2f (0.2, 0.4);
  glVertex2f (0.4, 0.4);
  glVertex2f (0.4, 0.2);
  
  glEnd ();
  
  ws_window_raise (gl_window);
  
  glEnable (GL_TEXTURE_2D);
  draw_windows (screen, scr_info->compositor_nodes);
  
  /* FIXME: we should probably grab the server around the raise/swap
   */

#if 0
  ws_display_grab (ws_drawable_get_display ((WsDrawable *)gl_window));
#endif
  
  
  ws_window_gl_swap_buffers (gl_window);
  glFinish();

#if 0
  ws_display_ungrab (ws_drawable_get_display ((WsDrawable *)gl_window));
#endif
  
  scr_info->idle_id = 0;

  return FALSE;
}

static void
do_repaint (CmDrawableNode *node, gpointer data)
{
    MetaScreen *screen = data;
    ScreenInfo *scr_info = screen->compositor_data;

#if 0
    g_print ("queueing repaint for %p\n", node);
#endif
    
    if (!scr_info->idle_id)
	scr_info->idle_id = g_idle_add (update, screen);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

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
  
  if (node != NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
		  "Window 0x%lx already added\n", xwindow);
      return;
    }
  
  drawable = (WsDrawable *)ws_window_lookup (compositor->display, xwindow);

  scr_info = screen->compositor_data;

  g_assert (scr_info);
  
  if (ws_window_query_input_only ((WsWindow *)drawable) ||
      drawable == (WsDrawable *)scr_info->glw)
    {
      return;
    }
  else
    {
      node = cm_drawable_node_new (drawable);

      cm_drawable_node_set_damage_func (node, do_repaint, screen);
      
#if 0
      drawable_node_set_deformation_func (node, wavy, NULL);
#endif
    }
  
  /* FIXME: we should probably just store xid's directly */
  g_hash_table_insert (compositor->window_hash,
		       &(node->drawable->xid), node);
  
  /* assume cwindow is at the top of the stack as it was either just
   * created or just reparented to the root window
   */
  scr_info->compositor_nodes = g_list_prepend (scr_info->compositor_nodes,
					       node);
  
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
  
  scr_info->glw = ws_window_new_gl (root);
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
  
  ws_window_map (scr_info->glw);
  
  ws_display_sync (compositor->display);
  
#endif
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS  
  ScreenInfo *scr_info = screen->compositor_data;
  
  if (!compositor->enabled)
    return; /* no extension */
  
  while (scr_info->compositor_nodes != NULL)
    {
      CmDrawableNode *node = scr_info->compositor_nodes->data;
      
      meta_compositor_remove_window (compositor, node->drawable->xid);
    }
  /* FIXME: free scr_info */
  
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

typedef struct
{
  double x;
  double y;
  double width;
  double height;
} DoubleRect;

typedef struct
{
  MetaWindow *window;
  CmDrawableNode *node;
  
  DoubleRect start;
  DoubleRect target;
  
  double start_time;
  int idle_id;

  MetaMinimizeFinishedFunc finished_func;
  gpointer		   finished_data;
} MiniInfo;

static gdouble
interpolate (gdouble t, gdouble begin, gdouble end, double power)
{
  return (begin + (end - begin) * pow (t, power));
}

static gboolean
stop_minimize (gpointer data)
{
  MiniInfo *info = data;
  
  cm_drawable_node_set_deformation_func (info->node, NULL, NULL);
  
  if (info->finished_func)
    info->finished_func (info->finished_data);
  
  g_free (info);
  
  return FALSE;
}

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

static void
convert (MetaScreen *screen,
	 int x, int y, int width, int height,
	 DoubleRect *rect)
{
  rect->x = x / (double)screen->rect.width;
  rect->y = y / (double)screen->rect.height;
  rect->width = width / (double)screen->rect.width;
  rect->height = height / (double)screen->rect.height;
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

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
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MiniInfo *info = g_new (MiniInfo, 1);
  CmDrawableNode *node = window_to_node (compositor, window);
  WsRectangle start;
  MetaScreen *screen = window->screen;
  
  info->node = node;
  
  info->idle_id = 0;
  
  ws_drawable_query_geometry (node->drawable, &start);
  
  convert (screen, start.x, start.y, start.width, start.height,
	   &info->start);
  convert (screen, x, y, width, height,
	   &info->target);
  
  info->window = window;
  
  info->target.y = 1 - info->target.y;
  
  info->start_time = -1;

  info->finished_func = finished;
  info->finished_data = data;
  
  cm_drawable_node_set_deformation_func (node, minimize_deformation, info);
#endif
}
