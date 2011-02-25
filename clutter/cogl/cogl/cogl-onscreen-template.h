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
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ONSCREEN_TEMPLATE_H__
#define __COGL_ONSCREEN_TEMPLATE_H__

#include <cogl/cogl-swap-chain.h>

G_BEGIN_DECLS

typedef struct _CoglOnscreenTemplate	      CoglOnscreenTemplate;

#define COGL_ONSCREEN_TEMPLATE(OBJECT) ((CoglOnscreenTemplate *)OBJECT)

#define cogl_onscreen_template_new cogl_onscreen_template_new_EXP
CoglOnscreenTemplate *
cogl_onscreen_template_new (CoglSwapChain *swap_chain);

G_END_DECLS

#endif /* __COGL_ONSCREEN_TEMPLATE_H__ */
