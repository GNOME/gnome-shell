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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl.h>
#include <cogl-internal.h>
#include <cogl-handle.h>
#include <cogl-context.h>
#include <cogl-framebuffer-private.h>

#include <X11/Xlib.h>

CoglXlibFilterReturn
_cogl_xlib_handle_event (XEvent *xevent)
{
  _COGL_GET_CONTEXT (ctx, COGL_XLIB_FILTER_CONTINUE);

  switch (xevent->type)
    {
      /* TODO... */
    default:
      break;
    }

  return COGL_XLIB_FILTER_CONTINUE;
}

