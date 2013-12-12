#include <stdlib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>
#ifdef CLUTTER_WINDOWING_X11
#include <clutter/x11/clutter-x11.h>
#endif

static ClutterActor *
create_content_actor (void)
{
  ClutterActor *content;
  ClutterContent *image;
  GdkPixbuf *pixbuf;

  content = clutter_actor_new ();
  clutter_actor_set_size (content, 720, 720);

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

  clutter_actor_set_content_scaling_filters (content,
                                             CLUTTER_SCALING_FILTER_TRILINEAR,
                                             CLUTTER_SCALING_FILTER_LINEAR);
  clutter_actor_set_content_gravity (content, CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT);
  clutter_actor_set_content (content, image);
  g_object_unref (image);

  return content;
}

static gboolean
on_pan (ClutterPanAction *action,
        ClutterActor     *scroll,
        gboolean          is_interpolated,
        gpointer         *user_data)
{
  gfloat delta_x, delta_y;
  const ClutterEvent *event = NULL;

  if (is_interpolated)
    clutter_pan_action_get_interpolated_delta (action, &delta_x, &delta_y);
  else
    {
      clutter_gesture_action_get_motion_delta (CLUTTER_GESTURE_ACTION (action), 0, &delta_x, &delta_y);
      event = clutter_gesture_action_get_last_event (CLUTTER_GESTURE_ACTION (action), 0);
    }

  g_print ("[%s] panning dx:%.2f dy:%.2f\n",
           event == NULL ? "INTERPOLATED" :
           event->type == CLUTTER_MOTION ? "MOTION" :
           event->type == CLUTTER_TOUCH_UPDATE ? "TOUCH UPDATE" :
           "?",
           delta_x, delta_y);

  return TRUE;
}

static ClutterActor *
create_scroll_actor (ClutterActor *stage)
{
  ClutterActor *scroll;
  ClutterAction *pan_action;

  /* our scrollable viewport */
  scroll = clutter_actor_new ();
  clutter_actor_set_name (scroll, "scroll");

  clutter_actor_add_constraint (scroll, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0));
  clutter_actor_add_constraint (scroll, clutter_bind_constraint_new (stage, CLUTTER_BIND_SIZE, 0));

  clutter_actor_add_child (scroll, create_content_actor ());

  pan_action = clutter_pan_action_new ();
  clutter_pan_action_set_interpolate (CLUTTER_PAN_ACTION (pan_action), TRUE);
  g_signal_connect (pan_action, "pan", G_CALLBACK (on_pan), NULL);
  clutter_actor_add_action (scroll, pan_action);

  clutter_actor_set_reactive (scroll, TRUE);

  return scroll;
}

static gboolean
on_key_press (ClutterActor *stage,
              ClutterEvent *event,
              gpointer      unused)
{
  ClutterActor *scroll;
  guint key_symbol;

  scroll = clutter_actor_get_first_child (stage);

  key_symbol = clutter_event_get_key_symbol (event);

  if (key_symbol == CLUTTER_KEY_space)
    {
      clutter_actor_save_easing_state (scroll);
      clutter_actor_set_easing_duration (scroll, 1000);
      clutter_actor_set_child_transform (scroll, NULL);
      clutter_actor_restore_easing_state (scroll);
    }

  return CLUTTER_EVENT_STOP;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *scroll, *info;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  /* create a new stage */
  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Pan Action");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  scroll = create_scroll_actor (stage);
  clutter_actor_add_child (stage, scroll);

  info = clutter_text_new_with_text (NULL, "Press <space> to reset the image position.");
  clutter_actor_add_child (stage, info);
  clutter_actor_set_position (info, 12, 12);

  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  g_signal_connect (stage, "key-press-event", G_CALLBACK (on_key_press), scroll);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
