/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include <clutter/x11/clutter-x11.h>

#include "screen.h"
#include "errors.h"
#include "window.h"
#include "compositor-private.h"
#include "compositor-mutter.h"
#include "xprops.h"
#include "prefs.h"
#include "mutter-window-private.h"
#include "mutter-window-group.h"
#include "../core/window-private.h" /* to check window->hidden */
#include "../core/display-private.h" /* for meta_display_lookup_x_window() */
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>

/* #define DEBUG_TRACE g_print */
#define DEBUG_TRACE(X)

static inline gboolean
composite_at_least_version (MetaDisplay *display, int maj, int min)
{
  static int major = -1;
  static int minor = -1;

  if (major == -1)
    meta_display_get_compositor_version (display, &major, &minor);

  return (major > maj || (major == maj && minor >= min));
}

static MutterWindow*
find_window_for_screen (MetaScreen *screen, Window xwindow)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (info == NULL)
    return NULL;

  return g_hash_table_lookup (info->windows_by_xid,
                              (gpointer) xwindow);
}

static MutterWindow *
find_window_in_display (MetaDisplay *display, Window xwindow)
{
  GSList *index;
  MetaWindow *window = meta_display_lookup_x_window (display, xwindow);

  if (window)
    {
      void *priv = meta_window_get_compositor_private (window);
      if (priv)
	return priv;
    }

  for (index = meta_display_get_screens (display);
       index;
       index = index->next)
    {
      MutterWindow *cw = find_window_for_screen (index->data, xwindow);

      if (cw != NULL)
        return cw;
    }

  return NULL;
}

static MutterWindow *
find_window_for_child_window_in_display (MetaDisplay *display, Window xwindow)
{
  Window ignored1, *ignored2, parent;
  guint  ignored_children;

  XQueryTree (meta_display_get_xdisplay (display), xwindow, &ignored1,
              &parent, &ignored2, &ignored_children);

  if (parent != None)
    return find_window_in_display (display, parent);

  return NULL;
}

static void sync_actor_stacking (GList *windows);

static void
mutter_finish_workspace_switch (MetaCompScreen *info)
{
  GList *l;

  /* Finish hiding and showing actors for the new workspace */
  for (l = info->windows; l; l = l->next)
    mutter_window_sync_visibility (l->data);

  /*
   * Fix up stacking order in case the plugin messed it up.
   */
  sync_actor_stacking (info->windows);

/*   printf ("... FINISHED DESKTOP SWITCH\n"); */

}

void
mutter_switch_workspace_completed (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  /* FIXME -- must redo stacking order */
  info->switch_workspace_in_progress--;
  if (info->switch_workspace_in_progress < 0)
    {
      g_warning ("Error in workspace_switch accounting!");
      info->switch_workspace_in_progress = 0;
    }

  if (!info->switch_workspace_in_progress)
    mutter_finish_workspace_switch (info);
}

void
meta_compositor_destroy (MetaCompositor *compositor)
{
  clutter_threads_remove_repaint_func (compositor->repaint_func_id);
}

static void
add_win (MetaWindow *window)
{
  MetaScreen		*screen = meta_window_get_screen (window);
  MetaCompScreen        *info = meta_screen_get_compositor_data (screen);

  g_return_if_fail (info != NULL);

  mutter_window_new (window);

  sync_actor_stacking (info->windows);
}

static void
process_damage (MetaCompositor     *compositor,
                XDamageNotifyEvent *event)
{
  MutterWindow *cw = find_window_in_display (compositor->display, event->drawable);
  if (cw == NULL)
    return;

  mutter_window_process_damage (cw, event);
}

#ifdef HAVE_SHAPE
static void
process_shape (MetaCompositor *compositor,
               XShapeEvent    *event)
{
  MutterWindow *cw = find_window_in_display (compositor->display,
                                             event->window);
  if (cw == NULL)
    return;

  if (event->kind == ShapeBounding)
    {
      mutter_window_update_shape (cw, event->shaped);
    }
}
#endif

