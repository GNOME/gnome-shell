#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include <clutter/clutter.h>

static const gchar *test_ui =
"{"
"  \"Scene\" : {"
"    \"id\"       : \"main-stage\","
"    \"type\"     : \"ClutterStage\","
"    \"color\"    : \"white\","
"    \"width\"    : 500,"
"    \"height\"   : 200,"
"    \"children\" : ["
"      {"
"        \"id\"       : \"red-button\","
"        \"type\"     : \"ClutterRectangle\","
"        \"color\"    : \"#ff0000ff\","
"        \"x\"        : 50,"
"        \"y\"        : 50,"
"        \"width\"    : 100,"
"        \"height\"   : 100,"
"        \"visible\"  : true,"
"      },"
"      {"
"        \"id\"       : \"green-button\","
"        \"type\"     : \"ClutterRectangle\","
"        \"color\"    : \"#00ff00ff\","
"        \"x\"        : 200,"
"        \"y\"        : 50,"
"        \"width\"    : 100,"
"        \"height\"   : 100,"
"        \"visible\"  : true,"
"      },"
"      {"
"        \"id\"       : \"blue-button\","
"        \"type\"     : \"ClutterRectangle\","
"        \"color\"    : \"#0000ffff\","
"        \"x\"        : 350,"
"        \"y\"        : 50,"
"        \"width\"    : 100,"
"        \"height\"   : 100,"
"        \"visible\"  : true,"
"      }"
"    ]"
"  }"
"}";

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *rect;
  ClutterScript *script;
  GError *error;

  clutter_init (&argc, &argv);

  script = clutter_script_new ();
  error = NULL;
  clutter_script_load_from_data (script, test_ui, -1, &error);
  if (error)
    {
      g_print ("*** Error:\n"
               "***   %s\n", error->message);
      g_error_free (error);
      g_object_unref (script);
      return EXIT_FAILURE;
    }

  stage = CLUTTER_ACTOR (clutter_script_get_object (script, "main-stage"));
  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (script);

  return EXIT_SUCCESS;
}
