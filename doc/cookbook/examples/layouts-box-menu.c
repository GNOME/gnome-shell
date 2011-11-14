#include <stdlib.h>
#include <clutter/clutter.h>

#define FONT "Sans 20px"

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor yellow_color = { 0xaa, 0xaa, 0x00, 0xff };
static const ClutterColor black_color = { 0x00, 0x00, 0x00, 0xff };

static void
menu_run_option (ClutterActor *actor,
                 ClutterEvent *event,
                 gpointer      user_data)
{
  g_debug ("%s pressed", (gchar *) user_data);
}

static void
menu_add_option (ClutterBox *menu,
                 gchar      *text,
                 gchar      *shortcut)
{
  ClutterActor *entry;

  entry = clutter_box_new (clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                                   CLUTTER_BIN_ALIGNMENT_CENTER));
  clutter_box_set_color (CLUTTER_BOX (entry), &black_color);
  clutter_actor_set_width (entry, 250);
  clutter_actor_set_reactive (entry, TRUE);

  clutter_box_pack (CLUTTER_BOX (entry),
                    clutter_text_new_full (FONT, text, &yellow_color),
                    "x-align", CLUTTER_BIN_ALIGNMENT_START,
                    NULL);

  clutter_box_pack (CLUTTER_BOX (entry),
                    clutter_text_new_full (FONT, shortcut, &yellow_color),
                    "x-align", CLUTTER_BIN_ALIGNMENT_END,
                    NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (menu), entry);

  g_signal_connect (entry,
                    "button-press-event",
                    G_CALLBACK (menu_run_option),
                    text);
}

static void
menu_toggle (ClutterActor *actor,
             ClutterEvent *event,
             gpointer      user_data)
{
  ClutterAnimation *animation;
  ClutterActor *menu = CLUTTER_ACTOR (user_data);

  if (clutter_actor_get_animation (menu))
    return;

  if (clutter_actor_get_opacity (menu) > 0)
    {
      animation = clutter_actor_animate (menu, CLUTTER_EASE_OUT_CUBIC, 200,
                                         "opacity", 0,
                                         NULL);

      /* hide the menu once it is fully transparent */
      g_signal_connect_swapped (animation,
                                "completed",
                                G_CALLBACK (clutter_actor_hide),
                                menu);
    }
  else
    {
      clutter_actor_show (menu);

      clutter_actor_animate (menu, CLUTTER_EASE_OUT_CUBIC, 200,
                             "opacity", 255,
                             NULL);
    }
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *button;
  ClutterLayoutManager *menu_layout;
  ClutterActor *menu;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* button */
  button = clutter_box_new (clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                                    CLUTTER_BIN_ALIGNMENT_CENTER));
  clutter_actor_set_width (button, 100);
  clutter_actor_set_position (button, 50, 50);
  clutter_actor_set_reactive (button, TRUE);
  clutter_box_set_color (CLUTTER_BOX (button), &black_color);
  clutter_box_pack (CLUTTER_BOX (button),
                    clutter_text_new_full (FONT, "Edit", &yellow_color),
                    "x-align", CLUTTER_BIN_ALIGNMENT_FILL,
                    "y-align", CLUTTER_BIN_ALIGNMENT_FILL,
                    NULL);

  /* menu */
  menu_layout = clutter_box_layout_new ();
  clutter_box_layout_set_homogeneous (CLUTTER_BOX_LAYOUT (menu_layout), TRUE);
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (menu_layout), TRUE);
  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (menu_layout), 2);

  menu = clutter_box_new (menu_layout);
  clutter_box_set_color (CLUTTER_BOX (menu), &yellow_color);
  menu_add_option (CLUTTER_BOX (menu), "Undo", "Ctrl-z");
  menu_add_option (CLUTTER_BOX (menu), "Redo", "Ctrl-Shift-z");
  menu_add_option (CLUTTER_BOX (menu), "Cut", "Ctrl-x");
  menu_add_option (CLUTTER_BOX (menu), "Copy", "Ctrl-c");
  menu_add_option (CLUTTER_BOX (menu), "Paste", "Ctrl-v");

  /* align left-hand side of menu with left-hand side of button */
  clutter_actor_add_constraint (menu, clutter_align_constraint_new (button,
                                                                    CLUTTER_ALIGN_X_AXIS,
                                                                    0.0));

  /* align top of menu with the bottom of the button */
  clutter_actor_add_constraint (menu, clutter_bind_constraint_new (button,
                                                                   CLUTTER_BIND_Y,
                                                                   clutter_actor_get_height (button)));

  /* hide the menu until we're ready to animate it in */
  clutter_actor_set_opacity (menu, 0);
  clutter_actor_hide (menu);

  /* clicking on the button toggles the menu */
  g_signal_connect (button, "button-press-event", G_CALLBACK (menu_toggle), menu);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), menu);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), button);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
