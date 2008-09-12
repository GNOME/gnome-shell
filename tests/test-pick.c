#include <clutter/clutter.h>

#define STAGE_WIDTH  320
#define STAGE_HEIGHT 200
#define ACTORS_X 12
#define ACTORS_Y 16

typedef struct _Data Data;

struct _Data
{
  ClutterActor *stage;
  int y, x;
  guint32 gids[ACTORS_X * ACTORS_Y];
  guint actor_width, actor_height;
  int ret;
};

static gboolean
on_timeout (Data *data)
{
  int y, x;

  for (y = 0; y < ACTORS_Y; y++)
    for (x = 0; x < ACTORS_X; x++)
      {
	gboolean pass = FALSE;
	guint32 gid;
	ClutterActor *actor
	  = clutter_stage_get_actor_at_pos (CLUTTER_STAGE (data->stage),
					    x * data->actor_width
					    + data->actor_width / 2,
					    y * data->actor_height
					    + data->actor_height / 2);

	printf ("actor %u -> ", data->gids[y * ACTORS_X + x]);
	
	if (actor == NULL)
	  printf ("NULL:     FAIL\n");
	else
	  {
	    gid = clutter_actor_get_gid (actor);
	    if (gid == data->gids[y * ACTORS_X + x])
	      pass = TRUE;
	    printf ("% 8i: %s\n", gid, pass ? "pass" : "FAIL");
	  }

	if (!pass)
	  data->ret = 1;
      }

  clutter_main_quit ();

  return FALSE;
}

int
main (int argc, char **argv)
{
  int y, x;
  Data data;
  
  data.ret = 0;

  clutter_init (&argc, &argv);

  data.stage = clutter_stage_get_default ();

  clutter_actor_set_size (data.stage, STAGE_WIDTH, STAGE_HEIGHT);
  data.actor_width = STAGE_WIDTH / ACTORS_X;
  data.actor_height = STAGE_HEIGHT / ACTORS_Y;

  for (y = 0; y < ACTORS_Y; y++)
    for (x = 0; x < ACTORS_X; x++)
      {
	ClutterColor color = { x * 255 / (ACTORS_X - 1),
			       y * 255 / (ACTORS_Y - 1),
			       128, 255 };
	ClutterGeometry geom = { x * data.actor_width, y * data.actor_height,
				 data.actor_width, data.actor_height };
	ClutterActor *rect = clutter_rectangle_new_with_color (&color);

	clutter_actor_set_geometry (rect, &geom);

	clutter_container_add (CLUTTER_CONTAINER (data.stage), rect, NULL);

	data.gids[y * ACTORS_X + x] = clutter_actor_get_gid (rect);
      }

  clutter_actor_show (data.stage);

  g_timeout_add (250, (GSourceFunc) on_timeout, &data);

  clutter_main ();

  printf ("end result: %s\n", data.ret ? "FAIL" : "pass");

  return data.ret;
}
