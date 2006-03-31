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

#include "screen.h"
#include "c-screen.h"

typedef struct WindowInfo WindowInfo;

struct WindowInfo
{
    Window	xwindow;
    CmNode     *node;
    gboolean	updates;

    WsRectangle size;
};

struct MetaScreenInfo
{
    WsDisplay *display;
    CmStacker *stacker;
    CmMagnifier *magnifier;
    
    WsWindow *gl_window;
    
    WsScreen *screen;
    MetaScreen *meta_screen;
    
    GHashTable *window_infos_by_xid;
    
    int repaint_id;
    int idle_id;
};

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
    MetaScreenInfo *info = data;
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

static WindowInfo *
find_win_info (MetaScreenInfo *info,
	       Window	       xwindow)
{
    WindowInfo *win_info =
	g_hash_table_lookup (info->window_infos_by_xid, (gpointer)xwindow);
    
    return win_info;
}

static CmNode *
find_node (MetaScreenInfo *info,
	   Window	   xwindow)
{
    WindowInfo *win_info = find_win_info (info, xwindow);
    
    if (win_info)
	return win_info->node;
    
    return NULL;
}

static GList *all_screen_infos;

MetaScreenInfo *
meta_screen_info_get_by_xwindow (Window xwindow)
{
    GList *list;
    
    for (list = all_screen_infos; list != NULL; list = list->next)
    {
	MetaScreenInfo *info = list->data;
	
	if (find_node (info, xwindow))
	    return info;
    }
    
    return NULL;
}

MetaScreenInfo *
meta_screen_info_new (WsDisplay *display,
		      MetaScreen *screen)
{
    MetaScreenInfo *scr_info = g_new0 (MetaScreenInfo, 1);
    WsServerRegion *region;
    
    scr_info->screen = ws_display_get_screen_from_number (
	display, screen->number);
    scr_info->display = display;
    scr_info->window_infos_by_xid =
	g_hash_table_new_full (g_direct_hash, g_direct_equal,
			       NULL, g_free);
    scr_info->meta_screen = screen;
    
    all_screen_infos = g_list_prepend (all_screen_infos, scr_info);
    
    return scr_info;
}

static gboolean
claim_selection (MetaScreenInfo *info)
{
    /* FIXME:
     *
     * The plan here is to
     *
     *    - Add Selections and Properties as first class objects
     *      in WS
     *
     *    - Use those to
     *          - claim the selection
     *          - back off if someone else claims the selection
     *          - back back in if that someone else disappears
     *
     */
    Display *xdisplay;
    char *buffer;
    Atom atom;
    Window current_cm_sn_owner;
    WsWindow *new_cm_sn_owner;
    
    xdisplay = info->meta_screen->display->xdisplay;
    
    buffer = g_strdup_printf ("CM_S%d", info->meta_screen->number);
    
    atom = XInternAtom (xdisplay, buffer, False);
    
    current_cm_sn_owner = XGetSelectionOwner (xdisplay, atom);
    
    if (current_cm_sn_owner != None)
    {
	return FALSE;
    }
    
    new_cm_sn_owner = ws_screen_get_root_window (info->screen);
    
    XSetSelectionOwner (xdisplay, atom,
			WS_RESOURCE_XID (new_cm_sn_owner),
			CurrentTime);
    
    return TRUE;
}

static void
queue_paint (CmNode *node,
	     MetaScreenInfo *info)
{
#if 0
    g_print ("queueing %s\n", G_OBJECT_TYPE_NAME (node));
#endif
    meta_screen_info_queue_paint (info);
}

void
meta_screen_info_redirect (MetaScreenInfo *info)
{
    WsWindow *root = ws_screen_get_root_window (info->screen);
    WsRectangle source;
    WsRectangle target;
    WsServerRegion *region;
    int screen_w;
    int screen_h;
    
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
    
    claim_selection (info);
    
    ws_window_map (info->gl_window);
    
    info->stacker = cm_stacker_new ();

    cm_stacker_add_child (info->stacker, cm_square_new (0.3, 0.3, 0.8, 1.0));

    screen_w = ws_screen_get_width (info->screen);
    screen_h = ws_screen_get_height (info->screen);

    g_print ("width: %d height %d\n", screen_w, screen_h);
    
    source.x = (screen_w - (screen_w / 4)) / 2;
    source.y = screen_h / 16;
    source.width = screen_w / 4;
    source.height = screen_h / 16;
    
    target.x = 0;
    target.y = screen_h - screen_h / 4;
    target.width = screen_w;
    target.height = screen_h / 4;
    
    info->magnifier = cm_magnifier_new (info->stacker, &source, &target);
    cm_magnifier_set_active (info->magnifier, TRUE);
    
    info->repaint_id =
	g_signal_connect (info->magnifier, "need_repaint",
			  G_CALLBACK (queue_paint), info);
    
    ws_display_sync (info->display);
}

