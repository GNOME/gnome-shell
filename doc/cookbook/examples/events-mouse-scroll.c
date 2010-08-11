#include <clutter/clutter.h>

#define STAGE_HEIGHT 400
#define STAGE_WIDTH STAGE_HEIGHT
#define SCROLL_AMOUNT STAGE_HEIGHT * 0.125

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };

static gboolean
_scroll_event_cb (ClutterActor *viewport,
                  ClutterEvent *event,
                  gpointer      user_data)
{
  ClutterActor *scrollable = CLUTTER_ACTOR (user_data);

  gfloat y = clutter_actor_get_y (scrollable);

  ClutterScrollDirection direction;
  direction = clutter_event_get_scroll_direction (event);

  switch (direction)
    {
    case CLUTTER_SCROLL_UP:
      y -= SCROLL_AMOUNT;
      break;
    case CLUTTER_SCROLL_DOWN:
      y += SCROLL_AMOUNT;
      break;
    case CLUTTER_SCROLL_LEFT:
    case CLUTTER_SCROLL_RIGHT:
      break;
    }

  y = CLAMP (y,
             clutter_actor_get_height (viewport)
             - clutter_actor_get_height (scrollable),
             0.0);

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

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* the scrollable actor */
  texture = clutter_texture_new ();
  clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture),
                                         TRUE);

  /* set the texture's height so it's as tall as the stage */
  clutter_actor_set_request_mode (texture, CLUTTER_REQUEST_WIDTH_FOR_HEIGHT);
  clutter_actor_set_height (texture, STAGE_HEIGHT);

  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                 TESTS_DATA_DIR "/redhand.png",
                                 NULL);

  /* the viewport which the box is scrolled within */
  viewport = clutter_group_new ();

  /* viewport is shorter than the stage */
  clutter_actor_set_size (viewport, STAGE_WIDTH, STAGE_HEIGHT * 0.5);

  /* align the viewport to the center of the stage's y axis */
  clutter_actor_add_constraint (viewport, clutter_align_constraint_new (stage, CLUTTER_BIND_Y, 0.5));

  /* viewport needs to respond to scroll events */
  clutter_actor_set_reactive (viewport, TRUE);

  /* clip all actors inside the viewport to that group's allocation */
  clutter_actor_set_clip_to_allocation (viewport, TRUE);

  /* put the texture inside the viewport */
  clutter_container_add_actor (CLUTTER_CONTAINER (viewport), texture);

  /* add the viewport to the stage */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), viewport);

  g_signal_connect (viewport,
                    "scroll-event",
                    G_CALLBACK (_scroll_event_cb),
                    texture);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
