/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include <clutter/x11/clutter-x11.h>

#include "core.h"
#include <meta/screen.h>
#include <meta/errors.h>
#include <meta/window.h>
#include "compositor-private.h"
#include <meta/compositor-mutter.h>
#include "xprops.h"
#include <meta/prefs.h>
#include <meta/main.h>
#include <meta/meta-shadow-factory.h>
#include "meta-window-actor-private.h"
#include "meta-window-group.h"
#include "meta-background-actor-private.h"
#include "window-private.h" /* to check window->hidden */
#include "display-private.h" /* for meta_display_lookup_x_window() */
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

static void sync_actor_stacking (MetaCompScreen *info);

static void
meta_finish_workspace_switch (MetaCompScreen *info)
{
  GList *l;

  /* Finish hiding and showing actors for the new workspace */
  for (l = info->windows; l; l = l->next)
    meta_window_actor_sync_visibility (l->data);

  /*
   * Fix up stacking order in case the plugin messed it up.
   */
  sync_actor_stacking (info);

/*   printf ("... FINISHED DESKTOP SWITCH\n"); */

}

void
meta_switch_workspace_completed (MetaScreen *screen)
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
    meta_finish_workspace_switch (info);
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

  meta_window_actor_new (window);

  sync_actor_stacking (info);
}

static void
process_damage (MetaCompositor     *compositor,
                XDamageNotifyEvent *event,
                MetaWindow         *window)
{
  MetaWindowActor *window_actor;

  if (window == NULL)
    return;

  window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  if (window_actor == NULL)
    return;

  meta_window_actor_process_damage (window_actor, event);
}

static void
process_property_notify (MetaCompositor	*compositor,
                         XPropertyEvent *event,
                         MetaWindow     *window)
{
  MetaWindowActor *window_actor;

  if (event->atom == compositor->atom_x_root_pixmap)
    {
      GSList *l;

      for (l = meta_display_get_screens (compositor->display); l; l = l->next)
        {
	  MetaScreen  *screen = l->data;
          if (event->window == meta_screen_get_xroot (screen))
            {
              meta_background_actor_update (screen);
              return;
            }
        }
    }

  if (window == NULL)
    return;

  window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  if (window_actor == NULL)
    return;

  /* Check for the opacity changing */
  if (event->atom == compositor->atom_net_wm_window_opacity)
    {
      meta_window_actor_update_opacity (window_actor);
      DEBUG_TRACE ("process_property_notify: net_wm_window_opacity\n");
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
  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

  xroot = meta_screen_get_xroot (screen);
  output = XCompositeGetOverlayWindow (xdisplay, xroot);

  meta_core_add_old_event_mask (xdisplay, output, &mask);

  XISetMask (mask.mask, XI_KeyPress);
  XISetMask (mask.mask, XI_KeyRelease);
  XISetMask (mask.mask, XI_ButtonPress);
  XISetMask (mask.mask, XI_ButtonRelease);
  XISetMask (mask.mask, XI_Enter);
  XISetMask (mask.mask, XI_Leave);
  XISetMask (mask.mask, XI_FocusIn);
  XISetMask (mask.mask, XI_FocusOut);
  XISetMask (mask.mask, XI_Motion);
  XISelectEvents (xdisplay, output, &mask, 1);

  event_mask = ExposureMask | PropertyChangeMask;
  if (XGetWindowAttributes (xdisplay, output, &attr))
    event_mask |= attr.your_event_mask;

  XSelectInput (xdisplay, output, event_mask);

  return output;
}

/**
 * meta_get_stage_for_screen:
 * @screen: a #MetaScreen
 *
 * Returns: (transfer none): The #ClutterStage for the screen
 */
ClutterActor *
meta_get_stage_for_screen (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (!info)
    return NULL;

  return info->stage;
}

/**
 * meta_get_overlay_group_for_screen:
 * @screen: a #MetaScreen
 *
 * Returns: (transfer none): The overlay group corresponding to @screen
 */
ClutterActor *
meta_get_overlay_group_for_screen (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (!info)
    return NULL;

  return info->overlay_group;
}

/**
 * meta_get_window_group_for_screen:
 * @screen: a #MetaScreen
 *
 * Returns: (transfer none): The window group corresponding to @screen
 */
ClutterActor *
meta_get_window_group_for_screen (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (!info)
    return NULL;

  return info->window_group;
}

/**
 * meta_get_background_actor_for_screen:
 * @screen: a #MetaScreen
 *
 * Gets the actor that draws the root window background under the windows.
 * The root window background automatically tracks the image or color set
 * by the environment.
 *
 * Returns: (transfer none): The background actor corresponding to @screen
 */
ClutterActor *
meta_get_background_actor_for_screen (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (!info)
    return NULL;

  return info->background_actor;
}

/**
 * meta_get_window_actors:
 * @screen: a #MetaScreen
 *
 * Returns: (transfer none) (element-type Clutter.Actor): The set of #MetaWindowActor on @screen
 */
GList *
meta_get_window_actors (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (!info)
    return NULL;

  return info->windows;
}

static void
do_set_stage_input_region (MetaScreen   *screen,
                           XserverRegion region)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);
  MetaDisplay *display = meta_screen_get_display (screen);
  Display        *xdpy = meta_display_get_xdisplay (display);
  Window        xstage = clutter_x11_get_stage_window (CLUTTER_STAGE (info->stage));

  XFixesSetWindowShapeRegion (xdpy, xstage, ShapeInput, 0, 0, region);

  /* It's generally a good heuristic that when a crossing event is generated because
   * we reshape the overlay, we don't want it to affect focus-follows-mouse focus -
   * it's not the user doing something, it's the environment changing under the user.
   */
  meta_display_add_ignored_crossing_serial (display, XNextRequest (xdpy));
  XFixesSetWindowShapeRegion (xdpy, info->output, ShapeInput, 0, 0, region);
}

