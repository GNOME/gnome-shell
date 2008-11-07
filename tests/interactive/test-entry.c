#include <gmodule.h>
#include <clutter/clutter.h>

static void
on_entry_activated (ClutterEntry *entry, gpointer null)
{
  g_print ("Activated: %s\n", clutter_entry_get_text (entry));
}

G_MODULE_EXPORT int
test_entry_main (int argc, char *argv[])
{
  ClutterActor    *entry;
  ClutterActor    *stage;
  ClutterColor     stage_color = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor     entry_color = { 0x33, 0xdd, 0xff, 0xff };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  clutter_actor_set_size (stage, 800, 600);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "ClutterEntry Test"); 
  
  entry = clutter_entry_new_with_text ("Sans 14", 
                                       "Type something, be sure to use the "
                                       "left/right arrow keys to move the "
                                       "cursor position.");
  clutter_entry_set_color (CLUTTER_ENTRY (entry), &entry_color);
  clutter_actor_set_size (entry, 600, 50);
  clutter_actor_set_position (entry, 100, 100);
  /*clutter_entry_set_visibility (CLUTTER_ENTRY (entry), FALSE);*/
  /*clutter_entry_set_max_length (CLUTTER_ENTRY (entry), 50);*/
  
  clutter_group_add (CLUTTER_GROUP (stage), entry);
  clutter_stage_set_key_focus (CLUTTER_STAGE (stage), entry);

  clutter_actor_show_all (stage);

  g_signal_connect (entry, "activate", 
		    G_CALLBACK (on_entry_activated), NULL);
  
  clutter_main();

  return 0;
}
