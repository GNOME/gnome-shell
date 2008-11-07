#include <clutter/clutter.h>

#include "test-conform-common.h"

#define STAGE_WIDTH  320
#define STAGE_HEIGHT 200
#define ACTORS_X 12
#define ACTORS_Y 16

typedef struct _State State;

struct _State
{
  ClutterActor *stage;
  int y, x;
  guint32 gids[ACTORS_X * ACTORS_Y];
  guint actor_width, actor_height;
  int ret;
};

static gboolean
on_timeout (State *state)
{
  int y, x;

  for (y = 0; y < ACTORS_Y; y++)
    for (x = 0; x < ACTORS_X; x++)
      {
	gboolean pass = FALSE;
	guint32 gid;
	ClutterActor *actor
	  = clutter_stage_get_actor_at_pos (CLUTTER_STAGE (state->stage),
					    x * state->actor_width
					    + state->actor_width / 2,
					    y * state->actor_height
					    + state->actor_height / 2);

	printf ("actor %u -> ", state->gids[y * ACTORS_X + x]);
	
	if (actor == NULL)
	  printf ("NULL:     FAIL\n");
	else
	  {
	    gid = clutter_actor_get_gid (actor);
	    if (gid == state->gids[y * ACTORS_X + x])
	      pass = TRUE;
	    printf ("% 8i: %s\n", gid, pass ? "pass" : "FAIL");
	  }

	if (!pass)
	  state->ret = 1;
      }

  clutter_main_quit ();

  return FALSE;
}

void
test_pick (TestConformSimpleFixture *fixture,
	   gconstpointer data)
{
  int y, x;
  State state;
  
  state.ret = 0;

  state.stage = clutter_stage_new ();

  clutter_actor_set_size (state.stage, STAGE_WIDTH, STAGE_HEIGHT);
  state.actor_width = STAGE_WIDTH / ACTORS_X;
  state.actor_height = STAGE_HEIGHT / ACTORS_Y;

  for (y = 0; y < ACTORS_Y; y++)
    for (x = 0; x < ACTORS_X; x++)
      {
	ClutterColor color = { x * 255 / (ACTORS_X - 1),
			       y * 255 / (ACTORS_Y - 1),
			       128, 255 };
	ClutterGeometry geom = { x * state.actor_width, y * state.actor_height,
				 state.actor_width, state.actor_height };
	ClutterActor *rect = clutter_rectangle_new_with_color (&color);

	clutter_actor_set_geometry (rect, &geom);

	clutter_container_add (CLUTTER_CONTAINER (state.stage), rect, NULL);

	state.gids[y * ACTORS_X + x] = clutter_actor_get_gid (rect);
      }

  clutter_actor_show (state.stage);

  g_timeout_add (250, (GSourceFunc) on_timeout, &state);

  clutter_main ();

  clutter_actor_destroy (state.stage);

  printf ("end result: %s\n", state.ret ? "FAIL" : "pass");

  if (state.ret)
    exit (state.ret);

  return;
}