static void
process_property_notify (MetaCompositor	*compositor,
                         XPropertyEvent *event)
{
  MetaDisplay *display = compositor->display;

  /* Check for the opacity changing */
  if (event->atom == compositor->atom_net_wm_window_opacity)
    {
      MutterWindow *cw = find_window_in_display (display, event->window);

      if (!cw)
        {
          /* Applications can set this for their toplevel windows, so
           * this must be propagated to the window managed by the compositor
           */
          cw = find_window_for_child_window_in_display (display,
                                                        event->window);
        }

      if (!cw)
	{
	  DEBUG_TRACE ("process_property_notify: opacity, early exit\n");
	  return;
	}

      mutter_window_update_opacity (cw);
    }
  else if (event->atom == meta_display_get_atom (display,
					       META_ATOM__NET_WM_WINDOW_TYPE))
    {
      MutterWindow *cw = find_window_in_display (display, event->window);

      if (!cw)
	{
	  DEBUG_TRACE ("process_property_notify: net_wm_type, early exit\n");
	  return;
	}

      mutter_window_update_window_type (cw);
      DEBUG_TRACE ("process_property_notify: net_wm_type\n");
      return;
    }
  DEBUG_TRACE ("process_property_notify: unknown\n");
}

static Window
get_output_window (MetaScreen *screen)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display     *xdisplay = meta_display_get_xdisplay (display);
  Window       output, xroot;
  XWindowAttributes attr;
  long         event_mask;

  xroot = meta_screen_get_xroot (screen);

  event_mask = FocusChangeMask |
               ExposureMask |
               EnterWindowMask | LeaveWindowMask |
	       PointerMotionMask |
               PropertyChangeMask |
               ButtonPressMask | ButtonReleaseMask |
               KeyPressMask | KeyReleaseMask;

  output = XCompositeGetOverlayWindow (xdisplay, xroot);

  if (XGetWindowAttributes (xdisplay, output, &attr))
      {
        event_mask |= attr.your_event_mask;
      }

  XSelectInput (xdisplay, output, event_mask);

  return output;
}

ClutterActor *
mutter_get_stage_for_screen (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (!info)
    return NULL;

  return info->stage;
}

ClutterActor *
mutter_get_overlay_group_for_screen (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (!info)
    return NULL;

  return info->overlay_group;
}

ClutterActor *
mutter_get_window_group_for_screen (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (!info)
    return NULL;

  return info->window_group;
}

GList *
mutter_get_windows (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (!info)
    return NULL;

  return info->windows;
}

static void
do_set_stage_input_region (MetaScreen *screen,
                           XserverRegion region)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);
  MetaDisplay *display = meta_screen_get_display (screen);
  Display        *xdpy = meta_display_get_xdisplay (display);
  Window        xstage = clutter_x11_get_stage_window (CLUTTER_STAGE (info->stage));

  XFixesSetWindowShapeRegion (xdpy, xstage, ShapeInput, 0, 0, region);
  XFixesSetWindowShapeRegion (xdpy, info->output, ShapeInput, 0, 0, region);
}

void
mutter_set_stage_input_region (MetaScreen *screen,
                               XserverRegion region)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);
  MetaDisplay  *display = meta_screen_get_display (screen);
  Display      *xdpy    = meta_display_get_xdisplay (display);

  if (info->stage && info->output)
    {
      do_set_stage_input_region (screen, region);
    }
  else 
    {
      /* Reset info->pending_input_region if one existed before and set the new
       * one to use it later. */ 
      if (info->pending_input_region)
        {
          XFixesDestroyRegion (xdpy, info->pending_input_region);
          info->pending_input_region = None;
        }
      if (region != None)
        {
          info->pending_input_region = XFixesCreateRegion (xdpy, NULL, 0);
          XFixesCopyRegion (xdpy, info->pending_input_region, region);
        }
    } 
}

