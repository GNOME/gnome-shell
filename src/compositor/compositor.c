/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:compositor
 * @Title: MetaCompositor
 * @Short_Description: Compositor API
 *
 * At a high-level, a window is not-visible or visible. When a
 * window is added (with meta_compositor_add_window()) it is not visible.
 * meta_compositor_show_window() indicates a transition from not-visible to
 * visible. Some of the reasons for this:
 *
 * - Window newly created
 * - Window is unminimized
 * - Window is moved to the current desktop
 * - Window was made sticky
 *
 * meta_compositor_hide_window() indicates that the window has transitioned from
 * visible to not-visible. Some reasons include:
 *
 * - Window was destroyed
 * - Window is minimized
 * - Window is moved to a different desktop
 * - Window no longer sticky.
 *
 * Note that combinations are possible - a window might have first
 * been minimized and then moved to a different desktop. The 'effect' parameter
 * to meta_compositor_show_window() and meta_compositor_hide_window() is a hint
 * as to the appropriate effect to show the user and should not
 * be considered to be indicative of a state change.
 *
 * When the active workspace is changed, meta_compositor_switch_workspace() is
 * called first, then meta_compositor_show_window() and
 * meta_compositor_hide_window() are called individually for each window
 * affected, with an effect of META_COMP_EFFECT_NONE.
 * If hiding windows will affect the switch workspace animation, the
 * compositor needs to delay hiding the windows until the switch
 * workspace animation completes.
 *
 * meta_compositor_maximize_window() and meta_compositor_unmaximize_window()
 * are transitions within the visible state. The window is resized __before__
 * the call, so it may be necessary to readjust the display based on the
 * old_rect to start the animation.
 *
 * # Containers #
 *
 * There's two containers in the stage that are used to place window actors, here
 * are listed in the order in which they are painted:
 *
 * - window group, accessible with meta_get_window_group_for_screen()
 * - top window group, accessible with meta_get_top_window_group_for_screen()
 *
 * Mutter will place actors representing windows in the window group, except for
 * override-redirect windows (ie. popups and menus) which will be placed in the
 * top window group.
 */

#include <config.h>

#include <clutter/x11/clutter-x11.h>

#include "core.h"
#include <meta/screen.h>
#include <meta/errors.h>
#include <meta/window.h>
#include "compositor-private.h"
#include <meta/compositor-mutter.h>
#include <meta/prefs.h>
#include <meta/main.h>
#include <meta/meta-background-actor.h>
#include <meta/meta-background-group.h>
#include <meta/meta-shadow-factory.h>
#include "meta-window-actor-private.h"
#include "meta-window-group.h"
#include "meta-stage.h"
#include "window-private.h" /* to check window->hidden */
#include "display-private.h" /* for meta_display_lookup_x_window() */
#include "util-private.h"
#include "frame.h"
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>

#include "backends/x11/meta-backend-x11.h"

#include "wayland/meta-wayland-private.h"

static gboolean
is_modal (MetaDisplay *display)
{
  return display->grab_op == META_GRAB_OP_COMPOSITOR;
}

static inline gboolean
composite_at_least_version (MetaDisplay *display, int maj, int min)
{
  static int major = -1;
  static int minor = -1;

  if (major == -1)
    meta_display_get_compositor_version (display, &major, &minor);

  return (major > maj || (major == maj && minor >= min));
}

static void sync_actor_stacking (MetaCompositor *compositor);

static void
meta_finish_workspace_switch (MetaCompositor *compositor)
{
  GList *l;

  /* Finish hiding and showing actors for the new workspace */
  for (l = compositor->windows; l; l = l->next)
    meta_window_actor_sync_visibility (l->data);

  /* Fix up stacking order. */
  sync_actor_stacking (compositor);
}

void
meta_switch_workspace_completed (MetaCompositor *compositor)
{
  /* FIXME -- must redo stacking order */
  compositor->switch_workspace_in_progress--;
  if (compositor->switch_workspace_in_progress < 0)
    {
      g_warning ("Error in workspace_switch accounting!");
      compositor->switch_workspace_in_progress = 0;
    }

  if (!compositor->switch_workspace_in_progress)
    meta_finish_workspace_switch (compositor);
}

void
meta_compositor_destroy (MetaCompositor *compositor)
{
  clutter_threads_remove_repaint_func (compositor->repaint_func_id);
}

static void
process_damage (MetaCompositor     *compositor,
                XDamageNotifyEvent *event,
                MetaWindow         *window)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  meta_window_actor_process_x11_damage (window_actor, event);
}

