#ifndef __CLUTTER_DEVICE_MANAGER_PRIVATE_H__
#define __CLUTTER_DEVICE_MANAGER_PRIVATE_H__

#include <clutter/clutter-device-manager.h>

G_BEGIN_DECLS

struct _ClutterInputDevice
{
  GObject parent_instance;

  gint id;

  ClutterInputDeviceType device_type;

  gchar *device_name;

  /* the actor underneath the pointer */
  ClutterActor *cursor_actor;

  /* the actor that has a grab in place for the device */
  ClutterActor *pointer_grab_actor;

  /* the current click count */
  gint click_count;

  /* the stage the device is on */
  ClutterStage *stage;

  /* the current state */
  gint current_x;
  gint current_y;
  guint32 current_time;
  gint current_button_number;
  ClutterModifierType current_state;

  /* the previous state, used for click count generation */
  gint previous_x;
  gint previous_y;
  guint32 previous_time;
  gint previous_button_number;
  ClutterModifierType previous_state;
};

/* device manager */
void          _clutter_device_manager_add_device     (ClutterDeviceManager *device_manager,
                                                      ClutterInputDevice   *device);
void          _clutter_device_manager_remove_device  (ClutterDeviceManager *device_manager,
                                                      ClutterInputDevice   *device);
void          _clutter_device_manager_update_devices (ClutterDeviceManager *device_manager);

/* input device */
void          _clutter_input_device_set_coords       (ClutterInputDevice   *device,
                                                      gint                  x,
                                                      gint                  y);
void          _clutter_input_device_set_state        (ClutterInputDevice   *device,
                                                      ClutterModifierType   state);
void          _clutter_input_device_set_time         (ClutterInputDevice   *device,
                                                      guint32               time_);
void          _clutter_input_device_set_stage        (ClutterInputDevice   *device,
                                                      ClutterStage         *stage);
void          _clutter_input_device_set_actor        (ClutterInputDevice   *device,
                                                      ClutterActor         *actor);
ClutterActor *_clutter_input_device_update           (ClutterInputDevice   *device);

G_END_DECLS

#endif /* __CLUTTER_DEVICE_MANAGER_PRIVATE_H__ */
