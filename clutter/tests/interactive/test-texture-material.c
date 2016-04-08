#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

G_MODULE_EXPORT int
test_texture_material_main (int argc, char *argv[])
{
  ClutterActor *stage, *box;
  ClutterLayoutManager *manager;
  int i;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Texture Material");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  manager = clutter_flow_layout_new (CLUTTER_FLOW_HORIZONTAL);
  box = clutter_box_new (manager);
  clutter_actor_add_constraint (box, clutter_bind_constraint_new (stage, CLUTTER_BIND_WIDTH, -25.0));
  clutter_actor_add_constraint (box, clutter_bind_constraint_new (stage, CLUTTER_BIND_HEIGHT, -25.0));
  clutter_actor_set_position (box, 25.0, 25.0);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);

  for (i = 0; i < 48; i++)
    {
      ClutterActor *texture = clutter_texture_new ();

      clutter_texture_set_load_data_async (CLUTTER_TEXTURE (texture), TRUE);
      clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture), TRUE);
      clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                     TESTS_DATADIR "/redhand.png",
                                     NULL);
      clutter_actor_set_width (texture, 96);

      clutter_container_add_actor (CLUTTER_CONTAINER (box), texture);
    }

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
