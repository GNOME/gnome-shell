#ifndef __CLUTTER_INPUT_DEVICE_X11_H__
#define __CLUTTER_INPUT_DEVICE_X11_H__

#include <clutter/clutter-input-device.h>
#include "clutter-backend-x11.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE_X11           (clutter_input_device_x11_get_type ())
#define CLUTTER_INPUT_DEVICE_X11(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE_X11, ClutterInputDeviceX11))
#define CLUTTER_IS_INPUT_DEVICE_X11(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE_X11))

typedef struct _ClutterInputDeviceX11           ClutterInputDeviceX11;

GType clutter_input_device_x11_get_type (void) G_GNUC_CONST;

gint _clutter_input_device_x11_construct (ClutterInputDevice *device,
                                          ClutterBackendX11  *backend);
void _clutter_input_device_x11_select_events (ClutterInputDevice *device,
                                              ClutterBackendX11  *backend,
                                              Window              xwin);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_X11_H__ */
