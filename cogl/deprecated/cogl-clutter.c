/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
