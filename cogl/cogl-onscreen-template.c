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
