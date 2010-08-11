/*
 * Scroll in the y axis by moving an enclosed box (with layout)
 * up and down inside a "letterbox" in the center of the stage
 *
 * gcc -g -O0 -DTESTS_DATA_DIR="\"/home/ell/dev/clutter_src/tests/data\"" -o events-mouse-scroll events-mouse-scroll.c `pkg-config --libs --cflags clutter-1.0 glib-2.0` -lm
 */
#include <clutter/clutter.h>

#define STAGE_HEIGHT 400
#define STAGE_WIDTH STAGE_HEIGHT * 2
#define SCROLL_AMOUNT STAGE_HEIGHT * 0.2

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor box_color = { 0xaa, 0xaa, 0x55, 0xff };

static gboolean
_scroll_event_cb (ClutterActor *scroll,
                  ClutterEvent *event,
                  gpointer      user_data)
{
  ClutterActor *viewport = CLUTTER_ACTOR (user_data);

  gfloat y = clutter_actor_get_y (viewport);

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
    }

  y = CLAMP (y,
             clutter_actor_get_height (scroll)
             + clutter_actor_get_y (scroll)
             - clutter_actor_get_height (viewport),
             0.0);

  clutter_actor_animate (viewport,
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
  ClutterActor *scroll;
  ClutterActor *viewport;
  ClutterActor *texture;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* the "letterbox" which the viewport is scrolled within */
  scroll = clutter_group_new ();
  clutter_actor_set_size (scroll, STAGE_WIDTH, STAGE_HEIGHT * 0.75);
  clutter_actor_add_constraint (scroll, clutter_align_constraint_new (stage, CLUTTER_BIND_Y, 0.5));
  clutter_actor_set_reactive (scroll, TRUE);

  /* this clips all actors inside the scroll group to that group's allocation */
  clutter_actor_set_clip_to_allocation (scroll, TRUE);

  viewport = clutter_box_new (clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                                      CLUTTER_BIN_ALIGNMENT_CENTER));
  clutter_box_set_color (CLUTTER_BOX (viewport), &box_color);

  /* the actor to scroll */
  texture = clutter_texture_new ();
  clutter_actor_set_request_mode (texture, CLUTTER_REQUEST_HEIGHT_FOR_WIDTH);
  clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture),
                                         TRUE);

  /* the box resizes itself to fit this texture */
  clutter_actor_set_width (texture, STAGE_WIDTH);

  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                 TESTS_DATA_DIR "/redhand.png",
                                 NULL);

  g_signal_connect (scroll,
                    "scroll-event",
                    G_CALLBACK (_scroll_event_cb),
                    viewport);

  clutter_container_add_actor (CLUTTER_CONTAINER (viewport), texture);
  clutter_container_add_actor (CLUTTER_CONTAINER (scroll), viewport);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), scroll);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
