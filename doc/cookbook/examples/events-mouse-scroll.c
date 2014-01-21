#include <clutter/clutter.h>

#define STAGE_HEIGHT 300
#define STAGE_WIDTH STAGE_HEIGHT
#define SCROLL_AMOUNT STAGE_HEIGHT * 0.125

static gboolean
_scroll_event_cb (ClutterActor *viewport,
                  ClutterEvent *event,
                  gpointer      user_data)
{
  ClutterActor *scrollable = CLUTTER_ACTOR (user_data);

  gfloat viewport_height = clutter_actor_get_height (viewport);
  gfloat scrollable_height = clutter_actor_get_height (scrollable);
  gfloat y;
  ClutterScrollDirection direction;

  /* no need to scroll if the scrollable is shorter than the viewport */
  if (scrollable_height < viewport_height)
    return TRUE;

  y = clutter_actor_get_y (scrollable);

  direction = clutter_event_get_scroll_direction (event);

  switch (direction)
    {
    case CLUTTER_SCROLL_UP:
      y -= SCROLL_AMOUNT;
      break;
    case CLUTTER_SCROLL_DOWN:
      y += SCROLL_AMOUNT;
      break;

    /* we're only interested in up and down */
    case CLUTTER_SCROLL_LEFT:
    case CLUTTER_SCROLL_RIGHT:
      break;
    }

  /*
   * the CLAMP macro returns a value for the first argument
   * that falls within the range specified by the second and
   * third arguments
   *
   * we allow the scrollable's y position to be decremented to the point
   * where its base is aligned with the base of the viewport
   */
  y = CLAMP (y,
             viewport_height - scrollable_height,
             0.0);

  /* animate the change to the scrollable's y coordinate */
  clutter_actor_animate (scrollable,
                         CLUTTER_EASE_OUT_CUBIC,
                         300,
                         "y", y,
                         NULL);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *viewport;
  ClutterActor *texture;

  const gchar *image_file_path = "redhand.png";

  if (argc > 1)
    {
      image_file_path = argv[1];
    }

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* the scrollable actor */
  texture = clutter_texture_new ();
  clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture),
                                         TRUE);

  /* set the texture's height so it's as tall as the stage */
  clutter_actor_set_request_mode (texture, CLUTTER_REQUEST_WIDTH_FOR_HEIGHT);
  clutter_actor_set_height (texture, STAGE_HEIGHT);

  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                 image_file_path,
                                 NULL);

  /* the viewport which the box is scrolled within */
  viewport = clutter_actor_new ();

  /* viewport is shorter than the stage */
  clutter_actor_set_size (viewport, STAGE_WIDTH, STAGE_HEIGHT * 0.5);

  /* align the viewport to the center of the stage's y axis */
  clutter_actor_add_constraint (viewport, clutter_align_constraint_new (stage, CLUTTER_BIND_Y, 0.5));

  /* viewport needs to respond to scroll events */
  clutter_actor_set_reactive (viewport, TRUE);

  /* clip all actors inside the viewport to that group's allocation */
  clutter_actor_set_clip_to_allocation (viewport, TRUE);

  /* put the texture inside the viewport */
  clutter_actor_add_child (viewport, texture);

  /* add the viewport to the stage */
  clutter_actor_add_child (stage, viewport);

  g_signal_connect (viewport,
                    "scroll-event",
                    G_CALLBACK (_scroll_event_cb),
                    texture);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
