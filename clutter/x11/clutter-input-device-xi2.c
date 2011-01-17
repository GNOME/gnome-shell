#include "config.h"

#include "clutter-input-device-xi2.h"

#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "clutter-backend-x11.h"
#include "clutter-stage-x11.h"

#include <X11/extensions/XInput2.h>

typedef struct _ClutterInputDeviceClass         ClutterInputDeviceXI2Class;

/* a specific XI2 input device */
struct _ClutterInputDeviceXI2
{
  ClutterInputDevice device;

  gint device_id;
};

#define N_BUTTONS       5

#define clutter_input_device_xi2_get_type       _clutter_input_device_xi2_get_type

G_DEFINE_TYPE (ClutterInputDeviceXI2,
               clutter_input_device_xi2,
               CLUTTER_TYPE_INPUT_DEVICE);

static void
clutter_input_device_xi2_select_stage_events (ClutterInputDevice *device,
                                              ClutterStage       *stage,
                                              gint                event_mask)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (device);
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11;
  XIEventMask xi_event_mask;
  unsigned char *mask;
  int len;

  backend_x11 = CLUTTER_BACKEND_X11 (device->backend);
  stage_x11 = CLUTTER_STAGE_X11 (_clutter_stage_get_window (stage));

  len = XIMaskLen (XI_LASTEVENT);
  mask = g_new0 (unsigned char, len);

  if (event_mask & PointerMotionMask)
    XISetMask (mask, XI_Motion);

  if (event_mask & ButtonPressMask)
    XISetMask (mask, XI_ButtonPress);

  if (event_mask & ButtonReleaseMask)
    XISetMask (mask, XI_ButtonRelease);

  if (event_mask & KeyPressMask)
    XISetMask (mask, XI_KeyPress);

  if (event_mask & KeyReleaseMask)
    XISetMask (mask, XI_KeyRelease);

  if (event_mask & EnterWindowMask)
    XISetMask (mask, XI_Enter);

  if (event_mask & LeaveWindowMask)
    XISetMask (mask, XI_Leave);

  xi_event_mask.deviceid = device_xi2->device_id;
  xi_event_mask.mask = mask;
  xi_event_mask.mask_len = len;

  CLUTTER_NOTE (BACKEND, "Selecting device id '%d' events",
                device_xi2->device_id);

  XISelectEvents (backend_x11->xdpy, stage_x11->xwin, &xi_event_mask, 1);

  g_free (mask);
}

static void
clutter_input_device_xi2_constructed (GObject *gobject)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (gobject);

  g_object_get (gobject, "id", &device_xi2->device_id, NULL);

  if (G_OBJECT_CLASS (clutter_input_device_xi2_parent_class)->constructed)
    G_OBJECT_CLASS (clutter_input_device_xi2_parent_class)->constructed (gobject);
}

static void
clutter_input_device_xi2_class_init (ClutterInputDeviceXI2Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterInputDeviceClass *device_class = CLUTTER_INPUT_DEVICE_CLASS (klass);

  gobject_class->constructed = clutter_input_device_xi2_constructed;

  device_class->select_stage_events = clutter_input_device_xi2_select_stage_events;
}

static void
clutter_input_device_xi2_init (ClutterInputDeviceXI2 *self)
{
}

guint
_clutter_input_device_xi2_translate_state (XIModifierState *modifiers_state,
                                           XIButtonState   *buttons_state)
{
  guint retval = 0;

  if (modifiers_state)
    retval = (guint) modifiers_state->effective;

  if (buttons_state)
    {
      int len, i;

      len = MIN (N_BUTTONS, buttons_state->mask_len * 8);

      for (i = 0; i < len; i++)
        {
          if (!XIMaskIsSet (buttons_state->mask, i))
            continue;

          switch (i)
            {
            case 1:
              retval |= CLUTTER_BUTTON1_MASK;
              break;

            case 2:
              retval |= CLUTTER_BUTTON2_MASK;
              break;

            case 3:
              retval |= CLUTTER_BUTTON3_MASK;
              break;

            case 4:
              retval |= CLUTTER_BUTTON4_MASK;
              break;

            case 5:
              retval |= CLUTTER_BUTTON5_MASK;
              break;

            default:
              break;
            }
        }
    }

  return retval;
}
