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

  /* Fill in values that calc_geometry will use */
  frame->window = window;
  frame->xwindow = None;
  frame->theme_acquired = FALSE;

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
  attrs.event_mask =
    StructureNotifyMask | SubstructureNotifyMask | ExposureMask |
    ButtonPressMask | ButtonReleaseMask |
    PointerMotionMask | PointerMotionHintMask |
    EnterWindowMask | LeaveWindowMask;
  
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
  
  frame->action = META_FRAME_ACTION_NONE;
  
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
  if (window->iconic)
    meta_window_hide (window);
  else
    meta_window_show (window);
  
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
  if (window->iconic)
    meta_window_hide (window);
  else
    meta_window_show (window);
}

void
meta_frame_move  (MetaFrame *frame,
                  int        root_x,
                  int        root_y)
{
  frame->rect.x = root_x;
  frame->rect.y = root_y;
  
  XMoveWindow (frame->window->display->xdisplay,
               frame->xwindow,
               root_x, root_y);
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

static MetaFrameControl
frame_get_control (MetaFrame *frame,
                   int x, int y)
{
  MetaFrameInfo info;
  meta_frame_init_info (frame, &info);

  return frame->window->screen->engine->get_control (&info,
                                                     x, y,
                                                     frame->theme_data);
}

static void
update_move (MetaFrame *frame)
{
  int x, y;
  int dx, dy;
  
  frame_query_root_pointer (frame, &x, &y);
  
  dx = x - frame->start_root_x;
  dy = y - frame->start_root_y;
  
  meta_frame_move (frame,
                   frame->start_window_x + dx,
                   frame->start_window_y + dy);
}

static void
update_resize_se (MetaFrame *frame)
{
  int x, y;
  int dx, dy;
  
  frame_query_root_pointer (frame, &x, &y);
  
  dx = x - frame->start_root_x;
  dy = y - frame->start_root_y;
  
  meta_window_resize (frame->window,
                      frame->start_window_x + dx,
                      frame->start_window_y + dy);
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
      if (frame->action == META_FRAME_ACTION_NONE)
        {
          MetaFrameControl control;
          control = frame_get_control (frame,
                                       event->xbutton.x,
                                       event->xbutton.y);

          if (((control == META_FRAME_CONTROL_TITLE ||
                control == META_FRAME_CONTROL_NONE) &&
               event->xbutton.button == 1) ||
              event->xbutton.button == 2)
            {
              meta_verbose ("Begin move on %s\n",
                            frame->window->desc);
              frame->action = META_FRAME_ACTION_MOVING;
              frame->start_root_x = event->xbutton.x_root;
              frame->start_root_y = event->xbutton.y_root;
              frame->start_window_x = frame->rect.x;
              frame->start_window_y = frame->rect.y;
              frame->start_button = event->xbutton.button; 
            }
          else if (control == META_FRAME_CONTROL_DELETE &&
                   event->xbutton.button == 1)
            {
              /* FIXME delete event */
              meta_verbose ("Close control clicked on %s\n",
                            frame->window->desc);
              meta_window_delete (frame->window, event->xbutton.time);
            }
          else if (control == META_FRAME_CONTROL_RESIZE_SE &&
                   event->xbutton.button == 1)
            {
              meta_verbose ("Resize control clicked on %s\n",
                            frame->window->desc);
              frame->action = META_FRAME_ACTION_RESIZING_SE;
              frame->start_root_x = event->xbutton.x_root;
              frame->start_root_y = event->xbutton.y_root;
              frame->start_window_x = frame->window->rect.width;
              frame->start_window_y = frame->window->rect.height;
              frame->start_button = event->xbutton.button;
            }
        }
      break;
    case ButtonRelease:
      if (event->xbutton.button == frame->start_button)
        {
          switch (frame->action)
            {
            case META_FRAME_ACTION_MOVING:
              update_move (frame);
              break;
              
            case META_FRAME_ACTION_RESIZING_SE:
              update_resize_se (frame);
              break;
              
            default:
              break;
            }
          
          frame->action = META_FRAME_ACTION_NONE;
        }
      break;
    case MotionNotify:
      switch (frame->action)
        {
        case META_FRAME_ACTION_MOVING:
          update_move (frame);
          break;

        case META_FRAME_ACTION_RESIZING_SE:
          update_resize_se (frame);
          break;

        case META_FRAME_ACTION_NONE:
#if 0
          meta_ui_slave_show_tip (frame->window->screen->uislave,
                                  frame->rect.x,
                                  frame->rect.y,
                                  "Hi this is a tooltip");
#endif
          break;
        default:
          break;
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
      frame->action = META_FRAME_ACTION_NONE;
      break;
    case MapNotify:
      frame->action = META_FRAME_ACTION_NONE;
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
