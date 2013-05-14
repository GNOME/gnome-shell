/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-xlib-renderer.h"
#include "cogl-util.h"
#include "cogl-object.h"

#include "cogl-output-private.h"
#include "cogl-renderer-private.h"
#include "cogl-xlib-renderer-private.h"
#include "cogl-x11-renderer-private.h"
#include "cogl-winsys-private.h"
#include "cogl-error-private.h"
#include "cogl-poll-private.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrandr.h>

#include <stdlib.h>
#include <string.h>

static char *_cogl_x11_display_name = NULL;
static GList *_cogl_xlib_renderers = NULL;

static void
destroy_xlib_renderer_data (void *user_data)
{
  g_slice_free (CoglXlibRenderer, user_data);
}

CoglXlibRenderer *
_cogl_xlib_renderer_get_data (CoglRenderer *renderer)
{
  static CoglUserDataKey key;
  CoglXlibRenderer *data;

  /* Constructs a CoglXlibRenderer struct on demand and attaches it to
     the object using user data. It's done this way instead of using a
     subclassing hierarchy in the winsys data because all EGL winsys's
     need the EGL winsys data but only one of them wants the Xlib
     data. */

  data = cogl_object_get_user_data (COGL_OBJECT (renderer), &key);

  if (data == NULL)
    {
      data = g_slice_new0 (CoglXlibRenderer);

      cogl_object_set_user_data (COGL_OBJECT (renderer),
                                 &key,
                                 data,
                                 destroy_xlib_renderer_data);
    }

  return data;
}

static void
register_xlib_renderer (CoglRenderer *renderer)
{
  GList *l;

  for (l = _cogl_xlib_renderers; l; l = l->next)
    if (l->data == renderer)
      return;

  _cogl_xlib_renderers = g_list_prepend (_cogl_xlib_renderers, renderer);
}

static void
unregister_xlib_renderer (CoglRenderer *renderer)
{
  _cogl_xlib_renderers = g_list_remove (_cogl_xlib_renderers, renderer);
}

static CoglRenderer *
get_renderer_for_xdisplay (Display *xdpy)
{
  GList *l;

  for (l = _cogl_xlib_renderers; l; l = l->next)
    {
      CoglRenderer *renderer = l->data;
      CoglXlibRenderer *xlib_renderer =
        _cogl_xlib_renderer_get_data (renderer);

      if (xlib_renderer->xdpy == xdpy)
        return renderer;
    }

  return NULL;
}

static int
error_handler (Display *xdpy,
               XErrorEvent *error)
{
  CoglRenderer *renderer;
  CoglXlibRenderer *xlib_renderer;

  renderer = get_renderer_for_xdisplay (xdpy);

  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);
  g_assert (xlib_renderer->trap_state);

  xlib_renderer->trap_state->trapped_error_code = error->error_code;

  return 0;
}

void
_cogl_xlib_renderer_trap_errors (CoglRenderer *renderer,
                                 CoglXlibTrapState *state)
{
  CoglXlibRenderer *xlib_renderer;

  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  state->trapped_error_code = 0;
  state->old_error_handler = XSetErrorHandler (error_handler);

  state->old_state = xlib_renderer->trap_state;
  xlib_renderer->trap_state = state;
}

int
_cogl_xlib_renderer_untrap_errors (CoglRenderer *renderer,
                                   CoglXlibTrapState *state)
{
  CoglXlibRenderer *xlib_renderer;

  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);
  g_assert (state == xlib_renderer->trap_state);

  XSetErrorHandler (state->old_error_handler);

  xlib_renderer->trap_state = state->old_state;

  return state->trapped_error_code;
}

static Display *
assert_xlib_display (CoglRenderer *renderer, CoglError **error)
{
  Display *xdpy = cogl_xlib_renderer_get_foreign_display (renderer);
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  /* A foreign display may have already been set... */
  if (xdpy)
    {
      xlib_renderer->xdpy = xdpy;
      return xdpy;
    }

  xdpy = XOpenDisplay (_cogl_x11_display_name);
  if (xdpy == NULL)
    {
      _cogl_set_error (error,
                   COGL_RENDERER_ERROR,
                   COGL_RENDERER_ERROR_XLIB_DISPLAY_OPEN,
                   "Failed to open X Display %s", _cogl_x11_display_name);
      return NULL;
    }

  xlib_renderer->xdpy = xdpy;
  return xdpy;
}

static int
compare_outputs (CoglOutput *a,
                 CoglOutput *b)
{
  return strcmp (a->name, b->name);
}

