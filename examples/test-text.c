#include <clutter/clutter.h>

int
main (int argc, char *argv[])
{
  ClutterElement *label;
  gchar          *text;
  gsize           size;

  clutter_init (&argc, &argv);

  if (!g_file_get_contents ("test-text.c", &text, &size, NULL)) 
    g_error("g_file_get_contents() of test-text.c failed");

  clutter_element_set_size (CLUTTER_ELEMENT(clutter_stage()), 800, 600);
  clutter_stage_set_color (CLUTTER_STAGE(clutter_stage()), 0x00000000);

  label = clutter_label_new_with_text("Mono 8", text);

  /* clutter_label_set_text_extents (CLUTTER_LABEL(label), 200, 0); */

  clutter_label_set_color (CLUTTER_LABEL(label), 0xffffffff);

  clutter_group_add(clutter_stage(), label);

  clutter_group_show_all(clutter_stage());

  clutter_main();

  return 0;
}
