/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-framebuffer-private.h"

/* This provides a stub winsys implementation for when Clutter still handles
 * creating an OpenGL context. This is useful so we don't have to guard all
 * calls into the winsys layer with #ifdef COGL_HAS_FULL_WINSYS
 */

CoglFuncPtr
_cogl_winsys_get_proc_address (const char *name)
{
  return NULL;
}

void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{

}

void
_cogl_winsys_onscreen_swap_region (CoglOnscreen *onscreen,
                                   int *rectangles,
                                   int n_rectangles)
{

}

void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{

}

unsigned int
_cogl_winsys_onscreen_add_swap_buffers_callback (CoglOnscreen *onscreen,
                                                 CoglSwapBuffersNotify callback,
                                                 void *user_data)
{
  g_assert (0);
  return 0;
}

void
_cogl_winsys_onscreen_remove_swap_buffers_callback (CoglOnscreen *onscreen,
                                                    unsigned int id)
{
  g_assert (0);
}

#ifdef COGL_HAS_XLIB_SUPPORT
XVisualInfo *
_cogl_winsys_xlib_get_visual_info (void)
{
  g_assert (0);
  return NULL;
}
#endif

#ifdef COGL_HAS_X11_SUPPORT
guint32
_cogl_winsys_onscreen_x11_get_window_xid (CoglOnscreen *onscreen)
{
  g_assert (0);
  return 0;
}
#endif

gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  return TRUE;
}

void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{

}

void
_cogl_winsys_context_deinit (CoglContext *context)
{

}