static Window
get_output_window (MetaCompositor *compositor)
{
  MetaBackendX11 *backend = META_BACKEND_X11 (meta_get_backend ());
  Display *xdisplay = meta_backend_x11_get_xdisplay (backend);
  Window output;
  XWindowAttributes attr;
  long event_mask;
  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

  output = XCompositeGetOverlayWindow (xdisplay, DefaultRootWindow (xdisplay));

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

/* compat helper */
static MetaCompositor *
get_compositor_for_screen (MetaScreen *screen)
{
  return screen->display->compositor;
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
  MetaCompositor *compositor = get_compositor_for_screen (screen);
  return compositor->stage;
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
  MetaCompositor *compositor = get_compositor_for_screen (screen);
  return compositor->window_group;
}

/**
 * meta_get_top_window_group_for_screen:
 * @screen: a #MetaScreen
 *
 * Returns: (transfer none): The top window group corresponding to @screen
 */
ClutterActor *
meta_get_top_window_group_for_screen (MetaScreen *screen)
{
  MetaCompositor *compositor = get_compositor_for_screen (screen);
  return compositor->top_window_group;
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
  MetaCompositor *compositor = get_compositor_for_screen (screen);
  return compositor->windows;
}

void
meta_set_stage_input_region (MetaScreen   *screen,
                             XserverRegion region)
{
  /* As a wayland compositor we can simply ignore all this trickery
   * for setting an input region on the stage for capturing events in
   * clutter since all input comes to us first and we get to choose
   * who else sees them.
   */
  if (!meta_is_wayland_compositor ())
    {
      MetaDisplay *display = screen->display;
      MetaCompositor *compositor = display->compositor;
      Display *xdpy = meta_display_get_xdisplay (display);
      Window xstage = clutter_x11_get_stage_window (CLUTTER_STAGE (compositor->stage));

      XFixesSetWindowShapeRegion (xdpy, xstage, ShapeInput, 0, 0, region);

      /* It's generally a good heuristic that when a crossing event is generated because
       * we reshape the overlay, we don't want it to affect focus-follows-mouse focus -
       * it's not the user doing something, it's the environment changing under the user.
       */
      meta_display_add_ignored_crossing_serial (display, XNextRequest (xdpy));
      XFixesSetWindowShapeRegion (xdpy, compositor->output, ShapeInput, 0, 0, region);
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

void
meta_focus_stage_window (MetaScreen *screen,
                         guint32     timestamp)
{
  ClutterStage *stage;
  Window window;

  stage = CLUTTER_STAGE (meta_get_stage_for_screen (screen));
  if (!stage)
    return;

  window = clutter_x11_get_stage_window (stage);

  if (window == None)
    return;

  meta_display_set_input_focus_xwindow (screen->display,
                                        screen,
                                        window,
                                        timestamp);
}

gboolean
meta_stage_is_focused (MetaScreen *screen)
{
  ClutterStage *stage;
  Window window;

  if (meta_is_wayland_compositor ())
    return TRUE;

  stage = CLUTTER_STAGE (meta_get_stage_for_screen (screen));
  if (!stage)
    return FALSE;

  window = clutter_x11_get_stage_window (stage);

  if (window == None)
    return FALSE;

  return (screen->display->focus_xwindow == window);
}

static gboolean
begin_modal_x11 (MetaCompositor   *compositor,
                 MetaPlugin       *plugin,
                 MetaModalOptions  options,
                 guint32           timestamp)
{
  MetaDisplay    *display     = compositor->display;
  Display        *xdpy        = meta_display_get_xdisplay (display);
  Window          grab_window = clutter_x11_get_stage_window (CLUTTER_STAGE (compositor->stage));
  int             result;
  gboolean        pointer_grabbed = FALSE;
  gboolean        keyboard_grabbed = FALSE;

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
                             None,
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

  return TRUE;

 fail:
  if (pointer_grabbed)
    XIUngrabDevice (xdpy, META_VIRTUAL_CORE_POINTER_ID, timestamp);
  if (keyboard_grabbed)
    XIUngrabDevice (xdpy, META_VIRTUAL_CORE_KEYBOARD_ID, timestamp);

  return FALSE;
}

gboolean
meta_begin_modal_for_plugin (MetaCompositor   *compositor,
                             MetaPlugin       *plugin,
                             MetaModalOptions  options,
                             guint32           timestamp)
{
  /* To some extent this duplicates code in meta_display_begin_grab_op(), but there
   * are significant differences in how we handle grabs that make it difficult to
   * merge the two.
   */
  MetaDisplay *display = compositor->display;

  if (is_modal (display) || display->grab_op != META_GRAB_OP_NONE)
    return FALSE;

  if (!meta_is_wayland_compositor ())
    if (!begin_modal_x11 (compositor, plugin, options, timestamp))
      return FALSE;

  display->grab_op = META_GRAB_OP_COMPOSITOR;
  display->grab_window = NULL;
  display->grab_have_pointer = TRUE;
  display->grab_have_keyboard = TRUE;

  if (meta_is_wayland_compositor ())
    meta_display_sync_wayland_input_focus (display);

  return TRUE;
}

void
meta_end_modal_for_plugin (MetaCompositor *compositor,
                           MetaPlugin     *plugin,
                           guint32         timestamp)
{
  MetaDisplay *display = compositor->display;
  Display *xdpy = meta_display_get_xdisplay (display);

  g_return_if_fail (is_modal (display));

  display->grab_op = META_GRAB_OP_NONE;
  display->grab_window = NULL;
  display->grab_have_pointer = FALSE;
  display->grab_have_keyboard = FALSE;

  if (meta_is_wayland_compositor ())
    {
      meta_display_sync_wayland_input_focus (display);
    }
  else
    {
      XIUngrabDevice (xdpy, META_VIRTUAL_CORE_POINTER_ID, timestamp);
      XIUngrabDevice (xdpy, META_VIRTUAL_CORE_KEYBOARD_ID, timestamp);
    }
}

static void
after_stage_paint (ClutterStage *stage,
                   gpointer      data)
{
  MetaCompositor *compositor = data;
  GList *l;

  for (l = compositor->windows; l; l = l->next)
    meta_window_actor_post_paint (l->data);

  if (meta_is_wayland_compositor ())
    meta_wayland_compositor_paint_finished (meta_wayland_compositor_get_default ());
}

static void
redirect_windows (MetaScreen *screen)
{
  MetaDisplay *display       = meta_screen_get_display (screen);
  Display     *xdisplay      = meta_display_get_xdisplay (display);
  Window       xroot         = meta_screen_get_xroot (screen);
  int          screen_number = meta_screen_get_screen_number (screen);
  guint        n_retries;
  guint        max_retries;

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
      meta_error_trap_push (display);
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
}

void
meta_compositor_manage (MetaCompositor *compositor)
{
  MetaDisplay *display = compositor->display;
  Display *xdisplay = display->xdisplay;
  MetaScreen *screen = display->screen;
  Window xwin = 0;
  gint width, height;

  meta_screen_set_cm_selection (display->screen);

  if (meta_is_wayland_compositor ())
    {
      MetaWaylandCompositor *wayland_compositor = meta_wayland_compositor_get_default ();

      compositor->stage = meta_stage_new ();
      clutter_actor_show (compositor->stage);

      wayland_compositor->stage = compositor->stage;

      meta_screen_get_size (screen, &width, &height);
      clutter_actor_set_size (compositor->stage, width, height);
    }
  else
    {
      compositor->stage = clutter_stage_new ();

      meta_screen_get_size (screen, &width, &height);
      clutter_actor_realize (compositor->stage);

      xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (compositor->stage));

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
          XIClearMask (mask.mask, XI_TouchBegin);
          XIClearMask (mask.mask, XI_TouchEnd);
          XIClearMask (mask.mask, XI_TouchUpdate);
          XISelectEvents (xdisplay, xwin, &mask, 1);

          event_mask = ExposureMask | PropertyChangeMask | StructureNotifyMask;
          if (XGetWindowAttributes (xdisplay, xwin, &attr))
            event_mask |= attr.your_event_mask;

          XSelectInput (xdisplay, xwin, event_mask);
        }
    }

  clutter_stage_set_paint_callback (CLUTTER_STAGE (compositor->stage),
                                    after_stage_paint,
                                    compositor,
                                    NULL);

  clutter_stage_set_sync_delay (CLUTTER_STAGE (compositor->stage), META_SYNC_DELAY);

  compositor->window_group = meta_window_group_new (screen);
  compositor->top_window_group = meta_window_group_new (screen);

  clutter_actor_add_child (compositor->stage, compositor->window_group);
  clutter_actor_add_child (compositor->stage, compositor->top_window_group);

  if (meta_is_wayland_compositor ())
    {
      /* NB: When running as a wayland compositor we don't need an X
       * composite overlay window, and we don't need to play any input
       * region tricks to redirect events into clutter. */
      compositor->output = None;
    }
  else
    {
      compositor->output = get_output_window (compositor);
      XReparentWindow (xdisplay, xwin, compositor->output, 0, 0);

      meta_empty_stage_input_region (screen);

      /* Make sure there isn't any left-over output shape on the 
       * overlay window by setting the whole screen to be an
       * output region.
       * 
       * Note: there doesn't seem to be any real chance of that
       *  because the X server will destroy the overlay window
       *  when the last client using it exits.
       */
      XFixesSetWindowShapeRegion (xdisplay, compositor->output, ShapeBounding, 0, 0, None);

      /* Map overlay window before redirecting windows offscreen so we catch their
       * contents until we show the stage.
       */
      XMapWindow (xdisplay, compositor->output);
    }

  redirect_windows (display->screen);

  compositor->plugin_mgr = meta_plugin_manager_new (compositor);
}

void
meta_compositor_unmanage (MetaCompositor *compositor)
{
  if (!meta_is_wayland_compositor ())
    {
      MetaDisplay *display = compositor->display;
      Display *xdisplay = meta_display_get_xdisplay (display);
      Window xroot = display->screen->xroot;

      /* This is the most important part of cleanup - we have to do this
       * before giving up the window manager selection or the next
       * window manager won't be able to redirect subwindows */
      XCompositeUnredirectSubwindows (xdisplay, xroot, CompositeRedirectManual);
    }
}

/**
 * meta_shape_cow_for_window:
 * @compositor: A #MetaCompositor
 * @window: (allow-none): A #MetaWindow to shape the COW for
 *
 * Sets an bounding shape on the COW so that the given window
 * is exposed. If @window is %NULL it clears the shape again.
 *
 * Used so we can unredirect windows, by shaping away the part
 * of the COW, letting the raw window be seen through below.
 */
static void
meta_shape_cow_for_window (MetaCompositor *compositor,
                           MetaWindow *window)
{
  MetaDisplay *display = compositor->display;
  Display *xdisplay = meta_display_get_xdisplay (display);

  if (window == NULL)
    XFixesSetWindowShapeRegion (xdisplay, compositor->output, ShapeBounding, 0, 0, None);
  else
    {
      XserverRegion output_region;
      XRectangle screen_rect, window_bounds;
      int width, height;
      MetaRectangle rect;

      meta_window_get_frame_rect (window, &rect);

      window_bounds.x = rect.x;
      window_bounds.y = rect.y;
      window_bounds.width = rect.width;
      window_bounds.height = rect.height;

      meta_screen_get_size (display->screen, &width, &height);
      screen_rect.x = 0;
      screen_rect.y = 0;
      screen_rect.width = width;
      screen_rect.height = height;

      output_region = XFixesCreateRegion (xdisplay, &window_bounds, 1);

      XFixesInvertRegion (xdisplay, output_region, &screen_rect, output_region);
      XFixesSetWindowShapeRegion (xdisplay, compositor->output, ShapeBounding, 0, 0, output_region);
      XFixesDestroyRegion (xdisplay, output_region);
    }
}

static void
set_unredirected_window (MetaCompositor *compositor,
                         MetaWindow     *window)
{
  if (compositor->unredirected_window == window)
    return;

  if (compositor->unredirected_window != NULL)
    {
      MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (compositor->unredirected_window));
      meta_window_actor_set_unredirected (window_actor, FALSE);
    }

  compositor->unredirected_window = window;

  if (compositor->unredirected_window != NULL)
    {
      MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (compositor->unredirected_window));
      meta_window_actor_set_unredirected (window_actor, TRUE);
    }

  meta_shape_cow_for_window (compositor, compositor->unredirected_window);
}