void
meta_set_stage_input_region (MetaScreen   *screen,
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
meta_empty_stage_input_region (MetaScreen *screen)
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

  meta_set_stage_input_region (screen, region);
}

gboolean
meta_begin_modal_for_plugin (MetaScreen       *screen,
                             MetaPlugin       *plugin,
                             Window            grab_window,
                             Cursor            cursor,
                             MetaModalOptions  options,
                             guint32           timestamp)
{
  /* To some extent this duplicates code in meta_display_begin_grab_op(), but there
   * are significant differences in how we handle grabs that make it difficult to
   * merge the two.
   */
  MetaDisplay    *display    = meta_screen_get_display (screen);
  Display        *xdpy       = meta_display_get_xdisplay (display);
  MetaCompositor *compositor = display->compositor;
  gboolean pointer_grabbed = FALSE;
  gboolean keyboard_grabbed = FALSE;
  int result;

  if (compositor->modal_plugin != NULL || display->grab_op != META_GRAB_OP_NONE)
    return FALSE;

  if ((options & META_MODAL_POINTER_ALREADY_GRABBED) == 0)
    {
      unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
      XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

      XISetMask (mask.mask, XI_ButtonPress);
      XISetMask (mask.mask, XI_ButtonRelease);
      XISetMask (mask.mask, XI_Enter);
      XISetMask (mask.mask, XI_Leave);
      XISetMask (mask.mask, XI_Motion);

      result = XIGrabDevice (xdpy,
                             META_VIRTUAL_CORE_POINTER_ID,
                             grab_window,
                             timestamp,
                             cursor,
                             XIGrabModeAsync, XIGrabModeAsync,
                             False, /* owner_events */
                             &mask);
      if (result != Success)
        goto fail;

      pointer_grabbed = TRUE;
    }

  if ((options & META_MODAL_KEYBOARD_ALREADY_GRABBED) == 0)
    {
      unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
      XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

      XISetMask (mask.mask, XI_KeyPress);
      XISetMask (mask.mask, XI_KeyRelease);

      result = XIGrabDevice (xdpy,
                             META_VIRTUAL_CORE_KEYBOARD_ID,
                             grab_window,
                             timestamp,
                             None,
                             XIGrabModeAsync, XIGrabModeAsync,
                             False, /* owner_events */
                             &mask);

      if (result != Success)
        goto fail;

      keyboard_grabbed = TRUE;
    }

  display->grab_op = META_GRAB_OP_COMPOSITOR;
  display->grab_window = NULL;
  display->grab_screen = screen;
  display->grab_have_pointer = TRUE;
  display->grab_have_keyboard = TRUE;

  compositor->modal_plugin = plugin;

  return TRUE;

 fail:
  if (pointer_grabbed)
    XIUngrabDevice (xdpy, META_VIRTUAL_CORE_POINTER_ID, timestamp);
  if (keyboard_grabbed)
    XIUngrabDevice (xdpy, META_VIRTUAL_CORE_KEYBOARD_ID, timestamp);

  return FALSE;
}

