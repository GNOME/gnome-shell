#ifndef __CLUTTER_MAIN_DEPRECATED_H__
#define __CLUTTER_MAIN_DEPRECATED_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-input-device.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED
void                    clutter_threads_init                    (void);

CLUTTER_DEPRECATED
guint                   clutter_threads_add_frame_source        (guint             fps,
                                                                 GSourceFunc       func,
                                                                 gpointer          data);
CLUTTER_DEPRECATED
guint                   clutter_threads_add_frame_source_full   (gint              priority,
                                                                 guint             fps,
                                                                 GSourceFunc       func,
                                                                 gpointer          data,
                                                                 GDestroyNotify    notify);

CLUTTER_DEPRECATED_FOR(clutter_stage_set_motion_events_enabled)
void                    clutter_set_motion_events_enabled       (gboolean          enable);

CLUTTER_DEPRECATED_FOR(clutter_stage_get_motion_events_enabled)
gboolean                clutter_get_motion_events_enabled       (void);

CLUTTER_DEPRECATED_FOR(clutter_stage_ensure_redraw)
void                    clutter_redraw                          (ClutterStage     *stage);

CLUTTER_DEPRECATED_FOR(cogl_pango_font_map_clear_glyph_cache)
void                    clutter_clear_glyph_cache               (void);

CLUTTER_DEPRECATED_FOR(clutter_backend_set_font_options)
void                    clutter_set_font_flags                  (ClutterFontFlags  flags);

CLUTTER_DEPRECATED_FOR(clutter_backend_get_font_options)
ClutterFontFlags        clutter_get_font_flags                  (void);

CLUTTER_DEPRECATED_FOR(clutter_device_manager_get_device)
ClutterInputDevice *    clutter_get_input_device_for_id         (gint id_);

CLUTTER_DEPRECATED_FOR(clutter_input_device_grab)
void                    clutter_grab_pointer_for_device         (ClutterActor     *actor,
                                                                 gint              id_);

CLUTTER_DEPRECATED_FOR(clutter_input_device_ungrab)
void                    clutter_ungrab_pointer_for_device       (gint              id_);

CLUTTER_DEPRECATED
void                    clutter_set_default_frame_rate          (guint             frames_per_sec);

G_END_DECLS

#endif /* __CLUTTER_MAIN_DEPRECATED_H__ */
