#include <gmodule.h>
#include <clutter/clutter.h>
#include <string.h>

#define DOT_SIZE 2
#define TEX_SIZE 64

typedef struct _CallbackData CallbackData;

struct _CallbackData
{
  ClutterActor *stage;
  ClutterActor *tex;
  ClutterActor *box;
  ClutterMotionEvent event;
  guint idle_source;
};

static ClutterActor *
make_label (void)
{
  ClutterActor *label;
  gchar *text;
  gchar *argv[] = { "ls", "--help", NULL };

  label = clutter_label_new ();
  clutter_label_set_font_name (CLUTTER_LABEL (label), "Sans 10");

  if (g_spawn_sync (NULL, argv, NULL,
		    G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH,
		    NULL, NULL, &text, NULL, NULL, NULL))
    {
      clutter_label_set_text (CLUTTER_LABEL (label), text);
      g_free (text);
    }

  return label;
}

static ClutterActor *
make_tex (void)
{
  ClutterActor *tex = clutter_texture_new ();

  clutter_actor_set_size (tex, TEX_SIZE * 2, TEX_SIZE * 2);

  return tex;
}

static ClutterActor *
make_box (void)
{
  ClutterActor *box;
  static const ClutterColor blue = { 0x00, 0x00, 0xff, 0xff };

  box = clutter_rectangle_new_with_color (&blue);
  clutter_actor_set_size (box, DOT_SIZE + 2, DOT_SIZE + 2);
  clutter_actor_hide (box);

  return box;
}

static gboolean
on_motion_idle (gpointer user_data)
{
  CallbackData *data = (CallbackData *) user_data;
  guchar *pixels, *p;
  guint stage_width, stage_height;
  gint x, y;

  data->idle_source = 0;

  clutter_actor_get_size (data->stage, &stage_width, &stage_height);

  x = CLAMP (data->event.x - TEX_SIZE / 2, 0, (int) stage_width - TEX_SIZE);
  y = CLAMP (data->event.y - TEX_SIZE / 2, 0, (int) stage_height - TEX_SIZE);

  clutter_actor_set_position (data->box, x + TEX_SIZE / 2 - 1,
			      y + TEX_SIZE / 2 - 1);
  clutter_actor_show (data->box);
  /* Redraw so that the layouting will be done and the box will be
     drawn in the right position */
  clutter_redraw (CLUTTER_STAGE (data->stage));

  pixels = clutter_stage_read_pixels (CLUTTER_STAGE (data->stage),
				      x, y, TEX_SIZE, TEX_SIZE);

  /* Make a red dot in the center */
  p = pixels + (TEX_SIZE / 2 - DOT_SIZE / 2) * TEX_SIZE * 4
    + (TEX_SIZE / 2 - DOT_SIZE / 2) * 4;
  for (y = 0; y < DOT_SIZE; y++)
    {
      for (x = 0; x < DOT_SIZE; x++)
	{
	  *(p++) = 255;
	  memset (p, 0, 3);
	  p += 3;
	}
      p += TEX_SIZE * 4 - DOT_SIZE * 4;
    }

  /* Set all of the alpa values to full */
  for (p = pixels + TEX_SIZE * TEX_SIZE * 4; p > pixels; p -= 4)
    *(p - 1) = 255;

  clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (data->tex),
				     pixels, TRUE,
				     TEX_SIZE, TEX_SIZE,
				     TEX_SIZE * 4, 4, 0, NULL);
  g_free (pixels);

  return FALSE;
}

static gboolean
on_motion (ClutterActor *stage, ClutterMotionEvent *event, CallbackData *data)
{
  /* Handle the motion event in an idle handler so that multiple
     events will be combined into one */
  if (data->idle_source == 0)
    data->idle_source = clutter_threads_add_idle (on_motion_idle, data);

  data->event = *event;

  return FALSE;
}

G_MODULE_EXPORT int
test_stage_read_pixels_main (int argc, char **argv)
{
  CallbackData data;

  clutter_init (&argc, &argv);

  data.idle_source = 0;
  data.stage = clutter_stage_get_default ();

  data.tex = make_tex ();
  data.box = make_box ();
  clutter_actor_set_position (data.tex,
			      clutter_actor_get_width (data.stage)
			      - clutter_actor_get_width (data.tex),
			      clutter_actor_get_height (data.stage)
			      - clutter_actor_get_height (data.tex));

  clutter_container_add (CLUTTER_CONTAINER (data.stage),
			 make_label (), data.tex, data.box, NULL);

  g_signal_connect (data.stage, "motion-event", G_CALLBACK (on_motion), &data);

  clutter_actor_show (data.stage);

  clutter_main ();

  return 0;
}
