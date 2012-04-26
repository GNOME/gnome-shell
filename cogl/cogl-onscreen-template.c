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

#include "cogl-object.h"

#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-template-private.h"

#include <stdlib.h>

static void _cogl_onscreen_template_free (CoglOnscreenTemplate *onscreen_template);

COGL_OBJECT_DEFINE (OnscreenTemplate, onscreen_template);

static void
_cogl_onscreen_template_free (CoglOnscreenTemplate *onscreen_template)
{
  g_slice_free (CoglOnscreenTemplate, onscreen_template);
}

CoglOnscreenTemplate *
cogl_onscreen_template_new (CoglSwapChain *swap_chain)
{
  CoglOnscreenTemplate *onscreen_template = g_slice_new0 (CoglOnscreenTemplate);
  char *user_config;

  onscreen_template->config.swap_chain = swap_chain;
  if (swap_chain)
    cogl_object_ref (swap_chain);
  else
    onscreen_template->config.swap_chain = cogl_swap_chain_new ();

  onscreen_template->config.swap_throttled = TRUE;
  onscreen_template->config.need_stencil = TRUE;
  onscreen_template->config.samples_per_pixel = 0;

  user_config = getenv ("COGL_POINT_SAMPLES_PER_PIXEL");
  if (user_config)
    {
      unsigned long samples_per_pixel = strtoul (user_config, NULL, 10);
      if (samples_per_pixel != ULONG_MAX)
        onscreen_template->config.samples_per_pixel =
          samples_per_pixel;
    }

  return _cogl_onscreen_template_object_new (onscreen_template);
}

void
cogl_onscreen_template_set_samples_per_pixel (
                                        CoglOnscreenTemplate *onscreen_template,
                                        int samples_per_pixel)
{
  onscreen_template->config.samples_per_pixel = samples_per_pixel;
}

void
cogl_onscreen_template_set_swap_throttled (
                                          CoglOnscreenTemplate *onscreen_template,
                                          CoglBool throttled)
{
  onscreen_template->config.swap_throttled = throttled;
}
