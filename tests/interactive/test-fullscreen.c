#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

enum
{
  START,
  HIDE,
  SHOW,
  DONE
};

static int state = START;

static void
on_fullscreen (ClutterStage *stage)
{
  g_debug ("fullscreen set, size: %.2fx%.2f, mapped: %s",
           clutter_actor_get_width (CLUTTER_ACTOR (stage)),
           clutter_actor_get_height (CLUTTER_ACTOR (stage)),
           CLUTTER_ACTOR_IS_MAPPED (stage) ? "true" : "false");
}

static void
on_unfullscreen (ClutterStage *stage)
{
  g_debug ("fullscreen unset, size: %.2fx%.2f, mapped: %s",
           clutter_actor_get_width (CLUTTER_ACTOR (stage)),
           clutter_actor_get_height (CLUTTER_ACTOR (stage)),
           CLUTTER_ACTOR_IS_MAPPED (stage) ? "true" : "false");
}

static gboolean
toggle_fullscreen (gpointer data)
{
  ClutterActor *stage = data;
  gboolean is_fullscreen = FALSE;

  g_object_get (G_OBJECT (stage), "fullscreen-set", &is_fullscreen, NULL);

  switch (state)
    {
    case START:
      g_debug ("start: is_fullscreen := %s", is_fullscreen ? "true" : "false");
      clutter_actor_hide (stage);
      state = HIDE;
      return TRUE;

    case HIDE:
      g_debug ("hide:  is_fullscreen := %s", is_fullscreen ? "true" : "false");
      clutter_actor_show (stage);
      state = SHOW;
      return TRUE;

    case SHOW:
      g_debug ("show:  is_fullscreen := %s", is_fullscreen ? "true" : "false");
      clutter_stage_set_fullscreen (CLUTTER_STAGE (stage), FALSE);
      state = DONE;
      return TRUE;

    case DONE:
      g_debug ("done:  is_fullscreen := %s", is_fullscreen ? "true" : "false");
      clutter_actor_destroy (stage);
      break;
    }

  return FALSE;
}

G_MODULE_EXPORT int
test_fullscreen_main (int argc, char *argv[])
{
  ClutterActor *stage;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Fullscreen");
  g_signal_connect (stage,
                    "fullscreen", G_CALLBACK (on_fullscreen),
                    NULL);
  g_signal_connect (stage,
                    "unfullscreen", G_CALLBACK (on_unfullscreen),
                    NULL);
  g_signal_connect (stage,
                    "destroy", G_CALLBACK (clutter_main_quit),
                    NULL);

  clutter_stage_set_fullscreen (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_show (stage);

  g_debug ("stage size: %.2fx%.2f, mapped: %s",
           clutter_actor_get_width (stage),
           clutter_actor_get_height (stage),
           CLUTTER_ACTOR_IS_MAPPED (stage) ? "true" : "false");

  clutter_threads_add_timeout (1000, toggle_fullscreen, stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_fullscreen_describe (void)
{
  return "Check behaviour of the Stage during fullscreen.";
}
