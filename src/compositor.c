/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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

#include "effects.h"

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
    
    ws_display_process_xerror (display, ev);
}
#endif

#ifdef HAVE_COMPOSITE_EXTENSIONS

static Window
get_xid (MetaWindow *window)
{
    if (window->frame)
	return window->frame->xwindow;
    else
	return window->xwindow;
}

#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS

static void
do_effect (MetaEffect *effect,
	   gpointer data)
{
    MetaCompScreen *screen;
    MetaCompWindow *window;

    screen = meta_comp_screen_get_by_xwindow (get_xid (effect->window));

    if (!screen)
      {
        /* sanity check: if no screen is found, bail */
        meta_warning ("No screen found for %s (%ld); aborting effect.\n",
            effect->window->desc, get_xid (effect->window));
        return;
      }

    window = meta_comp_screen_lookup_window (screen, get_xid (effect->window));
    
    switch (effect->type)
    {
    case META_EFFECT_MINIMIZE:
	if (!effect->window->frame)
	{
	    meta_effect_end (effect);
	    return;
	}
	
	meta_comp_screen_raise_window (screen, effect->window->frame->xwindow);
	
	meta_comp_window_run_minimize (window, effect);
	break;
	
    case META_EFFECT_UNMINIMIZE:
	meta_comp_window_run_unminimize (window, effect);
	break;

    case META_EFFECT_FOCUS:
	meta_comp_window_run_focus (window, effect);
	break;

    case META_EFFECT_CLOSE:
	meta_comp_window_freeze_stack (window);
	meta_comp_window_set_updates (window, FALSE);
	meta_comp_window_explode (window, effect);
	break;
    default:
	g_assert_not_reached();
	break;
    }
}

#endif /* HAVE_COMPOSITE_EXTENSIONS */

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
  
  meta_push_effect_handler (do_effect, compositor);
  
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
process_configure_notify (MetaCompositor  *compositor,
                          XConfigureEvent *event)
{
  MetaCompScreen *minfo = meta_comp_screen_get_by_xwindow (event->window);

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

  meta_comp_screen_restack (minfo, event->window, event->above);
  meta_comp_screen_set_size (minfo,
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
  
  meta_comp_screen_add_window (screen->compositor_data,
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

  meta_comp_screen_unmap (screen->compositor_data, event->window);
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


#if 0
  g_print ("destroywindow\n");
#endif
  
  if (screen == NULL)
    {
#if 0
	g_print ("ignoring\n");
#endif
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
      meta_comp_screen_raise_window (parent_screen->compositor_data,
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
static GTimer *timer;
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
  MetaScreen *screen = meta_screen_for_x_screen (attrs->screen);
  MetaCompScreen *minfo = screen->compositor_data;
  
  meta_comp_screen_add_window (minfo, xwindow);
#endif
}

void
meta_compositor_remove_window (MetaCompositor    *compositor,
                               Window             xwindow)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompScreen *minfo;

  minfo = meta_comp_screen_get_by_xwindow (xwindow);

  if (minfo)
      meta_comp_screen_remove_window (minfo, xwindow);
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    MetaCompScreen *info;

    if (screen->compositor_data)
	return;
    
    info = meta_comp_screen_new (compositor->display, screen);

    screen->compositor_data = info;
    
    meta_comp_screen_redirect (info);
#endif
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompScreen *info = screen->compositor_data;
  
  meta_comp_screen_unredirect (info);
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

void
meta_compositor_set_updates (MetaCompositor *compositor,
			     MetaWindow *window,
			     gboolean updates)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompScreen *info = window->screen->compositor_data;
  
  meta_comp_screen_set_updates (info, get_xid (window), updates);
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
  MetaCompositor *compositor;
};

#endif

#ifdef HAVE_COMPOSITE_EXTENSIONS

void
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

static GList *move_infos;

static gboolean
wobble (gpointer data)
{
  MoveInfo *info = data;
  MetaCompScreen *minfo = info->screen->compositor_data;
  double t = g_timer_elapsed (info->timer, NULL);

#if 0
  g_print ("info->window_destroyed: %d\n",
	   info->window_destroyed);
#endif
  if ((info->finished && model_is_calm (info->model)) ||
      info->window_destroyed)
    {
      if (!info->window_destroyed)
	meta_comp_screen_unset_patch (minfo, get_xid (info->window));

      move_infos = g_list_remove (move_infos, info);
      g_free (info);
#if 0
      g_print ("stop wobb\n");
#endif
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
      meta_comp_screen_set_patch (minfo,
				  get_xid (info->window),
				  points);
      
      return TRUE;
    }
}

#endif

void
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
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaRectangle rect;
  MoveInfo *move_info;

#if 0
  g_print ("begin move\n");
#endif

  if (!g_getenv ("USE_WOBBLY"))
      return;
  
  move_info = g_new0 (MoveInfo, 1);

  move_infos = g_list_prepend (move_infos, move_info);
  
  move_info->compositor = compositor;
  move_info->last_time = 0.0;
  move_info->timer = g_timer_new ();
  move_info->window_destroyed = FALSE;
  
  compute_window_rect (window, &rect);
  
#if 0
  g_print ("init: %d %d\n", initial->x, initial->y);
  g_print ("window: %d %d\n", window->rect.x, window->rect.y);
  g_print ("frame: %d %d\n", rect.x, rect.y);
  g_print ("grab: %d %d\n", grab_x, grab_y);
#endif
  
  move_info->model = model_new (&rect, TRUE);
  move_info->window = window;
  move_info->screen = window->screen;
  
  model_begin_move (move_info->model, grab_x, grab_y);
  
  g_idle_add (wobble, move_info);
#endif
}

#ifdef HAVE_COMPOSITE_EXTENSIONS
static MoveInfo *
find_info (MetaWindow *window)
{
    GList *list;

    for (list = move_infos; list != NULL; list = list->next)
    {
	MoveInfo *info = list->data;

	if (info->window == window)
	    return info;
    }

    return NULL;
}
#endif

void
meta_compositor_update_move (MetaCompositor *compositor,
			     MetaWindow *window,
			     int x, int y)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    MoveInfo *move_info = find_info (window);

    if (!g_getenv ("USE_WOBBLY"))
	return;

    model_update_move (move_info->model, x, y);
#endif
}

void
meta_compositor_end_move (MetaCompositor *compositor,
			  MetaWindow *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    MoveInfo *info = find_info (window);

    if (!g_getenv ("USE_WOBBLY"))
	return;
    
    info->finished = TRUE;
#endif
}


void
meta_compositor_free_window (MetaCompositor *compositor,
			     MetaWindow *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    MoveInfo *info = find_info (window);

    if (!g_getenv ("USE_WOBBLY"))
	return;
    
    if (info)
	info->window_destroyed = TRUE;
#endif
}