void
meta_compositor_add_window (MetaCompositor    *compositor,
                            MetaWindow        *window)
{
  MetaDisplay *display = compositor->display;

  meta_error_trap_push (display);

  meta_window_actor_new (window);
  sync_actor_stacking (compositor);

  meta_error_trap_pop (display);
}

void
meta_compositor_remove_window (MetaCompositor *compositor,
                               MetaWindow     *window)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));

  if (compositor->unredirected_window == window)
    set_unredirected_window (compositor, NULL);

  meta_window_actor_destroy (window_actor);
}

void
meta_compositor_set_updates_frozen (MetaCompositor *compositor,
                                    MetaWindow     *window,
                                    gboolean        updates_frozen)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  meta_window_actor_set_updates_frozen (window_actor, updates_frozen);
}

void
meta_compositor_queue_frame_drawn (MetaCompositor *compositor,
                                   MetaWindow     *window,
                                   gboolean        no_delay_frame)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  meta_window_actor_queue_frame_drawn (window_actor, no_delay_frame);
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
  if (!window_actor)
    return;

  meta_window_actor_update_shape (window_actor);
}

void
meta_compositor_window_opacity_changed (MetaCompositor *compositor,
                                        MetaWindow     *window)
{
  MetaWindowActor *window_actor;
  window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  if (!window_actor)
    return;

  meta_window_actor_update_opacity (window_actor);
}