void
mutter_empty_stage_input_region (MetaScreen *screen)
{
  /* Using a static region here is a bit hacky, but Metacity never opens more than
   * one XDisplay, so it works fine. */
  static XserverRegion region = None;

  if (region == None)
    {
      MetaDisplay  *display = meta_screen_get_display (screen);
      Display      *xdpy    = meta_display_get_xdisplay (display);
      region = XFixesCreateRegion (xdpy, NULL, 0);
    }

  mutter_set_stage_input_region (screen, region);
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
  MetaCompScreen *info;
  MetaDisplay    *display       = meta_screen_get_display (screen);
  Display        *xdisplay      = meta_display_get_xdisplay (display);
  int             screen_number = meta_screen_get_screen_number (screen);
  Window          xroot         = meta_screen_get_xroot (screen);
  Window          xwin;
  gint            width, height;
  XWindowAttributes attr;
  long            event_mask;

  /* Check if the screen is already managed */
  if (meta_screen_get_compositor_data (screen))
    return;

  meta_error_trap_push_with_return (display);
  XCompositeRedirectSubwindows (xdisplay, xroot, CompositeRedirectManual);
  XSync (xdisplay, FALSE);

  if (meta_error_trap_pop_with_return (display, FALSE))
    {
      g_warning ("Another compositing manager is running on screen %i",
                 screen_number);
      return;
    }

  info = g_new0 (MetaCompScreen, 1);
  /*
   * We use an empty input region for Clutter as a default because that allows
   * the user to interact with all the windows displayed on the screen.
   * We have to initialize info->pending_input_region to an empty region explicitly, 
   * because None value is used to mean that the whole screen is an input region.
   */
  info->pending_input_region = XFixesCreateRegion (xdisplay, NULL, 0);

  info->screen = screen;

  meta_screen_set_compositor_data (screen, info);

  info->output = None;
  info->windows = NULL;
  info->windows_by_xid = g_hash_table_new (g_direct_hash, g_direct_equal);

  meta_screen_set_cm_selection (screen);

  info->stage = clutter_stage_get_default ();

  meta_screen_get_size (screen, &width, &height);
  clutter_actor_set_size (info->stage, width, height);

  xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (info->stage));

  event_mask = FocusChangeMask |
               ExposureMask |
               EnterWindowMask | LeaveWindowMask |
               PointerMotionMask |
               PropertyChangeMask |
               ButtonPressMask | ButtonReleaseMask |
               KeyPressMask | KeyReleaseMask |
               StructureNotifyMask;

  if (XGetWindowAttributes (xdisplay, xwin, &attr))
      {
        event_mask |= attr.your_event_mask;
      }

  XSelectInput (xdisplay, xwin, event_mask);

  info->window_group = mutter_window_group_new (screen);
  info->overlay_group = clutter_group_new ();
  info->hidden_group = clutter_group_new ();

  clutter_container_add (CLUTTER_CONTAINER (info->stage),
                         info->window_group,
                         info->overlay_group,
			 info->hidden_group,
                         NULL);

  clutter_actor_hide (info->hidden_group);

  info->plugin_mgr =
    mutter_plugin_manager_new (screen);
  if (!mutter_plugin_manager_load (info->plugin_mgr))
    g_critical ("failed to load plugins");
  if (!mutter_plugin_manager_initialize (info->plugin_mgr))
    g_critical ("failed to initialize plugins");

  /*
   * Delay the creation of the overlay window as long as we can, to avoid
   * blanking out the screen. This means that during the plugin loading, the
   * overlay window is not accessible; if the plugin needs to access it
   * directly, it should hook into the "show" signal on stage, and do
   * its stuff there.
   */
  info->output = get_output_window (screen);
  XReparentWindow (xdisplay, xwin, info->output, 0, 0);

 /* Make sure there isn't any left-over output shape on the 
  * overlay window by setting the whole screen to be an
  * output region.
  * 
  * Note: there doesn't seem to be any real chance of that
  *  because the X server will destroy the overlay window
  *  when the last client using it exits.
  */
  XFixesSetWindowShapeRegion (xdisplay, info->output, ShapeBounding, 0, 0, None);

  do_set_stage_input_region (screen, info->pending_input_region);
  if (info->pending_input_region != None)
    {
      XFixesDestroyRegion (xdisplay, info->pending_input_region);
      info->pending_input_region = None;
    }

  clutter_actor_show (info->overlay_group);
  clutter_actor_show (info->stage);
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
}

void
meta_compositor_add_window (MetaCompositor    *compositor,
                            MetaWindow        *window)
{
  MetaScreen *screen = meta_window_get_screen (window);
  MetaDisplay *display = meta_screen_get_display (screen);

  DEBUG_TRACE ("meta_compositor_add_window\n");
  meta_error_trap_push (display);

  add_win (window);

  meta_error_trap_pop (display, FALSE);
}

