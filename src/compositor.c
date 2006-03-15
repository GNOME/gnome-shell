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
#include <cm/stacker.h>
#include <cm/cube.h>
#include <cm/rotation.h>

#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#include "spring-model.h"
#include <cm/state.h>

#include "c-screen.h"
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#define FRAME_INTERVAL_MILLISECONDS ((int)(1000.0/40.0))

#ifdef HAVE_COMPOSITE_EXTENSIONS

/* Screen specific information */
typedef struct MoveInfo MoveInfo;

struct MetaCompositor
{
  MetaDisplay *meta_display;
  
  WsDisplay *display;
  
  guint repair_idle;
  
  guint enabled : 1;
  guint have_composite : 1;
  guint have_damage : 1;
  guint have_fixes : 1;
  guint have_name_window_pixmap : 1;
  guint debug_updates : 1;
  
  GList *ignored_damage;
  
  MoveInfo *move_info;
};
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS

static WsDisplay *compositor_display;
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
handle_error (Display *dpy, XErrorEvent *ev, gpointer data)
{
    WsDisplay *display = data;
    
    ws_display_process_error (display, ev);
}
#endif

MetaCompositor *
meta_compositor_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositor *compositor;
  
  compositor = g_new0 (MetaCompositor, 1);
  
  if (!compositor_display)
    {
      gboolean has_extensions;
      
      compositor_display = ws_display_new (NULL);

      meta_errors_register_foreign_display (
	  compositor_display->xdisplay, handle_error, compositor_display);
      
      has_extensions = 
	ws_display_init_composite (compositor_display) &&
	ws_display_init_damage    (compositor_display) &&
	ws_display_init_fixes	  (compositor_display) &&
	ws_display_init_test      (compositor_display);
      
      if (!has_extensions)
	{
	  g_warning ("Disabling compositor since the server is missing at "
		     "least one of the COMPOSITE, DAMAGE, FIXES or TEST "
		     "extensions");
	  
	  return NULL;
	}
      
      ws_display_set_ignore_grabs (compositor_display, TRUE);
    }
  
  compositor->display = compositor_display;
  
  ws_display_set_synchronize (compositor_display,
			      getenv ("METACITY_SYNC") != NULL);
  
  compositor->meta_display = display;
  
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
  
  cm_node_render (node, NULL);
}

static void
process_configure_notify (MetaCompositor  *compositor,
                          XConfigureEvent *event)
{
  MetaScreenInfo *minfo = meta_screen_info_get_by_xwindow (event->window);

#if 0
  g_print ("minfo: %lx => %p\n", event->window, minfo);
#endif

#if 0
  g_print ("configure on %lx (above: %lx) %d %d %d %d\n", event->window, event->above,
	   event->x, event->y, event->width, event->height);
#endif
  
  if (!minfo)
  {
#if 0
      g_print (" --- ignoring configure (no screen info)\n");
#endif
      return;
  }

  meta_screen_info_restack (minfo, event->window, event->above);
  meta_screen_info_set_size (minfo,
			     event->window,
			     event->x, event->y,
			     event->width, event->height);
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

typedef struct
{
  CmDrawableNode *node;
  GTimer	   *timer;
} FadeInfo;

#define FADE_TIME 0.3

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
      g_object_unref (info->node);
      
      cm_drawable_node_set_viewable (info->node, FALSE);
      
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
      
      /* MapNotify wasn't for a child of the root */
      return; 
    }
  
  meta_screen_info_add_window (screen->compositor_data,
			       event->window);
}

#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_unmap (MetaCompositor     *compositor,
               XUnmapEvent        *event)
{
  MetaScreen *screen;
  
  /* See if window was unmapped as child of root */
  screen = meta_display_screen_for_root (compositor->meta_display,
					 event->event);
  
  if (screen == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
		  "UnmapNotify received on non-root 0x%lx for 0x%lx\n",
		  event->event, event->window);
      
      /* UnmapNotify wasn't for a child of the root */
      return;
    }

  meta_screen_info_unmap (screen->compositor_data, event->window);
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
  
  event_screen = meta_display_screen_for_root (compositor->meta_display,
					       event->event);
  
  if (event_screen == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
		  "ReparentNotify received on non-root 0x%lx for 0x%lx\n",
		  event->event, event->window);
      return;
    }

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
  else
    {
      meta_screen_info_raise_window (parent_screen->compositor_data,
				     event->window);
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

static GTimer *timer;

#if 0
static gboolean
update (gpointer data)
{
  MetaScreen *screen = data;
  ScreenInfo *scr_info = screen->compositor_data;
  WsWindow *gl_window = scr_info->glw;
  gdouble angle;
  
  glViewport (0, 0, screen->rect.width, screen->rect.height);
  
  if (!timer)
    timer = g_timer_new ();
  
#if 0
  g_print ("rotation: %f\n", 360 * g_timer_elapsed (timer, NULL));
#endif
  
  angle = g_timer_elapsed (timer, NULL) * 90;
#if 0
  
  angle = 180.0;
#endif
  
  cm_rotation_set_rotation (screen->display->compositor->rotation,
			    angle,
			    0.0, 1.0, 0.0);
  
  glClearColor (0.0, 0.0, 0.0, 0.0);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_DEPTH_TEST);
  ws_window_raise (gl_window);
  
#if 0
  glMatrixMode (GL_MODELVIEW);
  
  glLoadIdentity();
#endif
  
#if 0
  glTranslatef (-1.0, -1.0, 0.0);
#endif
  
#if 0
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
  gluPerspective( 45.0f, 1.0, 0.1f, 10.0f );
  
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef (0, 0, -3);
  
  glEnable (GL_DEPTH_TEST);
#endif
  
#if 0
  draw_windows (screen, scr_info->compositor_nodes);
#endif
  
  /* FIXME: we should probably grab the server around the raise/swap
   */
  
  CmState *state = cm_state_new ();
  
  cm_state_disable_depth_buffer_update (state);
  
  cm_node_render (CM_NODE (screen->display->compositor->stacker), state);
  
  cm_state_enable_depth_buffer_update (state);
  
  g_object_unref (state);
  
#if 0
  ws_display_grab (ws_drawable_get_display ((WsDrawable *)gl_window));
#endif
  
  ws_window_gl_swap_buffers (gl_window);
  glFinish();
  
  update_frame_counter ();
  
  scr_info->idle_id = 0;
  
  return FALSE;
}
#endif

