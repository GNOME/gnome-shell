#include <clutter/clutter.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <gmodule.h>

#define TL_SCALE 5.0f

typedef struct _CallbackData CallbackData;

struct _CallbackData
{
  ClutterActor *stage, *group, *rect, *hand;
};

static void
on_new_frame (ClutterTimeline *tl, int frame_num, CallbackData *data)
{
  int i;
  int stage_width = clutter_actor_get_width (data->stage);
  int stage_height = clutter_actor_get_height (data->stage);
  gdouble progress = clutter_timeline_get_progress (tl);
  gdouble angle = progress * 2 * M_PI * TL_SCALE;
  gdouble rotation[3];

  gdouble xpos = stage_width * 0.45 * sin (angle) + stage_width / 8;
  gdouble ypos = stage_height * 0.45 * sin (angle) + stage_height / 8;
  gdouble zpos = stage_width * cos (angle) - stage_width / 2;

  clutter_actor_set_position (data->hand, xpos, ypos);
  clutter_actor_set_depth (data->hand, zpos);
  clutter_actor_set_rotation (data->hand, CLUTTER_Y_AXIS,
			      angle / M_PI * 180.0 * 3,
			      clutter_actor_get_width (data->hand) / 2,
			      clutter_actor_get_height (data->hand) / 2,
			      0);

  memset (rotation, 0, sizeof (rotation));

  if (progress < 1 / 3.0)
    rotation[2] = 360 * progress * 3;
  else if (progress < 2 / 3.0)
    rotation[1] = 360 * progress * 3;
  else
    rotation[0] = 360 * progress * 3;

  for (i = 0; i < 3; i++)
    {
      clutter_actor_set_rotation (data->group, i,
				  rotation[i],
				  clutter_actor_get_width (data->rect) / 2,
				  clutter_actor_get_height (data->rect) / 2,
				  0);
      clutter_actor_set_rotation (data->rect, i,
				  rotation[i],
				  clutter_actor_get_width (data->rect) / 2,
				  clutter_actor_get_height (data->rect) / 2,
				  0);
    }
}

G_MODULE_EXPORT int
test_clip_main (int argc, char **argv)
{
  ClutterGeometry geom;
  ClutterTimeline *tl;
  ClutterColor blue = { 0x40, 0x40, 0xff, 0xff };
  CallbackData data;
  ClutterActor *other_hand;
  int x, y;

  clutter_init (&argc, &argv);

  data.stage = clutter_stage_get_default ();

  data.group = clutter_group_new ();

  clutter_actor_get_geometry (data.stage, &geom);
  geom.x = geom.width / 4;
  geom.y = geom.height / 4;
  geom.width /= 2;
  geom.height /= 2;
  clutter_actor_set_geometry (data.group, &geom);

  data.rect = clutter_rectangle_new_with_color (&blue);
  clutter_actor_set_geometry (data.rect, &geom);
  clutter_container_add (CLUTTER_CONTAINER (data.stage), data.rect, NULL);

  clutter_container_add (CLUTTER_CONTAINER (data.stage), data.group, NULL);

  clutter_actor_set_clip (data.group, 0, 0, geom.width, geom.height);

  data.hand = clutter_texture_new_from_file ("redhand.png", NULL);
  if (data.hand == NULL)
    {
      g_critical ("pixbuf loading failed");
      exit (1);
    }
  clutter_container_add (CLUTTER_CONTAINER (data.group), data.hand, NULL);

  /* Add a hand at each of the four corners of the group */
  for (y = 0; y < 2; y++)
    for (x = 0; x < 2; x++)
      {
	other_hand = clutter_clone_texture_new (CLUTTER_TEXTURE (data.hand));
	clutter_actor_set_anchor_point_from_gravity
	  (other_hand, CLUTTER_GRAVITY_CENTER);
	clutter_actor_set_position (other_hand,
				    x * geom.width,
				    y * geom.height);
	clutter_container_add (CLUTTER_CONTAINER (data.group),
			       other_hand, NULL);
      }

  clutter_actor_raise_top (data.hand);

  tl = clutter_timeline_new (360 * TL_SCALE, 60);
  clutter_timeline_start (tl);
  clutter_timeline_set_loop (tl, TRUE);

  g_signal_connect (tl, "new-frame", G_CALLBACK (on_new_frame), &data);

  clutter_actor_show (data.stage);

  clutter_main ();

  return 0;
}