void
meta_end_modal_for_plugin (MetaScreen     *screen,
                           MetaPlugin     *plugin,
                           guint32         timestamp)
{
  MetaDisplay    *display    = meta_screen_get_display (screen);
  Display        *xdpy = meta_display_get_xdisplay (display);
  MetaCompositor *compositor = display->compositor;

  g_return_if_fail (compositor->modal_plugin == plugin);

  XIUngrabDevice (xdpy, META_VIRTUAL_CORE_POINTER_ID, timestamp);
  XIUngrabDevice (xdpy, META_VIRTUAL_CORE_KEYBOARD_ID, timestamp);

  display->grab_op = META_GRAB_OP_NONE;
  display->grab_window = NULL;
  display->grab_screen = NULL;
  display->grab_have_pointer = FALSE;
  display->grab_have_keyboard = FALSE;

  compositor->modal_plugin = NULL;
}

/* This is used when reloading plugins to make sure we don't have
 * a left-over modal grab for this screen.
 */
void
meta_check_end_modal (MetaScreen *screen)
{
  MetaDisplay    *display    = meta_screen_get_display (screen);
  MetaCompositor *compositor = display->compositor;

  if (compositor->modal_plugin &&
      meta_plugin_get_screen (compositor->modal_plugin) == screen)
    {
      meta_end_modal_for_plugin (screen,
                                   compositor->modal_plugin,
                                   CurrentTime);
    }
}

