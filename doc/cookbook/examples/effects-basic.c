#include <stdlib.h>
#include <clutter/clutter.h>

#include "cb-border-effect.h"
#include "cb-background-effect.h"

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };

static gboolean
toggle_highlight (ClutterActor *actor,
                  ClutterEvent *event,
                  gpointer      user_data)
{
  ClutterActorMeta *meta = CLUTTER_ACTOR_META (user_data);

  gboolean effect_enabled = clutter_actor_meta_get_enabled (meta);

  clutter_actor_meta_set_enabled (meta, !effect_enabled);

  return CLUTTER_EVENT_STOP;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *box;
  ClutterLayoutManager *layout_manager;
  ClutterActor *texture;
  ClutterEffect *background_effect;
  ClutterEffect *border_effect;
  ClutterConstraint *width_constraint;
  gchar *filename;
  guint i;
  GError *error = NULL;

  if (argc < 2)
    {
      g_print ("Usage: %s <image files>\n", argv[0]);
      return EXIT_FAILURE;
    }

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_size (stage, 600, 400);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  layout_manager = clutter_flow_layout_new (CLUTTER_FLOW_HORIZONTAL);
  clutter_flow_layout_set_column_spacing (CLUTTER_FLOW_LAYOUT (layout_manager),
                                          10);
  clutter_flow_layout_set_row_spacing (CLUTTER_FLOW_LAYOUT (layout_manager),
                                       10);

  box = clutter_actor_new ();
  clutter_actor_set_layout_manager (box, layout_manager);
  width_constraint = clutter_bind_constraint_new (stage,
                                                  CLUTTER_BIND_WIDTH,
                                                  0.0);
  clutter_actor_add_constraint (box, width_constraint);

  /* loop through the files specified on the command line, adding
   * each one into the box
   */
  for (i = 1; i < argc; i++)
    {
      filename = argv[i];

      texture = clutter_texture_new ();
      clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture), TRUE);
      clutter_actor_set_width (texture, 150);
      clutter_actor_set_reactive (texture, TRUE);

      clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                     filename,
                                     &error);

      if (error != NULL)
        g_warning ("Error loading file %s:\n%s",
                   filename,
                   error->message);

      /* create a grey background effect */
      background_effect = cb_background_effect_new ();

      /* apply the effect to the actor */
      clutter_actor_add_effect (texture, background_effect);

      /* create a 5 pixel red border effect */
      border_effect = cb_border_effect_new (5.0, &red_color);

      /* apply the effect to the actor, but disabled */
      clutter_actor_add_effect (texture, border_effect);
      clutter_actor_meta_set_enabled (CLUTTER_ACTOR_META (border_effect),
                                      FALSE);

      /* on mouse click, toggle the "enabled" property of the border effect */
      g_signal_connect (texture,
                        "button-press-event",
                        G_CALLBACK (toggle_highlight),
                        border_effect);

      clutter_container_add_actor (CLUTTER_CONTAINER (box), texture);
    }

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
