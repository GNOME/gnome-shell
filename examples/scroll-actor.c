#include <stdlib.h>
#include <glib.h>
#include <clutter/clutter.h>

static const char *menu_items_name[] = {
  "Option 1",
  "Option 2",
  "Option 3",
  "Option 4",
  "Option 5",
  "Option 6",
  "Option 7",
  "Option 8",
  "Option 9",
  "Option 10",
  "Option 11",
};

static const guint menu_items_len = G_N_ELEMENTS (menu_items_name);

static void
select_item_at_index (ClutterActor *scroll, 
                      int           index_)
{
  ClutterPoint point;
  ClutterActor *menu, *item;
  gpointer old_selected;

  menu = clutter_actor_get_first_child (scroll);

  old_selected = g_object_get_data (G_OBJECT (scroll), "selected-item");
  if (old_selected != NULL)
    {
      item = clutter_actor_get_child_at_index (menu, GPOINTER_TO_INT (old_selected));
      clutter_text_set_color (CLUTTER_TEXT (item), CLUTTER_COLOR_White);
    }

  /* wrap around the index */
  if (index_ < 0)
    index_ = clutter_actor_get_n_children (menu) - 1;
  else if (index_ >= clutter_actor_get_n_children (menu))
    index_ = 0;

  item = clutter_actor_get_child_at_index (menu, index_);
  clutter_actor_get_position (item, &point.x, &point.y);

  /* scroll to the actor's position; the menu actor is always set at (0, 0),
   * so it does not contribute any further offset, and we can use the position
   * of its children to ask the ScrollActor to scroll the visible region; if
   * the menu actor had an offset, or was transformed, we would have needed to
   * get their relative transformed position instead.
   */
  clutter_actor_save_easing_state (scroll);
  clutter_scroll_actor_scroll_to_point (CLUTTER_SCROLL_ACTOR (scroll), &point);
  clutter_actor_restore_easing_state (scroll);

  clutter_text_set_color (CLUTTER_TEXT (item), CLUTTER_COLOR_LightSkyBlue);

  /* store the index of the currently selected item, so that we can
   * implement select_next_item() and select_prev_item()
   */
  g_object_set_data (G_OBJECT (scroll), "selected-item",
                     GINT_TO_POINTER (index_));
}

static void
select_next_item (ClutterActor *scroll)
{
  gpointer selected_ = g_object_get_data (G_OBJECT (scroll), "selected-item");

  select_item_at_index (scroll, GPOINTER_TO_INT (selected_) + 1);
}

static void
select_prev_item (ClutterActor *scroll)
{
  gpointer selected_ = g_object_get_data (G_OBJECT (scroll), "selected-item");

  select_item_at_index (scroll, GPOINTER_TO_INT (selected_) - 1);
}

static ClutterActor *
create_menu_item (const char *name)
{
  ClutterActor *text;

  text = clutter_text_new ();
  clutter_text_set_font_name (CLUTTER_TEXT (text), "Sans Bold 24");
  clutter_text_set_text (CLUTTER_TEXT (text), name);
  clutter_text_set_color (CLUTTER_TEXT (text), CLUTTER_COLOR_White);
  clutter_actor_set_margin_left (text, 12.f);
  clutter_actor_set_margin_right (text, 12.f);

  return text;
}

static ClutterActor *
create_menu_actor (ClutterActor *scroll)
{
  ClutterActor *menu;
  ClutterLayoutManager *layout_manager;
  guint i;

  /* this is our menu; it contains items in a vertical layout */
  layout_manager = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (layout_manager),
                                      CLUTTER_ORIENTATION_VERTICAL);
  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (layout_manager), 12.f);

  menu = clutter_actor_new ();
  clutter_actor_set_layout_manager (menu, layout_manager);
  clutter_actor_set_background_color (menu, CLUTTER_COLOR_Black);

  /* these are the items */
  for (i = 0; i < menu_items_len; i++)
    clutter_actor_add_child (menu, create_menu_item (menu_items_name[i]));

  return menu;
}

static ClutterActor *
create_scroll_actor (ClutterActor *stage)
{
  ClutterActor *scroll;

  /* our scrollable viewport */
  scroll = clutter_scroll_actor_new ();
  clutter_actor_set_name (scroll, "scroll");

  /* give a vertical offset, and constrain the viewport so that its size
   * is bound to the stage size
   */
  clutter_actor_set_position (scroll, 0.f, 18.f);
  clutter_actor_add_constraint (scroll, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (scroll, clutter_bind_constraint_new (stage, CLUTTER_BIND_HEIGHT, -36.f));

  /* we only want to scroll the contents vertically, and
   * ignore any horizontal component
   */
  clutter_scroll_actor_set_scroll_mode (CLUTTER_SCROLL_ACTOR (scroll),
                                        CLUTTER_SCROLL_VERTICALLY);

  clutter_actor_add_child (scroll, create_menu_actor (scroll));

  /* select the first item */
  select_item_at_index (scroll, 0);

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

  if (key_symbol == CLUTTER_KEY_Up)
    select_prev_item (scroll);
  else if (key_symbol == CLUTTER_KEY_Down)
    select_next_item (scroll);

  return CLUTTER_EVENT_STOP;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  /* create a new stage */
  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Scroll Actor");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  g_signal_connect (stage, "key-press-event", G_CALLBACK (on_key_press), NULL);

  clutter_actor_add_child (stage, create_scroll_actor (stage));

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
