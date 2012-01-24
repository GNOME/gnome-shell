#include <stdlib.h>
#include "cb-button.h"

/* colors */
static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor white_color = { 0xff, 0xff, 0xff, 0xff };
static const ClutterColor yellow_color = { 0x88, 0x88, 0x00, 0xff };

/* click handler */
static void
clicked (CbButton *button,
         gpointer  data)
{
  const gchar *current_text;

  g_debug ("Clicked");

  current_text = cb_button_get_text (button);

  if (g_strcmp0 (current_text, "hello") == 0)
    cb_button_set_text (button, "world");
  else
    cb_button_set_text (button, "hello");
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *button;
  ClutterConstraint *align_x_constraint;
  ClutterConstraint *align_y_constraint;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  button = cb_button_new ();
  cb_button_set_text (CB_BUTTON (button), "hello");

  /* the following is equivalent to the two lines above:
   *
   *  button = g_object_new (CB_TYPE_BUTTON,
   *                         "text", "winkle",
   *                         NULL);
   *
   * because we defined a set_property function, which can accept
   * a PROP_TEXT parameter, GObject can create a button and set one
   * or more properties with a single call to g_object_new()
   */

  /* note that the size of the button is left to Clutter's size requisition */
  cb_button_set_text_color (CB_BUTTON (button), &white_color);
  cb_button_set_background_color (CB_BUTTON (button), &yellow_color);
  g_signal_connect (button, "clicked", G_CALLBACK (clicked), NULL);

  align_x_constraint = clutter_align_constraint_new (stage,
                                                     CLUTTER_ALIGN_X_AXIS,
                                                     0.5);

  align_y_constraint = clutter_align_constraint_new (stage,
                                                     CLUTTER_ALIGN_Y_AXIS,
                                                     0.5);

  clutter_actor_add_constraint (button, align_x_constraint);
  clutter_actor_add_constraint (button, align_y_constraint);

  clutter_actor_add_child (stage, button);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
