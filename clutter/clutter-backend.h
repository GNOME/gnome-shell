/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BACKEND_H__
#define __CLUTTER_BACKEND_H__

#include <cairo.h>
#include <glib-object.h>
#include <pango/pango.h>
#ifdef COGL_ENABLE_EXPERIMENTAL_2_0_API
#include <cogl/cogl.h>
#endif

#include <clutter/clutter-actor.h>
#include <clutter/clutter-device-manager.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-feature.h>
#include <clutter/clutter-stage.h>
#include <clutter/clutter-stage-window.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND            (clutter_backend_get_type ())
#define CLUTTER_BACKEND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND, ClutterBackend))
#define CLUTTER_IS_BACKEND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND))

/**
 * ClutterBackend:
 *
 * <structname>ClutterBackend</structname> is an opaque structure whose
 * members cannot be directly accessed.
 *
 * Since: 0.4
 */
typedef struct _ClutterBackend          ClutterBackend;
typedef struct _ClutterBackendClass     ClutterBackendClass;

GType clutter_backend_get_type    (void) G_GNUC_CONST;

ClutterBackend *clutter_get_default_backend (void);

#ifndef CLUTTER_DISABLE_DEPRECATED
void                        clutter_backend_set_resolution            (ClutterBackend             *backend,
                                                                       gdouble                     dpi);
void                        clutter_backend_set_double_click_time     (ClutterBackend             *backend,
                                                                       guint                       msec);
guint                       clutter_backend_get_double_click_time     (ClutterBackend             *backend);
void                        clutter_backend_set_double_click_distance (ClutterBackend             *backend,
                                                                       guint                       distance);
guint                       clutter_backend_get_double_click_distance (ClutterBackend             *backend);
void                        clutter_backend_set_font_name             (ClutterBackend             *backend,
                                                                       const gchar                *font_name);
const gchar *               clutter_backend_get_font_name             (ClutterBackend             *backend);
#endif /* CLUTTER_DISABLE_DEPRECATED */

gdouble                     clutter_backend_get_resolution            (ClutterBackend             *backend);

void                        clutter_backend_set_font_options          (ClutterBackend             *backend,
                                                                       const cairo_font_options_t *options);
const cairo_font_options_t *clutter_backend_get_font_options          (ClutterBackend             *backend);

#if defined (COGL_ENABLE_EXPERIMENTAL_2_0_API) && defined (CLUTTER_ENABLE_EXPERIMENTAL_API)
CoglContext                *clutter_backend_get_cogl_context          (ClutterBackend             *backend);
#endif

G_END_DECLS

#endif /* __CLUTTER_BACKEND_H__ */
