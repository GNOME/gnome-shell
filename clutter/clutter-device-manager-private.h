/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifndef __CLUTTER_DEVICE_MANAGER_PRIVATE_H__
#define __CLUTTER_DEVICE_MANAGER_PRIVATE_H__

#include <clutter/clutter-backend.h>
#include <clutter/clutter-device-manager.h>
#include <clutter/clutter-event.h>

G_BEGIN_DECLS

typedef struct _ClutterAxisInfo
{
  ClutterInputAxis axis;

  gdouble min_axis;
  gdouble max_axis;

  gdouble min_value;
  gdouble max_value;

  gdouble resolution;
} ClutterAxisInfo;

typedef struct _ClutterKeyInfo
{
  guint keyval;
  ClutterModifierType modifiers;
} ClutterKeyInfo;

typedef struct _ClutterScrollInfo
{
  guint axis_id;
  ClutterScrollDirection direction;
  gdouble increment;

  gdouble last_value;
  guint last_value_valid : 1;
} ClutterScrollInfo;

typedef struct _ClutterTouchInfo
{
  ClutterEventSequence *sequence;
  ClutterActor *actor;

  gint current_x;
  gint current_y;
} ClutterTouchInfo;

struct _ClutterInputDevice
{
  GObject parent_instance;

  gint id;

  ClutterInputDeviceType device_type;
  ClutterInputMode device_mode;

  gchar *device_name;

  ClutterDeviceManager *device_manager;

  ClutterBackend *backend;

  /* the associated device */
  ClutterInputDevice *associated;

  GList *slaves;

  /* the actor underneath the pointer */
  ClutterActor *cursor_actor;
  GHashTable   *inv_touch_sequence_actors;

  /* the actor that has a grab in place for the device */
  ClutterActor *pointer_grab_actor;
  ClutterActor *keyboard_grab_actor;
  GHashTable   *sequence_grab_actors;
  GHashTable   *inv_sequence_grab_actors;

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

  /* the current touch points states */
  GHashTable *touch_sequences_info;

  /* the previous state, used for click count generation */
  gint previous_x;
  gint previous_y;
  guint32 previous_time;
  gint previous_button_number;
  ClutterModifierType previous_state;

  GArray *axes;

  guint n_keys;
  GArray *keys;

  GArray *scroll_info;

  guint has_cursor : 1;
  guint is_enabled : 1;
};

struct _ClutterInputDeviceClass
{
  GObjectClass parent_class;

  gboolean (* keycode_to_evdev) (ClutterInputDevice *device,
                                 guint               hardware_keycode,
                                 guint              *evdev_keycode);
};

/* device manager */
void            _clutter_device_manager_add_device              (ClutterDeviceManager *device_manager,
                                                                 ClutterInputDevice   *device);
void            _clutter_device_manager_remove_device           (ClutterDeviceManager *device_manager,
                                                                 ClutterInputDevice   *device);
void            _clutter_device_manager_update_devices          (ClutterDeviceManager *device_manager);
void            _clutter_device_manager_select_stage_events     (ClutterDeviceManager *device_manager,
                                                                 ClutterStage         *stage);
ClutterBackend *_clutter_device_manager_get_backend             (ClutterDeviceManager *device_manager);

/* input device */
gboolean        _clutter_input_device_has_sequence              (ClutterInputDevice   *device,
                                                                 ClutterEventSequence *sequence);
void            _clutter_input_device_add_event_sequence        (ClutterInputDevice   *device,
                                                                 ClutterEvent         *event);
void            _clutter_input_device_remove_event_sequence     (ClutterInputDevice   *device,
                                                                 ClutterEvent         *event);
void            _clutter_input_device_set_coords                (ClutterInputDevice   *device,
                                                                 ClutterEventSequence *sequence,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 ClutterStage         *stage);
void            _clutter_input_device_set_state                 (ClutterInputDevice   *device,
                                                                 ClutterModifierType   state);
void            _clutter_input_device_set_time                  (ClutterInputDevice   *device,
                                                                 guint32               time_);
void            _clutter_input_device_set_stage                 (ClutterInputDevice   *device,
                                                                 ClutterStage         *stage);
ClutterStage *  _clutter_input_device_get_stage                 (ClutterInputDevice   *device);
void            _clutter_input_device_set_actor                 (ClutterInputDevice   *device,
                                                                 ClutterEventSequence *sequence,
                                                                 ClutterActor         *actor,
                                                                 gboolean              emit_crossing);
ClutterActor *  _clutter_input_device_update                    (ClutterInputDevice   *device,
                                                                 ClutterEventSequence *sequence,
                                                                 gboolean              emit_crossing);
void            _clutter_input_device_set_n_keys                (ClutterInputDevice   *device,
                                                                 guint                 n_keys);
guint           _clutter_input_device_add_axis                  (ClutterInputDevice   *device,
                                                                 ClutterInputAxis      axis,
                                                                 gdouble               min_value,
                                                                 gdouble               max_value,
                                                                 gdouble               resolution);
void            _clutter_input_device_reset_axes                (ClutterInputDevice   *device);

void            _clutter_input_device_set_associated_device     (ClutterInputDevice   *device,
                                                                 ClutterInputDevice   *associated);
void            _clutter_input_device_add_slave                 (ClutterInputDevice   *master,
                                                                 ClutterInputDevice   *slave);
void            _clutter_input_device_remove_slave              (ClutterInputDevice   *master,
                                                                 ClutterInputDevice   *slave);

gboolean        _clutter_input_device_translate_axis            (ClutterInputDevice   *device,
                                                                 guint                 index_,
                                                                 gdouble               value,
                                                                 gdouble              *axis_value);

void            _clutter_input_device_add_scroll_info           (ClutterInputDevice   *device,
                                                                 guint                 index_,
                                                                 ClutterScrollDirection direction,
                                                                 gdouble               increment);
void            _clutter_input_device_reset_scroll_info         (ClutterInputDevice   *device);
gboolean        _clutter_input_device_get_scroll_delta          (ClutterInputDevice   *device,
                                                                 guint                 index_,
                                                                 gdouble               value,
                                                                 ClutterScrollDirection *direction_p,
                                                                 gdouble                *delta_p);

G_END_DECLS

#endif /* __CLUTTER_DEVICE_MANAGER_PRIVATE_H__ */
