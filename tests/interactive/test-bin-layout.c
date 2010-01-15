#include <stdlib.h>
#include <gmodule.h>
#include <cairo/cairo.h>
#include <clutter/clutter.h>

static ClutterActor *
make_background (const ClutterColor *color,
                 gfloat              width,
                 gfloat              height)
{
  ClutterActor *tex = clutter_cairo_texture_new (width, height);
  cairo_t *cr = clutter_cairo_texture_create (CLUTTER_CAIRO_TEXTURE (tex));
  cairo_pattern_t *pat;
  gfloat x, y;

#define BG_ROUND_RADIUS         12

  x = y = 0;

  cairo_move_to (cr, BG_ROUND_RADIUS, y);
  cairo_line_to (cr, width - BG_ROUND_RADIUS, y);
  cairo_curve_to (cr, width, y, width, y, width, BG_ROUND_RADIUS);
  cairo_line_to (cr, width, height - BG_ROUND_RADIUS);
  cairo_curve_to (cr, width, height, width, height, width - BG_ROUND_RADIUS, height);
  cairo_line_to (cr, BG_ROUND_RADIUS, height);
  cairo_curve_to (cr, x, height, x, height, x, height - BG_ROUND_RADIUS);
  cairo_line_to (cr, x, BG_ROUND_RADIUS);
  cairo_curve_to (cr, x, y, x, y, BG_ROUND_RADIUS, y);

  cairo_close_path (cr);

  clutter_cairo_set_source_color (cr, color);
  cairo_stroke (cr);

  x += 4;
  y += 4;
  width -= 4;
  height -= 4;

  cairo_move_to (cr, BG_ROUND_RADIUS, y);
  cairo_line_to (cr, width - BG_ROUND_RADIUS, y);
  cairo_curve_to (cr, width, y, width, y, width, BG_ROUND_RADIUS);
  cairo_line_to (cr, width, height - BG_ROUND_RADIUS);
  cairo_curve_to (cr, width, height, width, height, width - BG_ROUND_RADIUS, height);
  cairo_line_to (cr, BG_ROUND_RADIUS, height);
  cairo_curve_to (cr, x, height, x, height, x, height - BG_ROUND_RADIUS);
  cairo_line_to (cr, x, BG_ROUND_RADIUS);
  cairo_curve_to (cr, x, y, x, y, BG_ROUND_RADIUS, y);

  cairo_close_path (cr);

  pat = cairo_pattern_create_linear (0, 0, 0, height);
  cairo_pattern_add_color_stop_rgba (pat, 1, .85, .85, .85, 1);
  cairo_pattern_add_color_stop_rgba (pat, .95, 1, 1, 1, 1);
  cairo_pattern_add_color_stop_rgba (pat, .05, 1, 1, 1, 1);
  cairo_pattern_add_color_stop_rgba (pat, 0, .85, .85, .85, 1);

  cairo_set_source (cr, pat);
  cairo_fill (cr);

  cairo_pattern_destroy (pat);
  cairo_destroy (cr);

#undef BG_ROUND_RADIUS

  return tex;
}

static gboolean
on_box_enter (ClutterActor *box,
              ClutterEvent *event,
              ClutterActor *emblem)
{
  clutter_actor_animate (emblem, CLUTTER_LINEAR, 150,
                         "opacity", 255,
                         NULL);

  return TRUE;
}

static gboolean
on_box_leave (ClutterActor *box,
              ClutterEvent *event,
              ClutterActor *emblem)
{
  clutter_actor_animate (emblem, CLUTTER_LINEAR, 150,
                         "opacity", 0,
                         NULL);

  return TRUE;
}

G_MODULE_EXPORT int
test_bin_layout_main (int argc, char *argv[])
{
  ClutterActor *stage, *box, *rect;
  ClutterLayoutManager *layout;
  ClutterColor stage_color = { 0xe0, 0xf2, 0xfc, 0xff };
  ClutterColor bg_color = { 0xcc, 0xcc, 0xcc, 0x99 };
  ClutterColor *color;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Box test");
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_set_size (stage, 640, 480);

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);

  box = clutter_box_new (layout);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);
  clutter_actor_set_anchor_point_from_gravity (box, CLUTTER_GRAVITY_CENTER);
  clutter_actor_set_position (box, 320, 240);
  clutter_actor_set_reactive (box, TRUE);
  clutter_actor_set_name (box, "box");

  rect = make_background (&bg_color, 200, 200);

  /* first method: use clutter_box_pack() */
  clutter_box_pack (CLUTTER_BOX (box), rect,
                    "x-align", CLUTTER_BIN_ALIGNMENT_FILL,
                    "y-align", CLUTTER_BIN_ALIGNMENT_FILL,
                    NULL);

  clutter_actor_lower_bottom (rect);
  clutter_actor_set_name (rect, "background");

  {
    ClutterActor *tex;
    GError *error;
    gchar *file;

    error = NULL;
    file = g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
    tex = clutter_texture_new_from_file (file, &error);
    if (error)
      g_error ("Unable to create texture: %s", error->message);

    clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (tex), TRUE);

    /* second method: use clutter_bin_layout_add() */
    clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (layout), tex,
                            CLUTTER_BIN_ALIGNMENT_CENTER,
                            CLUTTER_BIN_ALIGNMENT_CENTER);

    clutter_actor_raise (tex, rect);
    clutter_actor_set_width (tex, 175);
    clutter_actor_set_name (tex, "texture");

    g_free (file);
  }

  color = clutter_color_new (g_random_int_range (0, 255),
                             g_random_int_range (0, 255),
                             g_random_int_range (0, 255),
                             224);

  rect = clutter_rectangle_new_with_color (color);

  /* third method: container_add() and set_alignment() */
  clutter_container_add_actor (CLUTTER_CONTAINER (box), rect);
  clutter_bin_layout_set_alignment (CLUTTER_BIN_LAYOUT (layout), rect,
                                    CLUTTER_BIN_ALIGNMENT_END,
                                    CLUTTER_BIN_ALIGNMENT_END);

  clutter_actor_set_size (rect, 50, 50);
  clutter_actor_set_opacity (rect, 0);
  clutter_actor_raise_top (rect);
  clutter_actor_set_name (rect, "emblem");


  g_signal_connect (box,
                    "enter-event", G_CALLBACK (on_box_enter),
                    rect);
  g_signal_connect (box,
                    "leave-event", G_CALLBACK (on_box_leave),
                    rect);

  clutter_actor_show_all (stage);

  clutter_main ();

  clutter_color_free (color);

  return EXIT_SUCCESS;
}
