/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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

#include <glib.h>
#include <string.h>

#include "cogl-util.h"
#include "cogl-types.h"
#include "cogl-private.h"
#include "cogl-context-private.h"
#include "cogl-winsys-private.h"
#include "cogl-winsys-stub-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#ifdef COGL_HAS_XLIB_SUPPORT
#include "cogl-clutter-xlib.h"
#endif
#include "cogl-clutter.h"

CoglBool
cogl_clutter_check_extension (const char *name, const char *ext)
{
  char *end;
  int name_len, n;

  if (name == NULL || ext == NULL)
    return FALSE;

  end = (char*)(ext + strlen(ext));

  name_len = strlen(name);

  while (ext < end)
    {
      n = strcspn(ext, " ");

      if ((name_len == n) && (!strncmp(name, ext, n)))
	return TRUE;
      ext += (n + 1);
    }

  return FALSE;
}

CoglBool
cogl_clutter_winsys_has_feature (CoglWinsysFeature feature)
{
  return _cogl_winsys_has_feature (feature);
}

void
cogl_onscreen_clutter_backend_set_size (int width, int height)
{
  CoglFramebuffer *framebuffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (_cogl_context_get_winsys (ctx) != _cogl_winsys_stub_get_vtable ())
    return;

  framebuffer = COGL_FRAMEBUFFER (ctx->window_buffer);

  _cogl_framebuffer_winsys_update_size (framebuffer, width, height);
}

#ifdef COGL_HAS_XLIB_SUPPORT
XVisualInfo *
cogl_clutter_winsys_xlib_get_visual_info (void)
{
  const CoglWinsysVtable *winsys;

  _COGL_GET_CONTEXT (ctx, NULL);

  winsys = _cogl_context_get_winsys (ctx);

  /* This should only be called for xlib contexts */
  _COGL_RETURN_VAL_IF_FAIL (winsys->xlib_get_visual_info != NULL, NULL);

  return winsys->xlib_get_visual_info ();
}
#endif
