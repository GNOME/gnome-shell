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

#include "screen.h"
#include "c-screen.h"

struct MetaScreenInfo
{
    WsDisplay *display;
    CmStacker *stacker;
    
    WsWindow *gl_window;
    
    WsScreen *screen;
    MetaScreen *meta_screen;
    
    GHashTable *nodes_by_xid;

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
    glViewport (0, 0,
		info->meta_screen->rect.width,
		info->meta_screen->rect.height);
    
    glClearColor (1.0, 0.8, 0.8, 0.0);
    glClear (GL_COLOR_BUFFER_BIT);
    
    ws_window_raise (info->gl_window);
    
    state = cm_state_new ();
    
    cm_state_disable_depth_buffer_update (state);
    
    cm_node_render (CM_NODE (info->stacker), state);
    
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

static CmNode *
find_node (MetaScreenInfo *info,
	   Window	   xwindow)
{
    CmNode *node;
    
    node = g_hash_table_lookup (info->nodes_by_xid, (gpointer)xwindow);
    
    return node;
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
    scr_info->gl_window = ws_screen_get_gl_window (scr_info->screen);
    scr_info->nodes_by_xid = g_hash_table_new (g_direct_hash, g_direct_equal);
    scr_info->meta_screen = screen;
    
    /* FIXME: This should probably happen in libcm */
    ws_window_set_override_redirect (scr_info->gl_window, TRUE);
    region = ws_server_region_new (scr_info->display);
    ws_window_set_input_shape (scr_info->gl_window, region);
    g_object_unref (G_OBJECT (region));
    
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

void
meta_screen_info_redirect (MetaScreenInfo *info)
{
    WsWindow *root = ws_screen_get_root_window (info->screen);

#if 0
    g_print ("redirecting %lx\n", WS_RESOURCE_XID (root));
#endif
    
    ws_window_redirect_subwindows (root);
    ws_window_unredirect (info->gl_window);
    
    claim_selection (info);
    
    ws_window_map (info->gl_window);
    
    info->stacker = cm_stacker_new ();
    
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
    CmNode *node = find_node (info, window);
    
    if (node)
	cm_drawable_node_set_geometry (CM_DRAWABLE_NODE (node),
				       x, y, width, height);
}

static void
queue_paint (CmStacker *stacker,
	     MetaScreenInfo *info)
{
    meta_screen_info_queue_paint (info);
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

void
meta_screen_info_add_window (MetaScreenInfo *info,
			     Window	     xwindow)
{
    CmNode *node;
    WsDrawable *drawable;
    
    ws_display_begin_error_trap (info->display);
    
    node = find_node (info, xwindow);
    drawable = WS_DRAWABLE (ws_window_lookup (info->display, xwindow));

    if (node)
	goto out;

    if (ws_window_query_input_only (WS_WINDOW (drawable)))
	goto out;
    
    node = CM_NODE (cm_drawable_node_new (drawable));
    
#if 0
    print_child_titles (WS_WINDOW (drawable));
#endif
    
    cm_stacker_add_child (info->stacker, node);
    
    g_hash_table_insert (info->nodes_by_xid, (gpointer)xwindow, node);
    g_object_unref (node);    
    
    info->repaint_id =
	g_signal_connect (info->stacker, "need_repaint",
			  G_CALLBACK (queue_paint), info);
    
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
	    cm_drawable_node_unset_geometry (node);
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

    g_print ("removing %lx\n", xwindow);
    
    g_hash_table_remove (info->nodes_by_xid, (gpointer)xwindow);
    
    cm_stacker_remove_child (info->stacker, node);
}

void
meta_screen_info_set_updates (MetaScreenInfo *info,
			      Window	      xwindow,
			      gboolean	      updates)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (find_node (info, xwindow));
    
    if (node)
	cm_drawable_node_set_updates (node, updates);
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
	cm_drawable_node_unset_geometry (node);
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
    
    size->x = node->real_x;
    size->y = node->real_y;
    size->width = node->real_width;
    size->height = node->real_height;
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

#endif
