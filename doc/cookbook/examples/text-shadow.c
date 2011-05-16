#include <stdlib.h>
#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>
#include <clutter/clutter.h>

#define SHADOW_X_OFFSET         3
#define SHADOW_Y_OFFSET         3

static void
_text_paint_cb (ClutterActor *actor)
{
  PangoLayout *layout;
  guint8 real_opacity;
  CoglColor color;
  ClutterText *text = CLUTTER_TEXT (actor);
  ClutterColor text_color = { 0, };

  /* Get the PangoLayout that the Text actor is going to paint */
  layout = clutter_text_get_layout (text);

  /* Get the color of the text, to extract the alpha component */
  clutter_text_get_color (text, &text_color);

  /* Composite the opacity so that the shadow is correctly blended */
  real_opacity = clutter_actor_get_paint_opacity (actor)
               * text_color.alpha
               / 255;

  /* Create a #ccc color and premultiply it */
  cogl_color_init_from_4ub (&color, 0xcc, 0xcc, 0xcc, real_opacity);
  cogl_color_premultiply (&color);

  /* Finally, render the Text layout at a given offset using the color */
  cogl_pango_render_layout (layout, SHADOW_X_OFFSET, SHADOW_Y_OFFSET, &color, 0);
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *text;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Text shadow");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

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