void
meta_screen_info_unredirect (MetaScreenInfo *info)
{
    WsScreen *ws_screen = info->screen;
    WsWindow *root = ws_screen_get_root_window (ws_screen);
    
#if 0
    g_print ("unredirecting %lx\n", WS_RESOURCE_XID (root));
#endif
    
    g_signal_handler_disconnect (info->stacker, info->repaint_id);
    g_object_unref (info->stacker);
    
    ws_window_unredirect_subwindows (root);
    ws_window_unmap (info->gl_window);
    
    ws_display_sync (info->display);
}

void
meta_screen_info_queue_paint (MetaScreenInfo *info)
{
#if 0
    g_print ("queuing\n");
#endif
    if (!info->idle_id)
	info->idle_id = g_idle_add (repaint, info);
}

void
meta_screen_info_restack (MetaScreenInfo *info,
			  Window	  window,
			  Window	  above_this)
{
    CmNode *window_node = find_node (info, window);
    CmNode *above_node  = find_node (info, above_this);
    
#if 0
    g_print ("restack %lx over %lx \n", window, above_this);
#endif
    
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
    else
	g_print ("nothing happened\n");
    
#if 0
    g_print ("done restacking; new order:\n");
#endif
#if 0
    dump_stacking_order (info->stacker->children);
#endif
    
}

void
meta_screen_info_raise_window (MetaScreenInfo  *info,
			       Window           window)
{
    CmNode *node = find_node (info, window);
    
    if (node)
	cm_stacker_raise_child (info->stacker, node);
}

