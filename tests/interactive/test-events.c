#include <gmodule.h>
#include <clutter/clutter.h>
#include <string.h>

gboolean IsFullScreen = FALSE, IsMotion = TRUE;

static void
stage_state_cb (ClutterStage    *stage,
		gpointer         data)
{
  gchar *detail = (gchar*)data;

  printf("[stage signal] %s\n", detail);
}

static gboolean
blue_button_cb (ClutterActor    *actor,
		ClutterEvent    *event,
		gpointer         data)
{
  ClutterActor *stage;

  stage = clutter_stage_get_default ();

  if (IsFullScreen)
    IsFullScreen = FALSE;
  else
    IsFullScreen = TRUE;

  g_object_set (stage, "fullscreen", IsFullScreen, NULL);

  g_print ("*** Fullscreen %s ***\n",
           IsFullScreen ? "enabled" : "disabled");

  return FALSE;
}

static gboolean
red_button_cb (ClutterActor    *actor,
		ClutterEvent    *event,
		gpointer         data)
{

  if (IsMotion)
    IsMotion = FALSE;
  else
    IsMotion = TRUE;

  clutter_set_motion_events_enabled (IsMotion);

  g_print ("*** Per actor motion events %s ***\n",
           IsMotion ? "enabled" : "disabled");

  return FALSE;
}

static gboolean
capture_cb (ClutterActor *actor,
	    ClutterEvent *event,
	    gpointer      data)
{
  g_print ("* captured event for type '%s' *\n",
           G_OBJECT_TYPE_NAME (actor));

  return FALSE;
}

static void
key_focus_in_cb (ClutterActor    *actor,
		 gpointer         data)
{
  ClutterActor *focus_box = CLUTTER_ACTOR(data);  

  if (actor == clutter_stage_get_default ())
    clutter_actor_hide (focus_box);
  else
    {
      clutter_actor_set_position (focus_box,
				  clutter_actor_get_x (actor) - 5,
				  clutter_actor_get_y (actor) - 5);

      clutter_actor_set_size (focus_box,
			      clutter_actor_get_width (actor) + 10,
			      clutter_actor_get_height (actor) + 10);
      clutter_actor_show (focus_box);
    }
}

static void
fill_keybuf (char *keybuf, ClutterKeyEvent *event)
{
  char utf8[6];
  int len;

  /* printable character, if any (ß, ∑) */
  len = g_unichar_to_utf8 (event->unicode_value, utf8);
  utf8[len] = '\0';
  sprintf(keybuf, "'%s' ", utf8);

  /* key combination (<Mod1>s, <Shift><Mod1>S, <Ctrl><Mod1>Delete) */
  len = g_unichar_to_utf8 (clutter_keysym_to_unicode (event->keyval),
                           utf8);
  utf8[len] = '\0';

  if (event->modifier_state & CLUTTER_SHIFT_MASK)
    strcat (keybuf, "<Shift>");
  if (event->modifier_state & CLUTTER_LOCK_MASK)
    strcat (keybuf, "<Lock>");
  if (event->modifier_state & CLUTTER_CONTROL_MASK)
    strcat (keybuf, "<Control>");
  if (event->modifier_state & CLUTTER_MOD1_MASK)
    strcat (keybuf, "<Mod1>");
  if (event->modifier_state & CLUTTER_MOD2_MASK)
    strcat (keybuf, "<Mod2>");
  if (event->modifier_state & CLUTTER_MOD3_MASK)
    strcat (keybuf, "<Mod3>");
  if (event->modifier_state & CLUTTER_MOD4_MASK)
    strcat (keybuf, "<Mod4>");
  if (event->modifier_state & CLUTTER_MOD5_MASK)
    strcat (keybuf, "<Mod5>");
  strcat (keybuf, utf8);
}

static gboolean
input_cb (ClutterActor    *actor,
	  ClutterEvent    *event,
	  gpointer         data)
{
  ClutterStage *stage = CLUTTER_STAGE (clutter_stage_get_default ());
  gchar keybuf[128], *source = (gchar*)data;

  switch (event->type)
    {
    case CLUTTER_KEY_PRESS:
      fill_keybuf (keybuf, &event->key);
      printf ("[%s] KEY PRESS %s", source, keybuf);
      break;
    case CLUTTER_KEY_RELEASE:
      fill_keybuf (keybuf, &event->key);
      printf ("[%s] KEY RELEASE %s", source, keybuf);
      break;
    case CLUTTER_MOTION:
      g_print ("[%s] MOTION", source);
      break;
    case CLUTTER_ENTER:
      g_print ("[%s] ENTER", source);
      break;
    case CLUTTER_LEAVE:
      g_print ("[%s] LEAVE", source);
      break;
    case CLUTTER_BUTTON_PRESS:
      g_print ("[%s] BUTTON PRESS (click count:%i)", 
	       source, event->button.click_count);
      break;
    case CLUTTER_BUTTON_RELEASE:
      g_print ("[%s] BUTTON RELEASE (click count:%i)", 
	       source, event->button.click_count);

      if (clutter_event_get_source (event) == CLUTTER_ACTOR (stage))
        clutter_stage_set_key_focus (stage, NULL);
      else if (clutter_event_get_source (event) == actor
	       && clutter_actor_get_parent (actor) == CLUTTER_ACTOR (stage))
	clutter_stage_set_key_focus (stage, actor);
      break;
    case CLUTTER_SCROLL:
      g_print ("[%s] BUTTON SCROLL (click count:%i)",
	       source, event->button.click_count);
      break;
    case CLUTTER_STAGE_STATE:
      g_print ("[%s] STAGE STATE", source);
      break;
    case CLUTTER_DESTROY_NOTIFY:
      g_print ("[%s] DESTROY NOTIFY", source);
      break;
    case CLUTTER_CLIENT_MESSAGE:
      g_print ("[%s] CLIENT MESSAGE", source);
      break;
    case CLUTTER_DELETE:
      g_print ("[%s] DELETE", source);
      break;
    case CLUTTER_NOTHING:
      return FALSE;
    }

  if (clutter_event_get_source (event) == actor)
    g_print (" *source*");
  
  g_print ("\n");

  return FALSE;
}

