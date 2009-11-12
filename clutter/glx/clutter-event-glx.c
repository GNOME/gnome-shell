/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-x11.h"
#include "clutter-stage-x11.h"
#include "clutter-backend-x11.h"
#include "clutter-stage-glx.h"
#include "clutter-backend-glx.h"
#include "clutter-private.h"

#include <clutter/clutter-backend.h>
#include <clutter/clutter-stage-manager.h>

#include <X11/Xlib.h>

#include <GL/glxext.h>

#include <glib.h>

gboolean
clutter_backend_glx_handle_event (ClutterBackendX11 *backend_x11,
                                  XEvent            *xevent)
{
#ifdef GLX_INTEL_swap_event
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend_x11);
  ClutterStageManager *stage_manager;
  GLXBufferSwapComplete *swap_complete_event;
  const GSList *l;

  if (xevent->type != (backend_glx->event_base + GLX_BufferSwapComplete))
    return FALSE; /* Unhandled */

  swap_complete_event = (GLXBufferSwapComplete *)xevent;

#if 0
  {
    const char *event_name;
    if (swap_complete_event->event_type == GLX_EXCHANGE_COMPLETE_INTEL)
      event_name = "GLX_EXCHANGE_COMPLETE";
    else if (swap_complete_event->event_type == GLX_BLIT_COMPLETE_INTEL)
      event_name = "GLX_BLIT_COMPLETE";
    else
      {
        g_assert (swap_complete_event->event_type == GLX_FLIP_COMPLETE_INTEL);
        event_name = "GLX_FLIP_COMPLETE";
      }

    g_print ("XXX: clutter_backend_glx_event_handle event = %s\n",
             event_name);
  }
#endif

  stage_manager = clutter_stage_manager_get_default ();

  for (l = clutter_stage_manager_peek_stages (stage_manager); l; l = l->next)
    {
      ClutterStageWindow *stage_win = _clutter_stage_get_window (l->data);
      ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_win);
      ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_win);

      if (stage_x11->xwin == swap_complete_event->drawable)
        {
          g_assert (stage_glx->pending_swaps);
          stage_glx->pending_swaps--;
          return TRUE;
        }
    }

  return TRUE;
#else
  return FALSE;
#endif
}

