/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

#ifndef __CLUTTER_BACKEND_PRIVATE_H__
#define __CLUTTER_BACKEND_PRIVATE_H__

#include <clutter/clutter-backend.h>
#include <clutter/clutter-device-manager.h>
#include <clutter/clutter-stage-window.h>

#include "clutter-event-translator.h"

#define CLUTTER_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND, ClutterBackendClass))
#define CLUTTER_IS_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND))
#define CLUTTER_BACKEND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND, ClutterBackendClass))

G_BEGIN_DECLS

typedef struct _ClutterBackendPrivate   ClutterBackendPrivate;

struct _ClutterBackend
{
  /*< private >*/
  GObject parent_instance;

  CoglRenderer *cogl_renderer;
  CoglDisplay *cogl_display;
  CoglContext *cogl_context;
  GSource *cogl_source;

  ClutterDeviceManager *device_manager;

  ClutterBackendPrivate *priv;
};

struct _ClutterBackendClass
{
  /*< private >*/
  GObjectClass parent_class;

  GType stage_window_type;

  /* vfuncs */
  gboolean              (* pre_parse)          (ClutterBackend  *backend,
                                                GError         **error);
  gboolean              (* post_parse)         (ClutterBackend  *backend,
                                                GError         **error);
  ClutterStageWindow *  (* create_stage)       (ClutterBackend  *backend,
                                                ClutterStage    *wrapper,
                                                GError         **error);
  void                  (* init_events)        (ClutterBackend  *backend);
  void                  (* init_features)      (ClutterBackend  *backend);
  void                  (* add_options)        (ClutterBackend  *backend,
                                                GOptionGroup    *group);
  ClutterFeatureFlags   (* get_features)       (ClutterBackend  *backend);
  CoglRenderer *        (* get_renderer)       (ClutterBackend  *backend,
                                                GError         **error);
  CoglDisplay *         (* get_display)        (ClutterBackend  *backend,
                                                CoglRenderer    *renderer,
                                                CoglSwapChain   *swap_chain,
                                                GError         **error);
  gboolean              (* create_context)     (ClutterBackend  *backend,
                                                GError         **error);
  void                  (* ensure_context)     (ClutterBackend  *backend,
                                                ClutterStage    *stage);
  ClutterDeviceManager *(* get_device_manager) (ClutterBackend  *backend);

  void                  (* copy_event_data)    (ClutterBackend     *backend,
                                                const ClutterEvent *src,
                                                ClutterEvent       *dest);
  void                  (* free_event_data)    (ClutterBackend     *backend,
                                                ClutterEvent       *event);

  gboolean              (* translate_event)    (ClutterBackend     *backend,
                                                gpointer            native,
                                                ClutterEvent       *event);

  /* signals */
  void (* resolution_changed) (ClutterBackend *backend);
  void (* font_changed)       (ClutterBackend *backend);
  void (* settings_changed)   (ClutterBackend *backend);
};

ClutterBackend *        _clutter_create_backend                         (void);

ClutterStageWindow *    _clutter_backend_create_stage                   (ClutterBackend         *backend,
                                                                         ClutterStage           *wrapper,
                                                                         GError                **error);
void                    _clutter_backend_ensure_context                 (ClutterBackend         *backend,
                                                                         ClutterStage           *stage);
void                    _clutter_backend_ensure_context_internal        (ClutterBackend         *backend,
                                                                         ClutterStage           *stage);
gboolean                _clutter_backend_create_context                 (ClutterBackend         *backend,
                                                                         GError                **error);

void                    _clutter_backend_add_options                    (ClutterBackend         *backend,
                                                                         GOptionGroup           *group);
gboolean                _clutter_backend_pre_parse                      (ClutterBackend         *backend,
                                                                         GError                **error);
gboolean                _clutter_backend_post_parse                     (ClutterBackend         *backend,
                                                                         GError                **error);

void                    _clutter_backend_init_events                    (ClutterBackend         *backend);
void                    _clutter_backend_copy_event_data                (ClutterBackend         *backend,
                                                                         const ClutterEvent     *src,
                                                                         ClutterEvent           *dest);
void                    _clutter_backend_free_event_data                (ClutterBackend         *backend,
                                                                         ClutterEvent           *event);
gboolean                _clutter_backend_translate_event                (ClutterBackend         *backend,
                                                                         gpointer                native,
                                                                         ClutterEvent           *event);
void                    _clutter_backend_add_event_translator           (ClutterBackend         *backend,
                                                                         ClutterEventTranslator *translator);
void                    _clutter_backend_remove_event_translator        (ClutterBackend         *backend,
                                                                         ClutterEventTranslator *translator);

ClutterFeatureFlags     _clutter_backend_get_features                   (ClutterBackend         *backend);

gfloat                  _clutter_backend_get_units_per_em               (ClutterBackend         *backend,
                                                                         PangoFontDescription   *font_desc);
gint32                  _clutter_backend_get_units_serial               (ClutterBackend         *backend);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_PRIVATE_H__ */
