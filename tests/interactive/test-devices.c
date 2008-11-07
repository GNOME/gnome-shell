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
  ClutterActor *hand = NULL;
  ClutterMotionEvent *mev = (ClutterMotionEvent *)event;

  hand = g_hash_table_lookup (app->devices, mev->device);
  clutter_actor_set_position (hand, mev->x, mev->y);

  return FALSE;
}

G_MODULE_EXPORT int
test_devices_main (int argc, char **argv)
{
  ClutterActor *stage = NULL;
  GSList *stage_devices = NULL;
  TestDevicesApp *app = NULL;
  ClutterColor stage_color = { 0x61, 0x64, 0x8c, 0xff };

  clutter_x11_enable_xinput ();
  clutter_init (&argc, &argv);

  app = g_new0 (TestDevicesApp, 1);
  app->devices = g_hash_table_new (g_direct_hash, g_direct_equal) ;

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  //clutter_stage_fullscreen (CLUTTER_STAGE (stage));

  g_signal_connect (stage, 
                    "motion-event", 
                    G_CALLBACK(stage_motion_event_cb), 
                    app);
  clutter_actor_show_all (stage);

  stage_devices = clutter_x11_get_input_devices ();

  if (stage_devices == NULL)
    g_error ("No extended input devices found.");

  do 
    {
      if (stage_devices)
        {
          ClutterX11XInputDevice *device = NULL;
          ClutterActor *hand = NULL;

          device = (ClutterX11XInputDevice *)stage_devices->data;

          if (clutter_x11_get_input_device_type (device)
              == CLUTTER_X11_XINPUT_POINTER_DEVICE)
            {

              g_debug("got a pointer device...\n");

              hand = clutter_texture_new_from_file ("redhand.png", NULL);
              g_hash_table_insert (app->devices, device, hand);
              clutter_container_add_actor (CLUTTER_CONTAINER (stage), hand);
            }

        }
    } while ((stage_devices = stage_devices->next) != NULL);

  clutter_main ();

  return 0;
} 
