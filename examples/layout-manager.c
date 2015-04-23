#include <math.h>
#include <stdlib.h>
#include <clutter/clutter.h>

typedef struct _MultiLayout             MultiLayout;
typedef struct _MultiLayoutClass        MultiLayoutClass;

typedef enum {
  MULTI_LAYOUT_GRID,
  MULTI_LAYOUT_CIRCLE
} MultiLayoutState;

struct _MultiLayout
{
  ClutterLayoutManager parent_instance;

  /* the state of the layout */
  MultiLayoutState state;

  /* spacing between children */
  float spacing;

  /* cell size */
  float cell_width;
  float cell_height;
};

struct _MultiLayoutClass
{
  ClutterLayoutManagerClass parent_class;
};

GType multi_layout_get_type (void);

ClutterLayoutManager *  multi_layout_new                (void);
void                    multi_layout_set_state          (MultiLayout      *layout,
                                                         MultiLayoutState  state);
MultiLayoutState        multi_layout_get_state          (MultiLayout      *layout);
void                    multi_layout_set_spacing        (MultiLayout      *layout,
                                                         float             spacing);

G_DEFINE_TYPE (MultiLayout, multi_layout, CLUTTER_TYPE_LAYOUT_MANAGER)

static void
multi_layout_get_preferred_width (ClutterLayoutManager *manager,
                                  ClutterContainer     *container,
                                  float                 for_height,
                                  float                *min_width_p,
                                  float                *nat_width_p)
{
  MultiLayout *self = (MultiLayout *) manager;
  float minimum, natural;
  float max_natural_width;
  ClutterActorIter iter;
  ClutterActor *child;
  int n_children;

  minimum = natural = 0.f;
  max_natural_width = 0.f;
  n_children = 0;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      float child_minimum, child_natural;

      if (!clutter_actor_is_visible (child))
        continue;

      clutter_actor_get_preferred_width (child, -1.f,
                                         &child_minimum,
                                         &child_natural);

      max_natural_width = MAX (max_natural_width, child_natural);

      if (self->state == MULTI_LAYOUT_GRID)
        {
          minimum += child_minimum;
          natural += child_natural;
        }
      else if (self->state == MULTI_LAYOUT_CIRCLE)
        {
          minimum = MAX (minimum, child_minimum);
          natural = MAX (natural, child_natural);
        }

      n_children += 1;
    }

  self->cell_width = max_natural_width;

  minimum += (self->spacing * (n_children - 1));
  natural += (self->spacing * (n_children - 1));

  if (min_width_p != NULL)
    *min_width_p = minimum;

  if (nat_width_p != NULL)
    *nat_width_p = natural;
}

static void
multi_layout_get_preferred_height (ClutterLayoutManager *manager,
                                   ClutterContainer     *container,
                                   float                 for_width,
                                   float                *min_height_p,
                                   float                *nat_height_p)
{
  MultiLayout *self = (MultiLayout *) manager;
  float minimum, natural;
  ClutterActorIter iter;
  ClutterActor *child;
  int n_children;

  minimum = natural = self->spacing * 2.f;
  n_children = 0;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      float child_minimum, child_natural;

      if (!clutter_actor_is_visible (child))
        continue;

      clutter_actor_get_preferred_height (child, -1.f,
                                          &child_minimum,
                                          &child_natural);

      minimum = MAX (minimum, child_minimum);
      natural = MAX (natural, child_natural);

      n_children += 1;
    }

  self->cell_height = natural;

  minimum += (self->spacing * (n_children - 1));
  natural += (self->spacing * (n_children - 1));

  if (min_height_p != NULL)
    *min_height_p = minimum;

  if (nat_height_p != NULL)
    *nat_height_p = natural;
}

static int
get_items_per_row (MultiLayout *self,
                   float        for_width)
{
  int n_columns;

  if (for_width < 0)
    return 1;

  if (self->cell_width <= 0)
    return 1;

  n_columns = (int) ((for_width + self->spacing) / (self->cell_width + self->spacing));

  return MAX (n_columns, 1);
}

static int
get_visible_children (ClutterActor *actor)
{
  ClutterActorIter iter;
  ClutterActor *child;
  int n_visible_children = 0;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (clutter_actor_is_visible (child))
        n_visible_children += 1;
    }

  return n_visible_children;
}

