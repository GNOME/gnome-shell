/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

#ifndef __COGL_H__
#define __COGL_H__

#define __COGL_H_INSIDE__

#ifdef COGL_ENABLE_EXPERIMENTAL_2_0_API
#ifndef COGL_ENABLE_EXPERIMENTAL_API
#define COGL_ENABLE_EXPERIMENTAL_API
#endif
#endif

/* We currently keep gtype integration delimited in case we eventually
 * want to split it out into a separate utility library when Cogl
 * becomes a standalone project. (like cairo-gobject.so)
 */
#define _COGL_SUPPORTS_GTYPE_INTEGRATION

/*
 * API common to the 1.x and 2.0 api...
 */

#include <cogl/cogl-defines.h>
#include <cogl/cogl-macros.h>

#include <cogl/cogl-error.h>

#include <cogl/cogl-object.h>
#include <cogl/cogl1-context.h>
#include <cogl/cogl-bitmap.h>
#include <cogl/cogl-color.h>
#include <cogl/cogl-matrix.h>
#include <cogl/cogl-matrix-stack.h>
#include <cogl/cogl-offscreen.h>
#include <cogl/cogl-primitives.h>
#include <cogl/cogl-texture.h>
#include <cogl/cogl-types.h>
#include <cogl/cogl-version.h>

/*
 * 1.x only api...
 */
#ifndef COGL_ENABLE_EXPERIMENTAL_2_0_API
#include <cogl/cogl-clip-state.h>
#include <cogl/cogl-enum-types.h>
#include <cogl/deprecated/cogl-vertex-buffer.h>
#include <cogl/deprecated/cogl-fixed.h>
#include <cogl/deprecated/cogl-material-compat.h>
#include <cogl/deprecated/cogl-shader.h>
#endif

/*
 * 2.0 api that's compatible with the 1.x api...
 */
#if defined (COGL_ENABLE_EXPERIMENTAL_API)
#include <cogl/cogl-swap-chain.h>
#include <cogl/cogl-renderer.h>
#include <cogl/cogl-output.h>
#include <cogl/cogl-display.h>
#include <cogl/cogl-context.h>
#include <cogl/cogl-buffer.h>
#include <cogl/cogl-pixel-buffer.h>
#include <cogl/cogl-vector.h>
#include <cogl/cogl-euler.h>
#include <cogl/cogl-quaternion.h>
#include <cogl/cogl-texture-2d.h>
#include <cogl/cogl-texture-2d-gl.h>
#include <cogl/cogl-texture-rectangle.h>
#include <cogl/cogl-texture-3d.h>
#include <cogl/cogl-texture-2d-sliced.h>
#include <cogl/cogl-sub-texture.h>
#include <cogl/cogl-atlas-texture.h>
#include <cogl/cogl-meta-texture.h>
#include <cogl/cogl-primitive-texture.h>
#include <cogl/cogl-index-buffer.h>
#include <cogl/cogl-attribute-buffer.h>
#include <cogl/cogl-indices.h>
#include <cogl/cogl-attribute.h>
#include <cogl/cogl-primitive.h>
#include <cogl/cogl-depth-state.h>
#include <cogl/cogl-pipeline.h>
#include <cogl/cogl-pipeline-state.h>
#include <cogl/cogl-pipeline-layer-state.h>
#include <cogl/cogl-snippet.h>
#include <cogl/cogl-framebuffer.h>
#include <cogl/cogl-onscreen.h>
#include <cogl/cogl-frame-info.h>
#include <cogl/cogl-poll.h>
#include <cogl/cogl-fence.h>
#if defined (COGL_HAS_EGL_PLATFORM_KMS_SUPPORT)
#include <cogl/cogl-kms-renderer.h>
#include <cogl/cogl-kms-display.h>
#endif
#ifdef COGL_HAS_WIN32_SUPPORT
#include <cogl/cogl-win32-renderer.h>
#endif
#ifdef COGL_HAS_GLIB_SUPPORT
#include <cogl/cogl-glib-source.h>
#endif
/* XXX: This will definitly go away once all the Clutter winsys
 * code has been migrated down into Cogl! */
#include <cogl/deprecated/cogl-clutter.h>
#endif
#ifdef COGL_HAS_SDL_SUPPORT
#include <cogl/cogl-sdl.h>
#endif

/*
 * API deprecations
 */
#include <cogl/cogl-deprecated.h>

/*
 * Cogl Path api compatability
 *
 * The cogl_path_ api used to be part of the core Cogl api so for
 * compatability we include cogl-path.h via cogl.h
 */
#if defined (COGL_HAS_COGL_PATH_SUPPORT)
#include <cogl-path/cogl-path.h>
#endif

/**
 * SECTION:cogl
 * @short_description: General purpose API
 *
 * General utility functions for COGL.
 */

#undef __COGL_H_INSIDE__

#endif /* __COGL_H__ */
