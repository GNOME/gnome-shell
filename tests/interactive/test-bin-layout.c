#include <stdlib.h>
#include <gmodule.h>
#include <cairo.h>
#include <clutter/clutter.h>

static const ClutterColor bg_color = { 0xcc, 0xcc, 0xcc, 0x99 };

static gboolean is_expanded = FALSE;

static gboolean
draw_background (ClutterCairoTexture *texture,
                 cairo_t             *cr)
{
  cairo_pattern_t *pat;
  gfloat x, y;
  guint width, height;

  clutter_cairo_texture_get_surface_size (texture, &width, &height);

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

  clutter_cairo_set_source_color (cr, &bg_color);
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

#undef BG_ROUND_RADIUS

  return TRUE;
}

static gboolean
on_box_enter (ClutterActor *box,
              ClutterEvent *event,
              ClutterActor *emblem)
{
  clutter_actor_save_easing_state (emblem);
  clutter_actor_set_easing_duration (emblem, 150);
  clutter_actor_set_easing_mode (emblem, CLUTTER_LINEAR);
  clutter_actor_set_opacity (emblem, 255);
  clutter_actor_restore_easing_state (emblem);

  return CLUTTER_EVENT_STOP;
}

static gboolean
on_box_leave (ClutterActor *box,
              ClutterEvent *event,
              ClutterActor *emblem)
{
  clutter_actor_save_easing_state (emblem);
  clutter_actor_set_easing_duration (emblem, 150);
  clutter_actor_set_easing_mode (emblem, CLUTTER_LINEAR);
  clutter_actor_set_opacity (emblem, 0);
  clutter_actor_restore_easing_state (emblem);

  return CLUTTER_EVENT_STOP;
}

static void
on_rect_clicked (ClutterClickAction *action,
                 ClutterActor       *rect,
                 ClutterActor       *box)
{
  clutter_actor_save_easing_state (box);
  clutter_actor_set_easing_mode (box, CLUTTER_EASE_OUT_BOUNCE);
  clutter_actor_set_easing_duration (box, 500);

  if (!is_expanded)
    clutter_actor_set_size (box, 400, 400);
  else
    clutter_actor_set_size (box, 200, 200);

  clutter_actor_restore_easing_state (box);

  is_expanded = !is_expanded;
}

static gboolean
on_rect_long_press (ClutterClickAction    *action,
                    ClutterActor          *rect,
                    ClutterLongPressState  state,
                    ClutterActor          *box)
{
  switch (state)
    {
    case CLUTTER_LONG_PRESS_QUERY:
      g_print ("*** long press: query ***\n");
      return is_expanded;

    case CLUTTER_LONG_PRESS_CANCEL:
      g_print ("*** long press: cancel ***\n");
      break;

    case CLUTTER_LONG_PRESS_ACTIVATE:
      g_print ("*** long press: activate ***\n");
      break;
    }

  return TRUE;
}

static void
on_box_allocation_changed (ClutterActor           *box,
                           const ClutterActorBox  *allocation,
                           ClutterAllocationFlags  flags,
                           ClutterActor           *background)
{
  gfloat new_width, new_height;

  clutter_actor_box_get_size (allocation, &new_width, &new_height);
  clutter_cairo_texture_set_surface_size (CLUTTER_CAIRO_TEXTURE (background),
                                          new_width,
                                          new_height);
  clutter_cairo_texture_invalidate (CLUTTER_CAIRO_TEXTURE (background));
}

G_MODULE_EXPORT int
test_bin_layout_main (int argc, char *argv[])
{
  ClutterActor *stage, *box, *rect;
  ClutterLayoutManager *layout;
  ClutterColor stage_color = { 0xe0, 0xf2, 0xfc, 0xff };
  ClutterColor *color;
  ClutterAction *action;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "BinLayout");
  clutter_actor_set_background_color (stage, &stage_color);
  clutter_actor_set_size (stage, 640, 480);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);

  box = clutter_box_new (layout);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);
  clutter_actor_set_anchor_point_from_gravity (box, CLUTTER_GRAVITY_CENTER);
  clutter_actor_set_position (box, 320, 240);
  clutter_actor_set_reactive (box, TRUE);
  clutter_actor_set_name (box, "box");

  /* the contents of the texture are created every time the allocation
   * of the box changes, to keep the background's size the same as the
   * box's size
   */
  rect = clutter_cairo_texture_new (100, 100);
  g_signal_connect (rect, "draw", G_CALLBACK (draw_background), NULL);

  /* first method: use clutter_box_pack() */
  clutter_box_pack (CLUTTER_BOX (box), rect,
                    "x-align", CLUTTER_BIN_ALIGNMENT_FILL,
                    "y-align", CLUTTER_BIN_ALIGNMENT_FILL,
                    NULL);

  clutter_actor_lower_bottom (rect);
  clutter_actor_set_name (rect, "background");
  g_signal_connect (box, "allocation-changed",
                    G_CALLBACK (on_box_allocation_changed),
                    rect);

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
  clutter_actor_set_reactive (rect, TRUE);
  clutter_actor_raise_top (rect);
  clutter_actor_set_name (rect, "emblem");

  action = clutter_click_action_new ();
  clutter_actor_add_action (rect, action);
  g_signal_connect (action, "clicked", G_CALLBACK (on_rect_clicked), box);
  g_signal_connect (action, "long-press", G_CALLBACK (on_rect_long_press), box);
  g_signal_connect (box,
                    "enter-event", G_CALLBACK (on_box_enter),
                    rect);
  g_signal_connect (box,
                    "leave-event", G_CALLBACK (on_box_leave),
                    rect);

  rect = clutter_text_new ();
  clutter_text_set_text (CLUTTER_TEXT (rect), "A simple test");
  clutter_container_add_actor (CLUTTER_CONTAINER (box), rect);
  clutter_bin_layout_set_alignment (CLUTTER_BIN_LAYOUT (layout), rect,
                                    CLUTTER_BIN_ALIGNMENT_CENTER,
                                    CLUTTER_BIN_ALIGNMENT_START);
  clutter_actor_raise_top (rect);
  clutter_actor_set_name (rect, "text");

  clutter_actor_show_all (stage);

  clutter_main ();

  clutter_color_free (color);

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_bin_layout_describe (void)
{
  return "BinLayout layout manager example";
}
