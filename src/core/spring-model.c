/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "spring-model.h"
#include <math.h>

struct XYPair
{
  double x, y;
};

#define GRID_WIDTH  4
#define GRID_HEIGHT 4

#define MODEL_MAX_OBJECTS (GRID_WIDTH * GRID_HEIGHT)
#define MODEL_MAX_SPRINGS (MODEL_MAX_OBJECTS * 2)

#define DEFAULT_SPRING_K  5.0
#define DEFAULT_FRICTION  1.4

struct Spring {
  Object *a;
  Object *b;
  /* Spring position at rest, from a to b:
     offset = b.position - a.position
  */
  Vector offset;
};

struct Object {
  Vector force;
  
  Point position;
  Vector velocity;
  
  double mass;
  double theta;
  
  int immobile;
};

struct Model {
  int num_objects;
  Object objects[MODEL_MAX_OBJECTS];
  
  int num_springs;
  Spring springs[MODEL_MAX_SPRINGS];
  
  Object *anchor_object;
  Vector anchor_offset;
  
  double friction;/* Friction constant */
  double k;/* Spring constant */
  
  double last_time;
  double steps;
};

static void
object_init (Object *object,
	     double position_x, double position_y,
	     double velocity_x, double velocity_y, double mass)
{
  object->position.x = position_x;
  object->position.y = position_y;
  
  object->velocity.x = velocity_x;
  object->velocity.y = velocity_y;
  
  object->mass = mass;
  
  object->force.x = 0;
  object->force.y = 0;
  
  object->immobile = 0;
}

static void
spring_init (Spring *spring,
	     Object *object_a, Object *object_b,
	     double offset_x, double offset_y)
{
  spring->a = object_a;
  spring->b = object_b;
  spring->offset.x = offset_x;
  spring->offset.y = offset_y;
}

static void
model_add_spring (Model *model,
		  Object *object_a, Object *object_b,
		  double offset_x, double offset_y)
{
  Spring *spring;
  
  g_assert (model->num_springs < MODEL_MAX_SPRINGS);
  
  spring = &model->springs[model->num_springs];
  model->num_springs++;
  
  spring_init (spring, object_a, object_b, offset_x, offset_y);
}

static void
object_apply_force (Object *object, double fx, double fy)
{
  object->force.x += fx;
  object->force.y += fy;
}

/* The model here can be understood as a rigid body of the spring's
 * rest shape, centered on the vector between the two object
 * positions. This rigid body is then connected by linear-force
 * springs to each object. This model does degnerate into a simple
 * spring for linear displacements, and does something reasonable for
 * rotation.
 *
 * There are other possibilities for handling the rotation of the
 * spring, and it might be interesting to explore something which has
 * better length-preserving properties. For example, with the current
 * model, an initial 180 degree rotation of the spring results in the
 * spring collapsing down to 0 size before expanding back to it's
 * natural size again.
 */

static void
spring_exert_forces (Spring *spring, double k)
{
  Vector da, db;
  Vector a, b;
  
  a = spring->a->position;
  b = spring->b->position;
  
  /* A nice vector diagram would likely help here, but my ASCII-art
   * skills aren't up to the task. Here's how to make your own
   * diagram:
   *
   * Draw a and b, and the vector AB from a to b
   * Find the center of AB
   * Draw spring->offset so that its center point is on the center of AB
   * Draw da from a to the initial point of spring->offset
   * Draw db from b to the final point of spring->offset
   *
   * The math below should be easy to verify from the diagram.
   */
  
  da.x = 0.5 * (b.x - a.x - spring->offset.x);
  da.y = 0.5 * (b.y - a.y - spring->offset.y);
  
  db.x = 0.5 * (a.x - b.x + spring->offset.x);
  db.y = 0.5 * (a.y - b.y + spring->offset.y);
  
  object_apply_force (spring->a, k * da.x, k * da.y);
  object_apply_force (spring->b, k * db.x, k * db.y);
}

static void
model_step_object (Model *model, Object *object)
{
  Vector acceleration;
  
  object->theta += 0.05;
  
  /* Slow down due to friction. */
  object->force.x -= model->friction * object->velocity.x;
  object->force.y -= model->friction * object->velocity.y;
  
  acceleration.x = object->force.x / object->mass;
  acceleration.y = object->force.y / object->mass;
  
  if (object->immobile)
    {
      object->velocity.x = 0;
      object->velocity.y = 0;
    }
  else
    {
      object->velocity.x += acceleration.x;
      object->velocity.y += acceleration.y;
      
      object->position.x += object->velocity.x;
      object->position.y += object->velocity.y;
    }
  
  object->force.x = 0.0;
  object->force.y = 0.0;
}

