/* Metacity X window decorations */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include "frame.h"
#include "errors.h"
#include "uislave.h"
#include "colors.h"

struct _MetaFrameActionGrab
{
  MetaFrameAction action;
  /* initial mouse position for drags */
  int start_root_x, start_root_y;
  /* initial window size or initial window position for drags */
  int start_window_x, start_window_y;
  /* button doing the dragging */
  int start_button;
};

/* This lacks ButtonReleaseMask to avoid the auto-grab
 * since it breaks our popup menu
 */
#define EVENT_MASK (StructureNotifyMask | SubstructureNotifyMask | \
                    ExposureMask |                                 \
                    ButtonPressMask | ButtonReleaseMask |          \
                    PointerMotionMask | PointerMotionHintMask |    \
                    EnterWindowMask | LeaveWindowMask)


static void clear_tip (MetaFrame *frame);

static void
meta_frame_init_info (MetaFrame     *frame,
                      MetaFrameInfo *info)
{
  info->flags =
    META_FRAME_ALLOWS_MENU | META_FRAME_ALLOWS_DELETE |
    META_FRAME_ALLOWS_ICONIFY | META_FRAME_ALLOWS_MAXIMIZE |
    META_FRAME_ALLOWS_RESIZE;

  if (frame->window->has_focus)
    info->flags |= META_FRAME_HAS_FOCUS;
  
  info->drawable = None;
  info->xoffset = 0;
  info->yoffset = 0;
  info->display = frame->window->display->xdisplay;
  info->screen = frame->window->screen->xscreen;
  info->visual = frame->window->xvisual;
  info->depth = frame->window->depth;
  info->title = frame->window->title;
  info->width = frame->rect.width;
  info->height = frame->rect.height;
  info->colors = &(frame->window->screen->colors);
  info->current_control = frame->current_control;
  if (frame->grab)
    info->current_control_state = META_STATE_ACTIVE;
  else
    info->current_control_state = META_STATE_PRELIGHT;
}

static void
meta_frame_calc_initial_pos (MetaFrame *frame,
                             int child_root_x, int child_root_y)
{
  MetaWindow *window;
  
  window = frame->window;
  
  switch (window->size_hints.win_gravity)
    {
    case NorthWestGravity:
      frame->rect.x = child_root_x;
      frame->rect.y = child_root_y;
      break;
    case NorthGravity:
      frame->rect.x = child_root_x - frame->rect.width / 2;
      frame->rect.y = child_root_y;
      break;
    case NorthEastGravity:
      frame->rect.x = child_root_x - frame->rect.width;
      frame->rect.y = child_root_y;
      break;
    case WestGravity:
      frame->rect.x = child_root_x;
      frame->rect.y = child_root_y - frame->rect.height / 2;
      break;
    case CenterGravity:
      frame->rect.x = child_root_x - frame->rect.width / 2;
      frame->rect.y = child_root_y - frame->rect.height / 2;
      break;
    case EastGravity:
      frame->rect.x = child_root_x - frame->rect.width;
      frame->rect.y = child_root_y - frame->rect.height / 2;
      break;
    case SouthWestGravity:
      frame->rect.x = child_root_x;
      frame->rect.y = child_root_y - frame->rect.height;
      break;
    case SouthGravity:
      frame->rect.x = child_root_x - frame->rect.width / 2;
      frame->rect.y = child_root_y - frame->rect.height;
      break;
    case SouthEastGravity:
      frame->rect.x = child_root_x - frame->rect.width;
      frame->rect.y = child_root_y - frame->rect.height;
      break;
    case StaticGravity:
    default:
      frame->rect.x = child_root_x - frame->child_x;
      frame->rect.y = child_root_y - frame->child_y;
      break;
    }
}

