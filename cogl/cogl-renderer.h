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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_RENDERER_H__
#define __COGL_RENDERER_H__

#include <glib.h>

#include <cogl/cogl-types.h>
#include <cogl/cogl-onscreen-template.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-renderer
 * @short_description:
 *
 */

#define cogl_renderer_error_quark cogl_renderer_error_quark_EXP

#define COGL_RENDERER_ERROR cogl_renderer_error_quark ()
GQuark
cogl_renderer_error_quark (void);

typedef struct _CoglRenderer CoglRenderer;

#define cogl_is_renderer cogl_is_renderer_EXP
gboolean
cogl_is_renderer (void *object);

#define cogl_renderer_new cogl_renderer_new_EXP
CoglRenderer *
cogl_renderer_new (void);

/* optional configuration APIs */

/**
 * CoglWinsysID:
 * @COGL_WINSYS_ID_ANY: Implies no preference for which backend is used
 * @COGL_WINSYS_ID_STUB: Use the no-op stub backend
 * @COGL_WINSYS_ID_GLX: Use the GLX window system binding API
 * @COGL_WINSYS_ID_EGL: Use the Khronos EGL window system binding API
 * @COGL_WINSYS_ID_WGL: Use the Microsoft Windows WGL binding API
 *
 * Identifies specific window system backends that Cogl supports.
 *
 * These can be used to query what backend Cogl is using or to try and
 * explicitly select a backend to use.
 */
typedef enum
{
  COGL_WINSYS_ID_ANY,
  COGL_WINSYS_ID_STUB,
  COGL_WINSYS_ID_GLX,
  COGL_WINSYS_ID_EGL,
  COGL_WINSYS_ID_WGL
} CoglWinsysID;

/**
 * cogl_renderer_set_winsys_id:
 * @renderer: A #CoglRenderer
 * @winsys_id: An ID of the winsys you explicitly want to use.
 *
 * This allows you to explicitly select a winsys backend to use instead
 * of letting Cogl automatically select a backend.
 *
 * if you select an unsupported backend then cogl_renderer_connect()
 * will fail and report an error.
 *
 * This may only be called on an un-connected #CoglRenderer.
 */
#define cogl_renderer_set_winsys_id cogl_renderer_set_winsys_id_EXP
void
cogl_renderer_set_winsys_id (CoglRenderer *renderer,
                             CoglWinsysID winsys_id);

/**
 * cogl_renderer_get_winsys_id:
 * @renderer: A #CoglRenderer
 *
 * Queries which window system backend Cogl has chosen to use.
 *
 * This may only be called on a connected #CoglRenderer.
 *
 * Returns: The #CoglWinsysID corresponding to the chosen window
 *          system backend.
 */
#define cogl_renderer_get_winsys_id cogl_renderer_get_winsys_id_EXP
CoglWinsysID
cogl_renderer_get_winsys_id (CoglRenderer *renderer);

#define cogl_renderer_check_onscreen_template \
  cogl_renderer_check_onscreen_template_EXP
gboolean
cogl_renderer_check_onscreen_template (CoglRenderer *renderer,
                                       CoglOnscreenTemplate *onscreen_template,
                                       GError **error);

/* Final connection API */

#define cogl_renderer_connect cogl_renderer_connect_EXP
gboolean
cogl_renderer_connect (CoglRenderer *renderer, GError **error);

G_END_DECLS

#endif /* __COGL_RENDERER_H__ */

