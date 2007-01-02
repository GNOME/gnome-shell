/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* 
 * Copyright (C) 2006 Red Hat, Inc.
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

#ifdef HAVE_COMPOSITE_EXTENSIONS
#include <cm/ws.h>
#include <cm/stacker.h>
#include <cm/wsint.h>
#include <cm/drawable-node.h>
#include <cm/state.h>
#include <cm/magnifier.h>
#include <cm/square.h>
#include <string.h>

#include "screen.h"
#include "c-screen.h"
#include "c-window.h"

struct MetaCompScreen
{
  WsDisplay *display;
  CmStacker *stacker;
  CmMagnifier *magnifier;
  
  WsWindow *gl_window;
  WsWindow *root_window;
  
  WsScreen *screen;
  MetaScreen *meta_screen;
  
  int repaint_id;
  int idle_id;
  
  WsWindow *selection_window;

  GHashTable *windows_by_xid;
};

static MetaCompWindow *
meta_comp_window_lookup (MetaCompScreen *info,
			 Window xid)
{
    MetaCompWindow *window;
    
    window = g_hash_table_lookup (info->windows_by_xid, (gpointer)xid);
    
    return window;
}

MetaCompWindow *
meta_comp_screen_lookup_window   (MetaCompScreen *info,
				  Window          xwindow)
{
    return meta_comp_window_lookup (info, xwindow);
}

#if 0
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
#endif

static void
dump_stacking_order (GList *nodes)
{
  GList *list;
  
  for (list = nodes; list != NULL; list = list->next)
    {
      CmDrawableNode *node = list->data;
      
      if (node)
	g_print ("%lx, ", WS_RESOURCE_XID (node->drawable));
    }
  g_print ("\n");
}

static gboolean
repaint (gpointer data)
{
  MetaCompScreen *info = data;
  CmState *state;
#if 0
  g_print ("repaint\n");
#endif
  glViewport (0, 0,
	      info->meta_screen->rect.width,
	      info->meta_screen->rect.height);
  
  glLoadIdentity();
  
#if 0
  glClearColor (0, 0, 0, 1.0);
  glClear (GL_COLOR_BUFFER_BIT);
#endif
  
  ws_window_raise (info->gl_window);
  
#if 0
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);
  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  glColor4f (0.0, 1.0, 0.0, 1.0);
  glRectf (-1.0, -1.0, 1.0, 1.0);
  glFinish();
#endif
  
  state = cm_state_new ();
  
  cm_state_disable_depth_buffer_update (state);
  
  cm_node_render (CM_NODE (info->magnifier), state);
  
  cm_state_enable_depth_buffer_update (state);
  
  g_object_unref (state);

  ws_window_gl_swap_buffers (info->gl_window);
  glFinish();
  
#if 0
  dump_stacking_order (info->stacker->children);
#endif
  
  info->idle_id = 0;
  return FALSE;
}

static MetaCompWindow *
find_comp_window (MetaCompScreen *info,
		  Window	       xwindow)
{
  return meta_comp_window_lookup (info, xwindow);
}

static CmNode *
find_node (MetaCompScreen *info,
	   Window	   xwindow)
{
  MetaCompWindow *window = meta_comp_window_lookup (info, xwindow);
  
  if (window)
    return meta_comp_window_get_node (window);
  
  return NULL;
}

static GList *all_screen_infos;

MetaCompScreen *
meta_comp_screen_get_by_xwindow (Window xwindow)
{
  GList *list;
  
  for (list = all_screen_infos; list != NULL; list = list->next)
    {
      MetaCompScreen *info = list->data;
      
      if (find_node (info, xwindow))
	return info;
    }
  
  return NULL;
}

