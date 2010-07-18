#include <stdlib.h>
#include <cogl/cogl.h>
#include <cogl-pango.h>
#include <clutter/clutter.h>

#define SHADOW_X_OFFSET         3
#define SHADOW_Y_OFFSET         3

static void
_text_paint_cb (ClutterActor *actor)
{
  ClutterText *text = CLUTTER_TEXT (actor);

  ClutterActorBox alloc = { 0, };
  clutter_actor_get_allocation_box (actor, &alloc);

  PangoLayout *layout;
  layout = clutter_text_get_layout (text);

  ClutterColor text_color = { 0, };
  clutter_text_get_color (text, &text_color);

  guint8 real_opacity;
  real_opacity = clutter_actor_get_paint_opacity (actor)
               * text_color.alpha
               / 255;

  CoglColor color;
  cogl_color_set_from_4ub (&color, 0xcc, 0xcc, 0xcc, real_opacity);
  cogl_pango_render_layout (layout, SHADOW_X_OFFSET, SHADOW_Y_OFFSET, &color, 0);
}

int
main (int argc, char *argv[])
{
  clutter_init (&argc, &argv);

  ClutterActor *stage;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Text shadow");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  ClutterActor *text;
  text = clutter_text_new ();
  clutter_text_set_text (CLUTTER_TEXT (text), "Hello, World!");
  clutter_text_set_font_name (CLUTTER_TEXT (text), "Sans 64px");
  clutter_actor_add_constraint (text, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (text, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));
  g_signal_connect (text, "paint", G_CALLBACK (_text_paint_cb), NULL);

  clutter_container_add (CLUTTER_CONTAINER (stage), text, NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
