#include <stdlib.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>

static const ClutterColor bg_color = { 0xcc, 0xcc, 0xcc, 0x99 };

static gboolean is_expanded = FALSE;

static gboolean
on_canvas_draw (ClutterCanvas *canvas,
                cairo_t      *cr,
                gint          width,
                gint          height)
{
  cairo_pattern_t *pat;
  gfloat x, y;

  g_print (G_STRLOC ": Painting at %d x %d\n", width, height);

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_restore (cr);

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
  /* we ease the opacity linearly */
  clutter_actor_save_easing_state (emblem);
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
  clutter_actor_set_easing_mode (emblem, CLUTTER_LINEAR);
  clutter_actor_set_opacity (emblem, 0);
  clutter_actor_restore_easing_state (emblem);

  return CLUTTER_EVENT_STOP;
}

static void
on_emblem_clicked (ClutterClickAction *action,
                   ClutterActor       *emblem,
                   ClutterActor       *box)
{
  /* we add a little bounce to the resizing of the box */
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
on_emblem_long_press (ClutterClickAction    *action,
                      ClutterActor          *emblem,
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
redraw_canvas (ClutterActor  *actor,
               ClutterCanvas *canvas)
{
  /* we want to invalidate the canvas and redraw its contents
   * only when the size changes at the end of the animation,
   * to avoid drawing all the states inbetween
   */
  clutter_canvas_set_size (canvas,
                           clutter_actor_get_width (actor),
                           clutter_actor_get_height (actor));
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *box, *bg, *icon, *emblem, *label;
  ClutterLayoutManager *layout;
  ClutterContent *canvas, *image;
  ClutterColor *color;
  ClutterAction *action;
  GdkPixbuf *pixbuf;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  /* prepare the stage */
  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "BinLayout");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Aluminium2);
  clutter_actor_set_size (stage, 640, 480);
  clutter_actor_show (stage);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* this is our BinLayout, with its default alignments */
  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);

  /* the main container; this actor will use the BinLayout to lay
   * out its children; we use the anchor point to keep it centered
   * on the same position even when we change its size
   */
  box = clutter_actor_new ();
  clutter_actor_set_layout_manager (box, layout);
  clutter_actor_add_constraint (box, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));
  clutter_actor_set_position (box, 320, 240);
  clutter_actor_set_reactive (box, TRUE);
  clutter_actor_set_name (box, "box");
  clutter_actor_add_child (stage, box);

  /* the background is drawn using a canvas content */
  canvas = clutter_canvas_new ();
  g_signal_connect (canvas, "draw", G_CALLBACK (on_canvas_draw), NULL);
  clutter_canvas_set_size (CLUTTER_CANVAS (canvas), 200, 200);

  /* this is the background actor; we want it to fill the whole
   * of the allocation given to it by its parent
   */
  bg = clutter_actor_new ();
  clutter_actor_set_name (bg, "background");
  clutter_actor_set_size (bg, 200, 200);
  clutter_actor_set_content (bg, canvas);
  clutter_actor_set_x_expand (bg, TRUE);
  clutter_actor_set_y_expand (bg, TRUE);
  clutter_actor_set_x_align (bg, CLUTTER_ACTOR_ALIGN_FILL);
  clutter_actor_set_y_align (bg, CLUTTER_ACTOR_ALIGN_FILL);
  clutter_actor_add_child (box, bg);
  /* we use the ::transitions-completed signal to get notification
   * of the end of the sizing animation; this allows us to redraw
   * the canvas only once the animation has stopped
   */
  g_signal_connect (box, "transitions-completed",
                    G_CALLBACK (redraw_canvas),
                    canvas);

  /* we use GdkPixbuf to load an image from our data directory */
  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", NULL);
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

  /* this is the icon; it's going to be centered inside the box actor.
   * we use the content gravity to keep the aspect ratio of the image,
   * and the scaling filters to get a better result when scaling the
   * image down.
   */
  icon = clutter_actor_new ();
  clutter_actor_set_name (icon, "icon");
  clutter_actor_set_size (icon, 196, 196);
  clutter_actor_set_x_expand (icon, TRUE);
  clutter_actor_set_y_expand (icon, TRUE);
  clutter_actor_set_x_align (icon, CLUTTER_ACTOR_ALIGN_CENTER);
  clutter_actor_set_y_align (icon, CLUTTER_ACTOR_ALIGN_CENTER);
  clutter_actor_set_content_gravity (icon, CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT);
  clutter_actor_set_content_scaling_filters (icon,
                                             CLUTTER_SCALING_FILTER_TRILINEAR,
                                             CLUTTER_SCALING_FILTER_LINEAR);
  clutter_actor_set_content (icon, image);
  clutter_actor_add_child (box, icon);

  color = clutter_color_new (g_random_int_range (0, 255),
                             g_random_int_range (0, 255),
                             g_random_int_range (0, 255),
                             224);

  /* this is the emblem: a small rectangle with a random color, that we
   * want to put in the bottom right corner
   */
  emblem = clutter_actor_new ();
  clutter_actor_set_name (emblem, "emblem");
  clutter_actor_set_size (emblem, 48, 48);
  clutter_actor_set_background_color (emblem, color);
  clutter_actor_set_x_expand (emblem, TRUE);
  clutter_actor_set_y_expand (emblem, TRUE);
  clutter_actor_set_x_align (emblem, CLUTTER_ACTOR_ALIGN_END);
  clutter_actor_set_y_align (emblem, CLUTTER_ACTOR_ALIGN_END);
  clutter_actor_set_reactive (emblem, TRUE);
  clutter_actor_set_opacity (emblem, 0);
  clutter_actor_add_child (box, emblem);
  clutter_color_free (color);

  /* when clicking on the emblem, we want to perform an action */
  action = clutter_click_action_new ();
  clutter_actor_add_action (emblem, action);
  g_signal_connect (action, "clicked", G_CALLBACK (on_emblem_clicked), box);
  g_signal_connect (action, "long-press", G_CALLBACK (on_emblem_long_press), box);

  /* whenever the pointer enters the box, we show the emblem; we hide
   * the emblem when the pointer leaves the box
   */
  g_signal_connect (box,
                    "enter-event", G_CALLBACK (on_box_enter),
                    emblem);
  g_signal_connect (box,
                    "leave-event", G_CALLBACK (on_box_leave),
                    emblem);

  /* a label, that we want to position at the top and center of the box */
  label = clutter_text_new ();
  clutter_actor_set_name (label, "text");
  clutter_text_set_text (CLUTTER_TEXT (label), "A simple test");
  clutter_actor_set_x_expand (label, TRUE);
  clutter_actor_set_x_align (label, CLUTTER_ACTOR_ALIGN_CENTER);
  clutter_actor_set_y_expand (label, TRUE);
  clutter_actor_set_y_align (label, CLUTTER_ACTOR_ALIGN_START);
  clutter_actor_add_child (box, label);

  clutter_main ();

  return EXIT_SUCCESS;
}
