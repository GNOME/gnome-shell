#include <clutter/clutter.h>

#include <stdlib.h>
#include <string.h>

#define STAGE_WIDTH  640
#define STAGE_HEIGHT 480

#define COLS 18
#define ROWS 20

gboolean idle (gpointer data)
{
  ClutterActor *stage = CLUTTER_ACTOR (data);
  
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
  
  clutter_actor_paint (stage);
  ++fps;

  return TRUE;
}

int
main (int argc, char *argv[])
{
  ClutterActor    *stage;
  ClutterColor     stage_color = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor     label_color = { 0xff, 0xff, 0xff, 0xff };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

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
          clutter_text_set_color (CLUTTER_TEXT (label), &label_color);
          clutter_actor_set_position (label, (1.0*STAGE_WIDTH/COLS)*col,
                                             (1.0*STAGE_HEIGHT/ROWS)*row);
          /*clutter_actor_set_clip (label, 0,0, (1.0*STAGE_WIDTH/COLS),
                                              (1.0*STAGE_HEIGHT/ROWS));*/
          clutter_actor_set_scale (label, scale, scale);
          clutter_text_set_line_wrap (CLUTTER_TEXT (label), FALSE);
          clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);
        }
  }
  clutter_actor_show_all (stage);

  g_signal_connect (stage, "key-press-event",
		    G_CALLBACK (clutter_main_quit), NULL);
  
  g_idle_add (idle, (gpointer) stage);

  clutter_main();

  return 0;
}
