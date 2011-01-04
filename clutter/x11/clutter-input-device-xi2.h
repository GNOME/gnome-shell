#ifndef __CLUTTER_INPUT_DEVICE_XI2_H__
#define __CLUTTER_INPUT_DEVICE_XI2_H__

#include <clutter/clutter-input-device.h>
#include <X11/extensions/XInput2.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE_XI2           (_clutter_input_device_xi2_get_type ())
#define CLUTTER_INPUT_DEVICE_XI2(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE_XI2, ClutterInputDeviceXI2))
#define CLUTTER_IS_INPUT_DEVICE_XI2(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE_XI2))

typedef struct _ClutterInputDeviceXI2           ClutterInputDeviceXI2;

GType _clutter_input_device_xi2_get_type (void) G_GNUC_CONST;

guint _clutter_input_device_xi2_translate_state (XIModifierState *modifiers_state,
                                                 XIButtonState   *buttons_state);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_XI2_H__ */