MetaCompScreen *
meta_comp_screen_new (WsDisplay *display,
		      MetaScreen *screen)
{
  MetaCompScreen *scr_info = g_new0 (MetaCompScreen, 1);
  
  scr_info->screen = ws_display_get_screen_from_number (
							display, screen->number);
  scr_info->root_window = ws_screen_get_root_window (scr_info->screen);
  scr_info->display = display;
  scr_info->meta_screen = screen;
  scr_info->windows_by_xid = g_hash_table_new (g_direct_hash, g_direct_equal);
  
  all_screen_infos = g_list_prepend (all_screen_infos, scr_info);
  
  return scr_info;
}

static char *
make_selection_name (MetaCompScreen *info)
{
  char *buffer;
  
  buffer = g_strdup_printf ("_NET_WM_CM_S%d", info->meta_screen->number);
  
  return buffer;
}

static void
on_selection_clear (WsWindow *window,
		    WsSelectionClearEvent *event,
		    gpointer data)
{
  MetaCompScreen *info = data;
  char *buffer = make_selection_name (info);
  
  if (strcmp (event->selection, buffer))
    {
      /* We lost the selection */
      meta_comp_screen_unredirect (info);
    }
}

static WsWindow *
claim_selection (MetaCompScreen *info)
{
  WsWindow *window = ws_window_new (info->root_window);
  char *buffer = make_selection_name (info);
  
#if 0
  g_print ("selection window: %lx\n", WS_RESOURCE_XID (window));
#endif
  
  ws_window_own_selection (window, buffer, WS_CURRENT_TIME);
  
  g_signal_connect (window, "selection_clear_event", G_CALLBACK (on_selection_clear), info);
  
  g_free (buffer);
  
  return window;
}

static void
queue_paint (CmNode *node,
	     MetaCompScreen *info)
{
#if 0
  g_print ("queueing %s\n", G_OBJECT_TYPE_NAME (node));
#endif
  meta_comp_screen_queue_paint (info);
}

void
meta_comp_screen_redirect (MetaCompScreen *info)
{
  WsWindow *root = ws_screen_get_root_window (info->screen);
  WsRectangle source;
  WsRectangle target;
  WsServerRegion *region;
  int screen_w;
  int screen_h;
  CmSquare *square;
  
#if 0
  g_print ("redirecting %lx\n", WS_RESOURCE_XID (root));
#endif
  
  ws_window_redirect_subwindows (root);
  info->gl_window = ws_screen_get_gl_window (info->screen);
  /* FIXME: This should probably happen in libcm */
  ws_window_set_override_redirect (info->gl_window, TRUE);
  region = ws_server_region_new (info->display);
  ws_window_set_input_shape (info->gl_window, region);
  g_object_unref (G_OBJECT (region));
  
  ws_display_begin_error_trap (info->display);
  
  ws_window_unredirect (info->gl_window);
  
  ws_display_end_error_trap (info->display);
  
  info->selection_window = claim_selection (info);
  
  ws_window_map (info->gl_window);
  
  info->stacker = cm_stacker_new ();
  
  square = cm_square_new (0.3, 0.3, 0.8, 1.0);
  
  cm_stacker_add_child (info->stacker, CM_NODE (square));
  
  g_object_unref (square);
  
  screen_w = ws_screen_get_width (info->screen);
  screen_h = ws_screen_get_height (info->screen);
  
#if 0
  g_print ("width: %d height %d\n", screen_w, screen_h);
#endif
  
  source.x = (screen_w - (screen_w / 4)) / 2;
  source.y = screen_h / 16;
  source.width = screen_w / 4;
  source.height = screen_h / 16;
  
  target.x = 0;
  target.y = screen_h - screen_h / 4;
  target.width = screen_w;
  target.height = screen_h / 4;
  
  info->magnifier = cm_magnifier_new (CM_NODE (info->stacker), &source, &target);
  
  if (g_getenv ("USE_MAGNIFIER"))
    cm_magnifier_set_active (info->magnifier, TRUE);
  else
    cm_magnifier_set_active (info->magnifier, FALSE);
  
  info->repaint_id =
    g_signal_connect (info->magnifier, "need_repaint",
		      G_CALLBACK (queue_paint), info);
  
  ws_display_sync (info->display);
}