static void
after_stage_paint (ClutterActor   *stage,
                   MetaCompScreen *info)
{
  GList *l;

  for (l = info->windows; l; l = l->next)
    meta_window_actor_post_paint (l->data);
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
  guint           n_retries;
  guint           max_retries;

  /* Check if the screen is already managed */
  if (meta_screen_get_compositor_data (screen))
    return;

  if (meta_get_replace_current_wm ())
    max_retries = 5;
  else
    max_retries = 1;

  n_retries = 0;

  /* Some compositors (like old versions of Mutter) might not properly unredirect
   * subwindows before destroying the WM selection window; so we wait a while
   * for such a compositor to exit before giving up.
   */
  while (TRUE)
    {
      meta_error_trap_push_with_return (display);
      XCompositeRedirectSubwindows (xdisplay, xroot, CompositeRedirectManual);
      XSync (xdisplay, FALSE);

      if (!meta_error_trap_pop_with_return (display))
        break;

      if (n_retries == max_retries)
        {
          /* This probably means that a non-WM compositor like xcompmgr is running;
           * we have no way to get it to exit */
          meta_fatal (_("Another compositing manager is already running on screen %i on display \"%s\"."),
                      screen_number, display->name);
        }

      n_retries++;
      g_usleep (G_USEC_PER_SEC);
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

  meta_screen_set_cm_selection (screen);

  info->stage = clutter_stage_new ();
  g_signal_connect_after (info->stage, "paint",
                          G_CALLBACK (after_stage_paint), info);

  /* Wait 2ms after vblank before starting to draw next frame */
  clutter_stage_set_sync_delay (CLUTTER_STAGE (info->stage), 2);

  meta_screen_get_size (screen, &width, &height);
  clutter_actor_realize (info->stage);

  xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (info->stage));

  XResizeWindow (xdisplay, xwin, width, height);

  {
    long event_mask;
    unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };
    XWindowAttributes attr;

    meta_core_add_old_event_mask (xdisplay, xwin, &mask);

    XISetMask (mask.mask, XI_KeyPress);
    XISetMask (mask.mask, XI_KeyRelease);
    XISetMask (mask.mask, XI_ButtonPress);
    XISetMask (mask.mask, XI_ButtonRelease);
    XISetMask (mask.mask, XI_Enter);
    XISetMask (mask.mask, XI_Leave);
    XISetMask (mask.mask, XI_FocusIn);
    XISetMask (mask.mask, XI_FocusOut);
    XISetMask (mask.mask, XI_Motion);
    XISelectEvents (xdisplay, xwin, &mask, 1);

    event_mask = ExposureMask | PropertyChangeMask | StructureNotifyMask;
    if (XGetWindowAttributes (xdisplay, xwin, &attr))
      event_mask |= attr.your_event_mask;

    XSelectInput (xdisplay, xwin, event_mask);
  }

  info->window_group = meta_window_group_new (screen);
  info->background_actor = meta_background_actor_new_for_screen (screen);
  info->overlay_group = clutter_group_new ();
  info->hidden_group = clutter_group_new ();

  clutter_container_add (CLUTTER_CONTAINER (info->window_group),
                         info->background_actor,
                         NULL);

  clutter_container_add (CLUTTER_CONTAINER (info->stage),
                         info->window_group,
                         info->overlay_group,
			 info->hidden_group,
                         NULL);

  clutter_actor_hide (info->hidden_group);

  info->plugin_mgr = meta_plugin_manager_new (screen);

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
  MetaDisplay    *display       = meta_screen_get_display (screen);
  Display        *xdisplay      = meta_display_get_xdisplay (display);
  Window          xroot         = meta_screen_get_xroot (screen);

  /* This is the most important part of cleanup - we have to do this
   * before giving up the window manager selection or the next
   * window manager won't be able to redirect subwindows */
  XCompositeUnredirectSubwindows (xdisplay, xroot, CompositeRedirectManual);
}

/*
 * Shapes the cow so that the given window is exposed,
 * when metaWindow is NULL it clears the shape again
 */
static void
meta_shape_cow_for_window (MetaScreen *screen,
                           MetaWindow *metaWindow)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);
  Display *xdisplay = meta_display_get_xdisplay (meta_screen_get_display (screen));

  if (metaWindow == NULL)
      XFixesSetWindowShapeRegion (xdisplay, info->output, ShapeBounding, 0, 0, None);
  else
    {
      XserverRegion output_region;
      XRectangle screen_rect, window_bounds;
      int width, height;
      MetaRectangle rect;

      meta_window_get_outer_rect (metaWindow, &rect);

      window_bounds.x = rect.x;
      window_bounds.y = rect.y;
      window_bounds.width = rect.width;
      window_bounds.height = rect.height;

      meta_screen_get_size (screen, &width, &height);
      screen_rect.x = 0;
      screen_rect.y = 0;
      screen_rect.width = width;
      screen_rect.height = height;

      output_region = XFixesCreateRegion (xdisplay, &window_bounds, 1);

      XFixesInvertRegion (xdisplay, output_region, &screen_rect, output_region);
      XFixesSetWindowShapeRegion (xdisplay, info->output, ShapeBounding, 0, 0, output_region);
      XFixesDestroyRegion (xdisplay, output_region);
    }
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

  meta_error_trap_pop (display);
}

