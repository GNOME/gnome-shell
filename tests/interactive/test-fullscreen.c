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
toggle_fullscreen (gpointer dummy)
{
  ClutterActor *stage = clutter_stage_get_default ();
  gboolean is_fullscreen = FALSE;

  g_object_get (G_OBJECT (stage), "fullscreen", &is_fullscreen, NULL);

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
      clutter_stage_unfullscreen (CLUTTER_STAGE (stage));
      state = DONE;
      return TRUE;

    case DONE:
      g_debug ("done:  is_fullscreen := %s", is_fullscreen ? "true" : "false");
      clutter_main_quit ();
      break;
    }

  return FALSE;
}

G_MODULE_EXPORT int
test_fullscreen_main (int argc, char *argv[])
{
  ClutterActor *stage;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  g_signal_connect (stage,
                    "fullscreen", G_CALLBACK (on_fullscreen),
                    NULL);
  g_signal_connect (stage,
                    "unfullscreen", G_CALLBACK (on_unfullscreen),
                    NULL);

  clutter_stage_fullscreen (CLUTTER_STAGE (stage));
  clutter_actor_show (stage);

  g_debug ("stage size: %.2fx%.2f, mapped: %s",
           clutter_actor_get_width (stage),
           clutter_actor_get_height (stage),
           CLUTTER_ACTOR_IS_MAPPED (stage) ? "true" : "false");

  g_timeout_add (1000, toggle_fullscreen, NULL);

  clutter_main ();

  return EXIT_SUCCESS;
}