static void
meta_frame_calc_geometry (MetaFrame *frame,
                          int child_width, int child_height,
                          MetaFrameGeometry *geomp)
{
  MetaFrameInfo info;
  MetaFrameGeometry geom;
  MetaWindow *window;  
  
  /* Remember this is called from the constructor
   * pre-window-creation.
   */

  window = frame->window;
  
  frame->rect.width = child_width;
  frame->rect.height = child_height;
  
  meta_frame_init_info (frame, &info);

  if (!frame->theme_acquired)
    frame->theme_data = window->screen->engine->acquire_frame (&info);
  
  geom.left_width = 0;
  geom.right_width = 0;
  geom.top_height = 0;
  geom.bottom_height = 0;
  geom.background_pixel =
    meta_screen_get_x_pixel (frame->window->screen,
                             &frame->window->screen->colors.bg[META_STATE_NORMAL]);

  geom.shape_mask = None;

  window->screen->engine->fill_frame_geometry (&info, &geom,
                                               frame->theme_data);

  frame->child_x = geom.left_width;
  frame->child_y = geom.top_height;
  
  frame->rect.width = frame->rect.width + geom.left_width + geom.right_width;
  frame->rect.height = frame->rect.height + geom.top_height + geom.bottom_height;

  meta_debug_spew ("Added top %d and bottom %d totalling %d over child height %d\n",
                   geom.top_height, geom.bottom_height, frame->rect.height, child_height);
  
  frame->bg_pixel = geom.background_pixel;
  
  *geomp = geom;
}

static void
set_background_none (MetaFrame *frame)
{
  XSetWindowAttributes attrs;

  attrs.background_pixmap = None;
  XChangeWindowAttributes (frame->window->display->xdisplay,
                           frame->xwindow,
                           CWBackPixmap,
                           &attrs);

  meta_debug_spew ("Frame size %d,%d %dx%d window size %d,%d %dx%d window pos %d,%d\n",
                   frame->rect.x,
                   frame->rect.y,
                   frame->rect.width,
                   frame->rect.height,
                   frame->window->rect.x,
                   frame->window->rect.y,
                   frame->window->rect.width,
                   frame->window->rect.height,
                   frame->child_x,
                   frame->child_y);
}

static void
set_background_color (MetaFrame *frame)
{
  XSetWindowAttributes attrs;

  attrs.background_pixel = None;
  XChangeWindowAttributes (frame->window->display->xdisplay,
                           frame->xwindow,
                           CWBackPixel,
                           &attrs);
}

void
meta_window_ensure_frame (MetaWindow *window)
{
  MetaFrame *frame;
  XSetWindowAttributes attrs;
  MetaFrameGeometry geom;
  
  if (window->frame)
    return;
  
  frame = g_new (MetaFrame, 1);

  frame->window = window;
  frame->xwindow = None;
  frame->theme_acquired = FALSE;
  frame->grab = NULL;
  frame->current_control = META_FRAME_CONTROL_NONE;
  frame->tooltip_timeout = 0;
  
  /* This fills in frame->rect as well. */
  meta_frame_calc_geometry (frame,
                            window->rect.width,
                            window->rect.height,
                            &geom);

  meta_frame_calc_initial_pos (frame, window->rect.x, window->rect.y);
                               
  meta_verbose ("Will create frame %d,%d %dx%d around window %s %d,%d %dx%d with child position inside frame %d,%d and gravity %d\n",
                frame->rect.x, frame->rect.y,
                frame->rect.width, frame->rect.height,
                window->desc,
                window->rect.x, window->rect.y,
                window->rect.width, window->rect.height,
                frame->child_x, frame->child_y,
                window->size_hints.win_gravity);
  
  attrs.background_pixel = frame->bg_pixel;
  attrs.event_mask = EVENT_MASK;
  
  frame->xwindow = XCreateWindow (window->display->xdisplay,
                                  window->screen->xroot,
                                  frame->rect.x,
                                  frame->rect.y,
                                  frame->rect.width,
                                  frame->rect.height,
                                  0,
                                  window->depth,
                                  InputOutput,
                                  window->xvisual,
                                  CWBackPixel | CWEventMask,
                                  &attrs);

  meta_verbose ("Frame is 0x%lx\n", frame->xwindow);
  
  meta_display_register_x_window (window->display, &frame->xwindow, window);

  /* Reparent the client window; it may be destroyed,
   * thus the error trap. We'll get a destroy notify later
   * and free everything. Comment in FVWM source code says
   * we need the server grab or the child can get its MapNotify
   * before we've finished reparenting and getting the decoration
   * window onscreen.
   */
  meta_display_grab (window->display);

  meta_error_trap_push (window->display);
  window->mapped = FALSE; /* the reparent will unmap the window,
                           * we don't want to take that as a withdraw
                           */
  XReparentWindow (window->display->xdisplay,
                   window->xwindow,
                   frame->xwindow,
                   frame->child_x,
                   frame->child_y);
  meta_error_trap_pop (window->display);

  /* Update window's location */
  window->rect.x = frame->child_x;
  window->rect.y = frame->child_y;
  
  /* stick frame to the window */
  window->frame = frame;

  /* Put our state back where it should be */
  meta_window_queue_calc_showing (window);
  
  /* Ungrab server */
  meta_display_ungrab (window->display);
}

