#ifndef __CLUTTER_SETTINGS_PRIVATE_H__
#define __CLUTTER_SETTINGS_PRIVATE_H__

#include <clutter/clutter-backend-private.h>
#include <clutter/clutter-settings.h>

G_BEGIN_DECLS

void    _clutter_settings_set_backend           (ClutterSettings *settings,
                                                 ClutterBackend  *backend);
void    _clutter_settings_read_from_key_file    (ClutterSettings *settings,
                                                 GKeyFile        *key_file);

void    clutter_settings_set_property_internal (ClutterSettings *settings,
                                                const char *property,
                                                GValue *value);

G_END_DECLS

#endif /* __CLUTTER_SETTINGS_PRIVATE_H__ */
