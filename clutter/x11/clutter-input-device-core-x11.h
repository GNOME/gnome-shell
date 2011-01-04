#ifndef __CLUTTER_INPUT_DEVICE_X11_H__
#define __CLUTTER_INPUT_DEVICE_X11_H__

#include <clutter/clutter-input-device.h>
#include <X11/Xlib.h>

#include "clutter-stage-x11.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE_X11           (_clutter_input_device_x11_get_type ())
#define CLUTTER_INPUT_DEVICE_X11(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE_X11, ClutterInputDeviceX11))
#define CLUTTER_IS_INPUT_DEVICE_X11(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE_X11))

typedef struct _ClutterInputDeviceX11           ClutterInputDeviceX11;

GType _clutter_input_device_x11_get_type (void) G_GNUC_CONST;

gboolean _clutter_input_device_x11_translate_xi_event (ClutterInputDeviceX11 *device_x11,
                                                       ClutterStageX11       *stage_x11,
                                                       XEvent                *xevent,
                                                       ClutterEvent          *event);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_X11_H__ */