G_MODULE_EXPORT int
test_events_main (int argc, char *argv[])
{
  ClutterActor    *stage, *actor, *focus_box, *group;
  ClutterColor    rcol = { 0xff, 0, 0, 0xff}, 
                  bcol = { 0, 0, 0xff, 0xff },
		  gcol = { 0, 0xff, 0, 0xff },
		  ycol = { 0xff, 0xff, 0, 0xff },
		  ncol = { 0, 0, 0, 0xff };

  clutter_init (&argc, &argv);



  stage = clutter_stage_get_default ();
  g_signal_connect (stage, "event", G_CALLBACK (input_cb), "stage");

  g_signal_connect (stage, "fullscreen", 
		    G_CALLBACK (stage_state_cb), "fullscreen");
  g_signal_connect (stage, "unfullscreen", 
		    G_CALLBACK (stage_state_cb), "unfullscreen");
  g_signal_connect (stage, "activate", 
		    G_CALLBACK (stage_state_cb), "activate");
  g_signal_connect (stage, "deactivate", 
		    G_CALLBACK (stage_state_cb), "deactivate");

  g_signal_connect (stage, "captured-event", G_CALLBACK (capture_cb), NULL);

  focus_box = clutter_rectangle_new_with_color (&ncol);
  clutter_container_add (CLUTTER_CONTAINER(stage), focus_box, NULL);

  actor = clutter_rectangle_new_with_color (&rcol);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_position (actor, 100, 100);

  clutter_actor_set_reactive (actor, TRUE);

  clutter_container_add (CLUTTER_CONTAINER (stage), actor, NULL);

  g_signal_connect (actor, "event", G_CALLBACK (input_cb), "red box");
  g_signal_connect (actor, "focus-in", G_CALLBACK (key_focus_in_cb), 
		    focus_box);

  /* Toggle motion - enter/leave capture */
  g_signal_connect (actor, "button-press-event",
                    G_CALLBACK (red_button_cb), NULL);

  clutter_stage_set_key_focus (CLUTTER_STAGE (stage), actor);

  actor = clutter_rectangle_new_with_color (&gcol);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_position (actor, 250, 100);

  clutter_actor_set_reactive (actor, TRUE);

  clutter_container_add (CLUTTER_CONTAINER (stage), actor, NULL);

  g_signal_connect (actor, "event", G_CALLBACK (input_cb), "green box");
  g_signal_connect (actor, "focus-in", G_CALLBACK (key_focus_in_cb), 
		    focus_box);

  g_signal_connect (actor, "captured-event", G_CALLBACK (capture_cb), NULL);

  actor = clutter_rectangle_new_with_color (&bcol);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_position (actor, 400, 100);

  clutter_actor_set_reactive (actor, TRUE);

  clutter_container_add (CLUTTER_CONTAINER(stage), actor, NULL);

  g_signal_connect (actor, "event", G_CALLBACK (input_cb), "blue box");
  g_signal_connect (actor, "focus-in", G_CALLBACK (key_focus_in_cb), 
		    focus_box);
  /* Fullscreen */
  g_signal_connect (actor, "button-press-event",
                    G_CALLBACK (blue_button_cb), NULL);

  /* non reactive */
  actor = clutter_rectangle_new_with_color (&ncol);
  clutter_actor_set_size (actor, 400, 50);
  clutter_actor_set_position (actor, 100, 250);

  clutter_container_add (CLUTTER_CONTAINER(stage), actor, NULL);

  g_signal_connect (actor, "event", G_CALLBACK (input_cb), "blue box");
  g_signal_connect (actor, "focus-in", G_CALLBACK (key_focus_in_cb), 
		    focus_box);

  g_signal_connect (stage, "focus-in", G_CALLBACK (key_focus_in_cb), 
		    focus_box);

  /* non reactive group, with reactive child */
  actor = clutter_rectangle_new_with_color (&ycol);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_reactive (actor, TRUE);

  g_signal_connect (actor, "event", G_CALLBACK (input_cb), "yellow box");

  /* note group not reactive */
  group = clutter_group_new ();
  clutter_container_add (CLUTTER_CONTAINER (group), actor, NULL);
  clutter_container_add (CLUTTER_CONTAINER (stage), group, NULL);
  clutter_actor_set_position (group, 100, 350);
  clutter_actor_show_all (group);

  clutter_actor_show_all (CLUTTER_ACTOR (stage));

  clutter_main();

  return 0;
}