void
meta_compositor_window_surface_changed (MetaCompositor *compositor,
                                        MetaWindow     *window)
{
  MetaWindowActor *window_actor;
  window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  if (!window_actor)
    return;

  meta_window_actor_update_surface (window_actor);
}

static gboolean
event_is_passive_button_grab (MetaDisplay   *display,
                              XIDeviceEvent *device_event)
{
  /* see display.c for which events are passive button
     grabs (meta_display_grab_window_buttons() and
     meta_display_handle_events())
     we need to filter them here because normally they
     would be sent to gtk+ (they are on gtk+ frame xwindow),
     but we want to redirect them to clutter
  */

  if (device_event->evtype != XI_ButtonPress)
    return FALSE;

  if (display->window_grab_modifiers == 0)
    return FALSE;

  if ((device_event->mods.effective & display->window_grab_modifiers) !=
      display->window_grab_modifiers)
    return FALSE;

  return device_event->detail < 4;
}

/* Clutter makes the assumption that there is only one X window
 * per stage, which is a valid assumption to make for a generic
 * application toolkit. As such, it will ignore any events sent
 * to the a stage that isn't its X window.
 *
 * When running as an X window manager, we need to respond to
 * events from lots of windows. Trick Clutter into translating
 * these events by pretending we got an event on the stage window.
 */
