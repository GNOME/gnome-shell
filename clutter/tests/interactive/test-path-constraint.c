#include <stdlib.h>
#include <gmodule.h>
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

static gchar *
node_to_string (const ClutterPathNode *node)
{
  GString *buffer = g_string_sized_new (256);
  gsize len = 0, i;

  switch (node->type)
    {
    case CLUTTER_PATH_MOVE_TO:
      g_string_append (buffer, "move-to ");
      len = 1;
      break;

    case CLUTTER_PATH_LINE_TO:
      g_string_append (buffer, "line-to ");
      len = 1;
      break;

    case CLUTTER_PATH_CURVE_TO:
      g_string_append (buffer, "curve-to ");
      len = 3;
      break;

    case CLUTTER_PATH_CLOSE:
      g_string_append (buffer, "close");
      len = 0;
      break;

    default:
      break;
    }

  for (i = 0; i < len; i++)
    {
      if (i == 0)
        g_string_append (buffer, "[ ");

      g_string_append_printf (buffer, "[ %d, %d ]",
                              node->points[i].x,
                              node->points[i].y);

      if (i == len - 1)
        g_string_append (buffer, " ]");
    }

  return g_string_free (buffer, FALSE);
}

static void
on_node_reached (ClutterPathConstraint *constraint,
                 ClutterActor          *actor,
                 guint                  index_)
{
  ClutterPath *path = clutter_path_constraint_get_path (constraint);
  ClutterPathNode node;
  gchar *str;

  clutter_path_get_node (path, index_, &node);

  str = node_to_string (&node);
  g_print ("Node %d reached: %s\n", index_, str);
  g_free (str);
}

G_MODULE_EXPORT int
test_path_constraint_main (int   argc,
                           char *argv[])
{
  ClutterActor *stage, *rect;
  ClutterPath *path;
  ClutterColor rect_color = { 0xcc, 0x00, 0x00, 0xff };

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

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
  g_signal_connect (clutter_actor_get_constraint (rect, "path"),
                    "node-reached",
                    G_CALLBACK (on_node_reached),
                    NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
