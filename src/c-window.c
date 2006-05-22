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
#include <math.h>

#include "effects.h"
#include "c-window.h"
#include "window.h"

struct _MetaCompWindow
{
    MetaDisplay *display;
    WsDrawable *drawable;
    WsPixmap   *pixmap;
    CmNode     *node;
    gboolean	updates;
    WsSyncAlarm *alarm;
    
    WsRectangle size;
    gboolean waiting_for_paint;
};

static Window
find_app_window (MetaCompWindow *comp_window)
{
    Window xwindow = WS_RESOURCE_XID (comp_window->drawable);
    MetaWindow *meta_window =
	meta_display_lookup_x_window (comp_window->display, xwindow);
    
    if (meta_window)
	return meta_window->xwindow;
    else
	return xwindow;
}

static WsPixmap *
take_snapshot (WsDrawable *drawable)
{
    WsDisplay *display = WS_RESOURCE (drawable)->display;
    WsRectangle geometry;
    WsPixmap *pixmap;
    
    ws_display_begin_error_trap (display);
    
    ws_drawable_query_geometry (drawable, &geometry);
    
    pixmap = ws_pixmap_new (drawable, geometry.width, geometry.height);
    
    ws_drawable_copy_area (drawable, 0, 0, geometry.width, geometry.height,
			   WS_DRAWABLE (pixmap), 0, 0,
			   NULL);
    
    ws_display_end_error_trap (display);
    
    return pixmap;
}

static void
on_alarm (WsSyncAlarm *alarm,
	  WsAlarmNotifyEvent *event,
	  MetaCompWindow *window)
{
    if (window->pixmap)
	g_object_unref (window->pixmap);
    
    window->pixmap = take_snapshot (window->drawable);
    
    ws_sync_alarm_set (window->alarm, event->counter_value + 2);
    ws_sync_counter_change (event->counter, 1);
}

static gboolean
has_counter (MetaCompWindow *comp_window)
{
    Window xwindow = find_app_window (comp_window);
    WsDisplay *display = WS_RESOURCE (comp_window->drawable)->display;
    WsWindow *window = ws_window_lookup (display, xwindow);
    WsSyncCounter *counter;
    
    ws_display_init_sync (display);
    
    counter = ws_window_get_property_sync_counter (
	window, "_NET_WM_FINISH_FRAME_COUNTER");
    
    if (counter)
    {
	WsSyncAlarm *alarm;
	gint64 value = ws_sync_counter_query_value (counter);
	
	g_print ("counter value %lld\n", ws_sync_counter_query_value (counter));
	alarm = ws_sync_alarm_new (display, counter);
	
	g_signal_connect (alarm, "alarm_notify_event",
			  G_CALLBACK (on_alarm), comp_window);
	
	if (value % 2 == 1)
	{
	    ws_sync_alarm_set (alarm, value + 2);
	    
	    g_print ("wait for %lld\n", value + 2);
	    
	    g_print ("increasing counter\n");
	    ws_sync_counter_change (counter, 1);
	    
	    g_print ("counter value %lld\n", ws_sync_counter_query_value (counter));
	}
	else
	{
	    g_print ("wait for %lld\n", value + 1);
	    ws_sync_alarm_set (alarm, value + 1);
	}
	
	comp_window->alarm = alarm;
	
    }
    
    if (counter)
	return TRUE;
    else
	return FALSE;
    
#if 0
    if (counter)
    {
	g_print ("found counter %lx on %lx\n",
		 WS_RESOURCE_XID (counter),
		 WS_RESOURCE_XID (window));
    }
    else
    {
	g_print ("no counter found for %lx\n", WS_RESOURCE_XID (window));
    }
#endif
    
    return TRUE;
}

MetaCompWindow *
meta_comp_window_new (MetaDisplay    *display,
		      WsDrawable     *drawable)
{
    MetaCompWindow *window;
    WsRectangle geometry;
    
    ws_drawable_query_geometry (drawable, &geometry);
    
    window = g_new0 (MetaCompWindow, 1);
    
    window->display = display;
    window->drawable = g_object_ref (drawable);
    window->node = CM_NODE (cm_drawable_node_new (drawable, &geometry));
    window->updates = TRUE;
    
    return window;
}

void
meta_comp_window_free (MetaCompWindow *window)
{
    g_object_unref (window->drawable);
    g_object_unref (window->node);
    if (window->alarm)
	g_object_unref (window->alarm);
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
	
	if (strcmp (type, check_type) == 0)
	{
	    result = TRUE;
	    break;
	}
    }
    
    g_strfreev (types);
    return result;
}

static MetaWindow *
find_meta_window (MetaCompWindow *comp_window)
{
    Window xwindow = WS_RESOURCE_XID (comp_window->drawable);
    MetaWindow *window =
	meta_display_lookup_x_window (comp_window->display, xwindow);
    
    return window;
}

void
meta_comp_window_refresh_attrs (MetaCompWindow *comp_window)
{
    /* FIXME: this function should not exist - the real problem is
     * probably in meta_screen_info_add_window() where it it called.
     */
    
    double alpha = 1.0;
    CmDrawableNode *node = CM_DRAWABLE_NODE (comp_window->node);
    
    if (ws_window_query_mapped (WS_WINDOW (comp_window->drawable)) &&
	!comp_window->waiting_for_paint)
    {
	WsWindow *window = WS_WINDOW (comp_window->drawable);
	
	cm_drawable_node_unset_patch (CM_DRAWABLE_NODE (node));
	
	find_meta_window (comp_window);
	
	if (has_type (window, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"))
	{
	    alpha = 0.9;
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

#if 0
	if (cm_drawable_node_get_viewable (node))
	    g_print ("mapping new window\n");
#endif
	
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


/*
 * Explosion effect
 */
#define EXPLODE_TIME 1.0

#define BASE 0.5

static double
transform (double in)
{
    return (pow (BASE, in) - 1) / (BASE - 1);
}

typedef struct
{
    MetaEffect		       *effect;
    MetaCompWindow	       *window;
    gdouble			level;
    GTimer *			timer;
} ExplodeInfo;

static gboolean
update_explosion (gpointer data)
{
    ExplodeInfo *info = data;
    CmDrawableNode *node = (CmDrawableNode *)info->window->node;
    gdouble elapsed = g_timer_elapsed (info->timer, NULL);

    if (elapsed > EXPLODE_TIME)
    {
	meta_effect_end (info->effect);
	
	cm_drawable_node_set_viewable (node, FALSE);
	cm_drawable_node_set_explosion_level (node, 0.0);
	return FALSE;
    }
    else
    {
	gdouble t = elapsed / EXPLODE_TIME;
	
	cm_drawable_node_set_explosion_level (node,
					      transform (t));
	return TRUE;
    }
}

void
meta_comp_window_explode (MetaCompWindow *comp_window,
			  MetaEffect *effect)
{
    ExplodeInfo *info = g_new0 (ExplodeInfo, 1);

    info->window = comp_window;
    info->effect = effect;
    info->level = 0.0;
    info->timer = g_timer_new ();

    g_idle_add (update_explosion, info);
}

#endif