static void
model_init_grid (Model *model, MetaRectangle *rect, gboolean expand)
{
  int x, y, i, v_x, v_y;
  int hpad, vpad;
  
  model->num_objects = MODEL_MAX_OBJECTS;
  
  model->num_springs = 0;
  
  i = 0;
  if (expand)
    {
      hpad = rect->width / 3;
      vpad = rect->height / 3;
    }
  else
    {
      hpad = rect->width / 6;
      vpad = rect->height / 6;
    }

#define EXPAND_DELTA 4
  
  for (y = 0; y < GRID_HEIGHT; y++)
    for (x = 0; x < GRID_WIDTH; x++)
      {
	if (expand)
	  {
	    if (y == 0)
	      v_y = - EXPAND_DELTA * g_random_double();
	    else if (y == GRID_HEIGHT - 1)
	      v_y = EXPAND_DELTA * g_random_double();
	    else
	      v_y = 2 * EXPAND_DELTA * g_random_double() - EXPAND_DELTA;

	    if (x == 0)
	      v_x = - EXPAND_DELTA * g_random_double();
	    else if (x == GRID_WIDTH - 1)
	      v_x = EXPAND_DELTA * g_random_double();
	    else
	      v_x = 2 * EXPAND_DELTA * g_random_double() - EXPAND_DELTA;
	  }
	else
	  {
	    v_x = v_y = 0;
	  }
	
#if 0
	if (expand)
	  object_init (&model->objects[i],
		       rect->x + x * rect->width / 6 + rect->width / 4,
		       rect->y + y * rect->height / 6 + rect->height / 4,
		       v_x, v_y, 20);
	else
#endif
	{
#if 0
	    g_print ("obj: %d %d\n", rect->x + x * rect->width / 3,
		     rect->y + y * rect->height / 3);
#endif
	    object_init (&model->objects[i],
		       rect->x + x * rect->width / 3,
		       rect->y + y * rect->height / 3,
		       v_x, v_y, 15);
	}	
	
	if (x > 0)
	  model_add_spring (model,
			    &model->objects[i - 1],
			    &model->objects[i],
			    hpad, 0);
	
	if (y > 0)
	  model_add_spring (model,
			    &model->objects[i - GRID_WIDTH],
			    &model->objects[i],
			    0, vpad);
	
	i++;
      }
}

static void
model_init (Model *model, MetaRectangle *rect, gboolean expand)
{
  model->anchor_object = NULL;
  
  model->k        = DEFAULT_SPRING_K;
  model->friction = DEFAULT_FRICTION;
  
  model_init_grid (model, rect, expand);
  model->steps = 0;
  model->last_time = 0;
}

Model *
model_new (MetaRectangle *rect, gboolean expand)
{
  Model *model = g_new0 (Model, 1);
  
  model_init (model, rect, expand);
  
  return model;
}

static double
object_distance (Object *object, double x, double y)
{
  double dx, dy;

  dx = object->position.x - x;
  dy = object->position.y - y;

  return sqrt (dx*dx + dy*dy);
}

static Object *
model_find_nearest (Model *model, double x, double y)
{
  Object *object = &model->objects[0];
  double distance, min_distance = 0.0;
  int i;

  for (i = 0; i < model->num_objects; i++) {
    distance = object_distance (&model->objects[i], x, y);
    if (i == 0 || distance < min_distance) {
      min_distance = distance;
      object = &model->objects[i];
    }
  }

  return object;
}

void
model_begin_move (Model *model, int x, int y)
{
  if (model->anchor_object)
    model->anchor_object->immobile = 0;

  model->anchor_object = model_find_nearest (model, x, y);
  
  model->anchor_offset.x = x - model->anchor_object->position.x;
  model->anchor_offset.y = y - model->anchor_object->position.y;

  g_print ("ypos: %f %f\n", model->anchor_object->position.y,
	   model->anchor_object->position.x);
  
  g_print ("anchor offset: %f %f\n",
	   model->anchor_offset.x,
	   model->anchor_offset.y);
  
  model->anchor_object->immobile = 1;
}

void
model_set_anchor (Model *model,
		  int	 x,
		  int    y)
{
  if (model->anchor_object)
    model->anchor_object->immobile = 0;

  model->anchor_object = model_find_nearest (model, x, y);
  model->anchor_offset.x = model->anchor_object->position.x - x;
  model->anchor_offset.y = model->anchor_object->position.y - y;

  model->anchor_object->immobile = 1;
}

void
model_update_move (Model *model, int x, int y)
{
  model->anchor_object->position.x = x - model->anchor_offset.x;
  model->anchor_object->position.y = y - model->anchor_offset.y;
}

#define EPSILON   0.02

gboolean
model_is_calm (Model *model)
{
    int i;

    for (i = 0; i < model->num_objects; i++)
    {
	if (model->objects[i].velocity.x > EPSILON	||
	    model->objects[i].velocity.y > EPSILON	||
	    model->objects[i].velocity.x < - EPSILON	||
	    model->objects[i].velocity.y < - EPSILON)
	{
	    return FALSE;
	}
    }

    return TRUE;
}

void
model_step (Model *model)
{
  int i;
  
  for (i = 0; i < model->num_springs; i++)
    spring_exert_forces (&model->springs[i], model->k);
  
  for (i = 0; i < model->num_objects; i++)
    model_step_object (model, &model->objects[i]);
}

void
model_destroy (Model *model)
{
  g_free (model);
}

void
model_get_position (Model *model,
		    int	   i,
		    int    j,
		    double *x,
		    double *y)
{
  if (x)
    *x = model->objects[j * 4 + i].position.x;

  if (y)
    *y = model->objects[j * 4 + i].position.y;
}
