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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_CLUTTER_H__
#define __COGL_CLUTTER_H__

COGL_BEGIN_DECLS

#define cogl_clutter_check_extension cogl_clutter_check_extension_CLUTTER
COGL_DEPRECATED_IN_1_16
CoglBool
cogl_clutter_check_extension (const char *name, const char *ext);

#define cogl_clutter_winsys_has_feature cogl_clutter_winsys_has_feature_CLUTTER
COGL_DEPRECATED_FOR (cogl_has_feature)
CoglBool
cogl_clutter_winsys_has_feature (CoglWinsysFeature feature);

#define cogl_onscreen_clutter_backend_set_size cogl_onscreen_clutter_backend_set_size_CLUTTER
void
cogl_onscreen_clutter_backend_set_size (int width, int height);

COGL_END_DECLS

#endif /* __COGL_CLUTTER_H__ */
