#include <clutter/clutter.h>

void                
on_key_release_cb (ClutterStage *stage, ClutterEvent *event, ClutterEntry *entry)
{
  if (event->type == CLUTTER_KEY_RELEASE) {
    ClutterKeyEvent* kev = (ClutterKeyEvent *) event;
    guint key = clutter_key_event_symbol (kev);
    
    gint pos = clutter_entry_get_position (entry);
    gint len = g_utf8_strlen (clutter_entry_get_text (entry), -1);
    
    switch (key)
      {
        case CLUTTER_Return:
        case CLUTTER_KP_Enter:
        case CLUTTER_ISO_Enter:
          break;
        case CLUTTER_Escape:
          clutter_main_quit ();
          break;
        case CLUTTER_BackSpace:
          clutter_entry_remove (entry, 1);
          break;
        case CLUTTER_Left:
          if (pos != 0)
            {
              if (pos == -1)
                {
                  clutter_entry_set_position (entry, len-1);  
                }
              else
                clutter_entry_set_position (entry, pos - 1);  
            }         
          break;
        case CLUTTER_Right:
          if (pos != -1)
            {
              if (pos != len)
                clutter_entry_set_position (entry, pos +1);  
            } 
          break;
        case CLUTTER_Up:
          clutter_entry_insert_text (entry, "insert", 5);
          break;
        case CLUTTER_Down:
          clutter_entry_delete_text (entry, 5, 11);
        default:
          clutter_entry_add (entry, clutter_key_event_unicode (kev));
          break;
      }
  }
}

int
main (int argc, char *argv[])
{
  ClutterTimeline *timeline;
  ClutterActor    *entry;
  ClutterActor    *stage;
  gchar           *text;
  gsize            size;
  ClutterColor     stage_color = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor     entry_color = { 0x33, 0xdd, 0xff, 0xff };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  clutter_actor_set_size (stage, 800, 600);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  entry = clutter_entry_new_with_text ("Sans 14", 
                                       "Type something, be sure to use the "
                                       "left/right arrow keys to move the "
                                       "cursor position.");
  clutter_entry_set_color (CLUTTER_ENTRY (entry), &entry_color);
  clutter_actor_set_size (entry, 600, 50);
  clutter_actor_set_position (entry, 100, 100);

  clutter_group_add (CLUTTER_GROUP (stage), entry);
  clutter_group_show_all (CLUTTER_GROUP (stage));

  g_signal_connect (stage, "key-release-event",
		    G_CALLBACK (on_key_release_cb), (gpointer)entry);

  clutter_main();

  return 0;
}
