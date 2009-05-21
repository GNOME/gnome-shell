#include <clutter/clutter.h>

#include <stdlib.h>
#include <string.h>

#define STAGE_WIDTH  800
#define STAGE_HEIGHT 600

static int font_size;
static int n_chars;
static int rows, cols;

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
      printf ("fps=%d, strings/sec=%d, chars/sec=%d\n",
	      fps,
	      fps * rows * cols,
	      fps * rows * cols * n_chars);
      g_timer_start (timer);
      fps = 0;
    }

  clutter_actor_paint (stage);
  ++fps;

  return TRUE;
}

static ClutterActor *
create_label ()
{
  ClutterColor label_color = { 0xff, 0xff, 0xff, 0xff };
  ClutterActor *label;
  char         *font_name;
  GString      *str;
  int           i;

  font_name = g_strdup_printf ("Monospace %dpx", font_size);

  str = g_string_new (NULL);
  for (i = 0; i < n_chars; i++)
    g_string_append_c (str, 'A' + (i % 26));

  label = clutter_text_new_with_text (font_name, str->str);
  clutter_text_set_color (CLUTTER_TEXT (label), &label_color);

  g_free (font_name);
  g_string_free (str, TRUE);

  return label;
}

int
main (int argc, char *argv[])
{
  ClutterActor    *stage;
  ClutterColor     stage_color = { 0x00, 0x00, 0x00, 0xff };
  ClutterActor    *label;
  int              w, h;
  int              row, col;

  g_setenv ("CLUTTER_VBLANK", "none", FALSE);

  clutter_init (&argc, &argv);

  if (argc != 3)
    {
      g_printerr ("Usage test-text-perf FONT_SIZE N_CHARS\n");
      exit (1);
    }

  font_size = atoi (argv[1]);
  n_chars = atoi (argv[2]);

  g_print ("Monospace %dpx, string length = %d\n", font_size, n_chars);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  label = create_label ();
  w = clutter_actor_get_width (label);
  h = clutter_actor_get_height (label);
  cols = STAGE_WIDTH / w;
  rows = STAGE_HEIGHT / h;
  clutter_actor_destroy (label);

  if (cols == 0 || rows == 0)
    {
      g_printerr("Too many characters to fit in stage\n");
      exit(1);
    }

  for (row=0; row<rows; row++)
    for (col=0; col<cols; col++)
      {
	label = create_label();
	clutter_actor_set_position (label, w * col, h * row);
	clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);
      }

  clutter_actor_show_all (stage);

  g_idle_add (idle, (gpointer) stage);

  clutter_main ();

  return 0;
}
