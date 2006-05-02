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

#include <X11/Xlib.h>
#include <glib.h>
#include <cm/ws.h>
#include <cm/wsint.h>
#include <cm/node.h>
#include <cm/drawable-node.h>
#include <string.h>

#include "c-window.h"

static GHashTable *windows_by_xid;

struct _MetaCompWindow
{
    WsDrawable *drawable;
    CmNode     *node;
    gboolean	updates;
    
    WsRectangle size;
};

static void
ensure_hash_table (void)
{
    if (!windows_by_xid)
    {
	windows_by_xid = g_hash_table_new (
	    g_direct_hash, g_direct_equal);
    }
}

MetaCompWindow *
meta_comp_window_new (WsDrawable *drawable)
{
    MetaCompWindow *window;
    WsRectangle geometry;
    
    ws_drawable_query_geometry (drawable, &geometry);
    
    window = g_new0 (MetaCompWindow, 1);
    
    window->drawable = g_object_ref (drawable);
    window->node = CM_NODE (cm_drawable_node_new (drawable, &geometry));
    window->updates = TRUE;
    
    ensure_hash_table ();
    
    g_hash_table_insert (windows_by_xid, (gpointer)WS_RESOURCE_XID (window->drawable), window);
    
    return window;
}

MetaCompWindow *
meta_comp_window_lookup (Window xid)
{
    MetaCompWindow *window;
    
    ensure_hash_table ();
    
    window = g_hash_table_lookup (windows_by_xid, (gpointer)xid);
    
    return window;
}

void
meta_comp_window_free (MetaCompWindow *window)
{
    ensure_hash_table ();

    g_hash_table_remove (windows_by_xid, WS_RESOURCE_XID (window->drawable));
    g_object_unref (window->drawable);
    g_object_unref (window->node);
    g_free (window);
}

void
meta_comp_window_set_size (MetaCompWindow *comp_window,
			   WsRectangle    *rect)
{
    if (comp_window->updates)
    {
	WsWindow *window = WS_WINDOW (comp_window->drawable);
	WsDisplay *display = WS_RESOURCE (window)->display;
	CmDrawableNode *dnode = CM_DRAWABLE_NODE (comp_window->node);
	WsRegion *shape;
	
	ws_display_begin_error_trap (display);
	
	cm_drawable_node_set_geometry (dnode, rect);
	shape = ws_window_get_output_shape (window);
	cm_drawable_node_set_shape (dnode, shape);
	ws_region_destroy (shape);
	
	if (rect->width != comp_window->size.width ||
	    rect->height != comp_window->size.height)
	{
	    cm_drawable_node_update_pixmap (dnode);
	}
	
	comp_window->size = *rect;
	
	ws_display_end_error_trap (display);
    }
}

static gboolean
has_type (WsWindow *window, const char *check_type)
{
    gchar **types = ws_window_get_property_atom_list (window, "_NET_WM_WINDOW_TYPE");
    int i;
    gboolean result;

    if (!types)
	return FALSE;

    result = FALSE;
    
    for (i = 0; types[i] != NULL; ++i)
    {
	gchar *type = types[i];

	g_print ("type: %s\n", type);
	
	if (strcmp (type, check_type) == 0)
	{
	    result = TRUE;
	    break;
	}
    }

    g_strfreev (types);
    return result;
}

void
meta_comp_window_refresh_attrs (MetaCompWindow *comp_window)
{
    /* FIXME: this function should not exist - the real problem is
     * probably in meta_screen_info_add_window() where it it called.
     */
    
    double alpha = 1.0;
    CmDrawableNode *node = CM_DRAWABLE_NODE (comp_window->node);
    
    if (ws_window_query_mapped (WS_WINDOW (comp_window->drawable)))
    {
	WsWindow *window = WS_WINDOW (comp_window->drawable);
	
	cm_drawable_node_unset_patch (CM_DRAWABLE_NODE (node));

	if (has_type (window, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"))
	{
	    alpha = 0.3;
	}
	else if (has_type (window, "_NET_WM_WINDOW_TYPE_POPUP_MENU"))
	{
	    alpha = 0.9;
	}
	else
	{
	    alpha = 1.0;
	}
	
	cm_drawable_node_set_alpha (node, alpha);
	cm_drawable_node_set_viewable (node, TRUE);
	cm_drawable_node_update_pixmap (node);
    }
    else
    {
	cm_drawable_node_set_viewable (node, FALSE);
    }
}

void
meta_comp_window_set_updates (MetaCompWindow *comp_window,
			      gboolean	      updates)
{
    CmDrawableNode *node = CM_DRAWABLE_NODE (comp_window->node);

    comp_window->updates = updates;

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

CmNode *
meta_comp_window_get_node (MetaCompWindow *comp_window)
{
    return comp_window->node;
}
   

#endif
