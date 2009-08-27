/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-stack
 * @short_description: Pure "Z-axis" container class
 *
 * A #ShellStack draws its children on top of each other,
 * aligned to the top left.  It will be sized in width/height
 * according to the largest such dimension of its children, and
 * all children will be allocated that size.  This differs
 * from #ClutterGroup which allocates its children their natural
 * size, even if that would overflow the size allocated to the stack.
 */

#include "shell-stack.h"

G_DEFINE_TYPE (ShellStack,
               shell_stack,
               CLUTTER_TYPE_GROUP);

static void
shell_stack_allocate (ClutterActor           *self,
                      const ClutterActorBox  *box,
                      ClutterAllocationFlags  flags)
{
  GList *children, *iter;
  float width, height;

  width = box->x2 - box->x1;
  height = box->y2 - box->y1;

  /* Chain up directly to ClutterActor to set actor->allocation.  We explicitly skip our parent class
   * ClutterGroup here because we want to override the allocate function. */
  (CLUTTER_ACTOR_CLASS (g_type_class_peek (clutter_actor_get_type ())))->allocate (self, box, flags);

  children = clutter_container_get_children (CLUTTER_CONTAINER (self));
  for (iter = children; iter; iter = iter->next)
    {
      ClutterActor *actor = CLUTTER_ACTOR (iter->data);
      ClutterActorBox child_box;
      child_box.x1 = 0;
      child_box.x2 = width;
      child_box.y1 = 0;
      child_box.y2 = height;
      clutter_actor_allocate (actor, &child_box, flags);
    }
  g_list_free (children);
}

static void
shell_stack_get_preferred_height (ClutterActor *actor,
                                  gfloat for_width,
                                  gfloat *min_height_p,
                                  gfloat *natural_height_p)
{
  ShellStack *stack = SHELL_STACK (actor);
  gboolean first = TRUE;
  float min = 0, natural = 0;
  GList *children;
  GList *iter;

  children = clutter_container_get_children (CLUTTER_CONTAINER (stack));

  for (iter = children; iter; iter = iter->next)
    {
      ClutterActor *child = iter->data;
      float child_min, child_natural;

      clutter_actor_get_preferred_height (child,
                                          for_width,
                                          &child_min,
                                          &child_natural);

      if (first)
        {
          first = FALSE;
          min = child_min;
          natural = child_natural;
        }
      else
        {
          if (child_min > min)
            min = child_min;

          if (child_natural > natural)
            natural = child_natural;
        }
    }

  if (min_height_p)
    *min_height_p = min;

  if (natural_height_p)
    *natural_height_p = natural;

  g_list_free (children);
}

static void
shell_stack_get_preferred_width (ClutterActor *actor,
                                 gfloat for_height,
                                 gfloat *min_width_p,
                                 gfloat *natural_width_p)
{
  ShellStack *stack = SHELL_STACK (actor);
  gboolean first = TRUE;
  float min = 0, natural = 0;
  GList *iter;
  GList *children;

  children = clutter_container_get_children (CLUTTER_CONTAINER (stack));

  for (iter = children; iter; iter = iter->next)
    {
      ClutterActor *child = iter->data;
      float child_min, child_natural;

      clutter_actor_get_preferred_width (child,
                                         for_height,
                                         &child_min,
                                         &child_natural);

      if (first)
        {
          first = FALSE;
          min = child_min;
          natural = child_natural;
        }
      else
        {
          if (child_min > min)
            min = child_min;

          if (child_natural > natural)
            natural = child_natural;
        }
    }

  if (min_width_p)
    *min_width_p = min;

  if (natural_width_p)
    *natural_width_p = natural;
  g_list_free (children);
}

static void
shell_stack_class_init (ShellStackClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->get_preferred_width = shell_stack_get_preferred_width;
  actor_class->get_preferred_height = shell_stack_get_preferred_height;
  actor_class->allocate = shell_stack_allocate;
}

static void
shell_stack_init (ShellStack *actor)
{
}