void
meta_compositor_remove_window (MetaCompositor *compositor,
                               MetaWindow     *window)
{
  MetaWindowActor         *window_actor     = NULL;
  MetaScreen *screen;
  MetaCompScreen *info;

  DEBUG_TRACE ("meta_compositor_remove_window\n");
  window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  if (!window_actor)
    return;

  screen = meta_window_get_screen (window);
  info = meta_screen_get_compositor_data (screen);

  if (window_actor == info->unredirected_window)
    {
      meta_window_actor_set_redirected (window_actor, TRUE);
      meta_shape_cow_for_window (meta_window_get_screen (meta_window_actor_get_meta_window (info->unredirected_window)),
                                 NULL);
      info->unredirected_window = NULL;
    }

  meta_window_actor_destroy (window_actor);
}

void
meta_compositor_set_updates_frozen (MetaCompositor *compositor,
                                    MetaWindow     *window,
                                    gboolean        updates_frozen)
{
  MetaWindowActor *window_actor;

  DEBUG_TRACE ("meta_compositor_set_updates_frozen\n");
  window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  if (!window_actor)
    return;

  meta_window_actor_set_updates_frozen (window_actor, updates_frozen);
}

static gboolean
is_grabbed_event (MetaDisplay *display,
                  XEvent      *event)
{
  if (event->type == GenericEvent &&
      event->xcookie.extension == display->xinput_opcode)
    {
      XIEvent *xev = (XIEvent *) event->xcookie.data;

      switch (xev->evtype)
        {
        case XI_Motion:
        case XI_ButtonPress:
        case XI_ButtonRelease:
        case XI_KeyPress:
        case XI_KeyRelease:
          return TRUE;
        }
    }

  return FALSE;
}

void
meta_compositor_window_shape_changed (MetaCompositor *compositor,
                                      MetaWindow     *window)
{
  MetaWindowActor *window_actor;
  window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  meta_window_actor_update_shape (window_actor);
}

/**
 * meta_compositor_process_event: (skip)
 *
 */
gboolean
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
  if (compositor->modal_plugin && is_grabbed_event (compositor->display, event))
    {
      MetaPluginClass *klass = META_PLUGIN_GET_CLASS (compositor->modal_plugin);

      if (klass->xevent_filter)
        klass->xevent_filter (compositor->modal_plugin, event);

      /* We always consume events even if the plugin says it didn't handle them;
       * exclusive is exclusive */
      return TRUE;
    }

  if (window)
    {
      MetaCompScreen *info;
      MetaScreen     *screen;

      screen = meta_window_get_screen (window);
      info = meta_screen_get_compositor_data (screen);

      if (meta_plugin_manager_xevent_filter (info->plugin_mgr, event))
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

	  if (meta_plugin_manager_xevent_filter (info->plugin_mgr, event))
	    {
	      DEBUG_TRACE ("meta_compositor_process_event (filtered,window==NULL)\n");
	      return TRUE;
	    }

	  l = l->next;
	}
    }

  switch (event->type)
    {
    case PropertyNotify:
      process_property_notify (compositor, (XPropertyEvent *) event, window);
      break;

    default:
      if (event->type == meta_display_get_damage_event_base (compositor->display) + XDamageNotify)
        {
          /* Core code doesn't handle damage events, so we need to extract the MetaWindow
           * ourselves
           */
          if (window == NULL)
            {
              Window xwin = ((XDamageNotifyEvent *) event)->drawable;
              window = meta_display_lookup_x_window (compositor->display, xwin);
            }

	  DEBUG_TRACE ("meta_compositor_process_event (process_damage)\n");
          process_damage (compositor, (XDamageNotifyEvent *) event, window);
        }
      break;
    }

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

