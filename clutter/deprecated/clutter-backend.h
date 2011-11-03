#ifndef __CLUTTER_BACKEND_DEPRECATED_H__
#define __CLUTTER_BACKEND_DEPRECATED_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_FOR(ClutterSettings:font_dpi)
void            clutter_backend_set_resolution                  (ClutterBackend *backend,
                                                                 gdouble         dpi);

CLUTTER_DEPRECATED_FOR(ClutterSettings:double_click_time)
void            clutter_backend_set_double_click_time           (ClutterBackend *backend,
                                                                 guint           msec);

CLUTTER_DEPRECATED_FOR(ClutterSettings:double_click_time)
guint           clutter_backend_get_double_click_time           (ClutterBackend *backend);

CLUTTER_DEPRECATED_FOR(ClutterSettings:double_click_distance)
void            clutter_backend_set_double_click_distance       (ClutterBackend *backend,
                                                                 guint           distance);

CLUTTER_DEPRECATED_FOR(ClutterSettings:double_click_distance)
guint           clutter_backend_get_double_click_distance       (ClutterBackend *backend);

CLUTTER_DEPRECATED_FOR(ClutterSettings:font_name)
void            clutter_backend_set_font_name                   (ClutterBackend *backend,
                                                                 const gchar    *font_name);

CLUTTER_DEPRECATED_FOR(ClutterSettings:font_name)
const gchar *   clutter_backend_get_font_name                   (ClutterBackend *backend);


G_END_DECLS

#endif /* __CLUTTER_BACKEND_DEPRECATED_H__ */
