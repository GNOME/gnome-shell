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
#include "frame.h"
#include "spring-model.h"

struct _MetaCompWindow
{
    MetaDisplay *display;
    MetaScreen  *screen;
    WsDrawable *drawable;
    WsPixmap   *pixmap;
    CmDrawableNode     *node;
    gboolean	updates;
    WsSyncAlarm *alarm;
    
    WsRectangle size;
    gboolean waiting_for_paint;
    
    gint64	counter_value;
    gint        ref_count;
    
    gboolean    animation_in_progress;
    gboolean	hide_after_animation;
    
    int		stack_freeze_count;
    int		fade_in_idle_id;

    MetaCompWindowDestroy destroy;
    gpointer              closure;
};

static void cancel_fade (MetaCompWindow *comp_window);

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

void
meta_comp_window_show (MetaCompWindow *comp_window)
{
    if (comp_window->animation_in_progress)
	comp_window->hide_after_animation = FALSE;
    
    cm_drawable_node_set_viewable (CM_DRAWABLE_NODE (comp_window->node), TRUE);
    cm_drawable_node_update_pixmap (CM_DRAWABLE_NODE (comp_window->node));
}

void
meta_comp_window_hide (MetaCompWindow *comp_window)
{
    if (comp_window->animation_in_progress)
    {
	comp_window->hide_after_animation = TRUE;
	return;
    }
    
    cancel_fade (comp_window);
    
    cm_drawable_node_set_viewable (CM_DRAWABLE_NODE (comp_window->node),
				   FALSE);
}

MetaCompWindow *
meta_comp_window_new (MetaScreen     *screen,
		      WsDrawable     *drawable,
		      MetaCompWindowDestroy destroy,
		      gpointer        closure)
{
    MetaDisplay    *display = screen->display;
    MetaCompWindow *window;
    WsRectangle geometry;
    
    ws_drawable_query_geometry (drawable, &geometry);
    
    window = g_new0 (MetaCompWindow, 1);
    
    window->screen = screen;
    window->display = display;
    window->drawable = g_object_ref (drawable);
    window->node = cm_drawable_node_new (drawable, &geometry);
    window->updates = TRUE;
    window->counter_value = 1;
    window->ref_count = 1;
    window->destroy = destroy;
    window->closure = closure;
    
    meta_comp_window_hide (window);
    
    return window;
}

static MetaCompWindow *
comp_window_ref (MetaCompWindow *comp_window)
{
    comp_window->ref_count++;

    return comp_window;
}

static gboolean
comp_window_unref (MetaCompWindow *comp_window)
{
    if (--comp_window->ref_count == 0)
    {
	if (comp_window->destroy)
	    comp_window->destroy (comp_window, comp_window->closure);
	
	g_object_unref (comp_window->drawable);
	g_object_unref (comp_window->node);
	if (comp_window->alarm)
	    g_object_unref (comp_window->alarm);
	memset (comp_window, 'e', sizeof (MetaCompWindow));
	g_free (comp_window);
	
	return TRUE;
    }
    
    return FALSE;
}

