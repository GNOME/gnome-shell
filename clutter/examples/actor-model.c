#include <glib-object.h>
#include <gio/gio.h>
#include <clutter/clutter.h>

/* {{{ MenuItemModel */

/* This is our "model" of a Menu item; it has a "label" property, and
 * a "selected" state property. The user is supposed to operate on the
 * model instance, and change its state.
 */

#define EXAMPLE_TYPE_MENU_ITEM_MODEL (example_menu_item_model_get_type ())

G_DECLARE_FINAL_TYPE (ExampleMenuItemModel, example_menu_item_model, EXAMPLE, MENU_ITEM_MODEL, GObject)

struct _ExampleMenuItemModel
{
  GObject parent_instance;

  char *label;

  gboolean selected;
};

struct _ExampleMenuItemModelClass
{
  GObjectClass parent_class;
};

enum {
  MENU_ITEM_MODEL_PROP_LABEL = 1,
  MENU_ITEM_MODEL_PROP_SELECTED,
  MENU_ITEM_MODEL_N_PROPS
};

static GParamSpec *menu_item_model_props[MENU_ITEM_MODEL_N_PROPS] = { NULL, };

G_DEFINE_TYPE (ExampleMenuItemModel, example_menu_item_model, G_TYPE_OBJECT)

static void
example_menu_item_model_finalize (GObject *gobject)
{
  ExampleMenuItemModel *self = (ExampleMenuItemModel *) gobject;

  g_free (self->label);

  G_OBJECT_CLASS (example_menu_item_model_parent_class)->finalize (gobject);
}

static void
example_menu_item_model_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ExampleMenuItemModel *self = (ExampleMenuItemModel *) gobject;

  switch (prop_id)
    {
    case MENU_ITEM_MODEL_PROP_LABEL:
      g_free (self->label);
      self->label = g_value_dup_string (value);
      break;

    case MENU_ITEM_MODEL_PROP_SELECTED:
      self->selected = g_value_get_boolean (value);
      break;
    }
}

static void
example_menu_item_model_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ExampleMenuItemModel *self = (ExampleMenuItemModel *) gobject;

  switch (prop_id)
    {
    case MENU_ITEM_MODEL_PROP_LABEL:
      g_value_set_string (value, self->label);
      break;

    case MENU_ITEM_MODEL_PROP_SELECTED:
      g_value_set_boolean (value, self->selected);
      break;
    }
}

