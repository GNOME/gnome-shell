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

/*
 * The purpose of this example is test key event and global event
 * implementation, specifically:
 *
 *  atk_add_global_event_listener
 *  atk_remove_global_event_listener
 *  atk_add_key_event_listener
 *  atk_remove_key_event_listener
 */
#include <atk/atk.h>
#include <clutter/clutter.h>
#include <cally/cally.h>

#include "cally-examples-util.h"

#define WIDTH 800
#define HEIGHT 600
#define HEIGHT_STEP 100
#define NUM_ENTRIES 3

struct _Data{
  gint value;
};
typedef struct _Data Data;

static gboolean
atk_key_listener (AtkKeyEventStruct *event, gpointer data)
{
  Data *my_data = (Data*) data;

  g_print ("atk_listener: 0x%x ", event->keyval);

  if (my_data != NULL) {
    g_print ("\t Data value: %i\n", my_data->value);
  } else {
    g_print ("\tNo data!!\n");
  }

  return FALSE;
}

static gboolean
window_event_listener (GSignalInvocationHint * signal_hint,
                       guint n_param_values,
                       const GValue * param_values, gpointer data)
{
  AtkObject *accessible;
  GSignalQuery signal_query;
  const gchar *name, *s;

  g_signal_query (signal_hint->signal_id, &signal_query);
  name = signal_query.signal_name;

  accessible = ATK_OBJECT (g_value_get_object (&param_values[0]));
  s = atk_object_get_name (accessible);

  g_print ("Detected window event \"%s\" from object \"%p\" named \"%s\"\n",
           name, accessible, s);

  return TRUE;
}
static void
make_ui (ClutterActor *stage)
{
  gint             i             = 0;
  ClutterActor    *editable      = NULL;
  ClutterActor    *rectangle     = NULL;
  ClutterActor    *label         = NULL;
  ClutterColor     color_stage   = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor     color_text    = { 0xff, 0x00, 0x00, 0xff };
  ClutterColor     color_sel     = { 0x00, 0xff, 0x00, 0x55 };
  ClutterColor     color_label   = { 0x00, 0xff, 0x55, 0xff };
  ClutterColor     color_rect    = { 0x00, 0xff, 0xff, 0x55 };
  ClutterGeometry  label_geom    = {0, 50, -1, -1};
  ClutterGeometry  editable_geom = {150, 50, 500, 75};


  clutter_stage_set_color (CLUTTER_STAGE (stage), &color_stage);
  clutter_actor_set_size (stage, WIDTH, HEIGHT);

  for (i = 0; i < NUM_ENTRIES; i++)
    {
      /* label */
      label = clutter_text_new_full ("Sans Bold 32px",
                                     "Entry",
                                     &color_label);
      clutter_actor_set_geometry (label, &label_geom);

      /* editable */
      editable = clutter_text_new_full ("Sans Bold 32px",
                                        "ddd",
                                        &color_text);
      clutter_actor_set_geometry (editable, &editable_geom);
      clutter_text_set_editable (CLUTTER_TEXT (editable), TRUE);
      clutter_text_set_selectable (CLUTTER_TEXT (editable), TRUE);
      clutter_text_set_selection_color (CLUTTER_TEXT (editable),
                                        &color_sel);
      clutter_actor_grab_key_focus (editable);
      clutter_actor_set_reactive (editable, TRUE);

      /* rectangle: to create a entry "feeling" */
      rectangle = clutter_rectangle_new_with_color (&color_rect);
      clutter_actor_set_geometry (rectangle, &editable_geom);

      clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), editable);
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), rectangle);

      label_geom.y += HEIGHT_STEP;
      editable_geom.y += HEIGHT_STEP;
    }
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage         = NULL;
  Data data1, data2,data3;
  guint id_2 = 0;

  g_set_application_name ("AtkText");

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  if (cally_util_a11y_init (&argc, &argv) == FALSE)
    {
      g_error ("This example requires the accessibility support, "
               "especifically AtkUtil implementation loaded, "
               "as it tries to register and remove event listeners");
    }

  data1.value = 10;
  data2.value = 20;
  data3.value = 30;

  /* key event listeners */
  atk_add_key_event_listener ((AtkKeySnoopFunc)atk_key_listener, &data1);
  id_2 = atk_add_key_event_listener ((AtkKeySnoopFunc)atk_key_listener, &data2);
  atk_add_key_event_listener ((AtkKeySnoopFunc)atk_key_listener, &data3);

  atk_remove_key_event_listener (id_2);

  /* event listeners */
  atk_add_global_event_listener (window_event_listener, "Atk:AtkWindow:create");
  atk_add_global_event_listener (window_event_listener, "Atk:AtkWindow:destroy");
  atk_add_global_event_listener (window_event_listener, "Atk:AtkWindow:activate");
  atk_add_global_event_listener (window_event_listener, "Atk:AtkWindow:deactivate");

  stage = clutter_stage_get_default ();
  make_ui (stage);

  clutter_actor_show_all (stage);

  if (clutter_feature_available (CLUTTER_FEATURE_STAGE_MULTIPLE))
    {
      stage = clutter_stage_new ();
      make_ui (stage);
      clutter_actor_show_all (stage);
    }

  clutter_main ();

  return 0;
}
