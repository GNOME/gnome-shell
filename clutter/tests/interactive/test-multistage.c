#include <gmodule.h>
#include <clutter/clutter.h>

static GList *stages = NULL;
static gint n_stages = 1;

static gboolean
tex_button_cb (ClutterActor    *actor,
               ClutterEvent    *event,
               gpointer         data)
{
  clutter_actor_hide (actor);

  return TRUE;
}

static void
on_destroy (ClutterActor *actor)
{
  stages = g_list_remove (stages, actor);
}

static gboolean
on_button_press (ClutterActor *actor,
                 ClutterEvent *event,
                 gpointer      data)
{
  ClutterActor *new_stage;
  ClutterActor *label, *tex;
  gint width, height;
  gchar *stage_label, *stage_name;
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;
  ClutterBehaviour *r_behave;

  new_stage = clutter_stage_new ();
  if (new_stage == NULL)
    return FALSE;

  stage_name = g_strdup_printf ("Stage [%d]", ++n_stages);

  clutter_stage_set_title (CLUTTER_STAGE (new_stage), stage_name);
  clutter_actor_set_background_color (new_stage,
                                      CLUTTER_COLOR_DarkScarletRed);
  clutter_actor_set_size (new_stage, 320, 240);
  clutter_actor_set_name (new_stage, stage_name);

  g_signal_connect (new_stage, "destroy", G_CALLBACK (on_destroy), NULL);

  tex = clutter_texture_new_from_file (TESTS_DATADIR
                                       G_DIR_SEPARATOR_S
                                       "redhand.png",
                                       NULL);

  if (!tex)
    g_error ("pixbuf load failed");

  clutter_actor_set_reactive (tex, TRUE);
  g_signal_connect (tex, "button-press-event", 
                    G_CALLBACK (tex_button_cb), NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (new_stage), tex);

  stage_label = g_strconcat ("<b>", stage_name, "</b>", NULL);
  label = clutter_text_new_with_text ("Mono 12", stage_label);

  clutter_text_set_color (CLUTTER_TEXT (label), CLUTTER_COLOR_White);
  clutter_text_set_use_markup (CLUTTER_TEXT (label), TRUE);
  width = (clutter_actor_get_width (new_stage) 
           - clutter_actor_get_width (label)) / 2;
  height = (clutter_actor_get_height (new_stage) 
            - clutter_actor_get_height (label)) / 2;
  clutter_actor_set_position (label, width, height);
  clutter_container_add_actor (CLUTTER_CONTAINER (new_stage), label);
  clutter_actor_show (label);
  g_free (stage_label);

  /*
  g_signal_connect (new_stage, "button-press-event",
                    G_CALLBACK (clutter_actor_destroy),
                    NULL);
  */

  timeline = clutter_timeline_new (2000);
  clutter_timeline_set_repeat_count (timeline, -1);

  alpha = clutter_alpha_new_full (timeline, CLUTTER_LINEAR);
  r_behave = clutter_behaviour_rotate_new (alpha,
					   CLUTTER_Y_AXIS,
					   CLUTTER_ROTATE_CW,
					   0.0, 360.0); 

  clutter_behaviour_rotate_set_center (CLUTTER_BEHAVIOUR_ROTATE (r_behave),
                                       clutter_actor_get_width (label)/2, 
                                       0, 
                                       0);
  
  clutter_behaviour_apply (r_behave, label);
  clutter_timeline_start (timeline);

  clutter_actor_show_all (new_stage);

  stages = g_list_prepend (stages, new_stage);

  g_free (stage_name);

  return TRUE;
}

G_MODULE_EXPORT int
test_multistage_main (int argc, char *argv[])
{
  ClutterActor *stage_default;
  ClutterActor *label;
  gint width, height;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;
  
  stage_default = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage_default), "Default Stage");
  clutter_actor_set_name (stage_default, "Default Stage");
  g_signal_connect (stage_default, "destroy",
                    G_CALLBACK (clutter_main_quit),
                    NULL);
  g_signal_connect (stage_default, "button-press-event",
                    G_CALLBACK (on_button_press),
                    NULL);

  label = clutter_text_new_with_text ("Mono 16", "Default stage");
  width = (clutter_actor_get_width (stage_default) 
           - clutter_actor_get_width (label))
             / 2;
  height = (clutter_actor_get_height (stage_default) 
            - clutter_actor_get_height (label))
            / 2;
  clutter_actor_set_position (label, width, height);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage_default), label);
  clutter_actor_show (label);

  clutter_actor_show (stage_default);

  clutter_main ();

  g_list_foreach (stages, (GFunc) clutter_actor_destroy, NULL);
  g_list_free (stages);

  return 0;
}
