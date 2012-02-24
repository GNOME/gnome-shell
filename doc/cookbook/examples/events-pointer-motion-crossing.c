#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor yellow = { 0xaa, 0x99, 0x00, 0xff };
static const ClutterColor white = { 0xff, 0xff, 0xff, 0xff };

static gboolean
_pointer_enter_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  ClutterState *transitions = CLUTTER_STATE (user_data);
  clutter_state_set_state (transitions, "fade-in");
  return TRUE;
}

static gboolean
_pointer_leave_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  ClutterState *transitions = CLUTTER_STATE (user_data);
  clutter_state_set_state (transitions, "fade-out");
  return TRUE;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterLayoutManager *layout;
  ClutterActor *box;
  ClutterActor *rect;
  ClutterActor *text;
  ClutterState *transitions;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "btn");
  clutter_actor_set_background_color (stage, &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FILL,
                                   CLUTTER_BIN_ALIGNMENT_FILL);

  box = clutter_actor_new ();
  clutter_actor_set_layout_manager (box, layout);
  clutter_actor_set_position (box, 25, 25);
  clutter_actor_set_reactive (box, TRUE);
  clutter_actor_set_size (box, 100, 30);

  /* background for the button */
  rect = clutter_rectangle_new_with_color (&yellow);
  clutter_actor_add_child (box, rect);

  /* text for the button */
  text = clutter_text_new_full ("Sans 10pt", "Hover me", &white);

  /*
   * NB don't set the height, so the actor assumes the height of the text;
   * then when added to the bin layout, it gets centred on it;
   * also if you don't set the width, the layout goes gets really wide;
   * the 10pt text fits inside the 30px height of the rectangle
   */
  clutter_actor_set_width (text, 100);
  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (layout),
                          text,
                          CLUTTER_BIN_ALIGNMENT_CENTER,
                          CLUTTER_BIN_ALIGNMENT_CENTER);

  /* animations */
  transitions = clutter_state_new ();
  clutter_state_set (transitions, NULL, "fade-out",
                     box, "opacity", CLUTTER_LINEAR, 180,
                     NULL);

 /*
  * NB you can't use an easing mode where alpha > 1.0 if you're
  * animating to a value of 255, as the value you're animating
  * to will possibly go > 255
  */
  clutter_state_set (transitions, NULL, "fade-in",
                     box, "opacity", CLUTTER_LINEAR, 255,
                     NULL);

  clutter_state_set_duration (transitions, NULL, NULL, 50);

  clutter_state_warp_to_state (transitions, "fade-out");

  g_signal_connect (box,
                    "enter-event",
                    G_CALLBACK (_pointer_enter_cb),
                    transitions);

  g_signal_connect (box,
                    "leave-event",
                    G_CALLBACK (_pointer_leave_cb),
                    transitions);

  /* bind the stage size to the box size + 50px in each axis */
  clutter_actor_add_constraint (stage, clutter_bind_constraint_new (box, CLUTTER_BIND_HEIGHT, 50.0));
  clutter_actor_add_constraint (stage, clutter_bind_constraint_new (box, CLUTTER_BIND_WIDTH, 50.0));

  clutter_actor_add_child (stage, box);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (transitions);

  return 0;
}
