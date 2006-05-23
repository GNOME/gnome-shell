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

    gint64	counter_value;
    gint        ref_count;

    gboolean    animation_in_progress;
    gboolean	hide_after_animation;
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
	
#if 0
	g_print ("counter value %lld\n", ws_sync_counter_query_value (counter));
	alarm = ws_sync_alarm_new (WS_RESOURCE (comp_window->drawable)->display,
				   
				   display, counter);
#endif
	
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

static void
show_node (MetaCompWindow *comp_window)
{
    if (comp_window->animation_in_progress)
	comp_window->hide_after_animation = FALSE;
    
    cm_drawable_node_set_viewable (CM_DRAWABLE_NODE (comp_window->node), TRUE);
    cm_drawable_node_update_pixmap (CM_DRAWABLE_NODE (comp_window->node));
}

static void
hide_node (MetaCompWindow *comp_window)
{
    if (comp_window->animation_in_progress)
    {
	comp_window->hide_after_animation = TRUE;
	return;
    }

    g_print ("hide %p\n", comp_window->node);
    cm_drawable_node_set_viewable (CM_DRAWABLE_NODE (comp_window->node), FALSE);
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
    window->counter_value = 1;
    window->ref_count = 1;

    hide_node (window);
    
    return window;
}

static MetaCompWindow *
comp_window_ref (MetaCompWindow *comp_window)
{
    comp_window->ref_count++;

    return comp_window;
}

static void
comp_window_unref (MetaCompWindow *comp_window)
{
    if (--comp_window->ref_count == 0)
    {
	g_object_unref (comp_window->drawable);
	g_object_unref (comp_window->node);
	if (comp_window->alarm)
	    g_object_unref (comp_window->alarm);
	g_free (comp_window);
    }
}

