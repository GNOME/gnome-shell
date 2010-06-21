/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_DEBUG_H__
#define __COGL_DEBUG_H__

#include "cogl-profile.h"

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  COGL_DEBUG_SLICING          = 1 << 1,
  COGL_DEBUG_OFFSCREEN        = 1 << 2,
  COGL_DEBUG_DRAW             = 1 << 3,
  COGL_DEBUG_PANGO            = 1 << 4,
  COGL_DEBUG_RECTANGLES       = 1 << 5,
  COGL_DEBUG_HANDLE           = 1 << 6,
  COGL_DEBUG_BLEND_STRINGS    = 1 << 7,
  COGL_DEBUG_DISABLE_BATCHING = 1 << 8,
  COGL_DEBUG_DISABLE_VBOS     = 1 << 9,
  COGL_DEBUG_DISABLE_PBOS     = 1 << 10,
  COGL_DEBUG_JOURNAL          = 1 << 11,
  COGL_DEBUG_BATCHING         = 1 << 12,
  COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM = 1 << 13,
  COGL_DEBUG_MATRICES         = 1 << 14,
  COGL_DEBUG_ATLAS            = 1 << 15,
  COGL_DEBUG_DUMP_ATLAS_IMAGE = 1 << 16,
  COGL_DEBUG_DISABLE_ATLAS    = 1 << 17,
  COGL_DEBUG_OPENGL           = 1 << 18,
  COGL_DEBUG_DISABLE_TEXTURING = 1 << 19,
  COGL_DEBUG_DISABLE_ARBFP    = 1 << 20,
  COGL_DEBUG_DISABLE_GLSL     = 1 << 21,
  COGL_DEBUG_SHOW_SOURCE      = 1 << 22,
  COGL_DEBUG_DISABLE_BLENDING = 1 << 23,
  COGL_DEBUG_TEXTURE_PIXMAP   = 1 << 24,
  COGL_DEBUG_BITMAP           = 1 << 25
} CoglDebugFlags;

#ifdef COGL_ENABLE_DEBUG

#ifdef __GNUC__
#define COGL_NOTE(type,x,a...)                      G_STMT_START {            \
        if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_##type)) {              \
          _cogl_profile_trace_message ("[" #type "] " G_STRLOC " & " x, ##a); \
        }                                           } G_STMT_END

#else
#define COGL_NOTE(type,...)                         G_STMT_START {            \
        if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_##type)) {              \
          char *_fmt = g_strdup_printf (__VA_ARGS__);                         \
          _cogl_profile_trace_message ("[" #type "] " G_STRLOC " & %s", _fmt);\
          g_free (_fmt);                                                      \
        }                                           } G_STMT_END

#endif /* __GNUC__ */

#else /* !COGL_ENABLE_DEBUG */

#define COGL_NOTE(type,...) G_STMT_START {} G_STMT_END

#endif /* COGL_ENABLE_DEBUG */

extern unsigned int cogl_debug_flags;

G_END_DECLS

#endif /* __COGL_DEBUG_H__ */