static void
maybe_spoof_event_as_stage_event (MetaCompositor *compositor,
                                  MetaWindow     *window,
                                  XEvent         *event)
{
  MetaDisplay *display = compositor->display;

  if (event->type == GenericEvent &&
      event->xcookie.extension == display->xinput_opcode)
    {
      XIEvent *input_event = (XIEvent *) event->xcookie.data;
      XIDeviceEvent *device_event = ((XIDeviceEvent *) input_event);

      switch (input_event->evtype)
        {
        case XI_Motion:
        case XI_ButtonPress:
        case XI_ButtonRelease:
          /* If this is a window frame, and we think GTK+ needs to handle the event,
             let GTK+ handle it without mangling */
          if (window && window->frame && device_event->event == window->frame->xwindow &&
              (meta_grab_op_is_clicking (display->grab_op) ||
               (display->grab_op == META_GRAB_OP_NONE && !event_is_passive_button_grab (display, device_event))))
            break;

        case XI_KeyPress:
        case XI_KeyRelease:
            /* If this is a GTK+ widget, like a window menu, let GTK+ handle
             * it as-is without mangling. */
            if (meta_ui_window_is_widget (display->screen->ui, device_event->event))
              break;

            device_event->event = clutter_x11_get_stage_window (CLUTTER_STAGE (compositor->stage));
            device_event->event_x = device_event->root_x;
            device_event->event_y = device_event->root_y;
          break;
        default:
          break;
        }
    }
}

/**
 * meta_compositor_process_event: (skip)
 * @compositor: 
 * @event: 
 * @window: 
 *
 */
gboolean
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
  MetaDisplay *display = compositor->display;

  if (is_modal (display) && is_grabbed_event (display, event))
    {
      meta_plugin_manager_xevent_filter (compositor->plugin_mgr, event);

      /* We always consume events even if the plugin says it didn't handle them;
       * exclusive is exclusive */
      return TRUE;
    }

  if (!meta_is_wayland_compositor ())
    maybe_spoof_event_as_stage_event (compositor, window, event);

  if (meta_plugin_manager_xevent_filter (compositor->plugin_mgr, event))
    return TRUE;

  if (!meta_is_wayland_compositor () &&
      event->type == meta_display_get_damage_event_base (compositor->display) + XDamageNotify)
    {
      /* Core code doesn't handle damage events, so we need to extract the MetaWindow
       * ourselves
       */
      if (window == NULL)
        {
          Window xwin = ((XDamageNotifyEvent *) event)->drawable;
          window = meta_display_lookup_x_window (compositor->display, xwin);
        }

      if (window)
        process_damage (compositor, (XDamageNotifyEvent *) event, window);
    }

  /* Clutter needs to know about MapNotify events otherwise it will
     think the stage is invisible */
  if (!meta_is_wayland_compositor () && event->type == MapNotify)
    clutter_x11_handle_event (event);

  /* The above handling is basically just "observing" the events, so we return
   * FALSE to indicate that the event should not be filtered out; if we have
   * GTK+ windows in the same process, GTK+ needs the ConfigureNotify event, for example.
   */
  return FALSE;
}

gboolean
meta_compositor_filter_keybinding (MetaCompositor *compositor,
                                   MetaKeyBinding *binding)
{
  return meta_plugin_manager_filter_keybinding (compositor->plugin_mgr, binding);
}

void
meta_compositor_show_window (MetaCompositor *compositor,
			     MetaWindow	    *window,
                             MetaCompEffect  effect)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
 meta_window_actor_show (window_actor, effect);
}

void
meta_compositor_hide_window (MetaCompositor *compositor,
                             MetaWindow     *window,
                             MetaCompEffect  effect)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  meta_window_actor_hide (window_actor, effect);
}

void
meta_compositor_maximize_window (MetaCompositor    *compositor,
                                 MetaWindow        *window,
				 MetaRectangle	   *old_rect,
				 MetaRectangle	   *new_rect)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  meta_window_actor_maximize (window_actor, old_rect, new_rect);
}

