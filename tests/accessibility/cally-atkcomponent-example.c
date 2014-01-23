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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <clutter/clutter.h>

#include "cally-examples-util.h"

#define WIDTH 300
#define HEIGHT 300
#define SIZE 50
#define DEPTH -100

int
main (int argc, char *argv[])
{
  ClutterActor *stage = NULL;
  ClutterActor *button1 = NULL;
  ClutterActor *button2 = NULL;
  ClutterActor *button3 = NULL;
  ClutterActor *button4 = NULL;
  ClutterActor *group[4];
  int i = 0;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  cally_util_a11y_init (&argc, &argv);

  stage = clutter_stage_new ();

  clutter_stage_set_title (CLUTTER_STAGE (stage), "Cally - AtkComponent Test");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_White);
  clutter_actor_set_size (stage, WIDTH, HEIGHT);

  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  button1 = clutter_actor_new ();
  clutter_actor_set_background_color (button1, CLUTTER_COLOR_Yellow);
  clutter_actor_set_size (button1, SIZE, SIZE);

  button2 = clutter_actor_new ();
  clutter_actor_set_background_color (button2, CLUTTER_COLOR_Green);
  clutter_actor_set_position (button2, 2 * SIZE, 0);
  clutter_actor_set_size (button2, SIZE, SIZE);

  button3 = clutter_actor_new ();
  clutter_actor_set_background_color (button3, CLUTTER_COLOR_Blue);
  clutter_actor_set_position (button3, 0, 2 * SIZE);
  clutter_actor_set_size (button3, SIZE, SIZE);
  clutter_actor_set_z_position (button3, DEPTH);

  /* a nested hierarchy, to check that the relative positions are
     computed properly */
  button4 = clutter_actor_new ();
  clutter_actor_set_background_color (button4, CLUTTER_COLOR_Magenta);
  clutter_actor_set_position (button4, SIZE / 2, SIZE / 2);
  clutter_actor_set_size (button4, SIZE, SIZE);

  for (i = 0; i < 4; i++) {
    group[i] = clutter_actor_new ();
    clutter_actor_set_position (group[i], SIZE / 2, SIZE / 2);
    clutter_actor_set_size (group[i], SIZE, SIZE);

    if (i > 0)
      clutter_actor_add_child (group[i], group[i - 1]);
  }

  clutter_actor_add_child (stage, button1);
  clutter_actor_add_child (stage, button2);
  clutter_actor_add_child (stage, button3);
  clutter_actor_add_child (stage, group[3]);
  clutter_actor_add_child (group[0], button4);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
