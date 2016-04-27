#include <stdio.h>
#include <stdlib.h>

#include <gmodule.h>
#include <cogl/cogl.h>

#include <clutter/clutter.h>

/* layout actor, by Lucas Rocha */

#define MY_TYPE_THING                (my_thing_get_type ())
#define MY_THING(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), MY_TYPE_THING, MyThing))
#define MY_IS_THING(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MY_TYPE_THING))
#define MY_THING_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass),  MY_TYPE_THING, MyThingClass))
#define MY_IS_THING_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass),  MY_TYPE_THING))
#define MY_THING_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj),  MY_TYPE_THING, MyThingClass))

typedef struct _MyThing             MyThing;
typedef struct _MyThingPrivate      MyThingPrivate;
typedef struct _MyThingClass        MyThingClass;

struct _MyThing
{
  ClutterActor parent_instance;

  MyThingPrivate *priv;
};

struct _MyThingClass
{
  ClutterActorClass parent_class;
};

enum
{
  PROP_0,

  PROP_SPACING,
  PROP_PADDING,
  PROP_USE_TRANSFORMED_BOX
};

G_DEFINE_TYPE (MyThing, my_thing, CLUTTER_TYPE_ACTOR)

#define MY_THING_GET_PRIVATE(obj)    \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), MY_TYPE_THING, MyThingPrivate))

struct _MyThingPrivate
{
  gfloat  spacing;
  gfloat  padding;

  guint   use_transformed_box : 1;
};

static void
my_thing_set_property (GObject      *gobject,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  MyThingPrivate *priv = MY_THING (gobject)->priv;
  gboolean needs_relayout = TRUE;

  switch (prop_id)
    {
    case PROP_SPACING:
      priv->spacing = g_value_get_float (value);
      break;

    case PROP_PADDING:
      priv->padding = g_value_get_float (value);
      break;

    case PROP_USE_TRANSFORMED_BOX:
      priv->use_transformed_box = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      needs_relayout = FALSE;
      break;
    }

  /* setting spacing or padding queues a relayout 
     because they are supposed to change the internal
     allocation of children */
  if (needs_relayout)
    clutter_actor_queue_relayout (CLUTTER_ACTOR (gobject));
}

static void
my_thing_get_property (GObject    *gobject,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  MyThingPrivate *priv = MY_THING (gobject)->priv;

  switch (prop_id)
    {
    case PROP_SPACING:
      g_value_set_float (value, priv->spacing);
      break;

    case PROP_PADDING:
      g_value_set_float (value, priv->padding);
      break;

    case PROP_USE_TRANSFORMED_BOX:
      g_value_set_boolean (value, priv->use_transformed_box);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
my_thing_get_preferred_width (ClutterActor *self,
                              gfloat   for_height,
                              gfloat  *min_width_p,
                              gfloat  *natural_width_p)
{
  ClutterActorIter iter;
  ClutterActor *child;
  gfloat min_left, min_right;
  gfloat natural_left, natural_right;

  min_left = 0;
  min_right = 0;
  natural_left = 0;
  natural_right = 0;

  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat child_x, child_min, child_natural;

      child_x = clutter_actor_get_x (child);

      clutter_actor_get_preferred_size (child,
                                        &child_min, NULL,
                                        &child_natural, NULL);

      if (child == clutter_actor_get_first_child (self))
        {
          /* First child */
          min_left = child_x;
          natural_left = child_x;
          min_right = min_left + child_min;
          natural_right = natural_left + child_natural;
        }
      else
        {
          /* Union of extents with previous children */
          if (child_x < min_left)
            min_left = child_x;

          if (child_x < natural_left)
            natural_left = child_x;

          if (child_x + child_min > min_right)
            min_right = child_x + child_min;

          if (child_x + child_natural > natural_right)
            natural_right = child_x + child_natural;
        }
    }

  if (min_left < 0)
    min_left = 0;

  if (natural_left < 0)
    natural_left = 0;

  if (min_right < 0)
    min_right = 0;

  if (natural_right < 0)
    natural_right = 0;

  g_assert (min_right >= min_left);
  g_assert (natural_right >= natural_left);

  if (min_width_p)
    *min_width_p = min_right - min_left;

  if (natural_width_p)
    *natural_width_p = natural_right - min_left;
}

static void
my_thing_get_preferred_height (ClutterActor *self,
                               gfloat   for_width,
                               gfloat  *min_height_p,
                               gfloat  *natural_height_p)
{
  ClutterActorIter iter;
  ClutterActor *child;
  gfloat min_top, min_bottom;
  gfloat natural_top, natural_bottom;

  min_top = 0;
  min_bottom = 0;
  natural_top = 0;
  natural_bottom = 0;

  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat child_y, child_min, child_natural;

      child_y = clutter_actor_get_y (child);

      clutter_actor_get_preferred_size (child,
                                        NULL, &child_min,
                                        NULL, &child_natural);

      if (child == clutter_actor_get_first_child (self))
        {
          /* First child */
          min_top = child_y;
          natural_top = child_y;
          min_bottom = min_top + child_min;
          natural_bottom = natural_top + child_natural;
        }
      else
        {
          /* Union of extents with previous children */
          if (child_y < min_top)
            min_top = child_y;

          if (child_y < natural_top)
            natural_top = child_y;

          if (child_y + child_min > min_bottom)
            min_bottom = child_y + child_min;

          if (child_y + child_natural > natural_bottom)
            natural_bottom = child_y + child_natural;
        }
    }

  if (min_top < 0)
    min_top = 0;

  if (natural_top < 0)
    natural_top = 0;

  if (min_bottom < 0)
    min_bottom = 0;

  if (natural_bottom < 0)
    natural_bottom = 0;

  g_assert (min_bottom >= min_top);
  g_assert (natural_bottom >= natural_top);

  if (min_height_p)
    *min_height_p = min_bottom - min_top;

  if (natural_height_p)
    *natural_height_p = natural_bottom - min_top;
}

