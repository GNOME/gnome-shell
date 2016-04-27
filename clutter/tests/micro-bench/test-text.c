#include <clutter/clutter.h>

#include <stdlib.h>
#include <string.h>

#define STAGE_WIDTH  640
#define STAGE_HEIGHT 480

#define COLS 18
#define ROWS 20

static void
on_paint (ClutterActor *actor, gconstpointer *data)
{
  static GTimer *timer = NULL;
  static int fps = 0;
  
  if (!timer)
    {
      timer = g_timer_new ();
      g_timer_start (timer);
    }
  
  if (g_timer_elapsed (timer, NULL) >= 1)
    {
      printf ("fps: %d\n", fps);
      g_timer_start (timer);
      fps = 0;
    }
  
  ++fps;
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return G_SOURCE_CONTINUE;
}

int
main (int argc, char *argv[])
{
  ClutterActor    *stage;
  ClutterActor    *group;

  g_setenv ("CLUTTER_VBLANK", "none", FALSE);
  g_setenv ("CLUTTER_DEFAULT_FPS", "1000", FALSE);

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  clutter_stage_set_color (CLUTTER_STAGE (stage), CLUTTER_COLOR_Black);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Text");

  group = clutter_group_new ();
  clutter_actor_set_size (group, STAGE_WIDTH, STAGE_WIDTH);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  clutter_threads_add_idle (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), NULL);

  {
    gint row, col;

    for (row=0; row<ROWS; row++)
      for (col=0; col<COLS; col++)
        {
          ClutterActor *label;
          gchar font_name[64];
          gchar text[64];
          gint  font_size = row+10;
          gdouble scale = 0.17 + (1.5 * col / COLS);

          sprintf (font_name, "Sans %ipx", font_size);
          sprintf (text, "OH");

          if (row==0)
            {
              sprintf (font_name, "Sans 10px");
              sprintf (text, "%1.2f", scale);
              font_size = 10;
              scale = 1.0;
            }
          if (col==0)
            {
              sprintf (font_name, "Sans 10px");
              sprintf (text, "%ipx", font_size);
              if (row == 0)
                strcpy (text, "");
              font_size = 10;
              scale = 1.0;
            }

          label = clutter_text_new_with_text (font_name, text);
          clutter_text_set_color (CLUTTER_TEXT (label), CLUTTER_COLOR_White);
          clutter_actor_set_position (label, (1.0*STAGE_WIDTH/COLS)*col,
                                             (1.0*STAGE_HEIGHT/ROWS)*row);
          /*clutter_actor_set_clip (label, 0,0, (1.0*STAGE_WIDTH/COLS),
                                              (1.0*STAGE_HEIGHT/ROWS));*/
          clutter_actor_set_scale (label, scale, scale);
          clutter_text_set_line_wrap (CLUTTER_TEXT (label), FALSE);
          clutter_container_add_actor (CLUTTER_CONTAINER (group), label);
        }
  }
  clutter_actor_show_all (stage);

  g_signal_connect (stage, "key-press-event",
		    G_CALLBACK (clutter_main_quit), NULL);

  clutter_main();

  clutter_actor_destroy (stage);

  return 0;
}
