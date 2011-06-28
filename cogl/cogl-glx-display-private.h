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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_DISPLAY_GLX_PRIVATE_H
#define __COGL_DISPLAY_GLX_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-xlib-display-private.h"

typedef struct _CoglGLXCachedConfig
{
  /* This will be -1 if there is no cached config in this slot */
  int depth;
  gboolean found;
  GLXFBConfig fb_config;
  gboolean can_mipmap;
} CoglGLXCachedConfig;

#define COGL_GLX_N_CACHED_CONFIGS 3

typedef struct _CoglGLXDisplay
{
  CoglXlibDisplay _parent;

  CoglGLXCachedConfig glx_cached_configs[COGL_GLX_N_CACHED_CONFIGS];

  gboolean found_fbconfig;
  gboolean fbconfig_has_rgba_visual;
  GLXFBConfig fbconfig;

  /* Single context for all wins */
  GLXContext glx_context;
  GLXWindow dummy_glxwin;
} CoglGLXDisplay;

#endif /* __COGL_DISPLAY_GLX_PRIVATE_H */