void
meta_compositor_unmaximize_window (MetaCompositor    *compositor,
                                   MetaWindow        *window,
				   MetaRectangle     *old_rect,
				   MetaRectangle     *new_rect)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  meta_window_actor_unmaximize (window_actor, old_rect, new_rect);
}

void
meta_compositor_switch_workspace (MetaCompositor     *compositor,
                                  MetaWorkspace      *from,
                                  MetaWorkspace      *to,
                                  MetaMotionDirection direction)
{
  gint to_indx, from_indx;

  to_indx   = meta_workspace_index (to);
  from_indx = meta_workspace_index (from);

  compositor->switch_workspace_in_progress++;

  if (!meta_plugin_manager_switch_workspace (compositor->plugin_mgr,
                                             from_indx,
                                             to_indx,
                                             direction))
    {
      compositor->switch_workspace_in_progress--;

      /* We have to explicitely call this to fix up stacking order of the
       * actors; this is because the abs stacking position of actors does not
       * necessarily change during the window hiding/unhiding, only their
       * relative position toward the destkop window.
       */
      meta_finish_workspace_switch (compositor);
    }
}

static void
sync_actor_stacking (MetaCompositor *compositor)
{
  GList *children;
  GList *expected_window_node;
  GList *tmp;
  GList *old;
  GList *backgrounds;
  gboolean has_windows;
  gboolean reordered;

  /* NB: The first entries in the lists are stacked the lowest */

  /* Restacking will trigger full screen redraws, so it's worth a
   * little effort to make sure we actually need to restack before
   * we go ahead and do it */

  children = clutter_actor_get_children (compositor->window_group);
  has_windows = FALSE;
  reordered = FALSE;

  /* We allow for actors in the window group other than the actors we
   * know about, but it's up to a plugin to try and keep them stacked correctly
   * (we really need extra API to make that reliable.)
   */

  /* First we collect a list of all backgrounds, and check if they're at the
   * bottom. Then we check if the window actors are in the correct sequence */
  backgrounds = NULL;
  expected_window_node = compositor->windows;
  for (old = children; old != NULL; old = old->next)
    {
      ClutterActor *actor = old->data;

      if (META_IS_BACKGROUND_GROUP (actor) ||
          META_IS_BACKGROUND_ACTOR (actor))
        {
          backgrounds = g_list_prepend (backgrounds, actor);

          if (has_windows)
            reordered = TRUE;
        }
      else if (META_IS_WINDOW_ACTOR (actor) && !reordered)
        {
          has_windows = TRUE;

          if (expected_window_node != NULL && actor == expected_window_node->data)
            expected_window_node = expected_window_node->next;
          else
            reordered = TRUE;
        }
    }

  g_list_free (children);

  if (!reordered)
    {
      g_list_free (backgrounds);
      return;
    }

  /* reorder the actors by lowering them in turn to the bottom of the stack.
   * windows first, then background.
   *
   * We reorder the actors even if they're not parented to the window group,
   * to allow stacking to work with intermediate actors (eg during effects)
   */
  for (tmp = g_list_last (compositor->windows); tmp != NULL; tmp = tmp->prev)
    {
      ClutterActor *actor = tmp->data, *parent;

      parent = clutter_actor_get_parent (actor);
      clutter_actor_set_child_below_sibling (parent, actor, NULL);
    }

  /* we prepended the backgrounds above so the last actor in the list
   * should get lowered to the bottom last.
   */
  for (tmp = backgrounds; tmp != NULL; tmp = tmp->next)
    {
      ClutterActor *actor = tmp->data, *parent;

      parent = clutter_actor_get_parent (actor);
      clutter_actor_set_child_below_sibling (parent, actor, NULL);
    }
  g_list_free (backgrounds);
}

void
meta_compositor_sync_stack (MetaCompositor  *compositor,
			    GList	    *stack)
{
  GList *old_stack;

  /* This is painful because hidden windows that we are in the process
   * of animating out of existence. They'll be at the bottom of the
   * stack of X windows, but we want to leave them in their old position
   * until the animation effect finishes.
   */

  /* Sources: first window is the highest */
  stack = g_list_copy (stack); /* The new stack of MetaWindow */
  old_stack = g_list_reverse (compositor->windows); /* The old stack of MetaWindowActor */
  compositor->windows = NULL;

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
      compositor->windows = g_list_prepend (compositor->windows, actor);

      stack = g_list_remove (stack, window);
      old_stack = g_list_remove (old_stack, actor);
    }

  sync_actor_stacking (compositor);
}

void
meta_compositor_sync_window_geometry (MetaCompositor *compositor,
				      MetaWindow *window,
                                      gboolean did_placement)
{
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
  meta_window_actor_sync_actor_geometry (window_actor, did_placement);
}