static void
example_menu_item_model_class_init (ExampleMenuItemModelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = example_menu_item_model_set_property;
  gobject_class->get_property = example_menu_item_model_get_property;
  gobject_class->finalize = example_menu_item_model_finalize;

  menu_item_model_props[MENU_ITEM_MODEL_PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  menu_item_model_props[MENU_ITEM_MODEL_PROP_SELECTED] =
    g_param_spec_boolean ("selected", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (gobject_class, MENU_ITEM_MODEL_N_PROPS, menu_item_model_props);
}

static void
example_menu_item_model_init (ExampleMenuItemModel *self)
{
}
/* }}} */

/* {{{ MenuItemView */

/* This is our "view" of a Menu item; it changes state depending on whether
 * the "selected" property is set. The "view" reflects the state of the
 * "model" instance, though it has no direct connection to it.
 */
#define EXAMPLE_TYPE_MENU_ITEM_VIEW (example_menu_item_view_get_type ())

G_DECLARE_FINAL_TYPE (ExampleMenuItemView, example_menu_item_view, EXAMPLE, MENU_ITEM_VIEW, ClutterText)

struct _ExampleMenuItemView
{
  ClutterText parent_instance;

  gboolean is_selected;
};

struct _ExampleMenuItemViewClass
{
  ClutterTextClass parent_class;
};

G_DEFINE_TYPE (ExampleMenuItemView, example_menu_item_view, CLUTTER_TYPE_TEXT)

enum {
  MENU_ITEM_VIEW_PROP_SELECTED = 1,
  MENU_ITEM_VIEW_N_PROPS
};

static GParamSpec *menu_item_view_props[MENU_ITEM_VIEW_N_PROPS] = { NULL, };

static void
example_menu_item_view_set_selected (ExampleMenuItemView *self,
                                     gboolean             selected)
{
  selected = !!selected;
  if (self->is_selected == selected)
    return;

  self->is_selected = selected;

  if (self->is_selected)
    clutter_text_set_color (CLUTTER_TEXT (self), CLUTTER_COLOR_LightSkyBlue);
  else
    clutter_text_set_color (CLUTTER_TEXT (self), CLUTTER_COLOR_White);

  g_object_notify_by_pspec (G_OBJECT (self), menu_item_view_props[MENU_ITEM_VIEW_PROP_SELECTED]);
}

static void
example_menu_item_view_set_property (GObject      *gobject,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case MENU_ITEM_VIEW_PROP_SELECTED:
      example_menu_item_view_set_selected (EXAMPLE_MENU_ITEM_VIEW (gobject),
                                           g_value_get_boolean (value));
      break;
    }
}

static void
example_menu_item_view_get_property (GObject    *gobject,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  switch (prop_id)
    {
    case MENU_ITEM_VIEW_PROP_SELECTED:
      g_value_set_boolean (value, EXAMPLE_MENU_ITEM_VIEW (gobject)->is_selected);
      break;
    }
}

static void
example_menu_item_view_class_init (ExampleMenuItemViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = example_menu_item_view_set_property;
  gobject_class->get_property = example_menu_item_view_get_property;

  menu_item_view_props[MENU_ITEM_VIEW_PROP_SELECTED] =
    g_param_spec_boolean ("selected", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, MENU_ITEM_VIEW_N_PROPS, menu_item_view_props);
}

static void
example_menu_item_view__transition_stopped (ClutterActor *actor,
                                            const char   *transition,
                                            gboolean      is_finished)
{
  clutter_actor_set_scale (actor, 1.0, 1.0);
  clutter_actor_set_opacity (actor, 255);
}

static void
example_menu_item_view_init (ExampleMenuItemView *self)
{
  ClutterText *text = CLUTTER_TEXT (self);
  ClutterActor *actor = CLUTTER_ACTOR (self);
  ClutterTransition *scalex_trans, *scaley_trans, *fade_trans;
  ClutterTransition *group;

  clutter_text_set_font_name (text, "Sans Bold 24px");
  clutter_text_set_color (text, CLUTTER_COLOR_White);

  clutter_actor_set_margin_left (actor, 12);
  clutter_actor_set_margin_right (actor, 12);

  clutter_actor_set_pivot_point (actor, 0.5, 0.5);

  scalex_trans = clutter_property_transition_new ("scale-x");
  clutter_transition_set_from (scalex_trans, G_TYPE_FLOAT, 1.0);
  clutter_transition_set_to (scalex_trans, G_TYPE_FLOAT, 3.0);

  scaley_trans = clutter_property_transition_new ("scale-y");
  clutter_transition_set_from (scaley_trans, G_TYPE_FLOAT, 1.0);
  clutter_transition_set_to (scaley_trans, G_TYPE_FLOAT, 3.0);

  fade_trans = clutter_property_transition_new ("opacity");
  clutter_transition_set_to (fade_trans, G_TYPE_UINT, 0);

  group = clutter_transition_group_new ();
  clutter_transition_group_add_transition (CLUTTER_TRANSITION_GROUP (group), scalex_trans);
  clutter_transition_group_add_transition (CLUTTER_TRANSITION_GROUP (group), scaley_trans);
  clutter_transition_group_add_transition (CLUTTER_TRANSITION_GROUP (group), fade_trans);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (group), 250);
  clutter_timeline_set_progress_mode (CLUTTER_TIMELINE (group), CLUTTER_EASE_OUT);

  clutter_actor_add_transition (actor, "activateTransition", group);
  g_object_unref (group);

  clutter_timeline_stop (CLUTTER_TIMELINE (group));

  g_signal_connect (actor, "transition-stopped",
                    G_CALLBACK (example_menu_item_view__transition_stopped),
                    group);
}

static void
example_menu_item_view_activate (ExampleMenuItemView *self)
{
  ClutterTransition *t;

  t = clutter_actor_get_transition (CLUTTER_ACTOR (self), "activateTransition");
  clutter_timeline_start (CLUTTER_TIMELINE (t));
}

/* }}} */

/* {{{ Menu */

/* This is our container actor, which binds the GListStore with the
 * ExampleMenuItemModel instances to the ExampleMenuItemView actors
 */

#define EXAMPLE_TYPE_MENU (example_menu_get_type ())

G_DECLARE_FINAL_TYPE (ExampleMenu, example_menu, EXAMPLE, MENU, ClutterActor)

struct _ExampleMenu
{
  ClutterActor parent_instance;

  int current_idx;
};

struct _ExampleMenuClass
{
  ClutterActorClass parent_class;
};

G_DEFINE_TYPE (ExampleMenu, example_menu, CLUTTER_TYPE_ACTOR)

static void
example_menu_class_init (ExampleMenuClass *klass)
{
}

static void
example_menu_init (ExampleMenu *self)
{
  ClutterActor *actor = CLUTTER_ACTOR (self);
  ClutterLayoutManager *layout;

  layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (layout), CLUTTER_ORIENTATION_VERTICAL);
  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (layout), 12);

  clutter_actor_set_layout_manager (actor, layout);
  clutter_actor_set_background_color (actor, CLUTTER_COLOR_Black);

  self->current_idx = -1;
}

static ClutterActor *
example_menu_select_item (ExampleMenu *self,
                          int          idx)
{
  ClutterActor *item;

  /* Any change in the view is reflected into the model */

  if (idx == self->current_idx)
    return clutter_actor_get_child_at_index (CLUTTER_ACTOR (self), self->current_idx);

  item = clutter_actor_get_child_at_index (CLUTTER_ACTOR (self), self->current_idx);
  if (item != NULL)
    example_menu_item_view_set_selected ((ExampleMenuItemView *) item, FALSE);

  if (idx < 0)
    idx = clutter_actor_get_n_children (CLUTTER_ACTOR (self)) - 1;
  else if (idx >= clutter_actor_get_n_children (CLUTTER_ACTOR (self)))
    idx = 0;

  self->current_idx = idx;

  item = clutter_actor_get_child_at_index (CLUTTER_ACTOR (self), self->current_idx);
  if (item != NULL)
    example_menu_item_view_set_selected ((ExampleMenuItemView *) item, TRUE);

  return item;
}

