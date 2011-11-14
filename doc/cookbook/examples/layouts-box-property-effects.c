/*
 * Experiment with permutations of layout properties for a ClutterBoxLayout
 *
 * See the text (in brackets) at the bottom of the application
 * window for available key presses
 */
#include <stdlib.h>
#include <clutter/clutter.h>

#define STAGE_SIDE 510
#define BOX_SIDE STAGE_SIDE * 0.75
#define RED_SIDE STAGE_SIDE / 4
#define GREEN_SIDE STAGE_SIDE / 8
#define BLUE_SIDE STAGE_SIDE / 16

typedef struct
{
  ClutterLayoutManager *box_layout;
  ClutterActor         *box;
  ClutterActor         *status_display;
  gboolean              x_fill;
  gboolean              y_fill;
  gboolean              expand;
  ClutterBoxAlignment   x_align;
  ClutterBoxAlignment   y_align;
} State;

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor box_color = { 0x66, 0x66, 0x00, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor green_color = { 0x00, 0xff, 0x00, 0xff };
static const ClutterColor blue_color = { 0x00, 0x00, 0xff, 0xff };
static const ClutterColor white_color = { 0xff, 0xff, 0xff, 0xff };

static GValue
gboolean_to_gvalue (gboolean value)
{
  GValue gval = {0};

  g_value_init (&gval, G_TYPE_BOOLEAN);
  g_value_set_boolean (&gval, value);

  return gval;
}

static GValue
alignment_to_gvalue (ClutterBoxAlignment value)
{
  GValue gval = {0};

  g_value_init (&gval, G_TYPE_INT);
  g_value_set_int (&gval, value);

  return gval;
}

static gchar*
alignment_as_string (ClutterBoxAlignment value)
{
  gchar *align_string = "start ";

  switch (value)
    {
    case CLUTTER_BOX_ALIGNMENT_CENTER:
      align_string = "center";
      break;

    case CLUTTER_BOX_ALIGNMENT_END:
      align_string = "end   ";
      break;

    case CLUTTER_BOX_ALIGNMENT_START:
      align_string = "start ";
      break;
    }

  return align_string;
}

static ClutterBoxAlignment
get_next_alignment (ClutterBoxAlignment alignment)
{
  alignment++;

  if (alignment > CLUTTER_BOX_ALIGNMENT_CENTER)
    alignment = CLUTTER_BOX_ALIGNMENT_START;

  return alignment;
}

static void
show_status (State *state)
{
  ClutterText *text = CLUTTER_TEXT (state->status_display);
  ClutterBoxLayout *box_layout = CLUTTER_BOX_LAYOUT (state->box_layout);

  gboolean homogeneous = clutter_box_layout_get_homogeneous (box_layout);
  gboolean vertical = clutter_box_layout_get_vertical (box_layout);

  gchar *message = g_strdup_printf ("x_fill (x): %s\t\t\t"
                                    "y_fill (y): %s\n"
                                    "expand (e): %s\t\t"
                                    "homogeneous (h): %s\n"
                                    "spacing (+/-): %dpx\t\t"
                                    "vertical (v): %s\n"
                                    "x_align (right): %s\t"
                                    "y_align (up): %s",
                                    (state->x_fill ? "true" : "false"),
                                    (state->y_fill ? "true" : "false"),
                                    (state->expand ? "true" : "false"),
                                    (homogeneous ? "true" : "false"),
                                    clutter_box_layout_get_spacing (box_layout),
                                    (vertical ? "true" : "false"),
                                    alignment_as_string (state->x_align),
                                    alignment_as_string (state->y_align));

  clutter_text_set_text (text, message);

  g_free (message);
}

static void
set_property_on_layout_children (State       *state,
                                 const gchar *property,
                                 GValue       value)
{
  ClutterActor *actor;
  ClutterContainer *container = CLUTTER_CONTAINER (state->box);
  ClutterLayoutManager *manager = CLUTTER_LAYOUT_MANAGER (state->box_layout);
  GList *actors = clutter_container_get_children (container);

  for (; actors; actors = actors->next)
    {
      actor = CLUTTER_ACTOR (actors->data);

      clutter_layout_manager_child_set_property (manager,
                                                 container,
                                                 actor,
                                                 property,
                                                 &value);
    }

  g_list_free (actors);
}

static void
toggle_x_fill (GObject            *instance,
               const gchar        *action_name,
               guint               key_val,
               ClutterModifierType modifiers,
               gpointer            user_data)
{
  State *state = (State *) user_data;

  state->x_fill = !state->x_fill;

  set_property_on_layout_children (state,
                                   "x-fill",
                                   gboolean_to_gvalue (state->x_fill));
}

static void
toggle_y_fill (GObject            *instance,
               const gchar        *action_name,
               guint               key_val,
               ClutterModifierType modifiers,
               gpointer            user_data)
{
  State *state = (State *) user_data;

  state->y_fill = !state->y_fill;

  set_property_on_layout_children (state,
                                   "y-fill",
                                   gboolean_to_gvalue (state->y_fill));
}

static void
toggle_expand (GObject            *instance,
               const gchar        *action_name,
               guint               key_val,
               ClutterModifierType modifiers,
               gpointer            user_data)
{
  State *state = (State *) user_data;

  state->expand = !state->expand;

  set_property_on_layout_children (state,
                                   "expand",
                                   gboolean_to_gvalue (state->expand));
}

static void
rotate_x_alignment (GObject            *instance,
                    const gchar        *action_name,
                    guint               key_val,
                    ClutterModifierType modifiers,
                    gpointer            user_data)
{
  State *state = (State *) user_data;

  state->x_align = get_next_alignment (state->x_align);

  set_property_on_layout_children (state,
                                   "x-align",
                                   alignment_to_gvalue (state->x_align));
}

static void
rotate_y_alignment (GObject            *instance,
                    const gchar        *action_name,
                    guint               key_val,
                    ClutterModifierType modifiers,
                    gpointer            user_data)
{
  State *state = (State *) user_data;

  state->y_align = get_next_alignment (state->y_align);

  set_property_on_layout_children (state,
                                   "y-align",
                                   alignment_to_gvalue (state->y_align));
}

static void
toggle_vertical (GObject            *instance,
                 const gchar        *action_name,
                 guint               key_val,
                 ClutterModifierType modifiers,
                 gpointer            user_data)
{
  State *state = (State *) user_data;
  ClutterBoxLayout *box_layout = CLUTTER_BOX_LAYOUT (state->box_layout);
  gboolean vertical = clutter_box_layout_get_vertical (box_layout);

  clutter_box_layout_set_vertical (box_layout, !vertical);
}

static void
toggle_homogeneous (GObject            *instance,
                    const gchar        *action_name,
                    guint               key_val,
                    ClutterModifierType modifiers,
                    gpointer            user_data)
{
  State *state = (State *) user_data;
  ClutterBoxLayout *box_layout = CLUTTER_BOX_LAYOUT (state->box_layout);
  gboolean homogeneous = clutter_box_layout_get_homogeneous (box_layout);

  clutter_box_layout_set_homogeneous (box_layout, !homogeneous);
}

static void
increase_spacing (GObject            *instance,
                  const gchar        *action_name,
                  guint               key_val,
                  ClutterModifierType modifiers,
                  gpointer            user_data)
{
  State *state = (State *) user_data;
  ClutterBoxLayout *box_layout = CLUTTER_BOX_LAYOUT (state->box_layout);
  guint spacing = clutter_box_layout_get_spacing (box_layout) + 5;

  clutter_box_layout_set_spacing (box_layout, spacing);
}

static void
decrease_spacing (GObject            *instance,
                  const gchar        *action_name,
                  guint               key_val,
                  ClutterModifierType modifiers,
                  gpointer            user_data)
{
  State *state = (State *) user_data;
  ClutterBoxLayout *box_layout = CLUTTER_BOX_LAYOUT (state->box_layout);
  guint spacing = clutter_box_layout_get_spacing (box_layout);

  if (spacing >= 5)
    clutter_box_layout_set_spacing (box_layout, spacing - 5);
}

static gboolean
key_pressed_cb (ClutterActor *actor,
                ClutterEvent *event,
                gpointer      user_data)
{
  State *state = (State *) user_data;
  ClutterBindingPool *pool;
  gboolean return_value;

  pool = clutter_binding_pool_find (G_OBJECT_TYPE_NAME (actor));

  return_value = clutter_binding_pool_activate (pool,
                                                clutter_event_get_key_symbol (event),
                                                clutter_event_get_state (event),
                                                G_OBJECT (actor));

  show_status (state);

  return return_value;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  GObjectClass *stage_class;
  ClutterBindingPool *binding_pool;
  ClutterActor *red;
  ClutterActor *green;
  ClutterActor *blue;

  State *state = g_new0 (State, 1);

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  state->x_fill = FALSE;
  state->y_fill = FALSE;
  state->expand = FALSE;
  state->x_align = CLUTTER_BOX_ALIGNMENT_START;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, STAGE_SIDE, STAGE_SIDE);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* for key bindings */
  stage_class = G_OBJECT_GET_CLASS (stage);
  binding_pool = clutter_binding_pool_get_for_class (stage_class);

  clutter_binding_pool_install_action (binding_pool,
                                       "toggle-expand",
                                       CLUTTER_KEY_e,
                                       0,
                                       G_CALLBACK (toggle_expand),
                                       state,
                                       NULL);

  clutter_binding_pool_install_action (binding_pool,
                                       "toggle-x-fill",
                                       CLUTTER_KEY_x,
                                       0,
                                       G_CALLBACK (toggle_x_fill),
                                       state,
                                       NULL);

  clutter_binding_pool_install_action (binding_pool,
                                       "toggle-y-fill",
                                       CLUTTER_KEY_y,
                                       0,
                                       G_CALLBACK (toggle_y_fill),
                                       state,
                                       NULL);

  clutter_binding_pool_install_action (binding_pool,
                                       "toggle-vertical",
                                       CLUTTER_KEY_v,
                                       0,
                                       G_CALLBACK (toggle_vertical),
                                       state,
                                       NULL);

  clutter_binding_pool_install_action (binding_pool,
                                       "toggle-homogeneous",
                                       CLUTTER_KEY_h,
                                       0,
                                       G_CALLBACK (toggle_homogeneous),
                                       state,
                                       NULL);

  clutter_binding_pool_install_action (binding_pool,
                                       "rotate-x-alignment",
                                       CLUTTER_KEY_Right,
                                       0,
                                       G_CALLBACK (rotate_x_alignment),
                                       state,
                                       NULL);

  clutter_binding_pool_install_action (binding_pool,
                                       "rotate-y-alignment",
                                       CLUTTER_KEY_Up,
                                       0,
                                       G_CALLBACK (rotate_y_alignment),
                                       state,
                                       NULL);

  clutter_binding_pool_install_action (binding_pool,
                                       "increase-spacing",
                                       CLUTTER_KEY_plus,
                                       CLUTTER_SHIFT_MASK,
                                       G_CALLBACK (increase_spacing),
                                       state,
                                       NULL);

  clutter_binding_pool_install_action (binding_pool,
                                       "decrease-spacing",
                                       CLUTTER_KEY_minus,
                                       0,
                                       G_CALLBACK (decrease_spacing),
                                       state,
                                       NULL);

  /* rectangles inside the layout */
  red = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_size (red, RED_SIDE, RED_SIDE);

  green = clutter_rectangle_new_with_color (&green_color);
  clutter_actor_set_size (green, GREEN_SIDE, GREEN_SIDE);

  blue = clutter_rectangle_new_with_color (&blue_color);
  clutter_actor_set_size (blue, BLUE_SIDE, BLUE_SIDE);

  /* the layout */
  state->box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_use_animations (CLUTTER_BOX_LAYOUT (state->box_layout),
                                         TRUE);

  state->box = clutter_box_new (state->box_layout);
  clutter_box_set_color (CLUTTER_BOX (state->box), &box_color);
  clutter_actor_set_size (state->box, BOX_SIDE, BOX_SIDE);
  clutter_actor_add_constraint (state->box,
                                clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (state->box,
                                clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.1));

  /* text to show status */
  state->status_display = clutter_text_new ();
  clutter_text_set_color (CLUTTER_TEXT (state->status_display), &white_color);
  clutter_actor_set_size (state->status_display,
                          STAGE_SIDE,
                          STAGE_SIDE * 0.2);
  clutter_actor_set_position (state->status_display,
                              (STAGE_SIDE - BOX_SIDE) / 2,
                              STAGE_SIDE * 0.8);

  /* set text for initial state */
  show_status (state);

  /* connect key presses to a callback on the binding pool */
  g_signal_connect (stage,
                    "key-press-event",
                    G_CALLBACK (key_pressed_cb),
                    state);

  /* pack UI */
  clutter_container_add (CLUTTER_CONTAINER (state->box), red, green, blue, NULL);

  clutter_container_add (CLUTTER_CONTAINER (stage),
                         state->box,
                         state->status_display,
                         NULL);

  /* show stage */
  clutter_actor_show (stage);

  clutter_main ();

  /* clean up */
  g_object_unref (binding_pool);
  g_free (state);

  return EXIT_SUCCESS;
}