#if 0
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
#endif

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
  MetaScreen *screen = meta_screen_for_x_screen (attrs->screen);
  MetaScreenInfo *minfo = screen->compositor_data;
  
  meta_screen_info_add_window (minfo, xwindow);
#endif
}

void
meta_compositor_remove_window (MetaCompositor    *compositor,
                               Window             xwindow)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaScreenInfo *minfo;
  
  minfo = meta_screen_info_get_by_xwindow (xwindow);
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    MetaScreenInfo *info;

    if (screen->compositor_data)
	return;
    
    info = meta_screen_info_new (compositor->display, screen);

    screen->compositor_data = info;
    
    meta_screen_info_redirect (info);
#endif
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaScreenInfo *info = screen->compositor_data;
  
  meta_screen_info_unredirect (info);
  screen->compositor_data = NULL;
#endif
}

#ifdef HAVE_COMPOSITE_EXTENSIONS  
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

#ifdef HAVE_COMPOSITE_EXTENSIONS

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

#endif

#define MINIMIZE_STYLE 1

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
			  MetaAnimationFinishedFunc  finished,
			  gpointer                  data)
{
}

#elif MINIMIZE_STYLE == 1

typedef struct
{
  MetaWindow *window;
  GTimer *timer;
  
  MetaCompositor *compositor;
  MetaScreenInfo *scr_info;
  
  MetaAnimationFinishedFunc finished_func;
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

static Window
get_xid (MetaWindow *window)
{
    if (window->frame)
	return window->frame->xwindow;
    else
	return window->xwindow;
}

static void
set_geometry (MiniInfo *info, gdouble elapsed)
{
  WsRectangle rect;
  
  interpolate_rectangle (elapsed, &info->current_geometry, &info->target_geometry, &rect);
  
  g_print ("y: %d %d  (%f  => %d)\n", info->current_geometry.y, info->target_geometry.y,
	   elapsed, rect.y);
  
  g_print ("setting: %d %d %d %d\n", rect.x, rect.y, rect.width, rect.height);
  
  meta_screen_info_set_target_rect (info->scr_info,
				    get_xid (info->window), &rect);
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
#if 0
      g_print ("starting phase 1\n");
#endif
      info->phase_1_started = TRUE;

      meta_screen_info_get_real_size (info->scr_info, get_xid (info->window),
				      &info->current_geometry);
      
#if 0
      info->current_geometry.x = info->node->real_x;
      info->current_geometry.y = info->node->real_y;
      info->current_geometry.width = info->node->real_width;
      info->current_geometry.height = info->node->real_height;
#endif
      
      info->target_geometry.height = info->button_height;
      info->target_geometry.width = info->button_height * info->aspect_ratio;
      info->target_geometry.x = info->button_x + center (info->target_geometry.width, info->button_width);
      info->target_geometry.y = info->current_geometry.y + center (info->button_height, info->current_geometry.height);
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
      WsRectangle real;

      meta_screen_info_get_real_size (info->scr_info, get_xid (info->window),
				      &real);
      
      g_print ("starting phase 3\n");
      info->phase_3_started = TRUE;
      
      info->current_geometry = cur;
      
      info->target_geometry.height = info->button_height;
      info->target_geometry.width = info->button_height * info->aspect_ratio;
      info->target_geometry.x = info->button_x + center (info->target_geometry.width, info->button_width);
      info->target_geometry.y = real.y + center (info->button_height, real.height);
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

  meta_screen_info_set_alpha (info->scr_info,
			      get_xid (info->window), 1 - elapsed);
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
			  MetaAnimationFinishedFunc  finished,
			  gpointer                  data)
{
  MiniInfo *info = g_new (MiniInfo, 1);
  WsRectangle start;
  MetaScreen *screen = window->screen;
  
  info->window = window;
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

#endif

void
meta_compositor_set_updates (MetaCompositor *compositor,
			     MetaWindow *window,
			     gboolean updates)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaScreenInfo *info = window->screen->compositor_data;
  
  meta_screen_info_set_updates (info, get_xid (window), updates);
#endif
}

#ifdef HAVE_COMPOSITE_EXTENSIONS

#define BALLOON_TIME 2

typedef struct
{
  CmDrawableNode *node;
  MetaAnimationFinishedFunc finished;
  gpointer finished_data;
  GTimer *timer;
} BalloonInfo;

#endif

void
meta_compositor_destroy (MetaCompositor *compositor)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS 
  g_free (compositor);
#endif
}

