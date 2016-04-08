#include <stdio.h>
#include <stdlib.h>
#include <clutter/clutter.h>

/* our thread-specific data */
typedef struct
{
  ClutterActor *stage;
  ClutterActor *label;
  ClutterActor *progress;
  ClutterActor *rect;

  ClutterTransition *flip;
  ClutterTransition *bounce;
} TestThreadData;

static TestThreadData *
test_thread_data_new (void)
{
  TestThreadData *data;

  data = g_new0 (TestThreadData, 1);

  return data;
}

static void
test_thread_data_free (gpointer _data)
{
  TestThreadData *data = _data;

  if (data == NULL)
    return;

  g_print ("Removing thread data [%p]\n", _data);

  g_clear_object (&data->progress);
  g_clear_object (&data->label);
  g_clear_object (&data->stage);
  g_clear_object (&data->rect);
  g_clear_object (&data->flip);
  g_clear_object (&data->bounce);

  g_free (data);
}

static gboolean
test_thread_done_idle (gpointer user_data)
{
  TestThreadData *data = user_data;

  g_print ("Last update [%p]\n", data);

  clutter_text_set_text (CLUTTER_TEXT (data->label), "Completed");

  clutter_actor_remove_transition (data->rect, "bounce");
  clutter_actor_remove_transition (data->rect, "flip");

  return G_SOURCE_REMOVE;
}

static void
test_thread_data_done (gpointer _data)
{
  if (_data == NULL)
    return;

  g_print ("Thread completed\n");

  /* since the TestThreadData structure references Clutter data structures
   * we need to free it from within the same thread that called clutter_main()
   * which means using an idle handler in the main loop.
   *
   * clutter_threads_add_idle() is guaranteed to run the callback passed to
   * to it under the Big Clutter Lock.
   */
  clutter_threads_add_idle_full (G_PRIORITY_DEFAULT,
                                 test_thread_done_idle,
                                 _data,
                                 test_thread_data_free);
}

/* thread local storage */
static GPrivate test_thread_data = G_PRIVATE_INIT (test_thread_data_done);

typedef struct {
  gint count;
  TestThreadData *thread_data;
} TestUpdate;

static gboolean
update_label_idle (gpointer data)
{
  TestUpdate *update = data;
  guint width;
  gchar *text;

  if (update->thread_data->label == NULL)
    return G_SOURCE_REMOVE;

  text = g_strdup_printf ("Count to %d", update->count);
  clutter_text_set_text (CLUTTER_TEXT (update->thread_data->label), text);

  clutter_actor_set_width (update->thread_data->label, -1);

  if (update->count == 0)
    width = 0;
  else if (update->count == 100)
    width = 350;
  else
    width = (guint) (update->count / 100.0 * 350.0);

  clutter_actor_save_easing_state (update->thread_data->progress);
  clutter_actor_set_width (update->thread_data->progress, width);
  clutter_actor_restore_easing_state (update->thread_data->progress);

  g_free (text);
  g_free (update);

  return G_SOURCE_REMOVE;
}

static void
do_something_very_slow (void)
{
  TestThreadData *data;
  gint i;

  data = g_private_get (&test_thread_data);

  for (i = 0; i <= 100; i++)
    {
      gint msecs;

      msecs = 1 + (int) (100.0 * rand () / ((RAND_MAX + 1.0) / 3));

      /* sleep for a while, to emulate some work being done */
      g_usleep (msecs * 1000);

      if ((i % 10) == 0)
        {
          TestUpdate *update;

          /* update the UI from within the main loop, making sure that the
           * Big Clutter Lock is held; only one thread at a time can call
           * Clutter API, and it's mandatory to do this from the same thread
           * that called clutter_init()/clutter_main().
           */
          update = g_new (TestUpdate, 1);
          update->count = i;
          update->thread_data = data;

          clutter_threads_add_idle_full (G_PRIORITY_HIGH,
                                         update_label_idle,
                                         update, NULL);
        }
    }
}

static gpointer
test_thread_func (gpointer user_data)
{
  TestThreadData *data = user_data;

  g_private_set (&test_thread_data, data);

  /* this function will block */
  do_something_very_slow ();

  return NULL;
}

