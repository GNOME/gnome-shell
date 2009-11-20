#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_DEVICE_MANAGER_H__
#define __CLUTTER_DEVICE_MANAGER_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_DEVICE_MANAGER             (clutter_device_manager_get_type ())
#define CLUTTER_DEVICE_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_DEVICE_MANAGER, ClutterDeviceManager))
#define CLUTTER_IS_DEVICE_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_DEVICE_MANAGER))

typedef struct _ClutterDeviceManager            ClutterDeviceManager;

GType clutter_stage_manager_get_type (void) G_GNUC_CONST;

ClutterDeviceManager *clutter_device_manager_get_default  (void);
GSList *              clutter_device_manager_list_devices (ClutterDeviceManager *device_manager);
const GSList *        clutter_device_manager_peek_devices (ClutterDeviceManager *device_manager);

ClutterInputDevice *  clutter_device_manager_get_device   (ClutterDeviceManager *device_manager,
                                                           gint                  device_id);

G_END_DECLS

#endif /* __CLUTTER_DEVICE_MANAGER_H__ */
