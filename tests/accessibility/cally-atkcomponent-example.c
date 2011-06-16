/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2009 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <clutter/clutter.h>

#include "cally-examples-util.h"

#define WIDTH 300
#define HEIGHT 300
#define SIZE 50
#define DEPTH -100

static const ClutterColor color1 = { 0xff, 0xff, 0x00, 0xff };
static const ClutterColor color2 = { 0x00, 0xff, 0x00, 0xff };
static const ClutterColor color3 = { 0x00, 0x00, 0xff, 0xff };
static const ClutterColor color4 = { 0xff, 0x00, 0xff, 0xff };

int
main (int argc, char *argv[])
{
  ClutterActor *stage = NULL;
  ClutterColor  color = { 0x00, 0x00, 0x00, 0xff };
  ClutterActor *button1 = NULL;
  ClutterActor *button2 = NULL;
  ClutterActor *button3 = NULL;
  ClutterActor *button4 = NULL;
  ClutterActor *group[4];
  ClutterGeometry geom = {0, 0, SIZE, SIZE};
  gint i = 0;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  cally_util_a11y_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &color);
  clutter_actor_set_size (stage, WIDTH, HEIGHT);

  button1 = clutter_rectangle_new_with_color (&color1);
  clutter_actor_set_geometry (button1, &geom);

  button2 = clutter_rectangle_new_with_color (&color2);
  geom.x = 2*SIZE;
  geom.y = 0;
  clutter_actor_set_geometry (button2, &geom);

  geom.x = 0;
  geom.y = 2*SIZE;
  button3 = clutter_rectangle_new_with_color (&color3);
  clutter_actor_set_geometry (button3, &geom);
  clutter_actor_set_depth( button3, DEPTH);

  /* a nested hierarchy, to check that the relative positions are
     computed properly */
  geom.x = SIZE/2;
  geom.y = SIZE/2;
  button4 = clutter_rectangle_new_with_color (&color4);
  clutter_actor_set_geometry (button4, &geom);
  clutter_actor_show (button4);

  for (i = 0; i < 4; i++) {
    group[i] = clutter_group_new ();
    clutter_actor_set_geometry (group[i], &geom);

    if (i > 0)
      clutter_container_add_actor (CLUTTER_CONTAINER (group[i]), group [i - 1]);

    clutter_actor_show_all (group[i]);
  }

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), button1);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), button2);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), button3);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group[3]);
  clutter_container_add_actor (CLUTTER_CONTAINER (group[0]), button4);

  clutter_actor_show_all (stage);

  clutter_main ();

  return 0;
}