static void
multi_layout_allocate (ClutterLayoutManager   *manager,
                       ClutterContainer       *container,
                       const ClutterActorBox  *allocation,
                       ClutterAllocationFlags  flags)
{
  MultiLayout *self = (MultiLayout *) manager;
  float avail_width, avail_height;
  float x_offset, y_offset;
  ClutterActorIter iter;
  ClutterActor *child;
  float item_x = 0.f, item_y = 0.f;
  int n_items, n_items_per_row = 0, item_index;
  ClutterPoint center = CLUTTER_POINT_INIT_ZERO;
  double radius = 0, theta = 0;

  n_items = get_visible_children (CLUTTER_ACTOR (container));
  if (n_items == 0)
    return;

  clutter_actor_box_get_origin (allocation, &x_offset, &y_offset);
  clutter_actor_box_get_size (allocation, &avail_width, &avail_height);

  /* ensure we have an updated value of cell_width and cell_height */
  multi_layout_get_preferred_width (manager, container, avail_width, NULL, NULL);
  multi_layout_get_preferred_height (manager, container, avail_height, NULL, NULL);

  item_index = 0;

  if (self->state == MULTI_LAYOUT_GRID)
    {
      n_items_per_row = get_items_per_row (self, avail_width);
      item_x = x_offset;
      item_y = y_offset;
    }
  else if (self->state == MULTI_LAYOUT_CIRCLE)
    {
      center.x = allocation->x2 / 2.f;
      center.y = allocation->y2 / 2.f;
      radius = MIN ((avail_width - self->cell_width) / 2.0,
                    (avail_height - self->cell_height) / 2.0);
    }

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      ClutterActorBox child_allocation = CLUTTER_ACTOR_BOX_INIT_ZERO;

      if (!clutter_actor_is_visible (child))
        continue;

      if (self->state == MULTI_LAYOUT_GRID)
        {
          if (item_index == n_items_per_row)
            {
              item_index = 0;
              item_x = x_offset;
              item_y += self->cell_height + self->spacing;
            }

          child_allocation.x1 = item_x;
          child_allocation.y1 = item_y;
          child_allocation.x2 = child_allocation.x1 + self->cell_width;
          child_allocation.y2 = child_allocation.y1 + self->cell_height;

          item_x += self->cell_width + self->spacing;
        }
      else if (self->state == MULTI_LAYOUT_CIRCLE)
        {
          theta = 2.0 * G_PI / n_items * item_index;
          child_allocation.x1 = center.x + radius * sinf (theta) - (self->cell_width / 2.f);
          child_allocation.y1 = center.y + radius * -cosf (theta) - (self->cell_height / 2.f);
          child_allocation.x2 = child_allocation.x1 + self->cell_width;
          child_allocation.y2 = child_allocation.y1 + self->cell_height;
        }

      clutter_actor_allocate (child, &child_allocation, flags);

      item_index += 1;
    }
}

static void
multi_layout_class_init (MultiLayoutClass *klass)
{
  ClutterLayoutManagerClass *manager_class = CLUTTER_LAYOUT_MANAGER_CLASS (klass);

  manager_class->get_preferred_width = multi_layout_get_preferred_width;
  manager_class->get_preferred_height = multi_layout_get_preferred_height;
  manager_class->allocate = multi_layout_allocate;
}

static void
multi_layout_init (MultiLayout *self)
{
  self->state = MULTI_LAYOUT_GRID;

  self->cell_width = -1.f;
  self->cell_height = -1.f;

  self->spacing = 0.f;
}

ClutterLayoutManager *
multi_layout_new (void)
{
  return g_object_new (multi_layout_get_type (), NULL);
}

void
multi_layout_set_state (MultiLayout *self,
                        MultiLayoutState  state)
{
  if (self->state == state)
    return;

  self->state = state;

  clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (self));
}

MultiLayoutState
multi_layout_get_state (MultiLayout *self)
{
  return self->state;
}

void
multi_layout_set_spacing (MultiLayout *self,
                          float spacing)
{
  self->spacing = spacing;

  clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (self));
}

#define N_RECTS         16
#define RECT_SIZE       64.0
#define N_ROWS          4
#define PADDING         12.0
#define BOX_SIZE        (RECT_SIZE * (N_RECTS / N_ROWS) + PADDING * (N_RECTS / N_ROWS - 1))