void
meta_compositor_sync_screen_size (MetaCompositor  *compositor,
				  guint		   width,
				  guint		   height)
{
  MetaDisplay *display = compositor->display;

  if (meta_is_wayland_compositor ())
    {
      /* FIXME: when we support a sliced stage, this is the place to do it
         But! This is not the place to apply KMS config, here we only
         notify Clutter/Cogl/GL that the framebuffer sizes changed.

         And because for now clutter does not do sliced, we use one
         framebuffer the size of the whole screen, and when running on
         bare metal MetaMonitorManager will do the necessary tricks to
         show the right portions on the right screens.
      */

      clutter_actor_set_size (compositor->stage, width, height);
    }
  else
    {
      Display        *xdisplay;
      Window          xwin;

      xdisplay = meta_display_get_xdisplay (display);
      xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (compositor->stage));

      XResizeWindow (xdisplay, xwin, width, height);
    }

  meta_verbose ("Changed size for stage on screen %d to %dx%d\n",
                meta_screen_get_screen_number (display->screen),
                width, height);
}

static void
frame_callback (CoglOnscreen  *onscreen,
                CoglFrameEvent event,
                CoglFrameInfo *frame_info,
                void          *user_data)
{
  MetaCompositor *compositor = user_data;
  GList *l;

  if (event == COGL_FRAME_EVENT_COMPLETE)
    {
      gint64 presentation_time_cogl = cogl_frame_info_get_presentation_time (frame_info);
      gint64 presentation_time;

      if (presentation_time_cogl != 0)
        {
          /* Cogl reports presentation in terms of its own clock, which is
           * guaranteed to be in nanoseconds but with no specified base. The
           * normal case with the open source GPU drivers on Linux 3.8 and
           * newer is that the base of cogl_get_clock_time() is that of
           * clock_gettime(CLOCK_MONOTONIC), so the same as g_get_monotonic_time),
           * but there's no exposure of that through the API. clock_gettime()
           * is fairly fast, so calling it twice and subtracting to get a
           * nearly-zero number is acceptable, if a litle ugly.
           */
          CoglContext *context = cogl_framebuffer_get_context (COGL_FRAMEBUFFER (onscreen));
          gint64 current_cogl_time = cogl_get_clock_time (context);
          gint64 current_monotonic_time = g_get_monotonic_time ();

          presentation_time =
            current_monotonic_time + (presentation_time_cogl - current_cogl_time) / 1000;
        }
      else
        {
          presentation_time = 0;
        }

      for (l = compositor->windows; l; l = l->next)
        meta_window_actor_frame_complete (l->data, frame_info, presentation_time);
    }
}

static void
pre_paint_windows (MetaCompositor *compositor)
{
  GList *l;
  MetaWindowActor *top_window;

  if (compositor->onscreen == NULL)
    {
      compositor->onscreen = COGL_ONSCREEN (cogl_get_draw_framebuffer ());
      compositor->frame_closure = cogl_onscreen_add_frame_callback (compositor->onscreen,
                                                                    frame_callback,
                                                                    compositor,
                                                                    NULL);
    }

  if (compositor->windows == NULL)
    return;

  top_window = g_list_last (compositor->windows)->data;

  if (meta_window_actor_should_unredirect (top_window) &&
      compositor->disable_unredirect_count == 0)
    set_unredirected_window (compositor, meta_window_actor_get_meta_window (top_window));
  else
    set_unredirected_window (compositor, NULL);

  for (l = compositor->windows; l; l = l->next)
    meta_window_actor_pre_paint (l->data);
}

static gboolean
meta_repaint_func (gpointer data)
{
  MetaCompositor *compositor = data;
  pre_paint_windows (compositor);
  return TRUE;
}

static void
on_shadow_factory_changed (MetaShadowFactory *factory,
                           MetaCompositor    *compositor)
{
  GList *l;

  for (l = compositor->windows; l; l = l->next)
    meta_window_actor_invalidate_shadow (l->data);
}

/**
 * meta_compositor_new: (skip)
 * @display:
 *
 */
MetaCompositor *
meta_compositor_new (MetaDisplay *display)
{
  MetaCompositor        *compositor;

  if (!composite_at_least_version (display, 0, 3))
    return NULL;

  compositor = g_new0 (MetaCompositor, 1);

  compositor->display = display;

  if (g_getenv("META_DISABLE_MIPMAPS"))
    compositor->no_mipmaps = TRUE;

  g_signal_connect (meta_shadow_factory_get_default (),
                    "changed",
                    G_CALLBACK (on_shadow_factory_changed),
                    compositor);

  compositor->repaint_func_id = clutter_threads_add_repaint_func (meta_repaint_func,
                                                                  compositor,
                                                                  NULL);

  return compositor;
}

