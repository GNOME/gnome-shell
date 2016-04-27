/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef __COGL_TEXTURE_PIXMAP_X11_PRIVATE_H
#define __COGL_TEXTURE_PIXMAP_X11_PRIVATE_H

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xdamage.h>

#include <sys/shm.h>

#ifdef COGL_HAS_GLX_SUPPORT
#include <GL/glx.h>
#endif

#include "cogl-object-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-pixmap-x11.h"

typedef struct _CoglDamageRectangle CoglDamageRectangle;

struct _CoglDamageRectangle
{
  unsigned int x1;
  unsigned int y1;
  unsigned int x2;
  unsigned int y2;
};

/* For stereo, there are a pair of textures, but we want to share most
 * other state (the GLXPixmap, visual, etc.) The way we do this is that
 * the left-eye texture has all the state (there is in fact, no internal
 * difference between the a MONO and a LEFT texture ), and the
 * right-eye texture simply points to the left eye texture, with all
 * other fields ignored.
 */
typedef enum
{
  COGL_TEXTURE_PIXMAP_MONO,
  COGL_TEXTURE_PIXMAP_LEFT,
  COGL_TEXTURE_PIXMAP_RIGHT
} CoglTexturePixmapStereoMode;

struct _CoglTexturePixmapX11
{
  CoglTexture _parent;

  CoglTexturePixmapStereoMode stereo_mode;
  CoglTexturePixmapX11 *left; /* Set only if stereo_mode=RIGHT */

  Pixmap pixmap;
  CoglTexture *tex;

  unsigned int depth;
  Visual *visual;

  XImage *image;

  XShmSegmentInfo shm_info;

  Damage damage;
  CoglTexturePixmapX11ReportLevel damage_report_level;
  CoglBool damage_owned;
  CoglDamageRectangle damage_rect;

  void *winsys;

  /* During the pre_paint method, this will be set to TRUE if we
     should use the winsys texture, otherwise we will use the regular
     texture */
  CoglBool use_winsys_texture;
};


#endif /* __COGL_TEXTURE_PIXMAP_X11_PRIVATE_H */