static gboolean
on_enter (ClutterActor *rect,
          ClutterEvent *event)
{
  clutter_actor_set_scale (rect, 1.2, 1.2);

  return CLUTTER_EVENT_STOP;
}

static gboolean
on_leave (ClutterActor *rect,
          ClutterEvent *event)
{
  clutter_actor_set_scale (rect, 1.0, 1.0);

  return CLUTTER_EVENT_STOP;
}

static gboolean
on_key_press (ClutterActor *stage,
              ClutterEvent *event,
              ClutterActor *box)
{
  guint keysym = clutter_event_get_key_symbol (event);
  MultiLayout *layout = (MultiLayout *) clutter_actor_get_layout_manager (box);


  switch (keysym)
    {
    case CLUTTER_KEY_q:
      clutter_main_quit ();
      break;

    case CLUTTER_KEY_t:
      {
        MultiLayoutState state = multi_layout_get_state (layout);

        if (state == MULTI_LAYOUT_GRID)
          multi_layout_set_state (layout, MULTI_LAYOUT_CIRCLE);

        if (state == MULTI_LAYOUT_CIRCLE)
          multi_layout_set_state (layout, MULTI_LAYOUT_GRID);
      }
      break;

    default:
      break;
    }

  return CLUTTER_EVENT_STOP;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *box, *label;
  ClutterLayoutManager *manager;
  ClutterMargin margin;
  ClutterTransition *transition;
  int i;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Multi-layout");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_actor_show (stage);

  /* the layout manager for the main container */
  manager = multi_layout_new ();
  multi_layout_set_spacing ((MultiLayout *) manager, PADDING);

  margin.top = margin.bottom = margin.left = margin.right = PADDING;

  /* our main container, centered on the stage */
  box = clutter_actor_new ();
  clutter_actor_set_margin (box, &margin);
  clutter_actor_set_layout_manager (box, manager);
  clutter_actor_set_size (box, BOX_SIZE, BOX_SIZE);
  clutter_actor_add_constraint (box, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));
  clutter_actor_add_child (stage, box);

  for (i = 0; i < N_RECTS; i++)
    {
      ClutterActor *rect = clutter_actor_new ();
      ClutterColor color;

      clutter_color_from_hls (&color,
                              360.0 / N_RECTS * i,
                              0.5,
                              0.8);

      color.alpha = 128 + 128 / N_RECTS * i;

      /* elements on the layout */
      clutter_actor_set_size (rect, RECT_SIZE, RECT_SIZE);
      clutter_actor_set_pivot_point (rect, .5f, .5f);
      clutter_actor_set_background_color (rect, &color);
      clutter_actor_set_opacity (rect, 0);
      clutter_actor_set_reactive (rect, TRUE);

      /* explicit transition that fades in the element; the delay on
       * the transition staggers the fade depending on the index
       */
      transition = clutter_property_transition_new ("opacity");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (transition), 250);
      clutter_timeline_set_delay (CLUTTER_TIMELINE (transition), i * 50);
      clutter_transition_set_from (transition, G_TYPE_UINT, 0);
      clutter_transition_set_to (transition, G_TYPE_UINT, 255);
      clutter_actor_add_transition (rect, "fadeIn", transition);
      g_object_unref (transition);

      /* we want all state transitions to be animated */
      clutter_actor_set_easing_duration (rect, 250);
      clutter_actor_set_easing_mode (rect, CLUTTER_EASE_OUT_CUBIC);

      clutter_actor_add_child (box, rect);

      /* simple hover effect */
      g_signal_connect (rect, "enter-event", G_CALLBACK (on_enter), NULL);
      g_signal_connect (rect, "leave-event", G_CALLBACK (on_leave), NULL);
    }

  label = clutter_text_new ();
  clutter_text_set_text (CLUTTER_TEXT (label),
                         "Press t\t\342\236\236\tToggle layout\n"
                         "Press q\t\342\236\236\tQuit");
  clutter_actor_add_constraint (label, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (label, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.95));
  clutter_actor_add_child (stage, label);

  g_signal_connect (stage, "key-press-event", G_CALLBACK (on_key_press), box);

  clutter_main ();

  return EXIT_SUCCESS;
}
