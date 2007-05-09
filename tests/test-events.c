#include <clutter/clutter.h>

static void
input_cb (ClutterStage    *stage,
              ClutterEvent    *event,
              gpointer         data)
{
  switch (event->type)
    {
    case CLUTTER_KEY_PRESS:
      printf("- KEY PRESS\n");
      break;
    case CLUTTER_KEY_RELEASE:
      printf("- KEY RELEASE\n");
      break;
    case CLUTTER_MOTION:
      printf("- MOTION\n");
      break;
    case CLUTTER_BUTTON_PRESS:
      printf("- BUTTON PRESS\n");
      break;
    case CLUTTER_2BUTTON_PRESS:
      printf("- BUTTON 2 PRESS\n");
      break;
    case CLUTTER_3BUTTON_PRESS:
      printf("- BUTTON 3 PRESS\n");
      break;
    case CLUTTER_BUTTON_RELEASE:
      printf("- BUTTON RELEASE\n");
      break;
    case CLUTTER_SCROLL:
      printf("- BUTTON SCROLL\n");
      break;
    case CLUTTER_STAGE_STATE:
      printf("- STAGE STATE\n");
      break;
    case CLUTTER_DESTROY_NOTIFY:
      printf("- DESTROY NOTIFY\n");
      break;
    case CLUTTER_CLIENT_MESSAGE:
      printf("- CLIENT MESSAGE\n");
      break;
    case CLUTTER_DELETE:
      printf("- DELETE\n");
      break;
    }
}

int
main (int argc, char *argv[])
{
  ClutterActor    *stage;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  g_signal_connect (stage, "event", G_CALLBACK (input_cb), NULL);
  clutter_actor_show_all (CLUTTER_ACTOR (stage));

  clutter_main();

  return 0;
}
