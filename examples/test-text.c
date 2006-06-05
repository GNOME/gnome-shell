#include <clutter/clutter.h>

int
main (int argc, char *argv[])
{
  ClutterElement *label;
  ClutterElement *stage;
  gchar          *text;
  gsize           size;
  ClutterColor    stage_color = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor    label_color = { 0x11, 0xdd, 0x11, 0xaa };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  if (!g_file_get_contents ("test-text.c", &text, &size, NULL)) 
    g_error("g_file_get_contents() of test-text.c failed");

  clutter_element_set_size (stage, 800, 600);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  label = clutter_label_new_with_text ("Mono 8", text);
  clutter_label_set_color (CLUTTER_LABEL (label), &label_color);
  /* clutter_label_set_text_extents (CLUTTER_LABEL(label), 200, 0); */

  clutter_group_add (CLUTTER_GROUP (stage), label);
  clutter_group_show_all (CLUTTER_GROUP (stage));
  g_signal_connect (stage, "button-press-event",
		    G_CALLBACK (clutter_main_quit), NULL);
  g_object_unref (stage);

  clutter_main();

  return 0;
}