#define CSO(X) COGL_SUBPIXEL_ORDER_ ## X
static CoglSubpixelOrder subpixel_map[6][6] = {
  { CSO(UNKNOWN), CSO(NONE), CSO(HORIZONTAL_RGB), CSO(HORIZONTAL_BGR),
    CSO(VERTICAL_RGB),   CSO(VERTICAL_BGR) },   /* 0 */
  { CSO(UNKNOWN), CSO(NONE), CSO(VERTICAL_RGB),   CSO(VERTICAL_BGR),
    CSO(HORIZONTAL_BGR), CSO(HORIZONTAL_RGB) }, /* 90 */
  { CSO(UNKNOWN), CSO(NONE), CSO(HORIZONTAL_BGR), CSO(HORIZONTAL_RGB),
    CSO(VERTICAL_BGR),   CSO(VERTICAL_RGB) },   /* 180 */
  { CSO(UNKNOWN), CSO(NONE), CSO(VERTICAL_BGR),   CSO(VERTICAL_RGB),
    CSO(HORIZONTAL_RGB), CSO(HORIZONTAL_BGR) }, /* 270 */
  { CSO(UNKNOWN), CSO(NONE), CSO(HORIZONTAL_BGR), CSO(HORIZONTAL_RGB),
    CSO(VERTICAL_RGB),   CSO(VERTICAL_BGR) },   /* Reflect_X */
  { CSO(UNKNOWN), CSO(NONE), CSO(HORIZONTAL_RGB), CSO(HORIZONTAL_BGR),
    CSO(VERTICAL_BGR),   CSO(VERTICAL_RGB) },   /* Reflect_Y */
};
#undef CSO