static void
listify (gpointer key,
	 gpointer value,
	 gpointer data)
{
    GList **windows = data;

    *windows = g_list_prepend (*windows, (gpointer)value);
}

static void
free_all_windows (MetaCompScreen *info)
{
    GList *windows = NULL, *list;
    
    g_hash_table_foreach (info->windows_by_xid, listify, &windows);

    for (list = windows; list != NULL; list = list->next)
    {
	MetaCompWindow *window = list->data;

	meta_comp_window_free (window);
    }

    g_list_free (windows);
}

void
meta_comp_screen_unredirect (MetaCompScreen *info)
{
  WsScreen *ws_screen = info->screen;
  WsWindow *root = ws_screen_get_root_window (ws_screen);
  
  g_signal_handler_disconnect (info->magnifier, info->repaint_id);
  g_object_unref (info->magnifier);
  
  ws_window_unredirect_subwindows (root);
  ws_screen_release_gl_window (ws_screen);

  free_all_windows (info);
  
  ws_display_sync (info->display);
  
  /* FIXME: libcm needs a way to guarantee that a window is destroyed,
   * without relying on ref counting having it as a side effect
   */
  g_object_unref (info->selection_window);
}

void
meta_comp_screen_queue_paint (MetaCompScreen *info)
{
#if 0
  g_print ("queuing\n");
#endif
  if (!info->idle_id)
    info->idle_id = g_idle_add (repaint, info);
}

void
meta_comp_screen_restack (MetaCompScreen *info,
			  Window	  window,
			  Window	  above_this)
{
  MetaCompWindow *comp_window = find_comp_window (info, window);
  MetaCompWindow *above_comp_window = find_comp_window (info, above_this);
  CmNode *window_node = find_node (info, window);
  CmNode *above_node  = find_node (info, above_this);

  if ((comp_window && meta_comp_window_stack_frozen (comp_window)) ||
      (above_comp_window && meta_comp_window_stack_frozen (above_comp_window)))
  {
      return;
  }
  
#if 0
  dump_stacking_order (info->stacker->children);
#endif
  
  if (window_node == above_node)
    return;
  
  if (window_node && above_this == WS_RESOURCE_XID (info->gl_window))
    {
      cm_stacker_raise_child (info->stacker, window_node);
    }
  else if (window_node && above_this == None)
    {
      cm_stacker_lower_child (info->stacker, window_node);
    }
  else if (window_node && above_node)
    {
      cm_stacker_restack_child (info->stacker, window_node, above_node);
    }
#if 0
  else
    g_print ("nothing happened\n");
#endif
  
#if 0
  g_print ("done restacking; new order:\n");
#endif
#if 0
  dump_stacking_order (info->stacker->children);
#endif
  
}

void
meta_comp_screen_raise_window (MetaCompScreen  *info,
			       Window           window)
{
  CmNode *node = find_node (info, window);
  
  if (node)
    cm_stacker_raise_child (info->stacker, node);
}

void
meta_comp_screen_set_size (MetaCompScreen *info,
			   Window	   xwindow,
			   gint		   x,
			   gint		   y,
			   gint		   width,
			   gint		   height)
{
  MetaCompWindow *comp_window = meta_comp_window_lookup (info, xwindow);
  
  if (comp_window)
    {
      WsRectangle rect;
      
      rect.x = x;
      rect.y = y;
      rect.width = width;
      rect.height = height;
      
      meta_comp_window_set_size (comp_window, &rect);
    }
}

static void
print_child_titles (WsWindow *window)
{
  GList *children = ws_window_query_subwindows (window);
  GList *list;
  int i;
  
  g_print ("window: %lx %s\n", WS_RESOURCE_XID (window), ws_window_query_title (window));
  
  i = 0;
  for (list = children; list != NULL; list = list->next)
    {
      WsWindow *child = list->data;
      
      g_print ("  %d adding: %lx %s\n", i++, WS_RESOURCE_XID (child), ws_window_query_title (child));
    }
}