gboolean
meta_compositor_filter_keybinding (MetaCompositor *compositor,
                                   MetaScreen     *screen,
                                   MetaKeyBinding *binding)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (info->plugin_mgr)
    return meta_plugin_manager_filter_keybinding (info->plugin_mgr, binding);

  return FALSE;
}

void
meta_compositor_show_window (MetaCompositor *compositor,
			     MetaWindow	    *window,
                             MetaCompEffect  effect)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_show_window\n");
  if (!window_actor)
    return;

  meta_window_actor_show (window_actor, effect);
}

void
meta_compositor_hide_window (MetaCompositor *compositor,
                             MetaWindow     *window,
                             MetaCompEffect  effect)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_hide_window\n");
  if (!window_actor)
    return;

  meta_window_actor_hide (window_actor, effect);
}

void
meta_compositor_maximize_window (MetaCompositor    *compositor,
                                 MetaWindow        *window,
				 MetaRectangle	   *old_rect,
				 MetaRectangle	   *new_rect)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_maximize_window\n");
  if (!window_actor)
    return;

  meta_window_actor_maximize (window_actor, old_rect, new_rect);
}

void
meta_compositor_unmaximize_window (MetaCompositor    *compositor,
                                   MetaWindow        *window,
				   MetaRectangle     *old_rect,
				   MetaRectangle     *new_rect)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_unmaximize_window\n");
  if (!window_actor)
    return;

  meta_window_actor_unmaximize (window_actor, old_rect, new_rect);
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
      !meta_plugin_manager_switch_workspace (info->plugin_mgr,
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
      meta_finish_workspace_switch (info);
    }
}

static void
sync_actor_stacking (MetaCompScreen *info)
{
  GList *children;
  GList *tmp;
  GList *old;
  gboolean reordered;

  /* NB: The first entries in the lists are stacked the lowest */

  /* Restacking will trigger full screen redraws, so it's worth a
   * little effort to make sure we actually need to restack before
   * we go ahead and do it */

  children = clutter_container_get_children (CLUTTER_CONTAINER (info->window_group));
  reordered = FALSE;

  old = children;

  /* We allow for actors in the window group other than the actors we
   * know about, but it's up to a plugin to try and keep them stacked correctly
   * (we really need extra API to make that reliable.)
   */

  /* Of the actors we know, the bottom actor should be the background actor */

  while (old && old->data != info->background_actor && !META_IS_WINDOW_ACTOR (old->data))
    old = old->next;
  if (old == NULL || old->data != info->background_actor)
    {
      reordered = TRUE;
      goto done_with_check;
    }

  /* Then the window actors should follow in sequence */

  old = old->next;
  for (tmp = info->windows; tmp != NULL; tmp = tmp->next)
    {
      while (old && !META_IS_WINDOW_ACTOR (old->data))
        old = old->next;

      /* old == NULL: someone reparented a window out of the window group,
       * order undefined, always restack */
      if (old == NULL || old->data != tmp->data)
        {
          reordered = TRUE;
          goto done_with_check;
        }

      old = old->next;
    }

 done_with_check:

  g_list_free (children);

  if (!reordered)
    return;

  for (tmp = g_list_last (info->windows); tmp != NULL; tmp = tmp->prev)
    {
      MetaWindowActor *window_actor = tmp->data;

      clutter_actor_lower_bottom (CLUTTER_ACTOR (window_actor));
    }

  clutter_actor_lower_bottom (info->background_actor);
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
  old_stack = g_list_reverse (info->windows); /* The old stack of MetaWindowActor */
  info->windows = NULL;

  while (TRUE)
    {
      MetaWindowActor *old_actor = NULL, *stack_actor = NULL, *actor;
      MetaWindow *old_window = NULL, *stack_window = NULL, *window;

      /* Find the remaining top actor in our existing stack (ignoring
       * windows that have been hidden and are no longer animating) */
      while (old_stack)
        {
          old_actor = old_stack->data;
          old_window = meta_window_actor_get_meta_window (old_actor);

          if (old_window->hidden &&
              !meta_window_actor_effect_in_progress (old_actor))
            {
              old_stack = g_list_delete_link (old_stack, old_stack);
              old_actor = NULL;
            }
          else
            break;
        }

      /* And the remaining top actor in the new stack */
      while (stack)
        {
          stack_window = stack->data;
          stack_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (stack_window));
          if (!stack_actor)
            {
              meta_verbose ("Failed to find corresponding MetaWindowActor "
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

  sync_actor_stacking (info);
}

void
meta_compositor_window_mapped (MetaCompositor *compositor,
                               MetaWindow     *window)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_window_mapped\n");
  if (!window_actor)
    return;

  meta_window_actor_mapped (window_actor);
}

void
meta_compositor_window_unmapped (MetaCompositor *compositor,
                                 MetaWindow     *window)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  DEBUG_TRACE ("meta_compositor_window_unmapped\n");
  if (!window_actor)
    return;

  meta_window_actor_unmapped (window_actor);
}

void
meta_compositor_sync_window_geometry (MetaCompositor *compositor,
				      MetaWindow *window)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  MetaScreen      *screen = meta_window_get_screen (window);
  MetaCompScreen  *info = meta_screen_get_compositor_data (screen);

  DEBUG_TRACE ("meta_compositor_sync_window_geometry\n");
  g_return_if_fail (info);

  if (!window_actor)
    return;

  meta_window_actor_sync_actor_position (window_actor);
}

