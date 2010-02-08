#include <stdlib.h>
#include <gmodule.h>
#include <cairo/cairo.h>
#include <clutter/clutter.h>

#define N_RECTS         20

static gboolean is_homogeneous = FALSE;
static gboolean vertical       = FALSE;
static gboolean random_size    = FALSE;

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
  { NULL }
};

static void
on_stage_resize (ClutterActor *stage,
                 const ClutterActorBox *allocation,
                 ClutterAllocationFlags flags,
                 ClutterActor *box)
{
  gfloat width, height;

  clutter_actor_box_get_size (allocation, &width, &height);

  if (vertical)
    clutter_actor_set_height (box, height);
  else
    clutter_actor_set_width (box, width);
}

G_MODULE_EXPORT int
test_flow_layout_main (int argc, char *argv[])
{
  ClutterActor *stage, *box;
  ClutterLayoutManager *layout;
  ClutterColor stage_color = { 0xe0, 0xf2, 0xfc, 0xff };
  ClutterColor box_color = { 255, 255, 255, 255 };
  GError *error;
  gint i;

  error = NULL;
  clutter_init_with_args (&argc, &argv,
                          NULL,
                          entries,
                          NULL,
                          &error);
  if (error)
    {
      g_print ("Unable to run test-flow: %s", error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  stage = clutter_stage_get_default ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Flow Layout");
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  layout = clutter_flow_layout_new (vertical ? CLUTTER_FLOW_VERTICAL
                                             : CLUTTER_FLOW_HORIZONTAL);
  clutter_flow_layout_set_homogeneous (CLUTTER_FLOW_LAYOUT (layout),
                                       is_homogeneous);
  clutter_flow_layout_set_column_spacing (CLUTTER_FLOW_LAYOUT (layout),
                                          x_spacing);
  clutter_flow_layout_set_row_spacing (CLUTTER_FLOW_LAYOUT (layout),
                                       y_spacing);

  box = clutter_box_new (layout);
  clutter_box_set_color (CLUTTER_BOX (box), &box_color);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);
  clutter_actor_set_position (box, 0, 0);

  if (vertical)
    clutter_actor_set_height (box, 480);
  else
    clutter_actor_set_width (box, 640);

  clutter_actor_set_name (box, "box");

  for (i = 0; i < n_rects; i++)
    {
      ClutterColor color = { 255, 255, 255, 224 };
      ClutterActor *rect;
      gchar *name;
      gfloat width, height;

      name = g_strdup_printf ("rect%02d", i);

      clutter_color_from_hls (&color,
                              360.0 / n_rects * i,
                              0.5,
                              0.8);
      rect = clutter_rectangle_new_with_color (&color);

      clutter_container_add_actor (CLUTTER_CONTAINER (box), rect);

      if (random_size)
        {
          width = g_random_int_range (50, 100);
          height = g_random_int_range (50, 100);
        }
      else
        {
          width = height = 50;
        }

      clutter_actor_set_size (rect, width, height);
      clutter_actor_set_name (rect, name);

      g_free (name);
    }

  g_signal_connect (stage,
                    "allocation-changed", G_CALLBACK (on_stage_resize),
                    box);

  clutter_actor_show_all (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