void
meta_compositor_remove_window (MetaCompositor *compositor,
                               MetaWindow     *window)
{
  MutterWindow         *cw     = NULL;

  DEBUG_TRACE ("meta_compositor_remove_window\n");
  cw = MUTTER_WINDOW (meta_window_get_compositor_private (window));
  if (!cw)
    return;

  mutter_window_destroy (cw);
}

void
meta_compositor_set_updates (MetaCompositor *compositor,
                             MetaWindow     *window,
                             gboolean        updates)
{
}

gboolean
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
  if (window)
    {
      MetaCompScreen *info;
      MetaScreen     *screen;

      screen = meta_window_get_screen (window);
      info = meta_screen_get_compositor_data (screen);

      if (mutter_plugin_manager_xevent_filter (info->plugin_mgr, event))
	{
	  DEBUG_TRACE ("meta_compositor_process_event (filtered,window==NULL)\n");
	  return TRUE;
	}
    }
  else
    {
      GSList *l;

      l = meta_display_get_screens (compositor->display);

      while (l)
	{
	  MetaScreen     *screen = l->data;
	  MetaCompScreen *info;

	  info = meta_screen_get_compositor_data (screen);

	  if (mutter_plugin_manager_xevent_filter (info->plugin_mgr, event))
	    {
	      DEBUG_TRACE ("meta_compositor_process_event (filtered,window==NULL)\n");
	      return TRUE;
	    }

	  l = l->next;
	}
    }

  /*
   * This trap is so that none of the compositor functions cause
   * X errors. This is really a hack, but I'm afraid I don't understand
   * enough about Metacity/X to know how else you are supposed to do it
   */


  meta_error_trap_push (compositor->display);
  switch (event->type)
    {
    case PropertyNotify:
      process_property_notify (compositor, (XPropertyEvent *) event);
      break;

    default:
      if (event->type == meta_display_get_damage_event_base (compositor->display) + XDamageNotify)
        {
	  DEBUG_TRACE ("meta_compositor_process_event (process_damage)\n");
          process_damage (compositor, (XDamageNotifyEvent *) event);
        }
#ifdef HAVE_SHAPE
      else if (event->type == meta_display_get_shape_event_base (compositor->display) + ShapeNotify)
	{
	  DEBUG_TRACE ("meta_compositor_process_event (process_shape)\n");
	  process_shape (compositor, (XShapeEvent *) event);
	}
#endif /* HAVE_SHAPE */
      break;
    }

  meta_error_trap_pop (compositor->display, FALSE);

  /* Clutter needs to know about MapNotify events otherwise it will
     think the stage is invisible */
  if (event->type == MapNotify)
    clutter_x11_handle_event (event);

  /* The above handling is basically just "observing" the events, so we return
   * FALSE to indicate that the event should not be filtered out; if we have
   * GTK+ windows in the same process, GTK+ needs the ConfigureNotify event, for example.
   */
  return FALSE;
}