void
meta_compositor_sync_screen_size (MetaCompositor  *compositor,
				  MetaScreen	  *screen,
				  guint		   width,
				  guint		   height)
{
  MetaDisplay    *display = meta_screen_get_display (screen);
  MetaCompScreen *info    = meta_screen_get_compositor_data (screen);
  Display        *xdisplay;
  Window          xwin;

  DEBUG_TRACE ("meta_compositor_sync_screen_size\n");
  g_return_if_fail (info);

  xdisplay = meta_display_get_xdisplay (display);
  xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (info->stage));

  XResizeWindow (xdisplay, xwin, width, height);

  meta_background_actor_screen_size_changed (screen);

  meta_verbose ("Changed size for stage on screen %d to %dx%d\n",
		meta_screen_get_screen_number (screen),
		width, height);
}

static void
pre_paint_windows (MetaCompScreen *info)
{
  GList *l;
  MetaWindowActor *top_window;
  MetaWindowActor *expected_unredirected_window = NULL;

  if (info->windows == NULL)
    return;

  top_window = g_list_last (info->windows)->data;

  if (meta_window_actor_should_unredirect (top_window) &&
      info->disable_unredirect_count == 0)
    expected_unredirected_window = top_window;

  if (info->unredirected_window != expected_unredirected_window)
    {
      if (info->unredirected_window != NULL)
        {
          meta_window_actor_set_redirected (info->unredirected_window, TRUE);
          meta_shape_cow_for_window (meta_window_get_screen (meta_window_actor_get_meta_window (info->unredirected_window)),
                                     NULL);
        }

      if (expected_unredirected_window != NULL)
        {
          meta_shape_cow_for_window (meta_window_get_screen (meta_window_actor_get_meta_window (top_window)),
                                     meta_window_actor_get_meta_window (top_window));
          meta_window_actor_set_redirected (top_window, FALSE);
        }

      info->unredirected_window = expected_unredirected_window;
    }

  for (l = info->windows; l; l = l->next)
    meta_window_actor_pre_paint (l->data);
}

static gboolean
meta_repaint_func (gpointer data)
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