static ClutterActor *count_label   = NULL;
static ClutterActor *help_label    = NULL;
static ClutterActor *progress_rect = NULL;
static ClutterActor *rect          = NULL;
static ClutterTransition *flip     = NULL;
static ClutterTransition *bounce   = NULL;

static gboolean
on_key_press_event (ClutterStage *stage,
                    ClutterEvent *event,
                    gpointer      user_data)
{
  TestThreadData *data;

  switch (clutter_event_get_key_symbol (event))
    {
    case CLUTTER_KEY_s:
      clutter_text_set_text (CLUTTER_TEXT (help_label), "Press 'q' to quit");

      /* start the animations */
      clutter_actor_add_transition (rect, "flip", flip);
      clutter_actor_add_transition (rect, "bounce", bounce);

      /* the data structure holding all our objects */
      data = test_thread_data_new ();
      data->stage = g_object_ref (stage);
      data->label = g_object_ref (count_label);
      data->progress = g_object_ref (progress_rect);
      data->rect = g_object_ref (rect);
      data->flip = g_object_ref (flip);
      data->bounce = g_object_ref (bounce);

      /* start the thread that updates the counter and the progress bar */
      g_thread_new ("counter", test_thread_func, data);

      return CLUTTER_EVENT_STOP;

    case CLUTTER_KEY_q:
      clutter_main_quit ();

      return CLUTTER_EVENT_STOP;

    default:
      break;
    }

  return CLUTTER_EVENT_PROPAGATE;
}

int
main (int argc, char *argv[])
{
  ClutterTransition *transition;
  ClutterActor *stage;
  ClutterPoint start = CLUTTER_POINT_INIT (75.f, 150.f);
  ClutterPoint end = CLUTTER_POINT_INIT (400.f, 150.f);

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Threading");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Aluminium3);
  clutter_actor_set_size (stage, 600, 300);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  
  count_label = clutter_text_new_with_text ("Mono 12", "Counter");
  clutter_actor_set_position (count_label, 350, 50);
  clutter_actor_add_child (stage, count_label);

  help_label = clutter_text_new_with_text ("Mono 12", "Press 's' to start");
  clutter_actor_set_position (help_label, 50, 50);
  clutter_actor_add_child (stage, help_label);

  /* a progress bar */
  progress_rect = clutter_actor_new ();
  clutter_actor_set_background_color (progress_rect, CLUTTER_COLOR_DarkChameleon);
  clutter_actor_set_position (progress_rect, 50, 225);
  clutter_actor_set_size (progress_rect, 350, 50);
  clutter_actor_add_child (stage, progress_rect);

  /* an actor we bounce around */
  rect = clutter_actor_new ();
  clutter_actor_set_background_color (rect, CLUTTER_COLOR_LightScarletRed);
  clutter_actor_set_position (rect, 75, 150);
  clutter_actor_set_size (rect, 50, 50);
  clutter_actor_set_pivot_point (rect, .5f, .5f);
  clutter_actor_set_opacity (rect, 224);
  clutter_actor_add_child (stage, rect);

  /* two transitions we use to bounce rect around */
  transition = clutter_property_transition_new ("rotation-angle-z");
  clutter_transition_set_from (transition, G_TYPE_DOUBLE, 0.0);
  clutter_transition_set_to (transition, G_TYPE_DOUBLE, 360.0);
  clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (transition), -1);
  clutter_timeline_set_auto_reverse (CLUTTER_TIMELINE (transition), TRUE);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition), 3000);
  flip = transition;

  transition = clutter_property_transition_new ("position");
  clutter_transition_set_from (transition, CLUTTER_TYPE_POINT, &start);
  clutter_transition_set_to (transition, CLUTTER_TYPE_POINT, &end);
  clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (transition), -1);
  clutter_timeline_set_auto_reverse (CLUTTER_TIMELINE (transition), TRUE);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition), 3000);
  bounce = transition;

  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit),
                    NULL);
  g_signal_connect (stage,
                    "key-press-event", G_CALLBACK (on_key_press_event),
                    NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