typedef struct
{
    MetaCompScreen *cscreen;
    XID		    xid;
} DestroyData;

static void
on_window_destroy (MetaCompWindow *comp_window,
		   gpointer        closure)
{
    DestroyData *data = closure;
    CmNode *node = meta_comp_window_get_node (comp_window);
    
    cm_stacker_remove_child (data->cscreen->stacker, node);
    g_hash_table_remove (data->cscreen->windows_by_xid, (gpointer)data->xid);
}

void
meta_comp_screen_add_window (MetaCompScreen *info,
			     Window	     xwindow)
{
  WsDrawable *drawable;
  MetaCompWindow *comp_window;
  DestroyData *data;
  
  ws_display_begin_error_trap (info->display);
  
  comp_window = meta_comp_window_lookup (info, xwindow);
  
  if (comp_window)
    goto out;
  
  drawable = WS_DRAWABLE (ws_window_lookup (info->display, xwindow));
  
  if (ws_window_query_input_only (WS_WINDOW (drawable)))
    goto out;
  
  if (WS_WINDOW (drawable) == info->gl_window ||
      WS_WINDOW (drawable) == info->screen->overlay_window)
    {
#if 0
      g_print ("gl window\n");
#endif
      goto out;
    }

  data = g_new (DestroyData, 1);
  data->cscreen = info;
  data->xid = WS_RESOURCE_XID (drawable);
  
  comp_window = meta_comp_window_new (info->meta_screen, drawable,
				      on_window_destroy, data);
  
  g_hash_table_insert (info->windows_by_xid, (gpointer)WS_RESOURCE_XID (drawable), comp_window);
  
  cm_stacker_add_child (info->stacker, meta_comp_window_get_node (comp_window));
  
 out:
  if (comp_window)
  {
      /* This function is called both when windows are created and when they
       * are mapped, so for now we have this silly function.
       */
      meta_comp_window_refresh_attrs (comp_window);
  }
  
  ws_display_end_error_trap (info->display);
  
#if 0
  g_print ("done checking\n");
#endif
  
  return;
}


void
meta_comp_screen_remove_window (MetaCompScreen *info,
				Window	        xwindow)
{
  MetaCompWindow *comp_window = meta_comp_window_lookup (info, xwindow);
  
  if (comp_window)
      meta_comp_window_free (comp_window);
}

void
meta_comp_screen_set_updates (MetaCompScreen *info,
			      Window	      xwindow,
			      gboolean	      updates)
{
  MetaCompWindow *comp_window = meta_comp_window_lookup (info, xwindow);
  
  meta_comp_window_set_updates (comp_window, updates);
}


void
meta_comp_screen_set_patch (MetaCompScreen *info,
			    Window	    xwindow,
			    CmPoint         points[4][4])
{
  CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
  
  if (node)
    cm_drawable_node_set_patch (node, points);
}

void
meta_comp_screen_unset_patch (MetaCompScreen *info,
			      Window	      xwindow)
{
  CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
  
  if (node)
    cm_drawable_node_unset_patch (node);
}

void
meta_comp_screen_set_alpha (MetaCompScreen *info,
			    Window	xwindow,
			    gdouble alpha)
{
  CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
#if 0
  g_print ("alpha: %f\n", alpha);
#endif
  cm_drawable_node_set_alpha (node, alpha);
}

void
meta_comp_screen_get_real_size (MetaCompScreen *info,
				Window xwindow,
				WsRectangle *size)
{
  CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
  
  if (!size)
    return;
  
  cm_drawable_node_get_clipbox (node, size);
}

void
meta_comp_screen_unmap (MetaCompScreen *info,
			Window		xwindow)
{
    MetaCompWindow *window = find_comp_window (info, xwindow);

    if (window)
	meta_comp_window_hide (window);
}

void
meta_comp_screen_set_target_rect (MetaCompScreen *info,
				  Window xwindow,
				  WsRectangle *rect)
{
  CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
  
  if (node)
    cm_drawable_node_set_scale_rect (node, rect);
}

#endif

