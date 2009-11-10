/*
 * Copyright 2009 Intel Corporation.
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
#include <stdio.h>
#include <stdlib.h>

#include <gmodule.h>

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include "pango/cogl-pango.h"

static ClutterActor *hover_actor = NULL;
static guint         last_index  = 0;

static void
on_paint (ClutterActor *actor,
          gpointer      user_data)
{
  guint index_ = GPOINTER_TO_UINT (user_data);
  gchar *text = g_strdup_printf ("%u", index_);
  ClutterActorBox alloc = { 0, };
  CoglColor color;
  gint layout_width, layout_height;
  gfloat width, height;
  PangoLayout *layout;

  clutter_actor_get_allocation_box (actor, &alloc);
  clutter_actor_box_get_size (&alloc, &width, &height);

  layout = clutter_actor_create_pango_layout (actor, text);
  pango_layout_get_size (layout, &layout_width, &layout_height);

  cogl_color_set_from_4ub (&color, 0, 0, 0, 255);
  cogl_pango_render_layout (layout,
                            (width - (layout_width / 1024)) / 2,
                            (height - (layout_height / 1024)) / 2,
                            &color, 0);

  g_object_unref (layout);
  g_free (text);
}

static void
enter_event (ClutterActor *actor,
             ClutterEvent *event,
             gpointer data)
{
  ClutterColor color = { 0x00, 0x00, 0x00, 0xff };

  clutter_rectangle_set_border_width (CLUTTER_RECTANGLE (actor), 2);
  clutter_rectangle_set_border_color (CLUTTER_RECTANGLE (actor), &color);

  hover_actor = actor;
}

static void
leave_event (ClutterActor *actor,
             ClutterEvent *event,
             gpointer data)
{
  clutter_rectangle_set_border_width (CLUTTER_RECTANGLE (actor), 0);

  hover_actor = NULL;
}

static gboolean
button_release_event (ClutterActor     *actor,
                      ClutterEvent     *event,
                      ClutterBoxLayout *box)
{
  gboolean xfill, yfill;
  ClutterBoxAlignment xalign, yalign;
  gint button;

  button = clutter_event_get_button (event);

  if (button == 1)
    {
      clutter_box_layout_get_fill (box, actor, &xfill, &yfill);
      clutter_box_layout_set_fill (box, actor,
                                   xfill ? FALSE : TRUE,
                                   yfill ? FALSE : TRUE);
    }
  else
    {
      clutter_box_layout_get_alignment (box, actor, &xalign, &yalign);

      if (xalign < 2)
        xalign += 1;
      else
        xalign = 0;

      if (yalign < 2)
        yalign += 1;
      else
        yalign = 0;

      clutter_box_layout_set_alignment (box, actor, xalign, yalign);
    }

  return TRUE;
}

static void
add_actor (ClutterBoxLayout *box,
           guint             index_)
{
  ClutterActor *rect;
  ClutterColor color = { 0xff, 0xff, 0xff, 255 };
  static gboolean expand = TRUE;

  clutter_color_from_hls (&color,
                          g_random_double_range (0.0, 360.0),
                          0.5,
                          0.5);

  rect = clutter_rectangle_new_with_color (&color);
  clutter_actor_set_size (rect, 32, 64);
  clutter_box_layout_pack (box, rect, expand,
                           FALSE, /* x-fill */
                           FALSE, /* y-fill */
                           CLUTTER_BOX_ALIGNMENT_CENTER,
                           CLUTTER_BOX_ALIGNMENT_CENTER);

  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect_after (rect, "paint",
                    G_CALLBACK (on_paint),
                    GUINT_TO_POINTER (index_));
  g_signal_connect (rect, "enter-event", G_CALLBACK (enter_event), NULL);
  g_signal_connect (rect, "leave-event", G_CALLBACK (leave_event), NULL);
  g_signal_connect (rect, "button-release-event",
                    G_CALLBACK (button_release_event),
                    box);

  expand = !expand;
}

static gboolean
key_release_cb (ClutterActor     *actor,
                ClutterEvent     *event,
                ClutterBoxLayout *layout)
{
  gboolean toggle;
  guint spacing;

  switch (clutter_event_get_key_symbol (event))
    {
    case CLUTTER_v:
      toggle = clutter_box_layout_get_vertical (layout);
      clutter_box_layout_set_vertical (layout, !toggle);
      break;

    case CLUTTER_p:
      toggle = clutter_box_layout_get_pack_start (layout);
      clutter_box_layout_set_pack_start (layout, !toggle);
      break;

    case CLUTTER_s:
      spacing = clutter_box_layout_get_spacing (layout);

      if (spacing > 12)
        spacing = 0;
      else
        spacing++;

      clutter_box_layout_set_spacing (layout, spacing);
      break;

    case CLUTTER_plus:
      add_actor (layout, last_index++);
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

static void
stage_size_changed_cb (ClutterActor *stage,
                       const ClutterActorBox *allocation,
                       ClutterAllocationFlags flags,
                       ClutterActor *box)
{
  gfloat width, height;

  clutter_actor_box_get_size (allocation, &width, &height);
  clutter_actor_set_size (box, width - 100, height - 100);
}

G_MODULE_EXPORT int
test_box_layout_main (int argc, char *argv[])
{
  ClutterActor *stage, *box;
  ClutterLayoutManager *layout;
  gint i;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Box Layout");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_size (stage, 640, 480);

  layout = clutter_box_layout_new ();

  box = clutter_box_new (layout);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);

  for (i = 0; i < 5; i++)
    add_actor (CLUTTER_BOX_LAYOUT (layout), last_index++);

  g_signal_connect (stage, "key-release-event",
                    G_CALLBACK (key_release_cb),
                    layout);
  g_signal_connect (stage, "allocation-changed",
                    G_CALLBACK (stage_size_changed_cb),
                    box);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