void
meta_window_destroy_frame (MetaWindow *window)
{
  MetaFrame *frame;
  MetaFrameInfo info;
  
  if (window->frame == NULL)
    return;

  frame = window->frame;

  if (frame->tooltip_timeout)
    clear_tip (frame);
  
  if (frame->theme_data)
    {
      meta_frame_init_info (frame, &info);
      window->screen->engine->release_frame (&info, frame->theme_data);
    }
  
  /* Unparent the client window; it may be destroyed,
   * thus the error trap.
   */
  meta_error_trap_push (window->display);
  window->mapped = FALSE; /* Keep track of unmapping it, so we
                           * can identify a withdraw initiated
                           * by the client.
                           */
  XReparentWindow (window->display->xdisplay,
                   window->xwindow,
                   window->screen->xroot,
                   /* FIXME where to put it back depends on the gravity */
                   window->frame->rect.x,
                   window->frame->rect.y);
  meta_error_trap_pop (window->display);

  meta_display_unregister_x_window (window->display,
                                    frame->xwindow);
  
  window->frame = NULL;

  /* should we push an error trap? */
  XDestroyWindow (window->display->xdisplay, frame->xwindow);
  
  g_free (frame);
  
  /* Put our state back where it should be */
  meta_window_queue_calc_showing (window);
}

/* Just a chunk of process_configure_event in window.c,
 * moved here since it's the part that deals with
 * the frame.
 */
void
meta_frame_child_configure_request (MetaFrame *frame)
{
  MetaFrameGeometry geom;
  
  /* This fills in frame->rect as well. */
  meta_frame_calc_geometry (frame,
                            frame->window->size_hints.width,
                            frame->window->size_hints.height,
                            &geom);

  meta_frame_calc_initial_pos (frame,
                               frame->window->size_hints.x,
                               frame->window->size_hints.y);

  set_background_none (frame);
  XMoveResizeWindow (frame->window->display->xdisplay,
                     frame->xwindow,
                     frame->rect.x,
                     frame->rect.y,
                     frame->rect.width,
                     frame->rect.height);
  set_background_color (frame);
}

void
meta_frame_recalc_now (MetaFrame *frame)
{
  int old_child_x, old_child_y;
  MetaFrameGeometry geom;

  old_child_x = frame->child_x;
  old_child_y = frame->child_y;
  
  /* This fills in frame->rect as well. */
  meta_frame_calc_geometry (frame,
                            frame->window->rect.width,
                            frame->window->rect.height,
                            &geom);

  /* See if we need to move the frame to keep child in
   * a constant position
   */
  if (old_child_x != frame->child_x)
    frame->rect.x += (frame->child_x - old_child_x);
  if (old_child_y != frame->child_y)
    frame->rect.y += (frame->child_y - old_child_y);

  set_background_none (frame);
  XMoveResizeWindow (frame->window->display->xdisplay,
                     frame->xwindow,
                     frame->rect.x,
                     frame->rect.y,
                     frame->rect.width,
                     frame->rect.height);
  set_background_color (frame);
  
  meta_verbose ("Frame of %s recalculated to %d,%d %d x %d child %d,%d\n",
                frame->window->desc, frame->rect.x, frame->rect.y,
                frame->rect.width, frame->rect.height,
                frame->child_x, frame->child_y);

  meta_frame_queue_draw (frame);
}

