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
 *
 */

#ifndef __COGL_DISPLAY_GLX_PRIVATE_H
#define __COGL_DISPLAY_GLX_PRIVATE_H

#include "cogl-object-private.h"

typedef struct _CoglGLXCachedConfig
{
  /* This will be -1 if there is no cached config in this slot */
  int depth;
  CoglBool found;
  GLXFBConfig fb_config;
  CoglBool can_mipmap;
} CoglGLXCachedConfig;

#define COGL_GLX_N_CACHED_CONFIGS 3

typedef struct _CoglGLXDisplay
{
  CoglGLXCachedConfig glx_cached_configs[COGL_GLX_N_CACHED_CONFIGS];

  CoglBool found_fbconfig;
  CoglBool fbconfig_has_rgba_visual;
  GLXFBConfig fbconfig;

  /* Single context for all wins */
  GLXContext glx_context;
  GLXWindow dummy_glxwin;
  Window dummy_xwin;
} CoglGLXDisplay;

#endif /* __COGL_DISPLAY_GLX_PRIVATE_H */
