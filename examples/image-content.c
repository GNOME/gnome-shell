#include <stdlib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>

static const struct {
  ClutterContentGravity gravity;
  const char *name;
} gravities[] = {
  { CLUTTER_CONTENT_GRAVITY_TOP_LEFT, "Top Left" },
  { CLUTTER_CONTENT_GRAVITY_TOP, "Top" },
  { CLUTTER_CONTENT_GRAVITY_TOP_RIGHT, "Top Right" },

  { CLUTTER_CONTENT_GRAVITY_LEFT, "Left" },
  { CLUTTER_CONTENT_GRAVITY_CENTER, "Center" },
  { CLUTTER_CONTENT_GRAVITY_RIGHT, "Right" },

  { CLUTTER_CONTENT_GRAVITY_BOTTOM_LEFT, "Bottom Left" },
  { CLUTTER_CONTENT_GRAVITY_BOTTOM, "Bottom" },
  { CLUTTER_CONTENT_GRAVITY_BOTTOM_RIGHT, "Bottom Right" },

  { CLUTTER_CONTENT_GRAVITY_RESIZE_FILL, "Resize Fill" },
  { CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT, "Resize Aspect" },
};

static int n_gravities = G_N_ELEMENTS (gravities);
static int cur_gravity = 0;

static void
on_tap (ClutterTapAction *action,
        ClutterActor     *actor,
        ClutterText      *label)
{
  gchar *str;

  clutter_actor_save_easing_state (actor);
  clutter_actor_set_content_gravity (actor, gravities[cur_gravity].gravity);
  clutter_actor_restore_easing_state (actor);

  str = g_strconcat ("Content gravity: ", gravities[cur_gravity].name, NULL);
  clutter_text_set_text (label, str);
  g_free (str);

  cur_gravity += 1;

  if (cur_gravity >= n_gravities)
    cur_gravity = 0;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *text;
  ClutterContent *image;
  ClutterAction *action;
  GdkPixbuf *pixbuf;
  gchar *str;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  clutter_actor_set_name (stage, "Stage");
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Content Box");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_actor_set_margin_top (stage, 12);
  clutter_actor_set_margin_right (stage, 12);
  clutter_actor_set_margin_bottom (stage, 12);
  clutter_actor_set_margin_left (stage, 12);
  clutter_actor_show (stage);

  pixbuf = gdk_pixbuf_new_from_file (TESTS_DATADIR G_DIR_SEPARATOR_S "redhand.png", NULL);
  image = clutter_image_new ();
  clutter_image_set_data (CLUTTER_IMAGE (image),
                          gdk_pixbuf_get_pixels (pixbuf),
                          gdk_pixbuf_get_has_alpha (pixbuf)
                            ? COGL_PIXEL_FORMAT_RGBA_8888
                            : COGL_PIXEL_FORMAT_RGB_888,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf),
                          gdk_pixbuf_get_rowstride (pixbuf),
                          NULL);
  g_object_unref (pixbuf);

  clutter_actor_set_content_scaling_filters (stage,
                                             CLUTTER_SCALING_FILTER_TRILINEAR,
                                             CLUTTER_SCALING_FILTER_LINEAR);
  clutter_actor_set_content_gravity (stage, gravities[n_gravities - 1].gravity);
  clutter_actor_set_content (stage, image);
  g_object_unref (image);

  str = g_strconcat ("Content gravity: ",
                     gravities[n_gravities - 1].name,
                     NULL);

  text = clutter_text_new ();
  clutter_text_set_text (CLUTTER_TEXT (text), str);
  clutter_actor_add_constraint (text, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));
  clutter_actor_add_child (stage, text);

  g_free (str);

  action = clutter_tap_action_new ();
  g_signal_connect (action, "tap", G_CALLBACK (on_tap), text);
  clutter_actor_add_action (stage, action);

  clutter_main ();

  return EXIT_SUCCESS;
}
