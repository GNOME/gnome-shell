#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_INPUT_DEVICE_H__
#define __CLUTTER_INPUT_DEVICE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE               (clutter_input_device_get_type ())
#define CLUTTER_INPUT_DEVICE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE, ClutterInputDevice))
#define CLUTTER_IS_INPUT_DEVICE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE))
#define CLUTTER_INPUT_DEVICE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_INPUT_DEVICE, ClutterInputDeviceClass))
#define CLUTTER_IS_INPUT_DEVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_INPUT_DEVICE))
#define CLUTTER_INPUT_DEVICE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_INPUT_DEVICE, ClutterInputDeviceClass))

/**
 * ClutterInputDevice:
 *
 * Generic representation of an input device. The
 * actual contents of this structure depend on the
 * backend used.
 */
typedef struct _ClutterInputDevice      ClutterInputDevice;
typedef struct _ClutterInputDeviceClass ClutterInputDeviceClass;

/**
 * ClutterInputDeviceType:
 * @CLUTTER_POINTER_DEVICE: A pointer device
 * @CLUTTER_KEYBOARD_DEVICE: A keyboard device
 * @CLUTTER_EXTENSION_DEVICE: A generic extension device
 * @CLUTTER_N_DEVICE_TYPES: The number of device types
 *
 * The types of input devices available.
 *
 * The #ClutterInputDeviceType enumeration can be extended at later
 * date; not every platform supports every input device type.
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_POINTER_DEVICE,
  CLUTTER_KEYBOARD_DEVICE,
  CLUTTER_EXTENSION_DEVICE,

  CLUTTER_N_DEVICE_TYPES
} ClutterInputDeviceType;

struct _ClutterInputDeviceClass
{
  /*< private >*/
  GObjectClass parent_class;
};

GType clutter_input_device_get_type (void) G_GNUC_CONST;

ClutterInputDeviceType clutter_input_device_get_device_type (ClutterInputDevice *device);
gint                   clutter_input_device_get_device_id   (ClutterInputDevice *device);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_H__ */