void
meta_frame_queue_recalc (MetaFrame *frame)
{
  /* FIXME, actually queue */
  meta_frame_recalc_now (frame);
}

static void
meta_frame_draw_now (MetaFrame *frame,
                     int x, int y, int width, int height)
{
  MetaFrameInfo info;
  Pixmap p;
  XGCValues vals;
  
  if (frame->xwindow == None)
    return;
  
  meta_frame_init_info (frame, &info);

  if (width < 0)
    width = frame->rect.width;

  if (height < 0)
    height = frame->rect.height;

  if (width == 0 || height == 0)
    return;
  
  p = XCreatePixmap (frame->window->display->xdisplay,
                     frame->xwindow,
                     width, height,
                     frame->window->screen->visual_info.depth);

  vals.foreground = frame->bg_pixel;
  XChangeGC (frame->window->display->xdisplay,
             frame->window->screen->scratch_gc,
             GCForeground,
             &vals);

  XFillRectangle (frame->window->display->xdisplay,
                  p,
                  frame->window->screen->scratch_gc,
                  0, 0,
                  width, height);

  info.drawable = p;
  info.xoffset = - x;
  info.yoffset = - y;
  frame->window->screen->engine->expose_frame (&info,
                                               0, 0, width, height,
                                               frame->theme_data);  


  XCopyArea (frame->window->display->xdisplay,
             p, frame->xwindow,
             frame->window->screen->scratch_gc,
             0, 0,
             width, height,
             x, y);

  XFreePixmap (frame->window->display->xdisplay,
               p);
}

void
meta_frame_queue_draw (MetaFrame *frame)
{
  /* FIXME, actually queue */
  meta_frame_draw_now (frame, 0, 0, -1, -1);
}

static void
frame_query_root_pointer (MetaFrame *frame,
                          int *x, int *y)
{
  Window root_return, child_return;
  int root_x_return, root_y_return;
  int win_x_return, win_y_return;
  unsigned int mask_return;

  XQueryPointer (frame->window->display->xdisplay,
                 frame->xwindow,
                 &root_return,
                 &child_return,
                 &root_x_return,
                 &root_y_return,
                 &win_x_return,
                 &win_y_return,
                 &mask_return);

  if (x)
    *x = root_x_return;
  if (y)
    *y = root_y_return;
}

static void
show_tip_now (MetaFrame *frame)
{
  const char *tiptext;

  tiptext = NULL;
  switch (frame->current_control)
    {
    case META_FRAME_CONTROL_TITLE:
      break;
    case META_FRAME_CONTROL_DELETE:
      tiptext = _("Close Window");
      break;
    case META_FRAME_CONTROL_MENU:
      tiptext = _("Menu");
      break;
    case META_FRAME_CONTROL_ICONIFY:
      tiptext = _("Minimize Window");
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      tiptext = _("Maximize Window");
      break;
    case META_FRAME_CONTROL_RESIZE_SE:
      break;
    case META_FRAME_CONTROL_RESIZE_S:
      break;
    case META_FRAME_CONTROL_RESIZE_SW:
      break;
    case META_FRAME_CONTROL_RESIZE_N:
      break;
    case META_FRAME_CONTROL_RESIZE_NE:
      break;
    case META_FRAME_CONTROL_RESIZE_NW:
      break;
    case META_FRAME_CONTROL_RESIZE_W:
      break;
    case META_FRAME_CONTROL_RESIZE_E:
      break;
    case META_FRAME_CONTROL_NONE:
      break;
    }

  if (tiptext)
    {
      int x, y, width, height;      
      MetaFrameInfo info;

      meta_frame_init_info (frame, &info);
      frame->window->screen->engine->get_control_rect (&info,
                                                       frame->current_control,
                                                       &x, &y, &width, &height,
                                                       frame->theme_data);

      /* Display tip a couple pixels below control */
      meta_screen_show_tip (frame->window->screen,
                            frame->rect.x + x,
                            frame->rect.y + y + height + 2,
                            tiptext);
    }
}

