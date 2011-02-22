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

#include "cogl.h"
#include "cogl-types.h"
#include "cogl-private.h"
#include "cogl-context-private.h"
#include "cogl-winsys-private.h"
#include "cogl-framebuffer-private.h"

gboolean
cogl_clutter_check_extension (const char *name, const char *ext)
{
  return _cogl_check_extension (name, ext);
}

void
cogl_onscreen_clutter_backend_set_size (int width, int height)
{
  CoglFramebuffer *framebuffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!ctx->stub_winsys)
    return;

  framebuffer = COGL_FRAMEBUFFER (ctx->window_buffer);

  _cogl_framebuffer_winsys_update_size (framebuffer, width, height);
}
