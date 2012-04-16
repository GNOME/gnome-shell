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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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

struct _CoglTexturePixmapX11
{
  CoglTexture _parent;

  Pixmap pixmap;
  CoglTexture *tex;

  unsigned int depth;
  Visual *visual;
  unsigned int width;
  unsigned int height;

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