static gboolean
tip_timeout_func (gpointer data)
{
  MetaFrame *frame;

  frame = data;

  show_tip_now (frame);

  return FALSE;
}

#define TIP_DELAY 250
static void
queue_tip (MetaFrame *frame)
{
  if (frame->tooltip_timeout)
    g_source_remove (frame->tooltip_timeout);

  frame->tooltip_timeout = g_timeout_add (250,
                                          tip_timeout_func,
                                          frame);  
}

static void
clear_tip (MetaFrame *frame)
{
  if (frame->tooltip_timeout)
    {
      g_source_remove (frame->tooltip_timeout);
      frame->tooltip_timeout = 0;
    }
  meta_screen_hide_tip (frame->window->screen);
}

static MetaFrameControl
frame_get_control (MetaFrame *frame,
                   int x, int y)
{
  MetaFrameInfo info;

  if (x < 0 || y < 0 ||
      x > frame->rect.width || y > frame->rect.height)
    return META_FRAME_CONTROL_NONE;
  
  meta_frame_init_info (frame, &info);
  
  return frame->window->screen->engine->get_control (&info,
                                                     x, y,
                                                     frame->theme_data);
}

static void
update_move (MetaFrame *frame,
             int        x,
             int        y)
{
  int dx, dy;
  
  dx = x - frame->grab->start_root_x;
  dy = y - frame->grab->start_root_y;
  
  meta_window_move (frame->window,
                    frame->grab->start_window_x + dx,
                    frame->grab->start_window_y + dy);
}

static void
update_resize_se (MetaFrame *frame,
                  int x, int y)
{
  int dx, dy;
  
  dx = x - frame->grab->start_root_x;
  dy = y - frame->grab->start_root_y;
  
  meta_window_resize (frame->window,
                      frame->grab->start_window_x + dx,
                      frame->grab->start_window_y + dy);
}

static void
update_current_control (MetaFrame *frame,
                        int x_root, int y_root)
{
  MetaFrameControl old;

  if (frame->grab)
    return;
  
  old = frame->current_control;

  frame->current_control = frame_get_control (frame,
                                              x_root - frame->rect.x,
                                              y_root - frame->rect.y);

  if (old != frame->current_control)
    {
      meta_frame_queue_draw (frame);

      if (frame->current_control == META_FRAME_CONTROL_NONE)
        clear_tip (frame);
      else
        queue_tip (frame);
    }
}

static void
grab_action (MetaFrame      *frame,
             MetaFrameAction action,
             Time            time)
{
  meta_verbose ("Grabbing action %d\n", action);
  
  frame->grab = g_new0 (MetaFrameActionGrab, 1);
  
  if (XGrabPointer (frame->window->display->xdisplay,
                    frame->xwindow,
                    False,
                    ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask | PointerMotionHintMask,
                    GrabModeAsync, GrabModeAsync,
                    None,
                    None,
                    time) != GrabSuccess)
    meta_warning ("Grab for frame action failed\n");

  frame->grab->action = action;

  /* display ACTIVE state */
  meta_frame_queue_draw (frame);

  clear_tip (frame);
}

