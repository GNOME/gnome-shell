#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

typedef struct {

  GHashTable *devices;

} TestDevicesApp;

 

static gboolean
stage_motion_event_cb (ClutterActor *actor, 
                       ClutterEvent *event, 
                       gpointer userdata)
{
  TestDevicesApp *app = (TestDevicesApp *)userdata;
  ClutterInputDevice *device;
  ClutterActor *hand = NULL;

  device = clutter_event_get_device (event);

  hand = g_hash_table_lookup (app->devices, device);

  if (hand != NULL)
    {
      gfloat event_x, event_y;

      clutter_event_get_coords (event, &event_x, &event_y);
      clutter_actor_set_position (hand, event_x, event_y);

      return TRUE;
    }

  return FALSE;
}

G_MODULE_EXPORT int
test_devices_main (int argc, char **argv)
{
  ClutterActor *stage;
  TestDevicesApp *app;
  ClutterColor stage_color = { 0x61, 0x64, 0x8c, 0xff };
  const GSList *stage_devices, *l;

  /* force enabling X11 support */
  clutter_x11_enable_xinput ();

  clutter_init (&argc, &argv);

  app = g_new0 (TestDevicesApp, 1);
  app->devices = g_hash_table_new (g_direct_hash, g_direct_equal) ;

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  //clutter_stage_fullscreen (CLUTTER_STAGE (stage));

  g_signal_connect (stage, 
                    "motion-event", G_CALLBACK(stage_motion_event_cb),
                    app);

  clutter_actor_show_all (stage);

  stage_devices = clutter_x11_get_input_devices ();

  if (stage_devices == NULL)
    g_error ("No extended input devices found.");

  for (l = stage_devices; l != NULL; l = l->next)
    {
      ClutterInputDevice *device = l->data;
      ClutterInputDeviceType device_type;
      ClutterActor *hand = NULL;

      device_type = clutter_input_device_get_device_type (device);
      if (device_type  == CLUTTER_POINTER_DEVICE)
        {
          g_print ("got a pointer device with id %d...\n",
                   clutter_input_device_get_device_id (device));

          hand = clutter_texture_new_from_file (TESTS_DATADIR
                                                G_DIR_SEPARATOR_S
                                                "redhand.png",
                                                NULL);
          g_hash_table_insert (app->devices, device, hand);

          clutter_container_add_actor (CLUTTER_CONTAINER (stage), hand);
        }
    }

  clutter_main ();

  return EXIT_SUCCESS;
} 
