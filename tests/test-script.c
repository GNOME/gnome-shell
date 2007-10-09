#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include <clutter/clutter.h>

static const gchar *test_behaviour =
"{"
"  \"id\"          : \"rotate-behaviour\","
"  \"type\"        : \"ClutterBehaviourRotate\","
"  \"angle-begin\" : 0.0,"
"  \"angle-end\"   : 360.0,"
"  \"axis\"        : \"z-axis\","
"  \"alpha\"       : {"
"    \"timeline\" : { \"num-frames\" : 300, \"fps\" : 60, \"loop\" : true },"
"    \"function\" : \"sine\""
"  }"
"}";

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
"      },"
"      {"
"        \"id\"         : \"red-hand\","
"        \"type\"       : \"ClutterTexture\","
"        \"pixbuf\"     : \"redhand.png\","
"        \"x\"          : 50,"
"        \"y\"          : 50,"
"        \"opacity\"    : 100,"
"        \"visible\"    : true,"
"        \"behaviours\" : [ \"rotate-behaviour\" ]"
"      }"
"    ]"
"  }"
"}";

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *texture;
  ClutterBehaviour *rotate;
  ClutterScript *script;
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

  rotate = CLUTTER_BEHAVIOUR (clutter_script_get_object (script, "rotate-behaviour"));
  clutter_timeline_start (clutter_alpha_get_timeline (clutter_behaviour_get_alpha (rotate)));

  clutter_main ();

  g_object_unref (script);

  return EXIT_SUCCESS;
}