#ifdef HAVE_COMPOSITE_EXTENSIONS

struct MoveInfo
{
  GTimer *timer;
  gboolean finished;
  Model *model;
  MetaScreen *screen;
  MetaWindow *window;
  gdouble last_time;
  gboolean window_destroyed;
};

#endif

#ifdef HAVE_COMPOSITE_EXTENSIONS

static void
get_patch_points (Model   *model,
		  CmPoint  points[4][4])
{
  int i, j;
  
  for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
	{
	  double obj_x, obj_y;
	  
	  model_get_position (model, i, j, &obj_x, &obj_y);
	  
	  points[j][i].x = obj_x;
	  points[j][i].y = obj_y;
	}
    }
}

static gboolean
wobble (gpointer data)
{
  MoveInfo *info = data;
  MetaScreenInfo *minfo = info->screen->compositor_data;
  double t = g_timer_elapsed (info->timer, NULL);

  g_print ("info->window_destroyed: %d\n",
	   info->window_destroyed);
  if ((info->finished && model_is_calm (info->model)) ||
      info->window_destroyed)
    {
      if (!info->window_destroyed)
	meta_screen_info_unset_patch (minfo, get_xid (info->window));
      g_free (info);
      info = NULL;
      g_print ("stop wobb\n");
      return FALSE;
    }
  else
    {
      int i;
      int n_steps;
      CmPoint points[4][4];
      n_steps = floor ((t - info->last_time) * 75);
      
      for (i = 0; i < n_steps; ++i)
	model_step (info->model);

      if (i > 0)
	info->last_time = t;
      
      get_patch_points (info->model, points);
      meta_screen_info_set_patch (minfo,
				  get_xid (info->window),
				  points);
      
      return TRUE;
    }
}

#endif

static void
compute_window_rect (MetaWindow *window,
		     MetaRectangle *rect)
{
  /* FIXME: does metacity include this function somewhere? */
  
  if (window->frame)
    {
      *rect = window->frame->rect;
    }
  else
    {
      *rect = window->user_rect;
    }
}

void
meta_compositor_begin_move (MetaCompositor *compositor,
			    MetaWindow *window,
			    MetaRectangle *initial,
			    int grab_x, int grab_y)
{
#if 0
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaRectangle rect;

  g_print ("begin move\n");
  
  compositor->move_info = g_new0 (MoveInfo, 1);
  
  compositor->move_info->last_time = 0.0;
  compositor->move_info->timer = g_timer_new ();
  compositor->move_info->window_destroyed = FALSE;
  
  compute_window_rect (window, &rect);
  
  g_print ("init: %d %d\n", initial->x, initial->y);
  g_print ("window: %d %d\n", window->rect.x, window->rect.y);
  g_print ("frame: %d %d\n", rect.x, rect.y);
  g_print ("grab: %d %d\n", grab_x, grab_y);
  
  compositor->move_info->model = model_new (&rect, TRUE);
  compositor->move_info->window = window;
  compositor->move_info->screen = window->screen;
  
  model_begin_move (compositor->move_info->model, grab_x, grab_y);
  
  g_idle_add (wobble, compositor->move_info);
#endif
#endif
}

void
meta_compositor_update_move (MetaCompositor *compositor,
			     MetaWindow *window,
			     int x, int y)
{
#if 0
#ifdef HAVE_COMPOSITE_EXTENSIONS
  model_update_move (compositor->move_info->model, x, y);
#endif
#endif
}

void
meta_compositor_end_move (MetaCompositor *compositor,
			  MetaWindow *window)
{
#if 0
#ifdef HAVE_COMPOSITE_EXTENSIONS
  compositor->move_info->finished = TRUE;
#endif
#endif
}


void
meta_compositor_free_window (MetaCompositor *compositor,
			     MetaWindow *window)
{
#if 0
#ifdef HAVE_COMPOSITE_EXTENSIONS
  g_print ("freeing\n");
  if (compositor->move_info)
    {
      g_print ("setting moveinfo to destroyed\n");
      compositor->move_info->window_destroyed = TRUE;
      compositor->move_info = NULL;
    }
#endif
#endif
}
