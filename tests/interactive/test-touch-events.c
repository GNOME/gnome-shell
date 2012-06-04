/*
 * Copyright (C) 2012 Collabora Ltd.
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

#define STAGE_WIDTH 800
#define STAGE_HEIGHT 550
#define NUM_COLORS 10
#define NUM_ACTORS 10

static GSList *events = NULL;
static const ClutterColor const static_colors[] = {
  { 0xff, 0x00, 0x00, 0xff },   /* red */
  { 0x80, 0x00, 0x00, 0xff },   /* dark red */
  { 0x00, 0xff, 0x00, 0xff },   /* green */
  { 0x00, 0x80, 0x00, 0xff },   /* dark green */
  { 0x00, 0x00, 0xff, 0xff },   /* blue */
  { 0x00, 0x00, 0x80, 0xff },   /* dark blue */
  { 0x00, 0xff, 0xff, 0xff },   /* cyan */
  { 0x00, 0x80, 0x80, 0xff },   /* dark cyan */
  { 0xff, 0x00, 0xff, 0xff },   /* magenta */
  { 0xff, 0xff, 0x00, 0xff },   /* yellow */
};

static void
canvas_paint (ClutterCairoTexture *canvas)
{
  clutter_cairo_texture_invalidate (canvas);
}

static void
draw_touch (ClutterEvent *event,
            cairo_t      *cr)
{
  gulong sequence = (gulong) clutter_event_get_event_sequence (event);
  ClutterColor color = static_colors[sequence % NUM_COLORS];

  cairo_set_source_rgba (cr, color.red / 255,
                             color.green / 255,
                             color.blue / 255,
                             color.alpha / 255);
  cairo_arc (cr, event->touch.x, event->touch.y, 5, 0, 2 * G_PI);
  cairo_fill (cr);
}

static gboolean
draw_touches (ClutterCairoTexture *canvas,
              cairo_t             *cr)
{
  g_slist_foreach (events, (GFunc) draw_touch, cr);
}

static gboolean
event_cb (ClutterActor *actor, ClutterEvent *event, ClutterActor *canvas)
{
  if (event->type != CLUTTER_TOUCH_UPDATE)
    return FALSE;

  events = g_slist_append (events, clutter_event_copy (event));

  clutter_actor_queue_redraw (canvas);

  return TRUE;
}

static gboolean
rect_event_cb (ClutterActor *actor, ClutterEvent *event, gpointer data)
{
  gulong sequence;
  ClutterColor color;

  if (event->type != CLUTTER_TOUCH_BEGIN)
    return FALSE;

  sequence = (gulong) clutter_event_get_event_sequence (event);
  color = static_colors[sequence % NUM_COLORS];
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (actor), &color);

  return TRUE;
}

G_MODULE_EXPORT int
test_touch_events_main (int argc, char *argv[])
{
  ClutterActor *stage, *canvas;
  int i;

  clutter_x11_enable_xinput ();

  /* initialize Clutter */
  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  /* create a resizable stage */
  stage = clutter_stage_new ();
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Touch events");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  clutter_actor_set_reactive (stage, TRUE);
  clutter_actor_show (stage);

  /* our 2D canvas, courtesy of Cairo */
  canvas = clutter_cairo_texture_new (1, 1);
  g_signal_connect (canvas, "paint", G_CALLBACK (canvas_paint), NULL);
  g_signal_connect (canvas, "draw", G_CALLBACK (draw_touches), NULL);
  clutter_cairo_texture_set_auto_resize (CLUTTER_CAIRO_TEXTURE (canvas), TRUE);
  clutter_actor_add_constraint (canvas,
                                clutter_bind_constraint_new (stage,
                                                             CLUTTER_BIND_SIZE,
                                                             0));
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), canvas);

  g_signal_connect (stage, "event", G_CALLBACK (event_cb), canvas);

  for (i = 0; i < NUM_ACTORS; i++)
    {
      gfloat size = STAGE_HEIGHT / NUM_ACTORS;
      ClutterColor color = static_colors[i % NUM_COLORS];
      ClutterActor *rectangle = clutter_rectangle_new_with_color (&color);

      /* Test that event delivery to actors work */
      g_signal_connect (rectangle, "event", G_CALLBACK (rect_event_cb), NULL);
      
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), rectangle);
      clutter_actor_set_size (rectangle, size, size);
      clutter_actor_set_position (rectangle, 0, i * size);
      clutter_actor_set_reactive (rectangle, TRUE);
    }

  clutter_main ();

  g_slist_free_full (events, (GDestroyNotify) clutter_event_free);

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_touch_events_describe (void)
{
  return "Draw shapes based on touch events";
}
