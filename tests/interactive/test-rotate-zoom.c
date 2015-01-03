/*
 * Copyright (C) 2013 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 */
#include <stdlib.h>
#include <math.h>
#include <cairo.h>
#include <glib.h>
#include <clutter/clutter.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define STAGE_WIDTH 800
#define STAGE_HEIGHT 550

static ClutterActor *
create_hand (void)
{
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (TESTS_DATADIR G_DIR_SEPARATOR_S "redhand.png", NULL);
  ClutterContent *image = clutter_image_new ();
  ClutterActor *actor = clutter_actor_new ();

  clutter_image_set_data (CLUTTER_IMAGE (image),
                          gdk_pixbuf_get_pixels (pixbuf),
                          gdk_pixbuf_get_has_alpha (pixbuf)
                            ? COGL_PIXEL_FORMAT_RGBA_8888
                            : COGL_PIXEL_FORMAT_RGB_888,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf),
                          gdk_pixbuf_get_rowstride (pixbuf),
                          NULL);
  clutter_actor_set_content (actor, image);
  clutter_actor_set_size (actor,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf));
  clutter_actor_set_reactive (actor, TRUE);

  g_object_unref (pixbuf);

  return actor;
}

G_MODULE_EXPORT int
test_rotate_zoom_main (int argc, char *argv[])
{
  ClutterActor *stage, *actor;
  gfloat width, height;

#ifdef CLUTTER_WINDOWING_X11
  clutter_x11_enable_xinput ();
#endif

  /* initialize Clutter */
  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  /* create a resizable stage */
  stage = clutter_stage_new ();
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Rotate and Zoom actions");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  clutter_actor_set_reactive (stage, FALSE);
  clutter_actor_show (stage);

  actor = create_hand ();
  clutter_actor_add_action (actor, clutter_rotate_action_new ());
  clutter_actor_add_action (actor, clutter_zoom_action_new ());
  clutter_actor_add_child (stage, actor);

  clutter_actor_get_size (actor, &width, &height);
  clutter_actor_set_position (actor,
                              STAGE_WIDTH / 2 - width / 2,
                              STAGE_HEIGHT / 2 - height / 2);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_rotate_zoom_describe (void)
{
  return "Rotates and zooms an actor using touch events";
}
