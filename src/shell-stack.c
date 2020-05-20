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

#include "config.h"

#include "shell-stack.h"

struct _ShellStack
{
    StWidget parent;
};

G_DEFINE_TYPE (ShellStack, shell_stack, ST_TYPE_WIDGET);

static void
shell_stack_allocate (ClutterActor           *self,
                      const ClutterActorBox  *box)
{
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));
  ClutterActorBox content_box;
  ClutterActor *child;

  clutter_actor_set_allocation (self, box);

  st_theme_node_get_content_box (theme_node, box, &content_box);

  for (child = clutter_actor_get_first_child (self);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      ClutterActorBox child_box = content_box;
      clutter_actor_allocate (child, &child_box);
    }
}

static void
shell_stack_get_preferred_height (ClutterActor *actor,
                                  gfloat for_width,
                                  gfloat *min_height_p,
                                  gfloat *natural_height_p)
{
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  gboolean first = TRUE;
  float min = 0, natural = 0;
  ClutterActor *child;

  st_theme_node_adjust_for_width (theme_node, &for_width);

  for (child = clutter_actor_get_first_child (actor);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
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

  st_theme_node_adjust_preferred_height (theme_node, min_height_p, natural_height_p);
}

static void
shell_stack_get_preferred_width (ClutterActor *actor,
                                 gfloat for_height,
                                 gfloat *min_width_p,
                                 gfloat *natural_width_p)
{
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  gboolean first = TRUE;
  float min = 0, natural = 0;
  ClutterActor *child;

  st_theme_node_adjust_for_height (theme_node, &for_height);

  for (child = clutter_actor_get_first_child (actor);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
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

  st_theme_node_adjust_preferred_width (theme_node, min_width_p, natural_width_p);
}

static gboolean
shell_stack_navigate_focus (StWidget         *widget,
                            ClutterActor     *from,
                            StDirectionType   direction)
{
  ClutterActor *top_actor;

  /* If the stack is itself focusable, then focus into or out of
   * it, as appropriate.
   */
  if (st_widget_get_can_focus (widget))
    {
      if (from && clutter_actor_contains (CLUTTER_ACTOR (widget), from))
        return FALSE;

      if (clutter_actor_is_mapped (CLUTTER_ACTOR (widget)))
        {
          clutter_actor_grab_key_focus (CLUTTER_ACTOR (widget));
          return TRUE;
        }
      else
        {
          return FALSE;
        }
    }

  top_actor = clutter_actor_get_last_child (CLUTTER_ACTOR (widget));
  while (top_actor && !clutter_actor_is_visible (top_actor))
    top_actor = clutter_actor_get_previous_sibling (top_actor);
  if (ST_IS_WIDGET (top_actor))
    return st_widget_navigate_focus (ST_WIDGET (top_actor), from, direction, FALSE);
  else
    return FALSE;
}

static void
shell_stack_class_init (ShellStackClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  actor_class->get_preferred_width = shell_stack_get_preferred_width;
  actor_class->get_preferred_height = shell_stack_get_preferred_height;
  actor_class->allocate = shell_stack_allocate;

  widget_class->navigate_focus = shell_stack_navigate_focus;
}

static void
shell_stack_init (ShellStack *actor)
{
}
