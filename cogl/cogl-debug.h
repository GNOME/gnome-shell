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
#include "cogl-flags.h"
#include "cogl-util.h"

#include <glib.h>

COGL_BEGIN_DECLS

typedef enum {
  COGL_DEBUG_SLICING,
  COGL_DEBUG_OFFSCREEN,
  COGL_DEBUG_DRAW,
  COGL_DEBUG_PANGO,
  COGL_DEBUG_RECTANGLES,
  COGL_DEBUG_OBJECT,
  COGL_DEBUG_BLEND_STRINGS,
  COGL_DEBUG_DISABLE_BATCHING,
  COGL_DEBUG_DISABLE_VBOS,
  COGL_DEBUG_DISABLE_PBOS,
  COGL_DEBUG_JOURNAL,
  COGL_DEBUG_BATCHING,
  COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM,
  COGL_DEBUG_MATRICES,
  COGL_DEBUG_ATLAS,
  COGL_DEBUG_DUMP_ATLAS_IMAGE,
  COGL_DEBUG_DISABLE_ATLAS,
  COGL_DEBUG_DISABLE_SHARED_ATLAS,
  COGL_DEBUG_OPENGL,
  COGL_DEBUG_DISABLE_TEXTURING,
  COGL_DEBUG_DISABLE_ARBFP,
  COGL_DEBUG_DISABLE_FIXED,
  COGL_DEBUG_DISABLE_GLSL,
  COGL_DEBUG_SHOW_SOURCE,
  COGL_DEBUG_DISABLE_BLENDING,
  COGL_DEBUG_TEXTURE_PIXMAP,
  COGL_DEBUG_BITMAP,
  COGL_DEBUG_DISABLE_NPOT_TEXTURES,
  COGL_DEBUG_WIREFRAME,
  COGL_DEBUG_DISABLE_SOFTWARE_CLIP,
  COGL_DEBUG_DISABLE_PROGRAM_CACHES,
  COGL_DEBUG_DISABLE_FAST_READ_PIXEL,
  COGL_DEBUG_CLIPPING,
  COGL_DEBUG_WINSYS,
  COGL_DEBUG_PERFORMANCE,

  COGL_DEBUG_N_FLAGS
} CoglDebugFlags;

extern GHashTable *_cogl_debug_instances;
#define COGL_DEBUG_N_LONGS COGL_FLAGS_N_LONGS_FOR_SIZE (COGL_DEBUG_N_FLAGS)

/* _cogl_debug_flags currently needs to exported outside of the shared
   library for cogl-pango. The special COGL_EXPORT macro is needed to
   get this to work when building with MSVC */
COGL_EXPORT extern unsigned long _cogl_debug_flags[COGL_DEBUG_N_LONGS];

#define COGL_DEBUG_ENABLED(flag) \
  COGL_FLAGS_GET (_cogl_debug_flags, flag)

#define COGL_DEBUG_SET_FLAG(flag) \
  COGL_FLAGS_SET (_cogl_debug_flags, flag, TRUE)

#define COGL_DEBUG_CLEAR_FLAG(flag) \
  COGL_FLAGS_SET (_cogl_debug_flags, flag, FALSE)

#ifdef __GNUC__
#define COGL_NOTE(type,x,a...)                      G_STMT_START {            \
        if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_##type))) {            \
          _cogl_profile_trace_message ("[" #type "] " G_STRLOC " & " x, ##a); \
        }                                           } G_STMT_END

#else
#define COGL_NOTE(type,...)                         G_STMT_START {            \
        if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_##type))) {            \
          char *_fmt = g_strdup_printf (__VA_ARGS__);                         \
          _cogl_profile_trace_message ("[" #type "] " G_STRLOC " & %s", _fmt);\
          g_free (_fmt);                                                      \
        }                                           } G_STMT_END

#endif /* __GNUC__ */

void
_cogl_debug_check_environment (void);

void
_cogl_parse_debug_string (const char *value,
                          CoglBool enable,
                          CoglBool ignore_help);

COGL_END_DECLS

#endif /* __COGL_DEBUG_H__ */

