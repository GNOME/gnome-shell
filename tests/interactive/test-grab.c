#include <gmodule.h>
#include <clutter/clutter.h>

static void
stage_state_cb (ClutterStage    *stage,
		gpointer         data)
{
  gchar *detail = (gchar*)data;

  printf("[stage signal] %s\n", detail);
}

static gboolean
debug_event_cb (ClutterActor *actor,
                ClutterEvent *event,
                gpointer      data)
{
  gchar keybuf[9], *source = (gchar*)data;
  int   len = 0;

  switch (event->type)
    {
    case CLUTTER_KEY_PRESS:
      len = g_unichar_to_utf8 (clutter_keysym_to_unicode (event->key.keyval),
			       keybuf);
      keybuf[len] = '\0';
      printf ("[%s] KEY PRESS '%s'", source, keybuf);
      break;
    case CLUTTER_KEY_RELEASE:
      len = g_unichar_to_utf8 (clutter_keysym_to_unicode (event->key.keyval),
			       keybuf);
      keybuf[len] = '\0';
      printf ("[%s] KEY RELEASE '%s'", source, keybuf);
      break;
    case CLUTTER_MOTION:
      printf("[%s] MOTION", source);
      break;
    case CLUTTER_ENTER:
      printf("[%s] ENTER", source);
      break;
    case CLUTTER_LEAVE:
      printf("[%s] LEAVE", source);
      break;
    case CLUTTER_BUTTON_PRESS:
      printf("[%s] BUTTON PRESS (click count:%i)", 
	     source, event->button.click_count);
      break;
    case CLUTTER_BUTTON_RELEASE:
      printf("[%s] BUTTON RELEASE", source);
      break;
    case CLUTTER_SCROLL:
      printf("[%s] BUTTON SCROLL", source);
      break;
    case CLUTTER_STAGE_STATE:
      printf("[%s] STAGE STATE", source);
      break;
    case CLUTTER_DESTROY_NOTIFY:
      printf("[%s] DESTROY NOTIFY", source);
      break;
    case CLUTTER_CLIENT_MESSAGE:
      printf("[%s] CLIENT MESSAGE\n", source);
      break;
    case CLUTTER_DELETE:
      printf("[%s] DELETE", source);
      break;
    case CLUTTER_NOTHING:
      return FALSE;
    }

  if (clutter_event_get_source (event) == actor)
    printf(" *source*");
  
  printf("\n");

  return FALSE;
}

static gboolean
grab_pointer_cb (ClutterActor    *actor,
                 ClutterEvent    *event,
                 gpointer         data)
{
  clutter_grab_pointer (actor);
  return FALSE;
}

static gboolean
red_release_cb (ClutterActor    *actor,
                ClutterEvent    *event,
                gpointer         data)
{
  clutter_ungrab_pointer ();
  return FALSE;
}

static gboolean
blue_release_cb (ClutterActor    *actor,
                 ClutterEvent    *event,
                 gpointer         data)
{
  clutter_actor_destroy (actor);
  return FALSE;
}

static gboolean
green_press_cb (ClutterActor    *actor,
                ClutterEvent    *event,
                gpointer         data)
{
  clutter_set_motion_events_enabled (!clutter_get_motion_events_enabled ());

  g_print ("per actor motion events are now %s\n",
           clutter_get_motion_events_enabled () ? "enabled" : "disabled");

  return FALSE;
}

static gboolean
toggle_grab_pointer_cb (ClutterActor    *actor,
                        ClutterEvent    *event,
                        gpointer         data)
{
  /* we only deal with the event if the source is ourself */
  if (event->button.source == actor)
    {
      if (clutter_get_pointer_grab () != NULL)
        clutter_ungrab_pointer ();
      else
        clutter_grab_pointer (actor);
    }
  return FALSE;
}

static gboolean
cyan_press_cb (ClutterActor    *actor,
               ClutterEvent    *event,
               gpointer         data)
{
  if (clutter_get_keyboard_grab () != NULL)
    clutter_ungrab_keyboard ();
  else
    clutter_grab_keyboard (actor);
  return FALSE;
}