void
meta_screen_info_set_size (MetaScreenInfo *info,
			   Window	   window,
			   gint		   x,
			   gint		   y,
			   gint		   width,
			   gint		   height)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, window));
    WindowInfo *winfo = find_win_info (info, window);
    WsRectangle rect;
    
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    
    if (node)
    {
	WsRegion *shape;
	
	if (winfo && winfo->updates)
	{
	    WsWindow *window = WS_WINDOW (node->drawable);
	    WsDisplay *display = WS_RESOURCE (window)->display;

	    ws_display_begin_error_trap (display);

#if 0
	    g_print ("meta screen info set: %d %d %d %d\n",
		     x, y, width, height);
#endif
	    
	    cm_drawable_node_set_geometry (CM_DRAWABLE_NODE (node), &rect);
	    shape = ws_window_get_output_shape (window);
	    cm_drawable_node_set_shape (node, shape);
	    ws_region_destroy (shape);

	    if (rect.width != winfo->size.width ||
		rect.height != winfo->size.height)
	    {
		cm_drawable_node_update_pixmap (node);
	    }

	    winfo->size = rect;

	    ws_display_end_error_trap (display);
	}
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

static WindowInfo *
window_info_new (Window xwindow,
		 CmNode *node)
{
    WindowInfo *win_info = g_new0 (WindowInfo, 1);
    win_info->xwindow = xwindow;
    win_info->node = node;
    win_info->updates = TRUE;
    return win_info;
}

void
meta_screen_info_add_window (MetaScreenInfo *info,
			     Window	     xwindow)
{
    CmNode *node;
    WsDrawable *drawable;
    WsRectangle geometry;
    
    ws_display_begin_error_trap (info->display);
    
    node = find_node (info, xwindow);
    drawable = WS_DRAWABLE (ws_window_lookup (info->display, xwindow));
    
    if (node)
	goto out;
    
    if (ws_window_query_input_only (WS_WINDOW (drawable)))
	goto out;

    if (WS_WINDOW (drawable) == info->gl_window ||
	WS_WINDOW (drawable) == info->screen->overlay_window)
    {
	g_print ("gl window\n");
	goto out;
    }
    
    ws_drawable_query_geometry (drawable, &geometry);
    
    node = CM_NODE (cm_drawable_node_new (drawable, &geometry));

    cm_drawable_node_set_alpha (node, 1.0);
    
#if 0
    print_child_titles (WS_WINDOW (drawable));
#endif
    
    cm_stacker_add_child (info->stacker, node);
    
    g_hash_table_insert (info->window_infos_by_xid,
			 (gpointer)xwindow,
			 window_info_new (xwindow, node));
    
    g_object_unref (node);    
out:
    if (node)
    {
#if 0
	g_print ("drawable %lx is now ", WS_RESOURCE_XID (drawable));
#endif
	if (ws_window_query_mapped (WS_WINDOW (drawable)))
	{
#if 0
	    g_print ("mapped\n");
#endif
	    cm_drawable_node_unset_patch (node);
	    cm_drawable_node_set_alpha (node, 1.0);
	    cm_drawable_node_set_viewable (node, TRUE);
	    cm_drawable_node_update_pixmap (node);
	}
	else
	{
#if 0
	    g_print ("unmapped\n");
#endif
	    cm_drawable_node_set_viewable (node, FALSE);
	}
    }
    
    ws_display_end_error_trap (info->display);
    
#if 0
    g_print ("done checking\n");
#endif
    
    return;
}


void
meta_screen_info_remove_window (MetaScreenInfo *info,
				Window	        xwindow)
{
    CmNode *node = find_node (info, xwindow);
    
#if 0
    g_print ("removing %lx\n", xwindow);
#endif

    if (node)
    {
	g_hash_table_remove (info->window_infos_by_xid, (gpointer)xwindow);
	
	cm_stacker_remove_child (info->stacker, node);
    }
}

void
meta_screen_info_set_updates (MetaScreenInfo *info,
			      Window	      xwindow,
			      gboolean	      updates)
{
    WindowInfo *win_info = find_win_info (info, xwindow);
    CmDrawableNode *node = CM_DRAWABLE_NODE (win_info->node);

#if 0
    g_print ("setting updates to %s\n", updates? "on" : "off");
#endif
    
    win_info->updates = updates;
    
    if (node)
	cm_drawable_node_set_updates (node, updates);
    
    if (updates)
    {
	WsRectangle rect;
	WsRegion *shape;
	WsDisplay *display = WS_RESOURCE (node->drawable)->display;
	
	ws_display_begin_error_trap (display);
	ws_drawable_query_geometry (node->drawable, &rect);
	cm_drawable_node_update_pixmap (node);
	cm_drawable_node_set_geometry (node, &rect);
	shape = ws_window_get_output_shape (WS_WINDOW (node->drawable));
	cm_drawable_node_set_shape (node, shape);
	ws_region_destroy (shape);
	ws_display_end_error_trap (display);
    }
}


void
meta_screen_info_set_patch (MetaScreenInfo *info,
			    Window	    xwindow,
			    CmPoint         points[4][4])
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
    
    if (node)
	cm_drawable_node_set_patch (node, points);
}

void
meta_screen_info_unset_patch (MetaScreenInfo *info,
			      Window	      xwindow)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
    
    if (node)
	cm_drawable_node_unset_patch (node);
}

void
meta_screen_info_set_alpha (MetaScreenInfo *info,
			    Window	xwindow,
			    gdouble alpha)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
    cm_drawable_node_set_alpha (node, alpha);
}

void
meta_screen_info_get_real_size (MetaScreenInfo *info,
				Window xwindow,
				WsRectangle *size)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
    
    if (!size)
	return;
    
    cm_drawable_node_get_clipbox (node, size);
}

void
meta_screen_info_unmap (MetaScreenInfo *info,
			Window		xwindow)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
    
#if 0
    g_print ("unmapping: %lx\n", xwindow);
#endif
    
    if (node)
	cm_drawable_node_set_viewable (node, FALSE);
}

void
meta_screen_info_set_target_rect (MetaScreenInfo *info,
				  Window xwindow,
				  WsRectangle *rect)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));

    if (node)
	cm_drawable_node_set_scale_rect (node, rect);
}

void
meta_screen_info_set_explode (MetaScreenInfo *info,
			      Window xwindow,
			      gdouble level)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));

    if (node)
    {
#if 0
	g_print ("level: %f\n", level);
#endif
    
	cm_drawable_node_set_explosion_level (node, level);
    }
}

void
meta_screen_info_hide_window (MetaScreenInfo *info,
			      Window          xwindow)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));

    cm_drawable_node_set_viewable (node, FALSE);
}

#endif

