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

static void
meta_frame_init_info (MetaFrame     *frame,
                      MetaFrameInfo *info)
{
  info->flags = 0;
  info->frame = frame->xwindow;
  info->display = frame->window->display->xdisplay;
  info->screen = frame->window->screen->xscreen;
  info->visual = frame->window->xvisual;
  info->depth = frame->window->depth;
  info->title = frame->window->title;
  info->width = frame->rect.width;
  info->height = frame->rect.height;
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
  geom.background_pixel = BlackPixel (window->display->xdisplay,
                                      window->screen->number);

  geom.shape_mask = None;

  window->screen->engine->fill_frame_geometry (&info, &geom,
                                               frame->theme_data);

  frame->child_x = geom.left_width;
  frame->child_y = geom.top_height;

  frame->rect.width = frame->rect.width + geom.left_width + geom.right_width;
  frame->rect.height = frame->rect.height + geom.top_height + geom.bottom_height;

  *geomp = geom;
}

void
meta_window_ensure_frame (MetaWindow *window)
{
  MetaFrame *frame;
  XSetWindowAttributes attrs;
  MetaFrameGeometry geom;
  
  if (window->frame)
    return;

  /* Need to fix Pango, it grabs the server */
  g_return_if_fail (window->display->server_grab_count == 0);
  
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
  
  attrs.background_pixel = geom.background_pixel;
  attrs.event_mask =
    StructureNotifyMask | SubstructureNotifyMask | ExposureMask |
    ButtonPressMask | ButtonReleaseMask | OwnerGrabButtonMask |
    PointerMotionMask | PointerMotionHintMask;
  
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
  frame->last_x = 0;
  frame->last_y = 0;
  frame->start_button = 0;
  
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
                               
  XMoveResizeWindow (frame->window->display->xdisplay,
                     frame->xwindow,
                     frame->rect.x,
                     frame->rect.y,
                     frame->rect.width,
                     frame->rect.height);
}

void
meta_frame_recalc_now (MetaFrame *frame)
{
  int old_child_x, old_child_y;
  MetaFrameGeometry geom;
  XSetWindowAttributes attrs;

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

  XMoveResizeWindow (frame->window->display->xdisplay,
                     frame->xwindow,
                     frame->rect.x,
                     frame->rect.y,
                     frame->rect.width,
                     frame->rect.height);

  attrs.background_pixel = geom.background_pixel;
  XChangeWindowAttributes (frame->window->display->xdisplay,
                           frame->xwindow,
                           CWBackPixel,
                           &attrs);

  meta_verbose ("Frame of %s recalculated to %d,%d %d x %d child %d,%d\n",
                frame->window->desc, frame->rect.x, frame->rect.y,
                frame->rect.width, frame->rect.height,
                frame->child_x, frame->child_y);
}

void
meta_frame_queue_recalc (MetaFrame *frame)
{
  /* FIXME */
  meta_frame_recalc_now (frame);
}

void
meta_frame_queue_draw (MetaFrame *frame)
{
  /* FIXME */

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
  int new_x, new_y;
  frame_query_root_pointer (frame, &x, &y);
  
  new_x = frame->rect.x + (x - frame->last_x);
  new_y = frame->rect.y + (y - frame->last_y);
  frame->last_x = x;
  frame->last_y = y;
  
  meta_frame_move (frame, new_x, new_y);
}

static void
update_resize_se (MetaFrame *frame)
{
  int x, y;
  int new_w, new_h;

  frame_query_root_pointer (frame, &x, &y);
  
  new_w = frame->window->rect.width + (x - frame->last_x);
  new_h = frame->window->rect.height + (y - frame->last_y);
  frame->last_x = x;
  frame->last_y = y;
  
  meta_window_resize (frame->window, new_w, new_h);
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
              frame->last_x = event->xbutton.x_root;
              frame->last_y = event->xbutton.y_root;
              frame->start_button = event->xbutton.button; 
            }
          else if (control == META_FRAME_CONTROL_DELETE &&
                   event->xbutton.button == 1)
            {
              /* FIXME delete event */
              meta_verbose ("Close control clicked on %s\n",
                            frame->window->desc);
            }
          else if (control == META_FRAME_CONTROL_RESIZE_SE &&
                   event->xbutton.button == 1)
            {
              meta_verbose ("Resize control clicked on %s\n",
                            frame->window->desc);
              frame->action = META_FRAME_ACTION_RESIZING_SE;
              frame->last_x = event->xbutton.x_root;
              frame->last_y = event->xbutton.y_root;
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
          
        default:
          break;
        }
      break;
    case EnterNotify:
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
      {
        MetaFrameInfo info;
        meta_frame_init_info (frame, &info);
        frame->window->screen->engine->expose_frame (&info,
                                                     event->xexpose.x,
                                                     event->xexpose.y,
                                                     event->xexpose.width,
                                                     event->xexpose.height,
                                                     frame->theme_data);
      }
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