G_MODULE_EXPORT int
test_grab_main (int argc, char *argv[])
{
  ClutterActor   *stage, *actor;
  ClutterColor    rcol = { 0xff, 0, 0, 0xff}, 
                  bcol = { 0, 0, 0xff, 0xff },
		  gcol = { 0, 0xff, 0, 0xff },
		  ccol = { 0, 0xff, 0xff, 0xff },
		  ycol = { 0xff, 0xff, 0, 0xff };

  clutter_init (&argc, &argv);

  g_print ("Red box:    aquire grab on press, releases it on next button release\n");
  g_print ("Blue box:   aquire grab on press, destroys the blue box actor on release\n");
  g_print ("Yellow box: aquire grab on press, releases grab on next press on yellow box\n");
  g_print ("Green box:  toggle per actor motion events.\n\n");
  g_print ("Cyan  box:  toggle grab (from cyan box) for keyboard events.\n\n");

  stage = clutter_stage_get_default ();
  g_signal_connect (stage, "event", G_CALLBACK (debug_event_cb), "stage");

  g_signal_connect (stage, "fullscreen", 
		    G_CALLBACK (stage_state_cb), "fullscreen");
  g_signal_connect (stage, "unfullscreen", 
		    G_CALLBACK (stage_state_cb), "unfullscreen");
  g_signal_connect (stage, "activate", 
		    G_CALLBACK (stage_state_cb), "activate");
  g_signal_connect (stage, "deactivate", 
		    G_CALLBACK (stage_state_cb), "deactivate");

  actor = clutter_rectangle_new_with_color (&rcol);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_position (actor, 100, 100);
  clutter_actor_set_reactive (actor, TRUE);
  clutter_container_add (CLUTTER_CONTAINER (stage), actor, NULL);
  g_signal_connect (actor, "event", G_CALLBACK (debug_event_cb), "red box");
  g_signal_connect (actor, "button-press-event",
                    G_CALLBACK (grab_pointer_cb), NULL);
  g_signal_connect (actor, "button-release-event",
                    G_CALLBACK (red_release_cb), NULL);

  actor = clutter_rectangle_new_with_color (&ycol);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_position (actor, 100, 300);
  clutter_actor_set_reactive (actor, TRUE);
  clutter_container_add (CLUTTER_CONTAINER (stage), actor, NULL);
  g_signal_connect (actor, "event", G_CALLBACK (debug_event_cb), "yellow box");
  g_signal_connect (actor, "button-press-event",
                    G_CALLBACK (toggle_grab_pointer_cb), NULL);

  actor = clutter_rectangle_new_with_color (&bcol);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_position (actor, 300, 100);
  clutter_actor_set_reactive (actor, TRUE);
  clutter_container_add (CLUTTER_CONTAINER (stage), actor, NULL);
  g_signal_connect (actor, "event",
                    G_CALLBACK (debug_event_cb), "blue box");
  g_signal_connect (actor, "button-press-event",
                    G_CALLBACK (grab_pointer_cb), NULL);
  g_signal_connect (actor, "button-release-event",
                    G_CALLBACK (blue_release_cb), NULL);

  actor = clutter_rectangle_new_with_color (&gcol);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_position (actor, 300, 300);
  clutter_actor_set_reactive (actor, TRUE);
  clutter_container_add (CLUTTER_CONTAINER (stage), actor, NULL);
  g_signal_connect (actor, "event",
                    G_CALLBACK (debug_event_cb), "green box");
  g_signal_connect (actor, "button-press-event",
                    G_CALLBACK (green_press_cb), NULL);


  actor = clutter_rectangle_new_with_color (&ccol);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_position (actor, 500, 100);
  clutter_actor_set_reactive (actor, TRUE);
  clutter_container_add (CLUTTER_CONTAINER (stage), actor, NULL);
  g_signal_connect (actor, "event",
                    G_CALLBACK (debug_event_cb), "cyan box");
  g_signal_connect (actor, "button-press-event",
                    G_CALLBACK (cyan_press_cb), NULL);

  clutter_actor_show_all (CLUTTER_ACTOR (stage));

  clutter_main();

  return 0;
}