static void
update_outputs (CoglRenderer *renderer,
                CoglBool notify)
{
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  XRRScreenResources *resources;
  CoglXlibTrapState state;
  CoglBool error = FALSE;
  GList *new_outputs = NULL;
  GList *l, *m;
  CoglBool changed = FALSE;
  int i;

  xlib_renderer->outputs_update_serial = XNextRequest (xlib_renderer->xdpy);

  resources = XRRGetScreenResources (xlib_renderer->xdpy,
                                     DefaultRootWindow (xlib_renderer->xdpy));

  _cogl_xlib_renderer_trap_errors (renderer, &state);

  for (i = 0; resources && i < resources->ncrtc && !error; i++)
    {
      XRRCrtcInfo *crtc_info = NULL;
      XRROutputInfo *output_info = NULL;
      CoglOutput *output;
      float refresh_rate = 0;
      int j;

      crtc_info = XRRGetCrtcInfo (xlib_renderer->xdpy,
                                  resources, resources->crtcs[i]);
      if (crtc_info == NULL)
        {
          error = TRUE;
          goto next;
        }

      if (crtc_info->mode == None)
        goto next;

      for (j = 0; j < resources->nmode; j++)
        {
          if (resources->modes[j].id == crtc_info->mode)
            refresh_rate = (resources->modes[j].dotClock /
                            ((float)resources->modes[j].hTotal *
                             resources->modes[j].vTotal));
        }

      output_info = XRRGetOutputInfo (xlib_renderer->xdpy,
                                      resources,
                                      crtc_info->outputs[0]);
      if (output_info == NULL)
        {
          error = TRUE;
          goto next;
        }

      output = _cogl_output_new (output_info->name);
      output->x = crtc_info->x;
      output->y = crtc_info->y;
      output->width = crtc_info->width;
      output->height = crtc_info->height;
      if ((crtc_info->rotation & (RR_Rotate_90 | RR_Rotate_270)) != 0)
        {
          output->mm_width = output_info->mm_height;
          output->mm_height = output_info->mm_width;
        }
      else
        {
          output->mm_width = output_info->mm_width;
          output->mm_height = output_info->mm_height;
        }

      output->refresh_rate = refresh_rate;

      switch (output_info->subpixel_order)
        {
        case SubPixelUnknown:
        default:
          output->subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
          break;
        case SubPixelNone:
          output->subpixel_order = COGL_SUBPIXEL_ORDER_NONE;
          break;
        case SubPixelHorizontalRGB:
          output->subpixel_order = COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB;
          break;
        case SubPixelHorizontalBGR:
          output->subpixel_order = COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR;
          break;
        case SubPixelVerticalRGB:
          output->subpixel_order = COGL_SUBPIXEL_ORDER_VERTICAL_RGB;
          break;
        case SubPixelVerticalBGR:
          output->subpixel_order = COGL_SUBPIXEL_ORDER_VERTICAL_BGR;
          break;
        }

      output->subpixel_order = COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB;

      /* Handle the effect of rotation and reflection on subpixel order (ugh) */
      for (j = 0; j < 6; j++)
        {
          if ((crtc_info->rotation & (1 << j)) != 0)
            output->subpixel_order = subpixel_map[j][output->subpixel_order];
        }

      new_outputs = g_list_prepend (new_outputs, output);

    next:
      if (crtc_info != NULL)
        XFree (crtc_info);

      if (output_info != NULL)
        XFree (output_info);
    }

  XFree (resources);

  if (!error)
    {
      new_outputs = g_list_sort (new_outputs, (GCompareFunc)compare_outputs);

      l = new_outputs;
      m = renderer->outputs;

      while (l || m)
        {
          int cmp;
          CoglOutput *output_l = l ? (CoglOutput *)l->data : NULL;
          CoglOutput *output_m = m ? (CoglOutput *)m->data : NULL;

          if (l && m)
            cmp = compare_outputs (output_l, output_m);
          else if (l)
            cmp = -1;
          else
            cmp = 1;

          if (cmp == 0)
            {
              GList *m_next = m->next;

              if (!_cogl_output_values_equal (output_l, output_m))
                {
                  renderer->outputs = g_list_remove_link (renderer->outputs, m);
                  renderer->outputs = g_list_insert_before (renderer->outputs,
                                                            m_next, output_l);
                  cogl_object_ref (output_l);

                  changed = TRUE;
                }

              l = l->next;
              m = m_next;
            }
          else if (cmp < 0)
            {
              renderer->outputs =
                g_list_insert_before (renderer->outputs, m, output_l);
              cogl_object_ref (output_l);
              changed = TRUE;
              l = l->next;
            }
          else
            {
              GList *m_next = m->next;
              renderer->outputs = g_list_remove_link (renderer->outputs, m);
              changed = TRUE;
              m = m_next;
            }
        }
    }

  g_list_free_full (new_outputs, (GDestroyNotify)cogl_object_unref);
  _cogl_xlib_renderer_untrap_errors (renderer, &state);

  if (changed)
    {
      const CoglWinsysVtable *winsys = renderer->winsys_vtable;

      if (notify)
        COGL_NOTE (WINSYS, "Outputs changed:");
      else
        COGL_NOTE (WINSYS, "Outputs:");

      for (l = renderer->outputs; l; l = l->next)
        {
          CoglOutput *output = l->data;
          const char *subpixel_string;

          switch (output->subpixel_order)
            {
            case COGL_SUBPIXEL_ORDER_UNKNOWN:
            default:
              subpixel_string = "unknown";
              break;
            case COGL_SUBPIXEL_ORDER_NONE:
              subpixel_string = "none";
              break;
            case COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB:
              subpixel_string = "horizontal_rgb";
              break;
            case COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR:
              subpixel_string = "horizontal_bgr";
              break;
            case COGL_SUBPIXEL_ORDER_VERTICAL_RGB:
              subpixel_string = "vertical_rgb";
              break;
            case COGL_SUBPIXEL_ORDER_VERTICAL_BGR:
              subpixel_string = "vertical_bgr";
              break;
            }

          COGL_NOTE (WINSYS,
                     " %10s: +%d+%dx%dx%d mm=%dx%d dpi=%.1fx%.1f "
                     "subpixel_order=%s refresh_rate=%.3f",
                     output->name,
                     output->x, output->y, output->width, output->height,
                     output->mm_width, output->mm_height,
                     output->width / (output->mm_width / 25.4),
                     output->height / (output->mm_height / 25.4),
                     subpixel_string,
                     output->refresh_rate);
        }

      if (notify && winsys->renderer_outputs_changed != NULL)
        winsys->renderer_outputs_changed (renderer);
    }
}

static CoglFilterReturn
randr_filter (XEvent *event,
              void   *data)
{
  CoglRenderer *renderer = data;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  CoglX11Renderer *x11_renderer =
    (CoglX11Renderer *) xlib_renderer;

  if (x11_renderer->randr_base != -1 &&
      (event->xany.type == x11_renderer->randr_base + RRScreenChangeNotify ||
       event->xany.type == x11_renderer->randr_base + RRNotify) &&
      event->xany.serial >= xlib_renderer->outputs_update_serial)
    update_outputs (renderer, TRUE);

  return COGL_FILTER_CONTINUE;
}

static int64_t
prepare_xlib_events_timeout (void *user_data)
{
  CoglRenderer *renderer = user_data;
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  return XPending (xlib_renderer->xdpy) ? 0 : -1;
}

static void
dispatch_xlib_events (void *user_data, int revents)
{
  CoglRenderer *renderer = user_data;
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  if (renderer->xlib_enable_event_retrieval)
    while (XPending (xlib_renderer->xdpy))
      {
        XEvent xevent;

        XNextEvent (xlib_renderer->xdpy, &xevent);

        cogl_xlib_renderer_handle_event (renderer, &xevent);
      }
}

