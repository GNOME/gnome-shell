#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include <clutter/clutter.h>
#include <string.h>

static CoglHandle
make_texture (void)
{
  guint32 *data = g_malloc (100 * 100 * 4);
  int x;
  int y;

  for (y = 0; y < 100; y ++)
    for (x = 0; x < 100; x++)
      {
        if (x < 50 && y < 50)
          data[y * 100 + x] = 0xff00ff00;
        else
          data[y * 100 + x] = 0xff00ffff;
      }
  return cogl_texture_new_from_data (100,
                                     100,
                                     COGL_TEXTURE_NONE,
                                     COGL_PIXEL_FORMAT_ARGB_8888,
                                     COGL_PIXEL_FORMAT_ARGB_8888,
                                     400,
                                     (guchar *)data);
}

static void
texture_pick_with_alpha (void)
{
  ClutterTexture *tex = CLUTTER_TEXTURE (clutter_texture_new ());
  ClutterStage *stage = CLUTTER_STAGE (clutter_test_get_stage ());
  ClutterActor *actor;

  clutter_texture_set_cogl_texture (tex, make_texture ());

  clutter_actor_add_child (CLUTTER_ACTOR (stage), CLUTTER_ACTOR (tex));

  clutter_actor_show (CLUTTER_ACTOR (stage));

  if (g_test_verbose ())
    {
      g_print ("\nstage = %p\n", stage);
      g_print ("texture = %p\n\n", tex);
    }

  clutter_texture_set_pick_with_alpha (tex, TRUE);
  if (g_test_verbose ())
    g_print ("Testing with pick-with-alpha enabled:\n");

  /* This should fall through and hit the stage: */
  actor = clutter_stage_get_actor_at_pos (stage, CLUTTER_PICK_ALL, 10, 10);
  if (g_test_verbose ())
    g_print ("actor @ (10, 10) = %p\n", actor);
  g_assert (actor == CLUTTER_ACTOR (stage));

  /* The rest should hit the texture */
  actor = clutter_stage_get_actor_at_pos (stage, CLUTTER_PICK_ALL, 90, 10);
  if (g_test_verbose ())
    g_print ("actor @ (90, 10) = %p\n", actor);
  g_assert (actor == CLUTTER_ACTOR (tex));
  actor = clutter_stage_get_actor_at_pos (stage, CLUTTER_PICK_ALL, 90, 90);
  if (g_test_verbose ())
    g_print ("actor @ (90, 90) = %p\n", actor);
  g_assert (actor == CLUTTER_ACTOR (tex));
  actor = clutter_stage_get_actor_at_pos (stage, CLUTTER_PICK_ALL, 10, 90);
  if (g_test_verbose ())
    g_print ("actor @ (10, 90) = %p\n", actor);
  g_assert (actor == CLUTTER_ACTOR (tex));

  clutter_texture_set_pick_with_alpha (tex, FALSE);
  if (g_test_verbose ())
    g_print ("Testing with pick-with-alpha disabled:\n");

  actor = clutter_stage_get_actor_at_pos (stage, CLUTTER_PICK_ALL, 10, 10);
  if (g_test_verbose ())
    g_print ("actor @ (10, 10) = %p\n", actor);
  g_assert (actor == CLUTTER_ACTOR (tex));
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/texture/pick-with-alpha", texture_pick_with_alpha)
)
