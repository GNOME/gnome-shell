#include <stdlib.h>
#include <gmodule.h>
#include <cairo.h>
#include <clutter/clutter.h>

#define N_RECTS         20

static gboolean is_homogeneous = FALSE;
static gboolean vertical       = FALSE;
static gboolean random_size    = FALSE;
static gboolean fixed_size     = FALSE;
static gboolean snap_to_grid   = TRUE;

static gint     n_rects        = N_RECTS;
static gint     x_spacing      = 0;
static gint     y_spacing      = 0;

static GOptionEntry entries[] = {
  {
    "random-size", 'r',
    0,
    G_OPTION_ARG_NONE,
    &random_size,
    "Randomly size the rectangles", NULL
  },
  {
    "num-rects", 'n',
    0,
    G_OPTION_ARG_INT,
    &n_rects,
    "Number of rectangles", "RECTS"
  },
  {
    "vertical", 'v',
    0,
    G_OPTION_ARG_NONE,
    &vertical,
    "Set vertical orientation", NULL
  },
  {
    "homogeneous", 'h',
    0,
    G_OPTION_ARG_NONE,
    &is_homogeneous,
    "Whether the layout should be homogeneous", NULL
  },
  {
    "x-spacing", 0,
    0,
    G_OPTION_ARG_INT,
    &x_spacing,
    "Horizontal spacing between elements", "PX"
  },
  {
    "y-spacing", 0,
    0,
    G_OPTION_ARG_INT,
    &y_spacing,
    "Vertical spacing between elements", "PX"
  },
  {
    "fixed-size", 'f',
    0,
    G_OPTION_ARG_NONE,
    &fixed_size,
    "Fix the layout size", NULL
  },
  {
    "no-snap-to-grid", 's',
    G_OPTION_FLAG_REVERSE,
    G_OPTION_ARG_NONE,
    &snap_to_grid,
    "Don't snap elements to grid", NULL
  },
  { NULL }
};

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *box;
  ClutterLayoutManager *layout;
  GError *error;
  gint i;

  error = NULL;
  if (clutter_init_with_args (&argc, &argv,
                              NULL,
                              entries,
                              NULL,
                              &error) != CLUTTER_INIT_SUCCESS)
    {
      g_print ("Unable to run flow-layout: %s", error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  stage = clutter_stage_new ();
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_LightSkyBlue);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Flow Layout");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  layout = clutter_flow_layout_new (vertical ? CLUTTER_FLOW_VERTICAL
                                             : CLUTTER_FLOW_HORIZONTAL);
  clutter_flow_layout_set_homogeneous (CLUTTER_FLOW_LAYOUT (layout),
                                       is_homogeneous);
  clutter_flow_layout_set_column_spacing (CLUTTER_FLOW_LAYOUT (layout),
                                          x_spacing);
  clutter_flow_layout_set_row_spacing (CLUTTER_FLOW_LAYOUT (layout),
                                       y_spacing);
  clutter_flow_layout_set_snap_to_grid (CLUTTER_FLOW_LAYOUT (layout),
                                        snap_to_grid);

  box = clutter_actor_new ();
  clutter_actor_set_layout_manager (box, layout);
  clutter_actor_set_background_color (box, CLUTTER_COLOR_Aluminium2);
  clutter_actor_add_child (stage, box);

  if (!fixed_size)
    clutter_actor_add_constraint (box, clutter_bind_constraint_new (stage, CLUTTER_BIND_SIZE, 0.0));

  clutter_actor_set_position (box, 0, 0);
  clutter_actor_set_name (box, "box");

  for (i = 0; i < n_rects; i++)
    {
      ClutterColor color = CLUTTER_COLOR_INIT (255, 255, 255, 255);
      gfloat width, height;
      ClutterActor *rect;
      gchar *name;

      name = g_strdup_printf ("rect%02d", i);

      clutter_color_from_hls (&color,
                              360.0 / n_rects * i,
                              0.5,
                              0.8);
      rect = clutter_actor_new ();
      clutter_actor_set_background_color (rect, &color);

      if (random_size)
        {
          width = g_random_int_range (50, 100);
          height = g_random_int_range (50, 100);
        }
      else
        width = height = 50.f;

      clutter_actor_set_size (rect, width, height);
      clutter_actor_set_name (rect, name);

      clutter_actor_add_child (box, rect);

      g_free (name);
    }

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
