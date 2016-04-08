#include <clutter/clutter.h>

#include <stdlib.h>
#include <string.h>

#define STAGE_WIDTH  800
#define STAGE_HEIGHT 600

static int font_size;
static int n_chars;
static int rows, cols;

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
      printf ("fps=%d, strings/sec=%d, chars/sec=%d\n",
	      fps,
	      fps * rows * cols,
	      fps * rows * cols * n_chars);
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

static gunichar
get_character (int ch)
{
  int total_letters = 0;
  int i;

  static const struct
  {
    gunichar first_letter;
    int n_letters;
  }
  ranges[] =
    {
      { 'a', 26 }, /* lower case letters */
      { 'A', 26 }, /* upper case letters */
      { '0', 10 }, /* digits */
      { 0x410, 0x40 }, /* cyrillic alphabet */
      { 0x3b1, 18 } /* greek alphabet */
    };

  for (i = 0; i < G_N_ELEMENTS (ranges); i++)
    total_letters += ranges[i].n_letters;

  ch %= total_letters;

  for (i = 0; i < G_N_ELEMENTS (ranges) - 1; i++)
    if (ch < ranges[i].n_letters)
      return ch + ranges[i].first_letter;
    else
      ch -= ranges[i].n_letters;

  return ch + ranges[i].first_letter;
}

static ClutterActor *
create_label (void)
{
  ClutterColor label_color = { 0xff, 0xff, 0xff, 0xff };
  ClutterActor *label;
  char         *font_name;
  GString      *str;
  int           i;

  font_name = g_strdup_printf ("Monospace %dpx", font_size);

  str = g_string_new (NULL);
  for (i = 0; i < n_chars; i++)
    g_string_append_unichar (str, get_character (i));

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
  ClutterActor    *label;
  int              w, h;
  int              row, col;
  float            scale = 1.0f;

  g_setenv ("CLUTTER_VBLANK", "none", FALSE);
  g_setenv ("CLUTTER_DEFAULT_FPS", "1000", FALSE);

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  if (argc != 3)
    {
      g_printerr ("Usage test-text-perf FONT_SIZE N_CHARS\n");
      exit (1);
    }

  font_size = atoi (argv[1]);
  n_chars = atoi (argv[2]);

  g_print ("Monospace %dpx, string length = %d\n", font_size, n_chars);

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  clutter_stage_set_color (CLUTTER_STAGE (stage), CLUTTER_COLOR_Black);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Text Performance");

  g_signal_connect (stage, "paint", G_CALLBACK (on_paint), NULL);

  label = create_label ();
  w = clutter_actor_get_width (label);
  h = clutter_actor_get_height (label);

  /* If the label is too big to fit on the stage then scale it so that
     it will fit */
  if (w > STAGE_WIDTH || h > STAGE_HEIGHT)
    {
      float x_scale = STAGE_WIDTH / (float) w;
      float y_scale = STAGE_HEIGHT / (float) h;

      if (x_scale < y_scale)
        {
          scale = x_scale;
          cols = 1;
          rows = STAGE_HEIGHT / (h * scale);
        }
      else
        {
          scale = y_scale;
          cols = STAGE_WIDTH / (w * scale);
          rows = 1;
        }

      g_print ("Text scaled by %f to fit on the stage\n", scale);
    }
  else
    {
      cols = STAGE_WIDTH / w;
      rows = STAGE_HEIGHT / h;
    }

  clutter_actor_destroy (label);

  for (row=0; row<rows; row++)
    for (col=0; col<cols; col++)
      {
	label = create_label();
        clutter_actor_set_scale (label, scale, scale);
	clutter_actor_set_position (label, w * col * scale, h * row * scale);
	clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);
      }

  clutter_actor_show_all (stage);

  clutter_threads_add_idle (queue_redraw, stage);

  clutter_main ();

  return 0;
}