static void
my_thing_allocate (ClutterActor           *self,
                   const ClutterActorBox  *box,
                   ClutterAllocationFlags  flags)
{
  MyThingPrivate *priv;
  gfloat current_x, current_y, max_row_height;
  ClutterActorIter iter;
  ClutterActor *child;

  clutter_actor_set_allocation (self, box, flags);

  priv = MY_THING (self)->priv;

  current_x = priv->padding;
  current_y = priv->padding;
  max_row_height = 0;

  /* The allocation logic here is to horizontally place children 
   * side-by-side and reflow into a new row when we run out of 
   * space 
   */
  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat natural_width, natural_height;
      ClutterActorBox child_box;

      clutter_actor_get_preferred_size (child,
                                        NULL, NULL,
                                        &natural_width,
                                        &natural_height);

      /* if it fits in the current row, keep it there; otherwise
       * reflow into another row
       */
      if (current_x + natural_width > box->x2 - box->x1 - priv->padding)
        {
          current_x = priv->padding;
          current_y += max_row_height + priv->spacing;
          max_row_height = 0;
        }

      child_box.x1 = current_x;
      child_box.y1 = current_y;
      child_box.x2 = child_box.x1 + natural_width;
      child_box.y2 = child_box.y1 + natural_height;

      clutter_actor_allocate (child, &child_box, flags);

      /* if we take into account the transformation of the children
       * then we first check if it's transformed; then we get the
       * onscreen coordinates of the two points of the bounding box
       * of the actor (origin(x, y) and (origin + size)(x,y)) and
       * we update the coordinates and area given to the next child
       */
      if (priv->use_transformed_box)
        {
          if (clutter_actor_is_scaled (child) ||
              clutter_actor_is_rotated (child))
            {
              ClutterVertex v1 = { 0, }, v2 = { 0, };
              ClutterActorBox transformed_box = { 0, };

              /* origin */
              if (!(flags & CLUTTER_ABSOLUTE_ORIGIN_CHANGED))
                {
                  v1.x = 0;
                  v1.y = 0;
                }
              else
                {
                  v1.x = box->x1;
                  v1.y = box->y1;
                }

              clutter_actor_apply_transform_to_point (child, &v1, &v2);
              transformed_box.x1 = v2.x;
              transformed_box.y1 = v2.y;

              /* size */
              v1.x = natural_width;
              v1.y = natural_height;
              clutter_actor_apply_transform_to_point (child, &v1, &v2);
              transformed_box.x2 = v2.x;
              transformed_box.y2 = v2.y;

              natural_width = transformed_box.x2 - transformed_box.x1;
              natural_height = transformed_box.y2 - transformed_box.y1;
            }
        }

      /* Record the maximum child height on current row to know
       * what's the increment that should be used for the next  
       * row 
       */
      if (natural_height > max_row_height)
        max_row_height = natural_height;

      current_x += natural_width + priv->spacing;
    }
}

