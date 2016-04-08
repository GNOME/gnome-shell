#include <stdlib.h>
#include <glib.h>
#include <clutter/clutter.h>

static GTimer *testtimer = NULL;
static gint testframes = 0;
static float testmaxtime = 1.0;

/* initialize environment to be suitable for fps testing */
void clutter_perf_fps_init (void)
{
  /* Force not syncing to vblank, we want free-running maximum FPS */
  g_setenv ("vblank_mode", "0", FALSE);
  g_setenv ("CLUTTER_VBLANK", "none", FALSE);

  /* also overrride internal default FPS */
  g_setenv ("CLUTTER_DEFAULT_FPS", "1000", FALSE);

  if (g_getenv ("CLUTTER_PERFORMANCE_TEST_DURATION"))
    testmaxtime = atof(g_getenv("CLUTTER_PERFORMANCE_TEST_DURATION"));
  else
    testmaxtime = 10.0;

  g_random_set_seed (12345678);
}

static void perf_stage_paint_cb (ClutterStage *stage, gpointer *data);
static gboolean perf_fake_mouse_cb (gpointer stage);

void clutter_perf_fps_start (ClutterStage *stage)
{
  g_signal_connect (stage, "paint", G_CALLBACK (perf_stage_paint_cb), NULL);
}

void clutter_perf_fake_mouse (ClutterStage *stage)
{
  clutter_threads_add_timeout (1000/60, perf_fake_mouse_cb, stage);
}

void clutter_perf_fps_report (const gchar *id)
{
  g_print ("\n@ %s: %.2f fps \n",
       id, testframes / g_timer_elapsed (testtimer, NULL));
}

static void perf_stage_paint_cb (ClutterStage *stage, gpointer *data)
{
  if (!testtimer)
    testtimer = g_timer_new ();
  testframes ++;
  if (g_timer_elapsed (testtimer, NULL) > testmaxtime)
    {
      clutter_main_quit ();
    }
}

static void wrap (gfloat *value, gfloat min, gfloat max)
{
  if (*value > max)
    *value = min;
  else if (*value < min)
    *value = max;
}

static gboolean perf_fake_mouse_cb (gpointer stage)
{
  ClutterEvent *event = clutter_event_new (CLUTTER_MOTION);
  static ClutterInputDevice *device = NULL;
  int i;
  static float x = 0.0;
  static float y = 0.0;
  static float xd = 0.0;
  static float yd = 0.0;
  static gboolean inited = FALSE;

  gfloat w, h;

  if (!inited) /* XXX:
                  force clutter to do handle our motion events,
                  by forcibly updating the input device's state
                  this shoudl be possible to do in a better
                  manner in the future, a versioning check
                  will have to be added when this is possible
                  without a hack... and the means to do the
                  hack is deprecated
                */
    {
      ClutterEvent *event2 = clutter_event_new (CLUTTER_ENTER);
      device = clutter_device_manager_get_core_device (clutter_device_manager_get_default (), CLUTTER_POINTER_DEVICE);

      event2->crossing.stage = stage;
      event2->crossing.source = stage;
      event2->crossing.x = 10;
      event2->crossing.y = 10;
      event2->crossing.device = device;
      event2->crossing.related = NULL;

      clutter_input_device_update_from_event (device, event2, TRUE);

      clutter_event_put (event2);
      clutter_event_free (event2);
      inited = TRUE;
    }

  clutter_actor_get_size (stage, &w, &h);
  event->motion.stage = stage;
  event->motion.device = device;

  /* called about every 60fps, and do 10 picks per stage */
  for (i = 0; i < 10; i++)
    {
      event->motion.x = x;
      event->motion.y = y;

      clutter_event_put (event);

      x += xd;
      y += yd;
      xd += g_random_double_range (-0.1, 0.1);
      yd += g_random_double_range (-0.1, 0.1);

      wrap (&x, 0, w);
      wrap (&y, 0, h);

      xd = CLAMP(xd, -1.3, 1.3);
      yd = CLAMP(yd, -1.3, 1.3);
    }
  clutter_event_free (event);
  return G_SOURCE_CONTINUE;
}