static ClutterActor *
example_menu_select_next (ExampleMenu *self)
{
  return example_menu_select_item (self, self->current_idx + 1);
}

static ClutterActor *
example_menu_select_prev (ExampleMenu *self)
{
  return example_menu_select_item (self, self->current_idx - 1);
}

static void
example_menu_activate_item (ExampleMenu *self)
{
  ClutterActor *child;

  child = clutter_actor_get_child_at_index (CLUTTER_ACTOR (self),
                                            self->current_idx);
  if (child == NULL)
    return;

  example_menu_item_view_activate ((ExampleMenuItemView *) child);
}

/* }}} */

/* {{{ main */
static gboolean
on_key_press (ClutterActor *stage,
              ClutterEvent *event)
{
  ClutterActor *scroll = clutter_actor_get_first_child (stage);
  ClutterActor *menu = clutter_actor_get_first_child (scroll);
  ClutterActor *item = NULL;
  guint key = clutter_event_get_key_symbol (event);
  ClutterPoint p;

  switch (key)
    {
    case CLUTTER_KEY_q:
      clutter_main_quit ();
      break;

    case CLUTTER_KEY_Up:
      item = example_menu_select_prev ((ExampleMenu *) menu);
      clutter_actor_get_position (item, &p.x, &p.y);
      break;

    case CLUTTER_KEY_Down:
      item = example_menu_select_next ((ExampleMenu *) menu);
      clutter_actor_get_position (item, &p.x, &p.y);
      break;

    case CLUTTER_KEY_Return:
    case CLUTTER_KEY_KP_Enter:
      example_menu_activate_item ((ExampleMenu *) menu);
      break;
    }

  if (item != NULL)
    clutter_scroll_actor_scroll_to_point (CLUTTER_SCROLL_ACTOR (scroll), &p);

  return CLUTTER_EVENT_PROPAGATE;
}

static void
on_model_item_selection (GObject    *model_item,
                         GParamSpec *pspec,
                         gpointer    data)
{
  char *label = NULL;
  gboolean is_selected = FALSE;

  g_object_get (model_item, "label", &label, "selected", &is_selected, NULL);

  if (is_selected)
    g_print ("Item '%s' selected!\n", label);

  g_free (label);
}

static ClutterActor *
create_menu_actor (void)
{
  /* Our store of menu item models */
  GListStore *model = g_list_store_new (EXAMPLE_TYPE_MENU_ITEM_MODEL);
  ClutterActor *menu = g_object_new (EXAMPLE_TYPE_MENU, NULL);
  int i;

  /* Populate the model */
  for (i = 0; i < 12; i++)
    {
      char *label = g_strdup_printf ("Option %02d", i + 1);

      ExampleMenuItemModel *item = g_object_new (EXAMPLE_TYPE_MENU_ITEM_MODEL,
                                                 "label", label,
                                                 NULL);

      g_list_store_append (model, item);

      g_signal_connect (item, "notify::selected",
                        G_CALLBACK (on_model_item_selection),
                        NULL);

      g_object_unref (item);
      g_free (label);
    }

  /* Bind the list of menu item models to the menu actor; this will
   * create ClutterActor views of each item in the model, and add them
   * to the menu actor
   */
  clutter_actor_bind_model_with_properties (menu, G_LIST_MODEL (model),
                                            EXAMPLE_TYPE_MENU_ITEM_VIEW,
                                            "label", "text", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                            "selected", "selected", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                                            NULL);

  /* We don't need a pointer to the model any more, so we transfer ownership
   * to the menu actor; this means that the model will go away when the menu
   * actor is destroyed
   */
  g_object_unref (model);

  /* Select the first item in the menu */
  example_menu_select_item ((ExampleMenu *) menu, 0);

  return menu;
}

/* The scrolling container for the menu */
static ClutterActor *
create_scroll_actor (void)
{
  ClutterActor *menu = clutter_scroll_actor_new ();
  clutter_actor_set_name (menu, "scroll");
  clutter_scroll_actor_set_scroll_mode (CLUTTER_SCROLL_ACTOR (menu),
                                        CLUTTER_SCROLL_VERTICALLY);
  clutter_actor_set_easing_duration (menu, 250);
  clutter_actor_add_child (menu, create_menu_actor ());

  return menu;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *menu;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Actor Model");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  g_signal_connect (stage, "key-press-event", G_CALLBACK (on_key_press), NULL);
  clutter_actor_show (stage);

#define PADDING 18.f

  menu = create_scroll_actor ();
  clutter_actor_set_position (menu, 0, PADDING);
  clutter_actor_add_constraint (menu, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (menu, clutter_bind_constraint_new (stage, CLUTTER_BIND_HEIGHT, -PADDING * 2));
  clutter_actor_add_child (stage, menu);

  clutter_main ();

  return 0;
}
/* }}} */
