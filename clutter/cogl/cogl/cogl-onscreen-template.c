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

#include "cogl.h"
#include "cogl-object.h"

#include "cogl-onscreen-template-private.h"

static void _cogl_onscreen_template_free (CoglOnscreenTemplate *onscreen_template);

COGL_OBJECT_DEFINE (OnscreenTemplate, onscreen_template);

GQuark
cogl_onscreen_template_error_quark (void)
{
  return g_quark_from_static_string ("cogl-onscreen-template-error-quark");
}

static void
_cogl_onscreen_template_free (CoglOnscreenTemplate *onscreen_template)
{
  g_slice_free (CoglOnscreenTemplate, onscreen_template);
}

CoglOnscreenTemplate *
cogl_onscreen_template_new (CoglSwapChain *swap_chain)
{
  CoglOnscreenTemplate *onscreen_template = g_slice_new0 (CoglOnscreenTemplate);

  onscreen_template->swap_chain = swap_chain;
  if (swap_chain)
    cogl_object_ref (swap_chain);

  return _cogl_onscreen_template_object_new (onscreen_template);
}
