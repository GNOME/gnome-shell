/*
 * Load an image into a texture, which can then be zoomed in/out
 * (double click on button 1, double click on button 3 respectively);
 * also resets the texture to the stage center when a key is pressed
 * (better would be to prevent drags taking the actor off-stage,
 * but the implementation is much more complicated)
 */
#include <stdlib.h>
#include <clutter/clutter.h>

#define STAGE_SIDE 400.0

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };

/* on key press, center the actor on the stage;
 * useful if you drag it off-stage accidentally
 */
static gboolean
key_press_cb (ClutterActor *actor,
              ClutterEvent *event,
              gpointer      user_data)
{
  gfloat width, height;

  clutter_actor_get_size (actor, &width, &height);

  clutter_actor_set_anchor_point (actor, width / 2, height / 2);

  clutter_actor_set_position (actor,
                              STAGE_SIDE / 2,
                              STAGE_SIDE / 2);

  return TRUE;
}

/* on double click, zoom in on the clicked point;
 * also keeps scale in the range 0.1 to 20
 */
static gboolean
clicked_cb (ClutterActor *actor,
            ClutterEvent *event,
            gpointer      user_data)
{
  gdouble scale;
  gfloat click_x, click_y;
  gfloat click_target_x, click_target_y;
  guint32 button;

  /* don't do anything unless there was a double click */
  if (clutter_event_get_click_count (event) < 2)
    return TRUE;

  /* work out new scale */
  button = clutter_event_get_button (event);

  clutter_actor_get_scale (actor, &scale, NULL);

  if (button == CLUTTER_BUTTON_PRIMARY)
    scale *= 1.2;
  else if (button == CLUTTER_BUTTON_SECONDARY)
    scale /= 1.2;

  /* don't do anything if scale is outside bounds */
  if (scale < 0.1 || scale > 20.0)
    return TRUE;

  /* get the location of the click on the scaled actor */
  clutter_event_get_coords (event, &click_x, &click_y);
  clutter_actor_transform_stage_point (actor,
                                       click_x, click_y,
                                       &click_target_x, &click_target_y);

  /* anchor the actor on the clicked point on its surface */
  clutter_actor_set_anchor_point (actor, click_target_x, click_target_y);

  /* set the actor's position to the click coords: it won't move,
   * because the anchor point is already there; but
   * the scale will now be centered on these coords (as the
   * scale center defaults to the anchor point); so the anchor point
   * on the actor won't move from under the pointer
   */
  clutter_actor_set_position (actor, click_x, click_y);

  clutter_actor_animate (actor, CLUTTER_LINEAR, 500,
                         "scale-x", scale,
                         "scale-y", scale,
                         NULL);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *texture;
  gchar *image_path;
  GError *error = NULL;

  if (argc < 2)
    {
      g_print ("Usage: %s <path to image file>\n", argv[0]);
      exit (EXIT_FAILURE);
    }

  image_path = argv[1];

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, STAGE_SIDE, STAGE_SIDE);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  texture = clutter_texture_new ();
  clutter_actor_set_reactive (texture, TRUE);
  clutter_actor_set_width (texture, STAGE_SIDE);
  clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture), TRUE);

  clutter_actor_add_action (texture, clutter_drag_action_new ());

  g_object_set (G_OBJECT (texture),
                "scale-gravity", CLUTTER_GRAVITY_NORTH_WEST,
                NULL);

  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture), image_path, &error);

  if (error != NULL)
    {
      g_warning ("Error loading %s\n%s", image_path, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  clutter_actor_set_y (texture, (STAGE_SIDE - clutter_actor_get_height (texture)) * 0.5);

  g_signal_connect (texture,
                    "button-release-event",
                    G_CALLBACK (clicked_cb),
                    NULL);

  g_signal_connect_swapped (stage,
                            "key-press-event",
                            G_CALLBACK (key_press_cb),
                            texture);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), texture);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