/**
 * meta_get_overlay_window: (skip)
 * @screen: a #MetaScreen
 *
 */
Window
meta_get_overlay_window (MetaScreen *screen)
{
  MetaCompositor *compositor = get_compositor_for_screen (screen);
  return compositor->output;
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
  MetaCompositor *compositor = get_compositor_for_screen (screen);
  compositor->disable_unredirect_count++;
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
  MetaCompositor *compositor = get_compositor_for_screen (screen);
  if (compositor->disable_unredirect_count == 0)
    g_warning ("Called enable_unredirect_for_screen while unredirection is enabled.");
  if (compositor->disable_unredirect_count > 0)
    compositor->disable_unredirect_count--;
}

#define FLASH_TIME_MS 50

static void
flash_out_completed (ClutterTimeline *timeline,
                     gboolean         is_finished,
                     gpointer         user_data)
{
  ClutterActor *flash = CLUTTER_ACTOR (user_data);
  clutter_actor_destroy (flash);
}

void
meta_compositor_flash_screen (MetaCompositor *compositor,
                              MetaScreen     *screen)
{
  ClutterActor *stage;
  ClutterActor *flash;
  ClutterTransition *transition;
  gfloat width, height;

  stage = meta_get_stage_for_screen (screen);
  clutter_actor_get_size (stage, &width, &height);

  flash = clutter_actor_new ();
  clutter_actor_set_background_color (flash, CLUTTER_COLOR_Black);
  clutter_actor_set_size (flash, width, height);
  clutter_actor_set_opacity (flash, 0);
  clutter_actor_add_child (stage, flash);

  clutter_actor_save_easing_state (flash);
  clutter_actor_set_easing_mode (flash, CLUTTER_EASE_IN_QUAD);
  clutter_actor_set_easing_duration (flash, FLASH_TIME_MS);
  clutter_actor_set_opacity (flash, 192);

  transition = clutter_actor_get_transition (flash, "opacity");
  clutter_timeline_set_auto_reverse (CLUTTER_TIMELINE (transition), TRUE);
  clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (transition), 2);

  g_signal_connect (transition, "stopped",
                    G_CALLBACK (flash_out_completed), flash);

  clutter_actor_restore_easing_state (flash);
}

/**
 * meta_compositor_monotonic_time_to_server_time:
 * @display: a #MetaDisplay
 * @monotonic_time: time in the units of g_get_monotonic_time()
 *
 * _NET_WM_FRAME_DRAWN and _NET_WM_FRAME_TIMINGS messages represent time
 * as a "high resolution server time" - this is the server time interpolated
 * to microsecond resolution. The advantage of this time representation
 * is that if  X server is running on the same computer as a client, and
 * the Xserver uses 'clock_gettime(CLOCK_MONOTONIC, ...)' for the server
 * time, the client can detect this, and all such clients will share a
 * a time representation with high accuracy. If there is not a common
 * time source, then the time synchronization will be less accurate.
 */
gint64
meta_compositor_monotonic_time_to_server_time (MetaDisplay *display,
                                               gint64       monotonic_time)
{
  MetaCompositor *compositor = display->compositor;

  if (compositor->server_time_query_time == 0 ||
      (!compositor->server_time_is_monotonic_time &&
       monotonic_time > compositor->server_time_query_time + 10*1000*1000)) /* 10 seconds */
    {
      guint32 server_time = meta_display_get_current_time_roundtrip (display);
      gint64 server_time_usec = (gint64)server_time * 1000;
      gint64 current_monotonic_time = g_get_monotonic_time ();
      compositor->server_time_query_time = current_monotonic_time;

      /* If the server time is within a second of the monotonic time,
       * we assume that they are identical. This seems like a big margin,
       * but we want to be as robust as possible even if the system
       * is under load and our processing of the server response is
       * delayed.
       */
      if (server_time_usec > current_monotonic_time - 1000*1000 &&
          server_time_usec < current_monotonic_time + 1000*1000)
        compositor->server_time_is_monotonic_time = TRUE;

      compositor->server_time_offset = server_time_usec - current_monotonic_time;
    }

  if (compositor->server_time_is_monotonic_time)
    return monotonic_time;
  else
    return monotonic_time + compositor->server_time_offset;
}

void
meta_compositor_show_tile_preview (MetaCompositor *compositor,
                                   MetaWindow     *window,
                                   MetaRectangle  *tile_rect,
                                   int             tile_monitor_number)
{
  meta_plugin_manager_show_tile_preview (compositor->plugin_mgr,
                                         window, tile_rect, tile_monitor_number);
}

void
meta_compositor_hide_tile_preview (MetaCompositor *compositor)
{
  meta_plugin_manager_hide_tile_preview (compositor->plugin_mgr);
}