static void
ungrab_action (MetaFrame      *frame,
               Time            time)
{
  int x, y;

  meta_verbose ("Ungrabbing action %d\n", frame->grab->action);
  
  XUngrabPointer (frame->window->display->xdisplay,
                  time);
  
  g_free (frame->grab);
  frame->grab = NULL;
  
  frame_query_root_pointer (frame, &x, &y);
  update_current_control (frame, x, y);

  /* undisplay ACTIVE state */
  meta_frame_queue_draw (frame);

  queue_tip (frame);
}

gboolean
meta_frame_event (MetaFrame *frame,
                  XEvent    *event)
{
  switch (event->type)
    {
    case KeyPress:
      break;
    case KeyRelease:
      break;
    case ButtonPress:
      /* you can use button 2 to move a window without raising it */
      if (event->xbutton.button == 1)
        meta_window_raise (frame->window);
      
      update_current_control (frame,
                              event->xbutton.x_root,
                              event->xbutton.y_root);
      
      if (frame->grab == NULL)
        {
          MetaFrameControl control;
          control = frame->current_control;

          if (((control == META_FRAME_CONTROL_TITLE ||
                control == META_FRAME_CONTROL_NONE) &&
               event->xbutton.button == 1) ||
              event->xbutton.button == 2)
            {
              meta_verbose ("Begin move on %s\n",
                            frame->window->desc);
              grab_action (frame, META_FRAME_ACTION_MOVING,
                           event->xbutton.time);
              frame->grab->start_root_x = event->xbutton.x_root;
              frame->grab->start_root_y = event->xbutton.y_root;
              /* pos of client in root coords */
              frame->grab->start_window_x =
                frame->rect.x + frame->window->rect.x;
              frame->grab->start_window_y =
                frame->rect.y + frame->window->rect.y;
              frame->grab->start_button = event->xbutton.button; 
            }
          else if (control == META_FRAME_CONTROL_DELETE &&
                   event->xbutton.button == 1)
            {
              /* FIXME delete event */
              meta_verbose ("Close control clicked on %s\n",
                            frame->window->desc);
              grab_action (frame, META_FRAME_ACTION_DELETING,
                           event->xbutton.time);
              frame->grab->start_button = event->xbutton.button;
            }
          else if (control == META_FRAME_CONTROL_RESIZE_SE &&
                   event->xbutton.button == 1)
            {
              meta_verbose ("Resize control clicked on %s\n",
                            frame->window->desc);
              grab_action (frame, META_FRAME_ACTION_RESIZING_SE,
                           event->xbutton.time);
              frame->grab->start_root_x = event->xbutton.x_root;
              frame->grab->start_root_y = event->xbutton.y_root;
              frame->grab->start_window_x = frame->window->rect.width;
              frame->grab->start_window_y = frame->window->rect.height;
              frame->grab->start_button = event->xbutton.button;
            }
          else if (control == META_FRAME_CONTROL_MENU &&
                   event->xbutton.button == 1)
            {
              int x, y, width, height;      
              MetaFrameInfo info;

              meta_verbose ("Menu control clicked on %s\n",
                            frame->window->desc);
              
              meta_frame_init_info (frame, &info);
              frame->window->screen->engine->get_control_rect (&info,
                                                               META_FRAME_CONTROL_MENU,
                                                               &x, &y, &width, &height,
                                                               frame->theme_data);

              /* Let the menu get a grab. The user could release button
               * before the menu gets the grab, in which case the
               * menu gets somewhat confused, but it's not that
               * disastrous.
               */
              XUngrabPointer (frame->window->display->xdisplay,
                              event->xbutton.time);
              
              meta_ui_slave_show_window_menu (frame->window->screen->uislave,
                                              frame->window,
                                              frame->rect.x + x,
                                              frame->rect.y + y + height,
                                              event->xbutton.button,
                                              META_MESSAGE_MENU_ALL,
                                              META_MESSAGE_MENU_MINIMIZE,
                                              event->xbutton.time);      
            }
        }
      break;
    case ButtonRelease:
      if (frame->grab)
        meta_debug_spew ("Here! grab %p action %d buttons %d %d\n",
                         frame->grab, frame->grab->action, frame->grab->start_button, event->xbutton.button);
      if (frame->grab &&
          event->xbutton.button == frame->grab->start_button)
        {
          switch (frame->grab->action)
            {
            case META_FRAME_ACTION_MOVING:
              update_move (frame, event->xbutton.x_root, event->xbutton.y_root);
              ungrab_action (frame, event->xbutton.time);
              update_current_control (frame,
                                      event->xbutton.x_root, event->xbutton.y_root);
              break;
              
            case META_FRAME_ACTION_RESIZING_SE:
              update_resize_se (frame, event->xbutton.x_root, event->xbutton.y_root);
              ungrab_action (frame, event->xbutton.time);
              update_current_control (frame,
                                      event->xbutton.x_root, event->xbutton.y_root);
              break;

            case META_FRAME_ACTION_DELETING:
              /* Must ungrab before getting "real" control position */
              ungrab_action (frame, event->xbutton.time);
              update_current_control (frame,
                                      event->xbutton.x_root,
                                      event->xbutton.y_root);
              /* delete if we're still over the button */
              if (frame->current_control == META_FRAME_CONTROL_DELETE)
                meta_window_delete (frame->window, event->xbutton.time);
              break;
            default:
              meta_warning ("Unhandled action in button release\n");
              break;
            }
        }
      break;
    case MotionNotify:
      {
        int x, y;

        frame_query_root_pointer (frame, &x, &y);
        if (frame->grab)
          {
            switch (frame->grab->action)
              {
              case META_FRAME_ACTION_MOVING:
                update_move (frame, x, y);
                break;
                
              case META_FRAME_ACTION_RESIZING_SE:
                update_resize_se (frame, x, y);
                break;
                
              case META_FRAME_ACTION_NONE:
                
                break;
              default:
                break;
              }
          }
        else
          {
            update_current_control (frame, x, y);
          }
        }
      break;
    case EnterNotify:
      /* We handle it here if a decorated window
       * is involved, otherwise we handle it in display.c
       */
      /* do this even if window->has_focus to avoid races */
      meta_window_focus (frame->window,
                         event->xcrossing.time);
      break;
    case LeaveNotify:
      update_current_control (frame, -1, -1);
      break;
    case FocusIn:
      break;
    case FocusOut:
      break;
    case KeymapNotify:
      break;
    case Expose:
      meta_frame_draw_now (frame,
                           event->xexpose.x,
                           event->xexpose.y,
                           event->xexpose.width,
                           event->xexpose.height);
      break;
    case GraphicsExpose:
      break;
    case NoExpose:
      break;
    case VisibilityNotify:
      break;
    case CreateNotify:
      break;
    case DestroyNotify:
      {
        MetaDisplay *display;
        
        meta_warning ("Unexpected destruction of frame 0x%lx, not sure if this should silently fail or be considered a bug\n", frame->xwindow);
        display = frame->window->display;
        meta_error_trap_push (display);
        meta_window_destroy_frame (frame->window);
        meta_error_trap_pop (display);
        return TRUE;
      }
      break;
    case UnmapNotify:
      if (frame->grab)
        ungrab_action (frame, CurrentTime);
      break;
    case MapNotify:
      if (frame->grab)
        ungrab_action (frame, CurrentTime);
      break;
    case MapRequest:
      break;
    case ReparentNotify:
      break;
    case ConfigureNotify:
      break;
    case ConfigureRequest:
      break;
    case GravityNotify:
      break;
    case ResizeRequest:
      break;
    case CirculateNotify:
      break;
    case CirculateRequest:
      break;
    case PropertyNotify:
      break;
    case SelectionClear:
      break;
    case SelectionRequest:
      break;
    case SelectionNotify:
      break;
    case ColormapNotify:
      break;
    case ClientMessage:
      break;
    case MappingNotify:
      break;
    default:
      break;
    }

  return FALSE;
}
