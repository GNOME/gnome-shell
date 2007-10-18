#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include <clutter/clutter.h>

static ClutterScript *script = NULL;
static guint merge_id = 0;

static const gchar *test_unmerge =
"{"
"  \"id\" : \"blue-button\","
"  \"type\" : \"ClutterRectangle\","
"  \"color\" : \"#0000ffff\","
"  \"x\" : 350,"
"  \"y\" : 50,"
"  \"width\" : 100,"
"  \"height\" : 100,"
"  \"visible\" : true,"
"  \"reactive\" : true"
"}";

static const gchar *test_behaviour =
"["
"  {"
"    \"id\" : \"main-timeline\","
"    \"type\" : \"ClutterTimeline\","
"    \"num-frames\" : 300,"
"    \"fps\" : 60,"
"    \"loop\" : true"
"  },"
"  {"
"    \"id\"          : \"rotate-behaviour\","
"    \"type\"        : \"ClutterBehaviourRotate\","
"    \"angle-begin\" : 0.0,"
"    \"angle-end\"   : 360.0,"
"    \"axis\"        : \"y-axis\","
"    \"alpha\"       : {"
"      \"timeline\" : \"main-timeline\","
"      \"function\" : \"sine\""
"    }"
"  },"
"  {"
"    \"id\"            : \"fade-behaviour\","
"    \"type\"          : \"ClutterBehaviourOpacity\","
"    \"opacity-start\" : 255,"
"    \"opacity-end\"   : 0,"
"    \"alpha\"         : {"
"      \"timeline\" : \"main-timeline\","
"      \"function\" : \"ramp-inc\""
"    }"
"  }"
"]";

static gboolean
blue_button_press (ClutterActor       *actor,
                   ClutterButtonEvent *event,
                   gpointer            data)
{
  clutter_script_unmerge_objects (script, merge_id);
}

int
main (int argc, char *argv[])
{
  GObject *stage, *timeline, *blue_button;
  GError *error = NULL;

  clutter_init (&argc, &argv);

  script = clutter_script_new ();
  g_assert (CLUTTER_IS_SCRIPT (script));

  clutter_script_load_from_data (script, test_behaviour, -1, &error);
  if (error)
    {
      g_print ("*** Error:\n"
               "***   %s\n", error->message);
      g_error_free (error);
      g_object_unref (script);
      return EXIT_FAILURE;
    }
  
  clutter_script_load_from_file (script, "test-script.json", &error);
  if (error)
    {
      g_print ("*** Error:\n"
               "***   %s\n", error->message);
      g_error_free (error);
      g_object_unref (script);
      return EXIT_FAILURE;
    }

  merge_id = clutter_script_load_from_data (script, test_unmerge, -1, &error);
  if (error)
    {
      g_print ("*** Error:\n"
               "***   %s\n", error->message);
      g_error_free (error);
      g_object_unref (script);
      return EXIT_FAILURE;
    }

  stage = clutter_script_get_object (script, "main-stage");
  clutter_actor_show (CLUTTER_ACTOR (stage));

  blue_button = clutter_script_get_object (script, "blue-button");
  g_signal_connect (blue_button,
                    "button-press-event",
                    G_CALLBACK (blue_button_press),
                    NULL);

  timeline = clutter_script_get_object (script, "main-timeline");
  clutter_timeline_start (CLUTTER_TIMELINE (timeline));

  clutter_main ();

  g_object_unref (script);

  return EXIT_SUCCESS;
}
