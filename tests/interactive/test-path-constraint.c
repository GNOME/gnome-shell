#include <stdlib.h>
#include <clutter/clutter.h>

#define PATH_DESCRIPTION        \
        "M 0, 0 "       \
        "L 0, 300 "     \
        "L 300, 300 "   \
        "L 300, 0 "     \
        "L 0, 0"

static gboolean toggled = FALSE;

static gboolean
on_button_press (ClutterActor *actor,
                 const ClutterEvent *event,
                 gpointer            dummy G_GNUC_UNUSED)
{
  if (!toggled)
    clutter_actor_animate (actor, CLUTTER_EASE_OUT_CUBIC, 500,
                           "@constraints.path.offset", 1.0,
                            NULL);
  else
    clutter_actor_animate (actor, CLUTTER_EASE_OUT_CUBIC, 500,
                           "@constraints.path.offset", 0.0,
                           NULL);

  toggled = !toggled;

  return TRUE;
}

int
test_path_constraint_main (int   argc,
                           char *argv[])
{
  ClutterActor *stage, *rect;
  ClutterPath *path;
  ClutterColor rect_color = { 0xcc, 0x00, 0x00, 0xff };

  clutter_init (&argc, &argv);

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Path Constraint");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  path = clutter_path_new ();
  clutter_path_set_description (path, PATH_DESCRIPTION);

  rect = clutter_rectangle_new ();
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &rect_color);
  clutter_actor_set_size (rect, 128, 128);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_actor_add_constraint_with_name (rect, "path", clutter_path_constraint_new (path, 0.0));
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  g_signal_connect (rect, "button-press-event", G_CALLBACK (on_button_press), NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