void
meta_compositor_show_window (MetaCompositor *compositor,
			     MetaWindow	    *window,
                             MetaCompEffect  effect)
{
  MutterWindow *cw = MUTTER_WINDOW (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_show_window\n");
  if (!cw)
    return;

  mutter_window_show (cw, effect);
}

void
meta_compositor_hide_window (MetaCompositor *compositor,
                             MetaWindow     *window,
                             MetaCompEffect  effect)
{
  MutterWindow *cw = MUTTER_WINDOW (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_hide_window\n");
  if (!cw)
    return;

  mutter_window_hide (cw, effect);
}

void
meta_compositor_maximize_window (MetaCompositor    *compositor,
                                 MetaWindow        *window,
				 MetaRectangle	   *old_rect,
				 MetaRectangle	   *new_rect)
{
  MutterWindow	 *cw = MUTTER_WINDOW (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_maximize_window\n");
  if (!cw)
    return;

  mutter_window_maximize (cw, old_rect, new_rect);
}

void
meta_compositor_unmaximize_window (MetaCompositor    *compositor,
                                   MetaWindow        *window,
				   MetaRectangle     *old_rect,
				   MetaRectangle     *new_rect)
{
  MutterWindow	 *cw = MUTTER_WINDOW (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_unmaximize_window\n");
  if (!cw)
    return;

  mutter_window_unmaximize (cw, old_rect, new_rect);
}

void
meta_compositor_update_workspace_geometry (MetaCompositor *compositor,
                                           MetaWorkspace  *workspace)
{
#if 0
  /* FIXME -- should do away with this function in favour of MetaWorkspace
   * signal.
   */
  MetaScreen     *screen = meta_workspace_get_screen (workspace);
  MetaCompScreen *info;
  MutterPluginManager *mgr;

  DEBUG_TRACE ("meta_compositor_update_workspace_geometry\n");
  info = meta_screen_get_compositor_data (screen);
  mgr  = info->plugin_mgr;

  if (!mgr || !workspace)
    return;

  mutter_plugin_manager_update_workspace (mgr, workspace);
#endif
}

void
meta_compositor_switch_workspace (MetaCompositor     *compositor,
                                  MetaScreen         *screen,
                                  MetaWorkspace      *from,
                                  MetaWorkspace      *to,
                                  MetaMotionDirection direction)
{
  MetaCompScreen *info;
  gint            to_indx, from_indx;

  info      = meta_screen_get_compositor_data (screen);
  to_indx   = meta_workspace_index (to);
  from_indx = meta_workspace_index (from);

  DEBUG_TRACE ("meta_compositor_switch_workspace\n");

  if (!info) /* During startup before manage_screen() */
    return;

  info->switch_workspace_in_progress++;

  if (!info->plugin_mgr ||
      !mutter_plugin_manager_switch_workspace (info->plugin_mgr,
					       (const GList **)&info->windows,
					       from_indx,
					       to_indx,
					       direction))
    {
      info->switch_workspace_in_progress--;

      /* We have to explicitely call this to fix up stacking order of the
       * actors; this is because the abs stacking position of actors does not
       * necessarily change during the window hiding/unhiding, only their
       * relative position toward the destkop window.
       */
      mutter_finish_workspace_switch (info);
    }
}

static void
sync_actor_stacking (GList *windows)
{
  GList *tmp;

  /* NB: The first entry in the list is stacked the lowest */

  for (tmp = g_list_last (windows); tmp != NULL; tmp = tmp->prev)
    {
      MutterWindow *cw = tmp->data;

      clutter_actor_lower_bottom (CLUTTER_ACTOR (cw));
    }
}

void
meta_compositor_sync_stack (MetaCompositor  *compositor,
			    MetaScreen	    *screen,
			    GList	    *stack)
{
  GList *old_stack;
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  DEBUG_TRACE ("meta_compositor_sync_stack\n");

  /* This is painful because hidden windows that we are in the process
   * of animating out of existence. They'll be at the bottom of the
   * stack of X windows, but we want to leave them in their old position
   * until the animation effect finishes.
   */

  /* Sources: first window is the highest */
  stack = g_list_copy (stack); /* The new stack of MetaWindow */
  old_stack = g_list_reverse (info->windows); /* The old stack of MutterWindow */
  info->windows = NULL;

  while (TRUE)
    {
      MutterWindow *old_actor = NULL, *stack_actor = NULL, *actor;
      MetaWindow *old_window = NULL, *stack_window = NULL, *window;

      /* Find the remaining top actor in our existing stack (ignoring
       * windows that have been hidden and are no longer animating) */
      while (old_stack)
        {
          old_actor = old_stack->data;
          old_window = mutter_window_get_meta_window (old_actor);

          if (old_window->hidden &&
              !mutter_window_effect_in_progress (old_actor))
            old_stack = g_list_delete_link (old_stack, old_stack);
          else
            break;
        }

      /* And the remaining top actor in the new stack */
      while (stack)
        {
          stack_window = stack->data;
          stack_actor = MUTTER_WINDOW (meta_window_get_compositor_private (stack_window));
          if (!stack_actor)
            {
              meta_verbose ("Failed to find corresponding MutterWindow "
                            "for window %s\n", meta_window_get_description (stack_window));
              stack = g_list_delete_link (stack, stack);
            }
          else
            break;
        }

      if (!old_actor && !stack_actor) /* Nothing more to stack */
        break;

      /* We usually prefer the window in the new stack, but if if we
       * found a hidden window in the process of being animated out
       * of existence in the old stack we use that instead. We've
       * filtered out non-animating hidden windows above.
       */
      if (old_actor &&
          (!stack_actor || old_window->hidden))
        {
          actor = old_actor;
          window = old_window;
        }
      else
        {
          actor = stack_actor;
          window = stack_window;
        }

      /* OK, we know what actor we want next. Add it to our window
       * list, and remove it from both source lists. (It will
       * be at the front of at least one, hopefully it will be
       * near the front of the other.)
       */
      info->windows = g_list_prepend (info->windows, actor);

      stack = g_list_remove (stack, window);
      old_stack = g_list_remove (old_stack, actor);
    }

  sync_actor_stacking (info->windows);
}

void
meta_compositor_window_mapped (MetaCompositor *compositor,
                               MetaWindow     *window)
{
  MutterWindow *cw = MUTTER_WINDOW (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_window_mapped\n");
  if (!cw)
    return;

  mutter_window_mapped (cw);
}

void
meta_compositor_window_unmapped (MetaCompositor *compositor,
                                 MetaWindow     *window)
{
  MutterWindow *cw = MUTTER_WINDOW (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_window_unmapped\n");
  if (!cw)
    return;

  mutter_window_unmapped (cw);
}

void
meta_compositor_sync_window_geometry (MetaCompositor *compositor,
				      MetaWindow *window)
{
  MutterWindow	 *cw = MUTTER_WINDOW (meta_window_get_compositor_private (window));
  MetaScreen	 *screen = meta_window_get_screen (window);
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  DEBUG_TRACE ("meta_compositor_sync_window_geometry\n");
  g_return_if_fail (info);

  if (!cw)
    return;

  mutter_window_sync_actor_position (cw);
}

void
meta_compositor_sync_screen_size (MetaCompositor  *compositor,
				  MetaScreen	  *screen,
				  guint		   width,
				  guint		   height)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  DEBUG_TRACE ("meta_compositor_sync_screen_size\n");
  g_return_if_fail (info);

  clutter_actor_set_size (info->stage, width, height);

  meta_verbose ("Changed size for stage on screen %d to %dx%d\n",
		meta_screen_get_screen_number (screen),
		width, height);
}

static void
pre_paint_windows (MetaCompScreen *info)
{
  GList *l;

  for (l = info->windows; l; l = l->next)
    mutter_window_pre_paint (l->data);
}

static gboolean
mutter_repaint_func (gpointer data)
{
  MetaCompositor *compositor = data;
  GSList *screens = meta_display_get_screens (compositor->display);
  GSList *l;

  for (l = screens; l; l = l->next)
    {
      MetaScreen *screen = l->data;
      MetaCompScreen *info = meta_screen_get_compositor_data (screen);
      if (!info)
        continue;

      pre_paint_windows (info);
    }

  return TRUE;
}

MetaCompositor *
meta_compositor_new (MetaDisplay *display)
{
  char *atom_names[] = {
    "_XROOTPMAP_ID",
    "_XSETROOT_ID",
    "_NET_WM_WINDOW_OPACITY",
  };
  Atom                   atoms[G_N_ELEMENTS(atom_names)];
  MetaCompositor        *compositor;
  Display               *xdisplay = meta_display_get_xdisplay (display);

  if (!composite_at_least_version (display, 0, 3))
    return NULL;

  compositor = g_new0 (MetaCompositor, 1);

  compositor->display = display;

  if (g_getenv("MUTTER_DISABLE_MIPMAPS"))
    compositor->no_mipmaps = TRUE;

  meta_verbose ("Creating %d atoms\n", (int) G_N_ELEMENTS (atom_names));
  XInternAtoms (xdisplay, atom_names, G_N_ELEMENTS (atom_names),
                False, atoms);

  compositor->atom_x_root_pixmap = atoms[0];
  compositor->atom_x_set_root = atoms[1];
  compositor->atom_net_wm_window_opacity = atoms[2];

  compositor->repaint_func_id = clutter_threads_add_repaint_func (mutter_repaint_func,
                                                                  compositor,
                                                                  NULL);

  return compositor;
}

Window
mutter_get_overlay_window (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  return info->output;
}
