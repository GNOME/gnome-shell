
#include <clutter/clutter.h>

#include <errno.h>
#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>

#define STAGE_WIDTH   800
#define STAGE_HEIGHT  600

ClutterActor *
make_source (void)
{
  ClutterActor *source, *actor;
  GError *error = NULL;
  gchar *file;

  ClutterColor  yellow = {0xff, 0xff, 0x00, 0xff};

  source  = clutter_group_new ();

  file = g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
  actor = clutter_texture_new_from_file (file, &error);
  if (!actor)
    g_error("pixbuf load failed: %s", error ? error->message : "Unknown");

  g_free (file);

  clutter_group_add (source, actor);

  actor = clutter_text_new_with_text ("Sans Bold 50px", "Clutter");

  clutter_text_set_color (CLUTTER_TEXT (actor), &yellow);
  clutter_actor_set_y (actor, clutter_actor_get_height(source) + 5);
  clutter_group_add (source, actor);

  return source;
}

G_MODULE_EXPORT int
test_fbo_main (int argc, char *argv[])
{
  ClutterColor      blue   = {0x33, 0x44, 0x55, 0xff};

  ClutterActor     *fbo;
  ClutterActor     *onscreen_source;
  ClutterActor     *stage;
  ClutterAnimation *animation;
  int               x_pos = 200;
  int               y_pos = 100;

  clutter_init (&argc, &argv);

  if (clutter_feature_available (CLUTTER_FEATURE_OFFSCREEN) == FALSE)
    g_error("This test requires CLUTTER_FEATURE_OFFSCREEN");

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &blue);

  /* Create the first source */
  onscreen_source = make_source();
  clutter_actor_show_all (onscreen_source);
  clutter_group_add (stage, onscreen_source);

  y_pos = (STAGE_HEIGHT/2.0) -
          (clutter_actor_get_height (onscreen_source)/2.0);
  clutter_actor_set_position (onscreen_source, x_pos, y_pos);
  x_pos += clutter_actor_get_width (onscreen_source);

  animation = clutter_actor_animate (onscreen_source,
                                     CLUTTER_LINEAR,
                                     5000, /* 1 second duration */
                                     "rotation-angle-y", 360.0f,
                                     NULL);
  clutter_animation_set_loop (animation, TRUE);

  /* Second hand = actor from onscreen_source */
  if ((fbo = clutter_texture_new_from_actor (onscreen_source)) == NULL)
    g_error("onscreen fbo creation failed");

  clutter_actor_set_position (fbo, x_pos, y_pos);
  x_pos += clutter_actor_get_width (fbo);
  clutter_group_add (stage, fbo);

  /* Third hand = actor from Second hand */
  if ((fbo = clutter_texture_new_from_actor (fbo)) == NULL)
    g_error("fbo from fbo creation failed");

  clutter_actor_set_position (fbo, x_pos, y_pos);
  x_pos += clutter_actor_get_width (fbo);
  clutter_group_add (stage, fbo);

  clutter_actor_show_all (stage);
  clutter_main ();

  return 0;
}
