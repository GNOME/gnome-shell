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

#include <clutter/clutter-actor.h>
#include <clutter/clutter-stage.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-feature.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND            (clutter_backend_get_type ())
#define CLUTTER_BACKEND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND, ClutterBackend))
#define CLUTTER_IS_BACKEND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND))
#define CLUTTER_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND, ClutterBackendClass))
#define CLUTTER_IS_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND))
#define CLUTTER_BACKEND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND, ClutterBackendClass))

typedef struct _ClutterBackend          ClutterBackend;
typedef struct _ClutterBackendPrivate   ClutterBackendPrivate;
typedef struct _ClutterBackendClass     ClutterBackendClass;

struct _ClutterBackend
{
  /*< private >*/
  GObject                parent_instance;
  ClutterBackendPrivate *priv;
};

struct _ClutterBackendClass
{
  /*< private >*/
  GObjectClass parent_class;

  /* vfuncs */
  gboolean            (* pre_parse)        (ClutterBackend  *backend,
                                            GError         **error);
  gboolean            (* post_parse)       (ClutterBackend  *backend,
                                            GError         **error);
  ClutterActor *      (* create_stage)     (ClutterBackend  *backend,
                                            ClutterStage    *wrapper,
                                            GError         **error);
  void                (* init_events)      (ClutterBackend  *backend);
  void                (* init_features)    (ClutterBackend  *backend);
  void                (* add_options)      (ClutterBackend  *backend,
                                            GOptionGroup    *group);
  ClutterFeatureFlags (* get_features)     (ClutterBackend  *backend);
  void                (* redraw)           (ClutterBackend  *backend,
                                            ClutterStage    *stage);
  void                (* ensure_context)   (ClutterBackend  *backend,
                                            ClutterStage    *stage);

  /* signals */
  void (* resolution_changed) (ClutterBackend *backend);
  void (* font_changed)       (ClutterBackend *backend);
};

GType clutter_backend_get_type    (void) G_GNUC_CONST;

ClutterBackend *clutter_get_default_backend (void);

void                  clutter_backend_set_resolution            (ClutterBackend       *backend,
                                                                 gdouble               dpi);
gdouble               clutter_backend_get_resolution            (ClutterBackend       *backend);
void                  clutter_backend_set_double_click_time     (ClutterBackend       *backend,
                                                                 guint                 msec);
guint                 clutter_backend_get_double_click_time     (ClutterBackend       *backend);
void                  clutter_backend_set_double_click_distance (ClutterBackend       *backend,
                                                                 guint                 distance);
guint                 clutter_backend_get_double_click_distance (ClutterBackend       *backend);
void                  clutter_backend_set_font_options          (ClutterBackend       *backend,
                                                                 cairo_font_options_t *options);
cairo_font_options_t *clutter_backend_get_font_options          (ClutterBackend       *backend);
void                  clutter_backend_set_font_name             (ClutterBackend       *backend,
                                                                 const gchar          *font_name);
G_CONST_RETURN gchar *clutter_backend_get_font_name             (ClutterBackend       *backend);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_H__ */
