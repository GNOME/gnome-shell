#ifndef __CLUTTER_BACKEND_PRIVATE_H__
#define __CLUTTER_BACKEND_PRIVATE_H__

#include <clutter/clutter-backend.h>

#define CLUTTER_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND, ClutterBackendClass))
#define CLUTTER_IS_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND))
#define CLUTTER_BACKEND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND, ClutterBackendClass))

G_BEGIN_DECLS

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
  void                  (* redraw)             (ClutterBackend  *backend,
                                                ClutterStage    *stage);
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

  /* signals */
  void (* resolution_changed) (ClutterBackend *backend);
  void (* font_changed)       (ClutterBackend *backend);
  void (* settings_changed)   (ClutterBackend *backend);
};

/* vfuncs implemented by backend */
GType         _clutter_backend_impl_get_type  (void);

void          _clutter_backend_redraw         (ClutterBackend  *backend,
                                               ClutterStage    *stage);
ClutterStageWindow *_clutter_backend_create_stage   (ClutterBackend  *backend,
                                               ClutterStage    *wrapper,
                                               GError         **error);
void          _clutter_backend_ensure_context (ClutterBackend  *backend,
                                               ClutterStage    *stage);
void          _clutter_backend_ensure_context_internal
                                              (ClutterBackend  *backend,
                                               ClutterStage    *stage);
gboolean      _clutter_backend_create_context (ClutterBackend  *backend,
                                               GError         **error);

void          _clutter_backend_add_options    (ClutterBackend  *backend,
                                               GOptionGroup    *group);
gboolean      _clutter_backend_pre_parse      (ClutterBackend  *backend,
                                               GError         **error);
gboolean      _clutter_backend_post_parse     (ClutterBackend  *backend,
                                               GError         **error);
void          _clutter_backend_init_events    (ClutterBackend  *backend);

void          _clutter_backend_copy_event_data (ClutterBackend     *backend,
                                                const ClutterEvent *src,
                                                ClutterEvent       *dest);
void          _clutter_backend_free_event_data (ClutterBackend     *backend,
                                                ClutterEvent       *event);

ClutterFeatureFlags _clutter_backend_get_features (ClutterBackend *backend);

gfloat        _clutter_backend_get_units_per_em   (ClutterBackend       *backend,
                                                   PangoFontDescription *font_desc);

gint32 _clutter_backend_get_units_serial (ClutterBackend *backend);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_PRIVATE_H__ */