gboolean
meta_comp_window_free (MetaCompWindow *window)
{
    return comp_window_unref (window);
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
private_metacity_window (MetaCompWindow *comp_window)
{
    /* Returns TRUE if this is a private metacity window
     * such as a tooltip or a menu
     */
    XID xid = WS_RESOURCE_XID (comp_window->drawable);
    
    return meta_ui_window_is_widget (comp_window->screen->ui, xid);
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
    
    result->x = interpolate (t, from->x, to->x, 2);
    result->y = interpolate (t, from->y, to->y, 0.5);
    result->width = interpolate (t, from->width, to->width, 0.7);
    result->height = interpolate (t, from->height, to->height, 0.7);
}

static void
comp_window_set_target_rect (MetaCompWindow *window,
			     WsRectangle *rect)
{
    cm_drawable_node_set_scale_rect (window->node, rect);
}

static void
comp_window_get_real_size (MetaCompWindow *window,
			   WsRectangle *size)
{
    if (!size)
	return;
    
    cm_drawable_node_get_clipbox (window->node, size);
}

#define FADE_TIME 0.225

typedef struct
{
    MetaEffect		       *effect;
    MetaCompWindow	       *window;
    GTimer *			timer;
    WsRectangle			from;
    WsRectangle			to;
    gboolean			first_time;
    gdouble			start_alpha;
    gdouble			end_alpha;
} FadeInfo;

static gboolean
update_fade (gpointer data)
{
    FadeInfo *info = data;
    gdouble elapsed = g_timer_elapsed (info->timer, NULL);
    gdouble t = elapsed / FADE_TIME;
    
    if (elapsed >= FADE_TIME)
    {
	comp_window_set_target_rect (info->window, &info->to);
	cm_drawable_node_set_alpha (info->window->node, info->end_alpha);
	cm_drawable_node_unset_patch (info->window->node);
	comp_window_unref (info->window);
	return FALSE;
    }
    else
    {
	gdouble alpha = interpolate (t, info->start_alpha, info->end_alpha, 1.0);
	WsRectangle cur;
	
	if (info->first_time)
	{
	    meta_comp_window_show (info->window);
	    info->first_time = FALSE;
	}
	
	interpolate_rectangle (t, &info->from, &info->to, &cur);
	comp_window_set_target_rect (info->window, &cur);
	cm_drawable_node_set_alpha (info->window->node, alpha);
	return TRUE;
    }
}

static void
cancel_fade (MetaCompWindow *comp_window)
{
    if (comp_window->fade_in_idle_id)
    {
	g_source_remove (comp_window->fade_in_idle_id);
	comp_window->fade_in_idle_id = 0;
    }
}

static void
meta_comp_window_fade_in (MetaCompWindow *comp_window)
{
    FadeInfo *info = g_new0 (FadeInfo, 1);
    WsWindow *window = find_client_window (comp_window);
    
    if (comp_window->fade_in_idle_id)
	return;
    
    info->window = comp_window_ref (comp_window);
    info->timer = g_timer_new ();
    
    comp_window_get_real_size (info->window, &info->to);
    info->from = info->to;
    
    info->start_alpha = 0.1;
    info->first_time = TRUE;
    
    if (has_type (window, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU") ||
	has_type (window, "_NET_WM_WINDOW_TYPE_POPUP_MENU"))
    {
	info->end_alpha = 0.9;
	info->from.width *= 0.6;
	info->from.height *= 0.4;
    }
    else if (has_type (window, "_NET_WM_WINDOW_TYPE_DIALOG"))
    {
	info->end_alpha = 0.9;
    }
    else
    {
	info->end_alpha = 1.0;
    }
    
    comp_window->fade_in_idle_id = g_idle_add (update_fade, info);
}

static void
on_request_alarm (WsSyncAlarm *alarm,
		  WsAlarmNotifyEvent *event,
		  MetaCompWindow *comp_window)
{
    /* This alarm means that the window is ready to be shown on screen */
    
    meta_comp_window_fade_in (comp_window);
    
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
	    if (frameless_managed (comp_window) ||
		private_metacity_window (comp_window) ||
		!send_sync_request (comp_window))
	    {
		meta_comp_window_show (comp_window);
	    }
	}
    }
    else
    {
#if 0
	g_print ("unmapping %p\n", node);
#endif
	
	meta_comp_window_hide (comp_window);
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
    return CM_NODE (comp_window->node);
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
    
    if (!cm_drawable_node_get_viewable (node) || elapsed > EXPLODE_TIME)
    {
	meta_effect_end (info->effect);
	
	info->comp_window->animation_in_progress = FALSE;
	if (info->comp_window->hide_after_animation)
	    meta_comp_window_hide (info->comp_window);
	
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
#if 0
	g_print ("%p wasn't even viewable to begin with\n", comp_window->node);
#endif
	return;
    }
    else
    {
#if 0
	g_print ("%p is viewable\n", comp_window->node);
#endif
    }
    
    comp_window->animation_in_progress = TRUE;
    
    info->comp_window = comp_window_ref (comp_window);
    info->effect = effect;
    info->level = 0.0;
    info->timer = g_timer_new ();
    
    g_idle_add (update_explosion, info);
}

/* shrinkydink minimize effect */

#define N_PHASES 5

typedef struct
{
    WsRectangle start_rect;
    WsRectangle end_rect;
    gdouble	start_alpha;
    gdouble	end_alpha;
    gdouble	start_time;
    gdouble	end_time;
} Phase;

typedef struct
{
    GTimer *timer;
    
    MetaCompWindow *comp_window;
    
    MetaEffect *effect;
    
    gdouble	aspect_ratio;

    Phase	phases[N_PHASES];
} MiniInfo;

static void
set_geometry (MetaCompWindow *window,
	      Phase          *phase,
	      gdouble         elapsed)
{
    WsRectangle rect;
    gdouble alpha;
    gdouble t = (elapsed - phase->start_time) / (phase->end_time - phase->start_time);
    
    interpolate_rectangle (t, &phase->start_rect, &phase->end_rect, &rect);
    alpha = interpolate (t, phase->start_alpha, phase->end_alpha, 1.0);
    
    comp_window_set_target_rect (window, &rect);
    cm_drawable_node_set_alpha (window->node, alpha);
}

static int
center (gdouble what, gdouble in)
{
    return (in - what) / 2.0 + 0.5;
}

#define WOBBLE_FACTOR 1.5

static void
generate_phases (WsRectangle *start,
		 WsRectangle *icon,
		 Phase        phases[N_PHASES])
{
    const double phase_times[5] = {
	0.225,				/* scale to size of button */
	0.100,				/* scale up a little */
	0.100,				/* scale back a little */
	0.100,				/* drop down to icon */
	0.350,				/* fade out */
    };
    
    WsRectangle cur;
    gdouble alpha;
    gdouble aspect_ratio;
    gdouble time;
    int i;
    
    aspect_ratio = (double)start->width / (double)start->height;
    cur = *start;
    time = 0.0;
    alpha = 1.0;
    for (i = 0; i < N_PHASES; ++i)
    {
	Phase *phase = &(phases[i]);
	WsRectangle *end = &(phase->end_rect);
	
	phase->start_time = time;
	phase->start_rect = cur;
	phase->start_alpha = alpha;
	phase->end_alpha = 1.0;
	phase->end_time = time + phase_times[i];
	
	if (i == 0)
	{
	    /* Shrink to a little rectangle */
	    end->height = icon->height;
	    end->width = icon->height * aspect_ratio;
	    end->x = icon->x + center (end->width, icon->width);
	    end->y = cur.y + center (icon->height, cur.height);
	}
	else if (i == 1)
	{
	    /* Zoom out a little */
	    end->x = cur.x + center (WOBBLE_FACTOR * cur.width, cur.width);
	    end->y = cur.y + center (WOBBLE_FACTOR * cur.height, cur.height);
	    end->width = cur.width * WOBBLE_FACTOR;
	    end->height = cur.height * WOBBLE_FACTOR;
	}
	else if (i == 2)
	{
	    /* Zoom back */
	    end->height = icon->height;
	    end->width = icon->height * aspect_ratio;
	    end->x = icon->x + center (end->width, icon->width);
	    end->y = cur.y + center (icon->height, cur.height);
	}
	else if (i == 3)
	{
	    /* Move down to the button */
	    end->height = icon->height;
	    end->width = icon->height * aspect_ratio;
	    end->x = icon->x + center (end->width, icon->width);
	    end->y = icon->y;
	}
	else if (i == 4)
	{
	    /* Fade out */
	    end->x = icon->x;
	    end->y = icon->y;
	    end->width = icon->width;
	    end->height = icon->height;

	    phases[i].end_alpha = 0.0;
	}

	alpha = phase->end_alpha;
	cur = phase->end_rect;
	time += phase_times[i];
    }
}

static gboolean
update_minimize (gpointer data)
{
    MiniInfo *info = data;
    Phase *current_phase;
    int i;
    gdouble elapsed = g_timer_elapsed (info->timer, NULL);

    current_phase = NULL;
    for (i = 0; i < N_PHASES; ++i)
    {
	Phase *p = &(info->phases[i]);
	
	if (p->start_time < elapsed && p->end_time >= elapsed)
	{
	    current_phase = p;
	    break;
	}
    }

    if (current_phase)
    {
	set_geometry (info->comp_window, current_phase, elapsed);
	return TRUE;
    }
    else
    {
	meta_comp_window_hide (info->comp_window);
	cm_drawable_node_set_alpha (info->comp_window->node, 1.0);
	cm_drawable_node_unset_patch (info->comp_window->node);
	comp_window_unref (info->comp_window);

	meta_effect_end (info->effect);
	
	return FALSE;
    }
}

void
meta_comp_window_run_minimize (MetaCompWindow           *window,
			       MetaEffect               *effect)
{
    MiniInfo *info = g_new (MiniInfo, 1);
    WsRectangle start, end;

    info->timer = g_timer_new ();
    
    info->comp_window = comp_window_ref (window);

    info->effect = effect;

    start.x = effect->u.minimize.window_rect.x;
    start.y = effect->u.minimize.window_rect.y;
    start.width = effect->u.minimize.window_rect.width;
    start.height = effect->u.minimize.window_rect.height;

    end.x = effect->u.minimize.icon_rect.x;
    end.y = effect->u.minimize.icon_rect.y;
    end.width = effect->u.minimize.icon_rect.width;
    end.height = effect->u.minimize.icon_rect.height;
    
    generate_phases (&start, &end, info->phases);
    
    g_idle_add (update_minimize, info);
}

/* bounce effect */

typedef struct
{
    MetaEffect	    *effect;
    MetaCompWindow  *window;
    GTimer	    *timer;
    Model	    *model;
    MetaRectangle   rect;
    gdouble	    last_time;
} BounceInfo;

/* XXX HATE */
extern void get_patch_points (Model *model, CmPoint points[4][4]);
extern void compute_window_rect (MetaWindow *window, MetaRectangle *rect);

static gboolean
update_bounce (gpointer data)
{
    BounceInfo *info = data;
    CmDrawableNode *node = (CmDrawableNode *)info->window->node;
    gdouble elapsed = g_timer_elapsed (info->timer, NULL);
    int i;
    int n_steps = floor ((elapsed - info->last_time) * 60);
    CmPoint points[4][4];

    if (model_is_calm (info->model) || elapsed > 0.7)
    {
	cm_drawable_node_unset_patch (node);
	meta_effect_end (info->effect);
	g_free(info);
	return FALSE;
    }

    for (i = 0; i < n_steps; ++i)
	model_step (info->model);

    if (i > 0)
	info->last_time = elapsed;

    get_patch_points (info->model, points);

    cm_drawable_node_set_patch (node, points);
    return TRUE;
}

void
meta_comp_window_bounce (MetaCompWindow *comp_window,
			 MetaEffect *effect)
{
    BounceInfo *info = g_new0 (BounceInfo, 1);
    MetaWindow *meta_window =
	meta_display_lookup_x_window (comp_window->display,
				      WS_RESOURCE_XID (comp_window->drawable));

    info->window = comp_window;
    info->effect = effect;
    info->timer = g_timer_new ();
    info->last_time = 0;

    compute_window_rect (meta_window, &info->rect);
    info->model = model_new (&info->rect, TRUE);

    g_idle_add (update_bounce, info);
}

void
meta_comp_window_freeze_stack (MetaCompWindow *comp_window)
{
    comp_window->stack_freeze_count++;
}

void
meta_comp_window_thaw_stack (MetaCompWindow *comp_window)
{
    comp_window->stack_freeze_count--;
}

gboolean
meta_comp_window_stack_frozen (MetaCompWindow *comp_window)
{
    return comp_window->stack_freeze_count > 0;
}

#endif