CoglBool
_cogl_xlib_renderer_connect (CoglRenderer *renderer, CoglError **error)
{
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  CoglX11Renderer *x11_renderer =
    (CoglX11Renderer *) xlib_renderer;
  int damage_error;
  int randr_error;

  if (!assert_xlib_display (renderer, error))
    return FALSE;

  if (getenv ("COGL_X11_SYNC"))
    XSynchronize (xlib_renderer->xdpy, TRUE);

  /* Check whether damage events are supported on this display */
  if (!XDamageQueryExtension (xlib_renderer->xdpy,
                              &x11_renderer->damage_base,
                              &damage_error))
    x11_renderer->damage_base = -1;

  /* Check whether randr is supported on this display */
  if (!XRRQueryExtension (xlib_renderer->xdpy,
                          &x11_renderer->randr_base,
                          &randr_error))
    x11_renderer->randr_base = -1;

  xlib_renderer->trap_state = NULL;

  if (renderer->xlib_enable_event_retrieval)
    {
      _cogl_poll_renderer_add_fd (renderer,
                                  ConnectionNumber (xlib_renderer->xdpy),
                                  COGL_POLL_FD_EVENT_IN,
                                  prepare_xlib_events_timeout,
                                  dispatch_xlib_events,
                                  renderer);
    }

  XRRSelectInput(xlib_renderer->xdpy,
                 DefaultRootWindow (xlib_renderer->xdpy),
                 RRScreenChangeNotifyMask
                 | RRCrtcChangeNotifyMask
                 | RROutputPropertyNotifyMask);
  update_outputs (renderer, FALSE);

  register_xlib_renderer (renderer);

  cogl_xlib_renderer_add_filter (renderer,
                                 randr_filter,
                                 renderer);

  return TRUE;
}

void
_cogl_xlib_renderer_disconnect (CoglRenderer *renderer)
{
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);

  g_list_free_full (renderer->outputs, (GDestroyNotify)cogl_object_unref);
  renderer->outputs = NULL;

  if (!renderer->foreign_xdpy && xlib_renderer->xdpy)
    XCloseDisplay (xlib_renderer->xdpy);

  unregister_xlib_renderer (renderer);
}

Display *
cogl_xlib_renderer_get_display (CoglRenderer *renderer)
{
  CoglXlibRenderer *xlib_renderer;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  return xlib_renderer->xdpy;
}

CoglFilterReturn
cogl_xlib_renderer_handle_event (CoglRenderer *renderer,
                                 XEvent *event)
{
  return _cogl_renderer_handle_native_event (renderer, event);
}

void
cogl_xlib_renderer_add_filter (CoglRenderer *renderer,
                               CoglXlibFilterFunc func,
                               void *data)
{
  _cogl_renderer_add_native_filter (renderer,
                                    (CoglNativeFilterFunc)func, data);
}

void
cogl_xlib_renderer_remove_filter (CoglRenderer *renderer,
                                  CoglXlibFilterFunc func,
                                  void *data)
{
  _cogl_renderer_remove_native_filter (renderer,
                                       (CoglNativeFilterFunc)func, data);
}

int64_t
_cogl_xlib_renderer_get_dispatch_timeout (CoglRenderer *renderer)
{
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  if (renderer->xlib_enable_event_retrieval)
    {
      if (XPending (xlib_renderer->xdpy))
        return 0;
      else
        return -1;
    }
  else
    return -1;
}

CoglOutput *
_cogl_xlib_renderer_output_for_rectangle (CoglRenderer *renderer,
                                          int x,
                                          int y,
                                          int width,
                                          int height)
{
  int max_overlap = 0;
  CoglOutput *max_overlapped = NULL;
  GList *l;
  int xa1 = x, xa2 = x + width;
  int ya1 = y, ya2 = y + height;

  for (l = renderer->outputs; l; l = l->next)
    {
      CoglOutput *output = l->data;
      int xb1 = output->x, xb2 = output->x + output->width;
      int yb1 = output->y, yb2 = output->y + output->height;

      int overlap_x = MIN(xa2, xb2) - MAX(xa1, xb1);
      int overlap_y = MIN(ya2, yb2) - MAX(ya1, yb1);

      if (overlap_x > 0 && overlap_y > 0)
        {
          int overlap = overlap_x * overlap_y;
          if (overlap > max_overlap)
            {
              max_overlap = overlap;
              max_overlapped = output;
            }
        }
    }

  return max_overlapped;
}
