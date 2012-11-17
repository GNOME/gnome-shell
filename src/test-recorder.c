/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#define GST_USE_UNSTABLE_API
#include "shell-recorder.h"
#include <clutter/clutter.h>
#include <gtk/gtk.h>
#include <gst/gst.h>

/* Very simple test of the ShellRecorder class; shows some text strings
 * moving around and records it.
 */
static ShellRecorder *recorder = NULL;

static gboolean
stop_recording_timeout (ClutterActor *stage)
{
  if (recorder)
    {
      shell_recorder_close (recorder);

      /* quit when the recorder finishes closing
       */
      g_object_weak_ref (G_OBJECT (recorder),
                         (GWeakNotify)
                         clutter_actor_destroy,
                         stage);

      g_object_unref (recorder);
    }
  else
    {
      clutter_actor_destroy (stage);
    }

  return FALSE;
}

static void
on_animation_completed (ClutterAnimation *animation,
                        ClutterStage     *stage)
{
  g_timeout_add (1000, (GSourceFunc) stop_recording_timeout, stage);
}

static void
on_stage_realized (ClutterActor *stage,
                   gpointer      data)
{
  recorder = shell_recorder_new (CLUTTER_STAGE (stage));
  shell_recorder_set_file_template (recorder, "test-recorder.webm");
  shell_recorder_record (recorder);
}

int main (int argc, char **argv)
{
  ClutterActor *stage;
  ClutterActor *text;
  ClutterAnimation *animation;
  ClutterColor red, green, blue;

  gtk_init (&argc, &argv);
  gst_init (&argc, &argv);
  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  clutter_color_from_string (&red, "red");
  clutter_color_from_string (&green, "green");
  clutter_color_from_string (&blue, "blue");
  stage = clutter_stage_new ();
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  text = g_object_new (CLUTTER_TYPE_TEXT,
		       "text", "Red",
		       "font-name", "Sans 40px",
		       "color", &red,
		       NULL);
  clutter_actor_add_child (stage, text);
  animation = clutter_actor_animate (text,
				     CLUTTER_EASE_IN_OUT_QUAD,
				     3000,
				     "x", 320.0,
				     "y", 240.0,
				     NULL);
  g_signal_connect (animation, "completed",
		    G_CALLBACK (on_animation_completed), stage);

  text = g_object_new (CLUTTER_TYPE_TEXT,
		       "text", "Blue",
		       "font-name", "Sans 40px",
		       "color", &blue,
		       "x", 640.0,
		       "y", 0.0,
		       NULL);
  clutter_actor_set_anchor_point_from_gravity (text, CLUTTER_GRAVITY_NORTH_EAST);
  clutter_actor_add_child (stage, text);
  animation = clutter_actor_animate (text,
				     CLUTTER_EASE_IN_OUT_QUAD,
				     3000,
				     "x", 320.0,
				     "y", 240.0,
				     NULL);

  text = g_object_new (CLUTTER_TYPE_TEXT,
		       "text", "Green",
		       "font-name", "Sans 40px",
		       "color", &green,
		       "x", 0.0,
		       "y", 480.0,
		       NULL);
  clutter_actor_set_anchor_point_from_gravity (text, CLUTTER_GRAVITY_SOUTH_WEST);
  clutter_actor_add_child (stage, text);
  animation = clutter_actor_animate (text,
				     CLUTTER_EASE_IN_OUT_QUAD,
				     3000,
				     "x", 320.0,
				     "y", 240.0,
				     NULL);

  g_signal_connect_after (stage, "realize",
                          G_CALLBACK (on_stage_realized), NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