void
meta_comp_window_free (MetaCompWindow *window)
{
    comp_window_unref (window);
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

static void
send_configure_notify (WsDrawable *drawable)
{
    WsWindow *window = WS_WINDOW (drawable);
    WsRectangle geo;

    ws_drawable_query_geometry (drawable, &geo);

#if 0
    g_print ("sending configure notify %d %d %d %d\n",
	     geo.x, geo.y, geo.width, geo.height);
#endif
    
    ws_window_send_configure_notify (
	window, geo.x, geo.y, geo.width, geo.height,
	0 /* border width */, ws_window_query_override_redirect (window));
}

static WsWindow *
find_client_window (MetaCompWindow *comp_window)
{
    MetaWindow *meta_window = find_meta_window (comp_window);

    if (meta_window && meta_window->frame)
    {
	WsDisplay *ws_display = WS_RESOURCE (comp_window->drawable)->display;

#if 0
	g_print ("framed window (client: %lx)\n", WS_RESOURCE_XID (comp_window->drawable));
#endif

#if 0
	g_print ("framed window (client: %lx\n", meta_window->xwindow);
#endif

#if 0
	g_print ("framed window: %p\n", comp_window);
#endif
	return ws_window_lookup (ws_display, meta_window->xwindow);
    }
    else
    {
#if 0
	if (meta_window)
	    g_print ("window not framed, but managed (%p)\n", comp_window);
	else
	    g_print ("no meta window %p\n", comp_window);
#endif
	
	return WS_WINDOW (comp_window->drawable);
    }
}

static gboolean
frameless_managed (MetaCompWindow *comp_window)
{
    /* For some reason frameless, managed windows don't respond to
     * sync requests messages. FIXME: at some point need to find out
     * what's going on
     */

    MetaWindow *mw = find_meta_window (comp_window);

    return mw && !mw->frame;
}

static void
on_request_alarm (WsSyncAlarm *alarm,
		  WsAlarmNotifyEvent *event,
		  MetaCompWindow *comp_window)
{
    /* This alarm means that the window is ready to be shown on screen */

#if 0
    g_print ("alarm for %p\n", comp_window);
#endif
    
    show_node (comp_window);

    g_object_unref (alarm);
}

static gboolean
send_sync_request (MetaCompWindow *comp_window)
{
    WsDisplay *display;
    WsWindow *client_window = find_client_window (comp_window);
    WsSyncCounter *request_counter;
    WsSyncAlarm *alarm;
    guint32 msg[5];
    display = WS_RESOURCE (comp_window->drawable)->display;
    ws_display_init_sync (display);
    
    if (!client_window)
	return FALSE;
    
    request_counter = ws_window_get_property_sync_counter (
	client_window, "_NET_WM_SYNC_REQUEST_COUNTER");

    if (!request_counter)
	return FALSE;
    
    comp_window->counter_value = ws_sync_counter_query_value (request_counter) + 1;
    
    msg[0] = comp_window->display->atom_net_wm_sync_request;
    msg[1] = meta_display_get_current_time (comp_window->display);
    msg[2] = comp_window->counter_value & 0xffffffff;
    msg[3] = (comp_window->counter_value >> 32) & 0xffffffff;
    
    alarm = ws_sync_alarm_new (display, request_counter);

    ws_sync_alarm_set (alarm, comp_window->counter_value);

    g_signal_connect (alarm, "alarm_notify_event",
		      G_CALLBACK (on_request_alarm), comp_window);

    ws_window_send_client_message (client_window,
				   "WM_PROTOCOLS", msg);

    send_configure_notify (WS_DRAWABLE (client_window));

    ws_display_flush (WS_RESOURCE (client_window)->display);
    
    return TRUE;
}

void
meta_comp_window_refresh_attrs (MetaCompWindow *comp_window)
{
    /* FIXME: this function should not exist - the real problem is
     * probably in meta_screen_info_add_window() where it it called.
     */
    
    double alpha = 1.0;
    CmDrawableNode *node = CM_DRAWABLE_NODE (comp_window->node);

#if 0
    g_print ("waiting for paint: %d\n", comp_window->waiting_for_paint);
#endif
    
    if (ws_window_query_mapped (WS_WINDOW (comp_window->drawable))
#if 0
	&& !comp_window->waiting_for_paint
#endif
	)
    {
	WsWindow *window = WS_WINDOW (comp_window->drawable);
	
	cm_drawable_node_unset_patch (CM_DRAWABLE_NODE (node));
	
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

	if (!cm_drawable_node_get_viewable (node))
	{
	    comp_window->waiting_for_paint = TRUE;
#if 0
	    alarm = ws_alarm_new (comp_window->display);
#endif
#if 0
	    finish_counter = ws_window_get_property_sync_counter (
		window, "_NET_WM_FINISH_FRAME_COUNTER");
#endif

	    /* For some reason the panel and nautilus don't respond to the
	     * sync counter stuff. FIXME: this should be figured out at
	     * some point.
	     */
	    if (frameless_managed (comp_window) || !send_sync_request (comp_window))
	    {
#if 0
		g_print ("directly showing %p\n", comp_window);
#endif
		show_node (comp_window);
	    }
	    else
	    {
#if 0
		g_print ("for %p waiting for alarm\n", comp_window);
#endif
	    }
	}
    }
    else
    {
#if 0
	g_print ("unmapping %p\n", node);
#endif
    
	hide_node (comp_window);
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
    MetaCompWindow             *comp_window;
    gdouble			level;
    GTimer *			timer;
} ExplodeInfo;

static gboolean
update_explosion (gpointer data)
{
    ExplodeInfo *info = data;
    CmDrawableNode *node = CM_DRAWABLE_NODE (info->comp_window->node);
    gdouble elapsed = g_timer_elapsed (info->timer, NULL);

    if (!cm_drawable_node_get_viewable (node))
    {
	g_print ("huh, how did that happen to %p?\n", node);
    }
    
    if (elapsed > EXPLODE_TIME)
    {
	meta_effect_end (info->effect);

	info->comp_window->animation_in_progress = FALSE;
	if (info->comp_window->hide_after_animation)
	    hide_node (info->comp_window);
	
	cm_drawable_node_set_explosion_level (node, 0.0);

	comp_window_unref (info->comp_window);
	return FALSE;
    }
    else
    {
	gdouble t = elapsed / EXPLODE_TIME;
	
	cm_drawable_node_set_explosion_level (
	    node, transform (t));
	return TRUE;
    }
}

void
meta_comp_window_explode (MetaCompWindow *comp_window,
			  MetaEffect *effect)
{
    ExplodeInfo *info = g_new0 (ExplodeInfo, 1);

    if (!cm_drawable_node_get_viewable (comp_window->node))
    {
	g_print ("%p wasn't even viewable to begin with\n", comp_window->node);
	return;
    }
    else
    {
	g_print ("%p is viewable\n", comp_window->node);
    }
    
    comp_window->animation_in_progress = TRUE;
    
    info->comp_window = comp_window_ref (comp_window);
    info->effect = effect;
    info->level = 0.0;
    info->timer = g_timer_new ();

    g_idle_add (update_explosion, info);
}

#endif
