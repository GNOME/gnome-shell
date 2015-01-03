#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_SETTINGS_H__
#define __CLUTTER_SETTINGS_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SETTINGS           (clutter_settings_get_type ())
#define CLUTTER_SETTINGS(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_SETTINGS, ClutterSettings))
#define CLUTTER_IS_SETTINGS(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_SETTINGS))

typedef struct _ClutterSettings         ClutterSettings;
typedef struct _ClutterSettingsClass    ClutterSettingsClass;

CLUTTER_AVAILABLE_IN_ALL
GType clutter_settings_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_ALL
ClutterSettings *clutter_settings_get_default (void);

G_END_DECLS

#endif /* __CLUTTER_SETTINGS_H__ */