#define MIN_SIZE 24
#define MAX_SIZE 64

static void
my_thing_class_init (MyThingClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->set_property = my_thing_set_property;
  gobject_class->get_property = my_thing_get_property;

  actor_class->get_preferred_width  = my_thing_get_preferred_width;
  actor_class->get_preferred_height = my_thing_get_preferred_height;
  actor_class->allocate = my_thing_allocate;

  g_object_class_install_property (gobject_class,
                                   PROP_SPACING,
                                   g_param_spec_float ("spacing",
                                                       "Spacing",
                                                       "Spacing of the thing",
                                                       0, G_MAXFLOAT,
                                                       0,
                                                       G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PADDING,
                                   g_param_spec_float ("padding",
                                                       "Padding",
                                                       "Padding around the thing",
                                                       0, G_MAXFLOAT,
                                                       0,
                                                       G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_USE_TRANSFORMED_BOX,
                                   g_param_spec_boolean ("use-transformed-box",
                                                         "Use Transformed Box",
                                                         "Use transformed box when allocating",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (MyThingPrivate));
}

static void
my_thing_init (MyThing *thing)
{
  thing->priv = MY_THING_GET_PRIVATE (thing);
}

ClutterActor *
my_thing_new (gfloat padding,
              gfloat spacing)
{
  return g_object_new (MY_TYPE_THING,
                       "padding", padding,
                       "spacing", spacing,
                       NULL);
}

/* test code */

static ClutterActor *box              = NULL;
static ClutterActor *icon             = NULL;
static ClutterTimeline *main_timeline = NULL;

static void
toggle_property_value (ClutterActor *actor,
                       const gchar  *property_name)
{
  gboolean value;

  g_object_get (actor, property_name, &value, NULL);

  value = !value;

  g_object_set (box, property_name, value, NULL);
}

static void
increase_property_value (ClutterActor *actor, 
                         const char   *property_name)
{
  gfloat value;

  g_object_get (actor, property_name, &value, NULL);

  value = value + 10.0;

  g_object_set (box, property_name, value, NULL);
}

static void
decrease_property_value (ClutterActor *actor, 
                         const char   *property_name)
{
  gfloat value;

  g_object_get (actor, property_name, &value, NULL);

  value = MAX (0, value - 10.0);

  g_object_set (box, property_name, value, NULL);
}

static ClutterActor *
create_item (void)
{
  ClutterActor *clone = clutter_clone_new (icon);
  
  gint32 size = g_random_int_range (MIN_SIZE, MAX_SIZE);
  
  clutter_actor_set_size (clone, size, size);
  clutter_actor_animate_with_timeline (clone, CLUTTER_EASE_OUT_CUBIC,
                                       main_timeline,
                                       "scale-x", 2.0,
                                       "scale-y", 2.0,
                                       "fixed::scale-gravity", CLUTTER_GRAVITY_CENTER,
                                       NULL);

  return clone;
}

static gboolean
keypress_cb (ClutterActor *actor,
	     ClutterEvent *event,
	     gpointer      data)
{
  switch (clutter_event_get_key_symbol (event))
    {
    case CLUTTER_KEY_q:
      clutter_main_quit ();
      break;

    case CLUTTER_KEY_a:
      {
        if (icon != NULL)
          {
            ClutterActor *clone = create_item (); 

            /* Add one item to container */
            clutter_actor_add_child (box, clone);
          }
        break;
      }

    case CLUTTER_KEY_d:
      {
        ClutterActor *last_child;

        last_child = clutter_actor_get_last_child (box);
        if (last_child != NULL)
          {
            /* Remove last item on container */
            clutter_actor_remove_child (box, last_child);
          }
        break;
      }

    case CLUTTER_KEY_w:
      {
        decrease_property_value (box, "padding");
        break;
      }

    case CLUTTER_KEY_e:
      {
        increase_property_value (box, "padding");
        break;
      }

    case CLUTTER_KEY_r:
      {
        decrease_property_value (box, "spacing");
        break;
      }

    case CLUTTER_KEY_s:
      {
        toggle_property_value (box, "use-transformed-box");
        break;
      }

    case CLUTTER_KEY_t:
      {
        increase_property_value (box, "spacing");
        break;
      }

    case CLUTTER_KEY_z:
      {
        if (clutter_timeline_is_playing (main_timeline))
          clutter_timeline_pause (main_timeline);
        else
          clutter_timeline_start (main_timeline);

        break;
      }

    default:
      break;
    }

  return FALSE;
}

static void
relayout_on_frame (ClutterTimeline *timeline)
{
  gboolean use_transformed_box;

  /* if we care about transformations updating the layout, we need to inform
   * the layout that a transformation is happening; this is either done by
   * attaching a notification on the transformation properties or by simply
   * queuing a relayout on each frame of the timeline used to drive the
   * behaviour. for simplicity's sake, we used the latter
   */

  g_object_get (G_OBJECT (box),
                "use-transformed-box", &use_transformed_box,
                NULL);

  if (use_transformed_box)
    clutter_actor_queue_relayout (box);
}

G_MODULE_EXPORT int
test_layout_main (int argc, char *argv[])
{
  ClutterActor *stage, *instructions;
  gint i, size;
  GError *error = NULL;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 800, 600);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Layout");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  main_timeline = clutter_timeline_new (2000);
  clutter_timeline_set_repeat_count (main_timeline, -1);
  clutter_timeline_set_auto_reverse (main_timeline, TRUE);
  g_signal_connect (main_timeline, "new-frame",
                    G_CALLBACK (relayout_on_frame),
                    NULL);


  box = my_thing_new (10, 10);

  clutter_actor_set_position (box, 20, 20);
  clutter_actor_set_size (box, 350, -1);

  icon = clutter_texture_new_from_file (TESTS_DATADIR
                                        G_DIR_SEPARATOR_S
                                        "redhand.png",
                                        &error);
  if (error)
    g_error ("Unable to load 'redhand.png': %s", error->message);

  size = g_random_int_range (MIN_SIZE, MAX_SIZE);
  clutter_actor_set_size (icon, size, size);
  clutter_actor_add_child (box, icon);
  clutter_actor_animate_with_timeline (icon, CLUTTER_EASE_OUT_CUBIC,
                                       main_timeline,
                                       "scale-x", 2.0,
                                       "scale-y", 2.0,
                                       "fixed::scale-gravity", CLUTTER_GRAVITY_CENTER,
                                       NULL);

  for (i = 1; i < 33; i++)
    {
      ClutterActor *clone = create_item (); 

      clutter_actor_add_child (box, clone);
    }

  clutter_actor_add_child (stage, box);

  instructions = clutter_text_new_with_text (NULL,
                                              "<b>Instructions:</b>\n"
                                              "a - add a new item\n"
                                              "d - remove last item\n"
                                              "z - start/pause behaviour\n"
                                              "w - decrease padding\n"
                                              "e - increase padding\n"
                                              "r - decrease spacing\n"
                                              "t - increase spacing\n"
                                              "s - use transformed box\n"
                                              "q - quit");

  clutter_text_set_use_markup (CLUTTER_TEXT (instructions), TRUE);
  clutter_actor_set_position (instructions, 450, 10);
  clutter_actor_add_child (stage, instructions);

  g_signal_connect (stage, "key-release-event",
		    G_CALLBACK (keypress_cb),
		    NULL);

  clutter_timeline_stop (main_timeline);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (main_timeline);

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_layout_describe (void)
{
  return "Container implementing a layout policy.";
}
