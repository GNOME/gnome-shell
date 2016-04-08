/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Intel Corporation.
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
 */

#ifndef __CLUTTER_COGL_COMPAT_H__
#define __CLUTTER_COGL_COMPAT_H__

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

G_BEGIN_DECLS

/* XXX: Some public Clutter apis depend on Cogl types that have been
 * removed from the Cogl 2.0 experimental api.
 *
 * If somone has opted to use the Cogl 2.0 experimental api by
 * defining COGL_ENABLE_EXPERIMENTAL_2_0_API then we need to define
 * some place holder typdefs for compatability.
 *
 * NB: we build all clutter internals with COGL_ENABLE_EXPERIMENTAL_2_0_API
 * defined.
 */

#ifdef COGL_ENABLE_EXPERIMENTAL_2_0_API

/* CoglMaterial has been replaced with CoglPipeline in Cogl 2.0 */
typedef struct _CoglMaterial CoglMaterial;

#endif

G_END_DECLS

#endif /* __CLUTTER_COGL_COMPAT_H__ */