static void
on_shadow_factory_changed (MetaShadowFactory *factory,
                           MetaCompositor    *compositor)
{
  GSList *screens = meta_display_get_screens (compositor->display);
  GList *l;
  GSList *sl;

  for (sl = screens; sl; sl = sl->next)
    {
      MetaScreen *screen = sl->data;
      MetaCompScreen *info = meta_screen_get_compositor_data (screen);
      if (!info)
        continue;

      for (l = info->windows; l; l = l->next)
        meta_window_actor_invalidate_shadow (l->data);
    }
}

/**
 * meta_compositor_new: (skip)
 *
 */
MetaCompositor *
meta_compositor_new (MetaDisplay *display)
{
  char *atom_names[] = {
    "_XROOTPMAP_ID",
    "_NET_WM_WINDOW_OPACITY",
  };
  Atom                   atoms[G_N_ELEMENTS(atom_names)];
  MetaCompositor        *compositor;
  Display               *xdisplay = meta_display_get_xdisplay (display);

  if (!composite_at_least_version (display, 0, 3))
    return NULL;

  compositor = g_new0 (MetaCompositor, 1);

  compositor->display = display;

  if (g_getenv("META_DISABLE_MIPMAPS"))
    compositor->no_mipmaps = TRUE;

  meta_verbose ("Creating %d atoms\n", (int) G_N_ELEMENTS (atom_names));
  XInternAtoms (xdisplay, atom_names, G_N_ELEMENTS (atom_names),
                False, atoms);

  g_signal_connect (meta_shadow_factory_get_default (),
                    "changed",
                    G_CALLBACK (on_shadow_factory_changed),
                    compositor);

  compositor->atom_x_root_pixmap = atoms[0];
  compositor->atom_net_wm_window_opacity = atoms[1];

  compositor->repaint_func_id = clutter_threads_add_repaint_func (meta_repaint_func,
                                                                  compositor,
                                                                  NULL);

  return compositor;
}

/**
 * meta_get_overlay_window: (skip)
 *
 */
Window
meta_get_overlay_window (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  return info->output;
}

/**
 * meta_disable_unredirect_for_screen:
 * @screen: a #MetaScreen
 *
 * Disables unredirection, can be usefull in situations where having
 * unredirected windows is undesireable like when recording a video.
 *
 */
void
meta_disable_unredirect_for_screen (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);
  if (info != NULL)
    info->disable_unredirect_count = info->disable_unredirect_count + 1;
}

/**
 * meta_enable_unredirect_for_screen:
 * @screen: a #MetaScreen
 *
 * Enables unredirection which reduces the overhead for apps like games.
 *
 */
void
meta_enable_unredirect_for_screen (MetaScreen *screen)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);
  if (info != NULL)
   info->disable_unredirect_count = MAX(0, info->disable_unredirect_count - 1);
}

#define FLASH_TIME_MS 50

static void
flash_out_completed (ClutterAnimation *animation,
                     ClutterActor     *flash)
{
  clutter_actor_destroy (flash);
}

static void
flash_in_completed (ClutterAnimation *animation,
                    ClutterActor     *flash)
{
  clutter_actor_animate (flash, CLUTTER_EASE_IN_QUAD,
                         FLASH_TIME_MS,
                         "opacity", 0,
                         "signal-after::completed", flash_out_completed, flash,
                         NULL);
}

void
meta_compositor_flash_screen (MetaCompositor *compositor,
                              MetaScreen     *screen)
{
  ClutterActor *stage;
  ClutterActor *flash;
  ClutterColor black = { 0, 0, 0, 255 };
  gfloat width, height;

  stage = meta_get_stage_for_screen (screen);
  clutter_actor_get_size (stage, &width, &height);

  flash = clutter_rectangle_new_with_color (&black);
  clutter_actor_set_size (flash, width, height);
  clutter_actor_set_opacity (flash, 0);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), flash);

  clutter_actor_animate (flash, CLUTTER_EASE_OUT_QUAD,
                         FLASH_TIME_MS,
                         "opacity", 192,
                         "signal-after::completed", flash_in_completed, flash,
                         NULL);
}
