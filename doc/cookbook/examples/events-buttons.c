#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor green_color = { 0x00, 0xff, 0x00, 0xff };

static gboolean
button_event_cb (ClutterActor *actor,
                 ClutterEvent *event,
                 gpointer      user_data)
{
  gfloat x, y;
  gchar *event_type;
  guint button_pressed;
  ClutterModifierType state;
  gchar *ctrl_pressed;
  guint32 click_count;

  /* where the pointer was when the button event occurred */
  clutter_event_get_coords (event, &x, &y);

  /* check whether it was a press or release event */
  event_type = "released";
  if (clutter_event_type (event) == CLUTTER_BUTTON_PRESS)
    event_type = "pressed";

  /* which button triggered the event */
  button_pressed = clutter_event_get_button (event);

  /* keys down when the button was pressed */
  state = clutter_event_get_state (event);

  ctrl_pressed = "ctrl not pressed";
  if (state & CLUTTER_CONTROL_MASK)
    ctrl_pressed = "ctrl pressed";

  /* click count */
  click_count = clutter_event_get_click_count (event);

  g_debug ("button %d %s at %.0f,%.0f; %s; click count %d",
           button_pressed,
           event_type,
           x,
           y,
           ctrl_pressed,
           click_count);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *red;
  ClutterActor *green;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  red = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_size (red, 100, 100);
  clutter_actor_set_position (red, 50, 150);
  clutter_actor_set_reactive (red, TRUE);

  green = clutter_rectangle_new_with_color (&green_color);
  clutter_actor_set_size (green, 100, 100);
  clutter_actor_set_position (green, 250, 150);
  clutter_actor_set_reactive (green, TRUE);

  g_signal_connect (red,
                    "button-press-event",
                    G_CALLBACK (button_event_cb),
                    NULL);

  g_signal_connect (red,
                    "button-release-event",
                    G_CALLBACK (button_event_cb),
                    NULL);

  g_signal_connect (green,
                    "button-press-event",
                    G_CALLBACK (button_event_cb),
                    NULL);

  g_signal_connect (green,
                    "button-release-event",
                    G_CALLBACK (button_event_cb),
                    NULL);

  clutter_container_add (CLUTTER_CONTAINER (stage),
                         red,
                         green,
                         NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
