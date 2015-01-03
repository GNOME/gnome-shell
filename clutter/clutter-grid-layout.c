/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2012 Bastian Winkler <buz@netbuz.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Bastian Winkler <buz@netbuz.org>
 *
 * Based on GtkGrid widget by:
 *   Matthias Clasen
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>

#include "clutter-grid-layout.h"

#include "clutter-actor-private.h"
#include "clutter-container.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-layout-meta.h"
#include "clutter-private.h"

/**
 * SECTION:clutter-grid-layout
 * @Short_description: A layout manager for a grid of actors
 * @Title: ClutterGridLayout
 * @See_also: #ClutterTableLayout, #ClutterBoxLayout
 *
 * #ClutterGridLayout is a layout manager which arranges its child widgets in
 * rows and columns. It is a very similar to #ClutterTableLayout and
 * #ClutterBoxLayout, but it consistently uses #ClutterActor's
 * alignment and expansion flags instead of custom child properties.
 *
 * Children are added using clutter_grid_layout_attach(). They can span
 * multiple rows or columns. It is also possible to add a child next to an
 * existing child, using clutter_grid_layout_attach_next_to(). The behaviour of
 * #ClutterGridLayout when several children occupy the same grid cell is undefined.
 *
 * #ClutterGridLayout can be used like a #ClutterBoxLayout by just using
 * clutter_actor_add_child(), which will place children next to each other in
 * the direction determined by the #ClutterGridLayout:orientation property.
 */

#define CLUTTER_TYPE_GRID_CHILD          (clutter_grid_child_get_type ())
#define CLUTTER_GRID_CHILD(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_GRID_CHILD, ClutterGridChild))
#define CLUTTER_IS_GRID_CHILD(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_GRID_CHILD))

typedef struct _ClutterGridChild        ClutterGridChild;
typedef struct _ClutterLayoutMetaClass  ClutterGridChildClass;

typedef struct _ClutterGridAttach       ClutterGridAttach;
typedef struct _ClutterGridLine         ClutterGridLine;
typedef struct _ClutterGridLines        ClutterGridLines;
typedef struct _ClutterGridLineData     ClutterGridLineData;
typedef struct _ClutterGridRequest      ClutterGridRequest;


struct _ClutterGridAttach
{
  gint pos;
  gint span;
};

struct _ClutterGridChild
{
  ClutterLayoutMeta parent_instance;

  ClutterGridAttach attach[2];
};

#define CHILD_LEFT(child)    ((child)->attach[CLUTTER_ORIENTATION_HORIZONTAL].pos)
#define CHILD_WIDTH(child)   ((child)->attach[CLUTTER_ORIENTATION_HORIZONTAL].span)
#define CHILD_TOP(child)     ((child)->attach[CLUTTER_ORIENTATION_VERTICAL].pos)
#define CHILD_HEIGHT(child)  ((child)->attach[CLUTTER_ORIENTATION_VERTICAL].span)

/* A ClutterGridLineData struct contains row/column specific parts
 * of the grid.
 */
struct _ClutterGridLineData
{
  gfloat spacing;
  guint homogeneous : 1;
};

struct _ClutterGridLayoutPrivate
{
  ClutterContainer *container;
  ClutterOrientation orientation;

  ClutterGridLineData linedata[2];
};

#define ROWS(priv)    (&(priv)->linedata[CLUTTER_ORIENTATION_HORIZONTAL])
#define COLUMNS(priv) (&(priv)->linedata[CLUTTER_ORIENTATION_VERTICAL])

/* A ClutterGridLine struct represents a single row or column
 * during size requests
 */
struct _ClutterGridLine
{
  gfloat minimum;
  gfloat natural;
  gfloat position;
  gfloat allocation;

  guint need_expand : 1;
  guint expand      : 1;
  guint empty       : 1;
};

struct _ClutterGridLines
{
  ClutterGridLine *lines;
  gint min, max;
};

struct _ClutterGridRequest
{
  ClutterGridLayout *grid;
  ClutterGridLines lines[2];
};

enum
{
  PROP_0,

  PROP_ORIENTATION,
  PROP_ROW_SPACING,
  PROP_COLUMN_SPACING,
  PROP_ROW_HOMOGENEOUS,
  PROP_COLUMN_HOMOGENEOUS,

  PROP_LAST
};
static GParamSpec *obj_props[PROP_LAST];

enum
{
  PROP_CHILD_0,

  PROP_CHILD_LEFT_ATTACH,
  PROP_CHILD_TOP_ATTACH,
  PROP_CHILD_WIDTH,
  PROP_CHILD_HEIGHT,

  PROP_CHILD_LAST
};
static GParamSpec *child_props[PROP_CHILD_LAST];

GType clutter_grid_child_get_type (void);

G_DEFINE_TYPE (ClutterGridChild, clutter_grid_child,
               CLUTTER_TYPE_LAYOUT_META)

G_DEFINE_TYPE_WITH_PRIVATE (ClutterGridLayout,
                            clutter_grid_layout,
                            CLUTTER_TYPE_LAYOUT_MANAGER)


#define GET_GRID_CHILD(grid, child) \
  (CLUTTER_GRID_CHILD(clutter_layout_manager_get_child_meta \
   (CLUTTER_LAYOUT_MANAGER((grid)),\
    CLUTTER_GRID_LAYOUT((grid))->priv->container,(child))))

static void
grid_attach (ClutterGridLayout *self,
             ClutterActor      *actor,
             gint               left,
             gint               top,
             gint               width,
             gint               height)
{
  ClutterGridChild *grid_child;

  grid_child = GET_GRID_CHILD (self, actor);

  CHILD_LEFT (grid_child) = left;
  CHILD_TOP (grid_child) = top;
  CHILD_WIDTH (grid_child) = width;
  CHILD_HEIGHT (grid_child) = height;
}

/* Find the position 'touching' existing
 * children. @orientation and @max determine
 * from which direction to approach (horizontal
 * + max = right, vertical + !max = top, etc).
 * @op_pos, @op_span determine the rows/columns
 * in which the touching has to happen.
 */
static gint
find_attach_position (ClutterGridLayout  *self,
                      ClutterOrientation  orientation,
                      gint                op_pos,
                      gint                op_span,
                      gboolean            max)
{
  ClutterGridLayoutPrivate *priv = self->priv;
  ClutterGridChild *grid_child;
  ClutterGridAttach *attach;
  ClutterGridAttach *opposite;
  ClutterActorIter iter;
  ClutterActor *child;
  gint pos;
  gboolean hit;

  if (max)
    pos = -G_MAXINT;
  else
    pos = G_MAXINT;

  hit = FALSE;

  if (!priv->container)
    return -1;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      grid_child = GET_GRID_CHILD (self, child);

      attach = &grid_child->attach[orientation];
      opposite = &grid_child->attach[1 - orientation];

      /* check if the ranges overlap */
      if (opposite->pos <= op_pos + op_span && op_pos <= opposite->pos + opposite->span)
        {
          hit = TRUE;

          if (max)
            pos = MAX (pos, attach->pos + attach->span);
          else
            pos = MIN (pos, attach->pos);
        }
     }

  if (!hit)
    pos = 0;

  return pos;
}
static void
grid_attach_next_to (ClutterGridLayout   *layout,
                     ClutterActor        *child,
                     ClutterActor        *sibling,
                     ClutterGridPosition  side,
                     gint                 width,
                     gint                 height)
{
  ClutterGridChild *grid_sibling;
  gint left, top;

  if (sibling)
    {
      grid_sibling = GET_GRID_CHILD (layout, sibling);

      switch (side)
        {
        case CLUTTER_GRID_POSITION_LEFT:
          left = CHILD_LEFT (grid_sibling) - width;
          top = CHILD_TOP (grid_sibling);
          break;

        case CLUTTER_GRID_POSITION_RIGHT:
          left = CHILD_LEFT (grid_sibling) + CHILD_WIDTH (grid_sibling);
          top = CHILD_TOP (grid_sibling);
          break;

        case CLUTTER_GRID_POSITION_TOP:
          left = CHILD_LEFT (grid_sibling);
          top = CHILD_TOP (grid_sibling) - height;
          break;

        case CLUTTER_GRID_POSITION_BOTTOM:
          left = CHILD_LEFT (grid_sibling);
          top = CHILD_TOP (grid_sibling) + CHILD_HEIGHT (grid_sibling);
          break;

        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      switch (side)
        {
        case CLUTTER_GRID_POSITION_LEFT:
          left = find_attach_position (layout, CLUTTER_ORIENTATION_HORIZONTAL,
                                       0, height, FALSE);
          left -= width;
          top = 0;
          break;

        case CLUTTER_GRID_POSITION_RIGHT:
          left = find_attach_position (layout, CLUTTER_ORIENTATION_HORIZONTAL,
                                       0, height, TRUE);
          top = 0;
          break;

        case CLUTTER_GRID_POSITION_TOP:
          left = 0;
          top = find_attach_position (layout, CLUTTER_ORIENTATION_VERTICAL,
                                      0, width, FALSE);
          top -= height;
          break;

        case CLUTTER_GRID_POSITION_BOTTOM:
          left = 0;
          top = find_attach_position (layout, CLUTTER_ORIENTATION_VERTICAL,
                                      0, width, TRUE);
          break;

        default:
          g_assert_not_reached ();
        }
    }

  grid_attach (layout, child, left, top, width, height);
}

static void
clutter_grid_request_update_child_attach (ClutterGridRequest *request,
                                          ClutterActor       *actor)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridChild *grid_child;

  grid_child = GET_GRID_CHILD (request->grid, actor);

  if (CHILD_LEFT (grid_child) == -1 || CHILD_TOP (grid_child) == -1)
    {
      ClutterGridPosition side;
      ClutterActor *sibling;

      if (priv->orientation == CLUTTER_ORIENTATION_HORIZONTAL)
        {
          ClutterTextDirection td;
          gboolean rtl;
          ClutterActor *container = CLUTTER_ACTOR (priv->container);

          td = clutter_actor_get_text_direction (container);
          rtl = (td == CLUTTER_TEXT_DIRECTION_RTL) ? TRUE : FALSE;
          side = rtl ? CLUTTER_GRID_POSITION_LEFT : CLUTTER_GRID_POSITION_RIGHT;
        }
      else
        {
          /* XXX: maybe we should also add a :pack-start property to modify
           * this */
          side = CLUTTER_GRID_POSITION_BOTTOM;
        }

      sibling = clutter_actor_get_previous_sibling (actor);
      if (sibling)
        clutter_grid_layout_insert_next_to (request->grid, sibling, side);
      grid_attach_next_to (request->grid, actor, sibling, side,
                           CHILD_WIDTH (grid_child),
                           CHILD_HEIGHT (grid_child));
    }
}

static void
clutter_grid_request_update_attach (ClutterGridRequest *request)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterActorIter iter;
  ClutterActor *child;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    clutter_grid_request_update_child_attach (request, child);
}

/* Calculates the min and max numbers for both orientations.
 */
static void
clutter_grid_request_count_lines (ClutterGridRequest *request)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridChild *grid_child;
  ClutterGridAttach *attach;
  ClutterActorIter iter;
  ClutterActor *child;
  gint min[2];
  gint max[2];

  min[0] = min[1] = G_MAXINT;
  max[0] = max[1] = G_MININT;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      grid_child = GET_GRID_CHILD (request->grid, child);
      attach = grid_child->attach;

      min[0] = MIN (min[0], attach[0].pos);
      max[0] = MAX (max[0], attach[0].pos + attach[0].span);
      min[1] = MIN (min[1], attach[1].pos);
      max[1] = MAX (max[1], attach[1].pos + attach[1].span);
    }

  request->lines[0].min = min[0];
  request->lines[0].max = max[0];
  request->lines[1].min = min[1];
  request->lines[1].max = max[1];
}

/* Sets line sizes to 0 and marks lines as expand
 * if they have a non-spanning expanding child.
 */
static void
clutter_grid_request_init (ClutterGridRequest *request,
                           ClutterOrientation  orientation)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridChild *grid_child;
  ClutterGridAttach *attach;
  ClutterGridLines *lines;
  ClutterActorIter iter;
  ClutterActor *child;
  gint i;

  lines = &request->lines[orientation];

  for (i = 0; i < lines->max - lines->min; i++)
    {
      lines->lines[i].minimum = 0;
      lines->lines[i].natural = 0;
      lines->lines[i].expand = FALSE;
    }

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      grid_child = GET_GRID_CHILD (request->grid, child);
      attach = &grid_child->attach[orientation];
      if (attach->span == 1 && clutter_actor_needs_expand (child, orientation))
        lines->lines[attach->pos - lines->min].expand = TRUE;
    }
}

/* Sums allocations for lines spanned by child and their spacing.
 */
static gfloat
compute_allocation_for_child (ClutterGridRequest *request,
                              ClutterActor       *child,
                              ClutterOrientation  orientation)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridChild *grid_child;
  ClutterGridLineData *linedata;
  ClutterGridLines *lines;
  ClutterGridLine *line;
  ClutterGridAttach *attach;
  gfloat size;
  gint i;

  grid_child = GET_GRID_CHILD (request->grid, child);
  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];
  attach = &grid_child->attach[orientation];

  size = (attach->span - 1) * linedata->spacing;
  for (i = 0; i < attach->span; i++)
    {
      line = &lines->lines[attach->pos - lines->min + i];
      size += line->allocation;
    }

  return size;
}

static void
compute_request_for_child (ClutterGridRequest *request,
                           ClutterActor       *child,
                           ClutterOrientation  orientation,
                           gboolean            contextual,
                           gfloat             *minimum,
                           gfloat             *natural)
{
  if (contextual)
    {
      gfloat size;

      size = compute_allocation_for_child (request, child, 1 - orientation);
      if (orientation == CLUTTER_ORIENTATION_HORIZONTAL)
        clutter_actor_get_preferred_width (child, size, minimum, natural);
      else
        clutter_actor_get_preferred_height (child, size, minimum, natural);
    }
  else
    {
      if (orientation == CLUTTER_ORIENTATION_HORIZONTAL)
        clutter_actor_get_preferred_width (child, -1, minimum, natural);
      else
        clutter_actor_get_preferred_height (child, -1, minimum, natural);
    }
}

/* Sets requisition to max. of non-spanning children.
 * If contextual is TRUE, requires allocations of
 * lines in the opposite orientation to be set.
 */
static void
clutter_grid_request_non_spanning (ClutterGridRequest *request,
                                   ClutterOrientation  orientation,
                                   gboolean            contextual)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridChild *grid_child;
  ClutterGridAttach *attach;
  ClutterGridLines *lines;
  ClutterGridLine *line;
  ClutterActorIter iter;
  ClutterActor *child;
  gfloat minimum;
  gfloat natural;

  lines = &request->lines[orientation];

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      grid_child = GET_GRID_CHILD (request->grid, child);

      attach = &grid_child->attach[orientation];
      if (attach->span != 1)
        continue;

      compute_request_for_child (request, child, orientation, contextual, &minimum, &natural);

      line = &lines->lines[attach->pos - lines->min];
      line->minimum = MAX (line->minimum, minimum);
      line->natural = MAX (line->natural, natural);
    }
}

/* Enforce homogeneous sizes.
 */
static void
clutter_grid_request_homogeneous (ClutterGridRequest *request,
                                  ClutterOrientation  orientation)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridLineData *linedata;
  ClutterGridLines *lines;
  gfloat minimum, natural;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  if (!linedata->homogeneous)
    return;

  minimum = 0.0f;
  natural = 0.0f;

  for (i = 0; i < lines->max - lines->min; i++)
    {
      minimum = MAX (minimum, lines->lines[i].minimum);
      natural = MAX (natural, lines->lines[i].natural);
    }

  for (i = 0; i < lines->max - lines->min; i++)
    {
      lines->lines[i].minimum = minimum;
      lines->lines[i].natural = natural;
    }
}

/* Deals with spanning children.
 * Requires expand fields of lines to be set for
 * non-spanning children.
 */
static void
clutter_grid_request_spanning (ClutterGridRequest *request,
                               ClutterOrientation  orientation,
                               gboolean            contextual)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridChild *grid_child;
  ClutterActor *child;
  ClutterActorIter iter;
  ClutterGridAttach *attach;
  ClutterGridLineData *linedata;
  ClutterGridLines *lines;
  ClutterGridLine *line;
  gfloat minimum;
  gfloat natural;
  gint span_minimum;
  gint span_natural;
  gint span_expand;
  gboolean force_expand;
  gint extra;
  gint expand;
  gint line_extra;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      grid_child = GET_GRID_CHILD (request->grid, child);

      attach = &grid_child->attach[orientation];
      if (attach->span == 1)
        continue;

      compute_request_for_child (request, child, orientation, contextual,
                                 &minimum, &natural);

      span_minimum = (attach->span - 1) * linedata->spacing;
      span_natural = (attach->span - 1) * linedata->spacing;
      span_expand = 0;
      force_expand = FALSE;
      for (i = 0; i < attach->span; i++)
        {
          line = &lines->lines[attach->pos - lines->min + i];
          span_minimum += line->minimum;
          span_natural += line->natural;
          if (line->expand)
            span_expand += 1;
        }
      if (span_expand == 0)
        {
          span_expand = attach->span;
          force_expand = TRUE;
        }

      /* If we need to request more space for this child to fill
       * its requisition, then divide up the needed space amongst the
       * lines it spans, favoring expandable lines if any.
       *
       * When doing homogeneous allocation though, try to keep the
       * line allocations even, since we're going to force them to
       * be the same anyway, and we don't want to introduce unnecessary
       * extra space.
       */
      if (span_minimum < minimum)
        {
          if (linedata->homogeneous)
            {
              gint total, m;

              total = minimum - (attach->span - 1) * linedata->spacing;
              m = total / attach->span + (total % attach->span ? 1 : 0);
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  line->minimum = MAX(line->minimum, m);
                }
            }
          else
            {
              extra = minimum - span_minimum;
              expand = span_expand;
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  if (force_expand || line->expand)
                    {
                      line_extra = extra / expand;
                      line->minimum += line_extra;
                      extra -= line_extra;
                      expand -= 1;
                    }
                }
            }
        }

      if (span_natural < natural)
        {
          if (linedata->homogeneous)
            {
              gint total, n;

              total = natural - (attach->span - 1) * linedata->spacing;
              n = total / attach->span + (total % attach->span ? 1 : 0);
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  line->natural = MAX(line->natural, n);
                }
            }
          else
            {
              extra = natural - span_natural;
              expand = span_expand;
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  if (force_expand || line->expand)
                    {
                      line_extra = extra / expand;
                      line->natural += line_extra;
                      extra -= line_extra;
                      expand -= 1;
                    }
                }
            }
        }
    }
}

/* Marks empty and expanding lines and counts them.
 */
static void
clutter_grid_request_compute_expand (ClutterGridRequest *request,
                                     ClutterOrientation  orientation,
                                     gint               *nonempty_lines,
                                     gint               *expand_lines)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridChild *grid_child;
  ClutterGridAttach *attach;
  ClutterActorIter iter;
  ClutterActor *child;
  gint i;
  ClutterGridLines *lines;
  ClutterGridLine *line;
  gboolean has_expand;
  gint expand;
  gint empty;

  lines = &request->lines[orientation];

  for (i = 0; i < lines->max - lines->min; i++)
    {
      lines->lines[i].need_expand = FALSE;
      lines->lines[i].expand = FALSE;
      lines->lines[i].empty = TRUE;
    }

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      grid_child = GET_GRID_CHILD (request->grid, child);

      attach = &grid_child->attach[orientation];
      if (attach->span != 1)
        continue;

      line = &lines->lines[attach->pos - lines->min];
      line->empty = FALSE;
      if (clutter_actor_needs_expand (child, orientation))
        line->expand = TRUE;
    }


  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      grid_child = GET_GRID_CHILD (request->grid, child);

      attach = &grid_child->attach[orientation];
      if (attach->span == 1)
        continue;

      has_expand = FALSE;
      for (i = 0; i < attach->span; i++)
        {
          line = &lines->lines[attach->pos - lines->min + i];
          line->empty = FALSE;
          if (line->expand)
            has_expand = TRUE;
        }

      if (!has_expand && clutter_actor_needs_expand (child, orientation))
        {
          for (i = 0; i < attach->span; i++)
            {
              line = &lines->lines[attach->pos - lines->min + i];
              line->need_expand = TRUE;
            }
        }
    }

  empty = 0;
  expand = 0;
  for (i = 0; i < lines->max - lines->min; i++)
    {
      line = &lines->lines[i];

      if (line->need_expand)
        line->expand = TRUE;

      if (line->empty)
        empty += 1;

      if (line->expand)
        expand += 1;
    }

  if (nonempty_lines)
    *nonempty_lines = lines->max - lines->min - empty;

  if (expand_lines)
    *expand_lines = expand;
}

/* Sums the minimum and natural fields of lines and their spacing.
 */
static void
clutter_grid_request_sum (ClutterGridRequest *request,
                          ClutterOrientation  orientation,
                          gfloat             *minimum,
                          gfloat             *natural)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridLineData *linedata;
  ClutterGridLines *lines;
  gint i;
  gfloat min, nat;
  gint nonempty;

  clutter_grid_request_compute_expand (request, orientation, &nonempty, NULL);

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  min = 0;
  nat = 0;
  if (nonempty > 0)
    {
      min = (nonempty - 1) * linedata->spacing;
      nat = (nonempty - 1) * linedata->spacing;
    }

  for (i = 0; i < lines->max - lines->min; i++)
    {
      min += lines->lines[i].minimum;
      nat += lines->lines[i].natural;
    }

  if (minimum)
    *minimum = min;

  if (natural)
    *natural = nat;
}

/* Computes minimum and natural fields of lines.
 * When contextual is TRUE, requires allocation of
 * lines in the opposite orientation to be set.
 */
static void
clutter_grid_request_run (ClutterGridRequest *request,
                          ClutterOrientation  orientation,
                          gboolean            contextual)
{
  clutter_grid_request_init (request, orientation);
  clutter_grid_request_non_spanning (request, orientation, contextual);
  clutter_grid_request_homogeneous (request, orientation);
  clutter_grid_request_spanning (request, orientation, contextual);
  clutter_grid_request_homogeneous (request, orientation);
}

typedef struct _RequestedSize
{
  gpointer data;

  gfloat minimum_size;
  gfloat natural_size;
} RequestedSize;


/* Pulled from gtksizerequest.c from Gtk+ */
static gint
compare_gap (gconstpointer p1,
             gconstpointer p2,
             gpointer      data)
{
  RequestedSize *sizes = data;
  const guint *c1 = p1;
  const guint *c2 = p2;

  const gint d1 = MAX (sizes[*c1].natural_size -
                       sizes[*c1].minimum_size,
                       0);
  const gint d2 = MAX (sizes[*c2].natural_size -
                       sizes[*c2].minimum_size,
                       0);

  gint delta = (d2 - d1);

  if (0 == delta)
    delta = (*c2 - *c1);

  return delta;
}

/*
 * distribute_natural_allocation:
 * @extra_space: Extra space to redistribute among children after subtracting
 *   minimum sizes and any child padding from the overall allocation
 * @n_requested_sizes: Number of requests to fit into the allocation
 * @sizes: An array of structs with a client pointer and a minimum/natural size
 *   in the orientation of the allocation.
 *
 * Distributes @extra_space to child @sizes by bringing smaller
 * children up to natural size first.
 *
 * The remaining space will be added to the @minimum_size member of the
 * RequestedSize struct. If all sizes reach their natural size then
 * the remaining space is returned.
 *
 * Returns: The remainder of @extra_space after redistributing space
 * to @sizes.
 *
 * Pulled from gtksizerequest.c from Gtk+
 */
static gint
distribute_natural_allocation (gint           extra_space,
                               guint          n_requested_sizes,
                               RequestedSize *sizes)
{
  guint *spreading;
  gint   i;

  g_return_val_if_fail (extra_space >= 0, 0);

  spreading = g_newa (guint, n_requested_sizes);

  for (i = 0; i < n_requested_sizes; i++)
    spreading[i] = i;

  /* Distribute the container's extra space c_gap. We want to assign
   * this space such that the sum of extra space assigned to children
   * (c^i_gap) is equal to c_cap. The case that there's not enough
   * space for all children to take their natural size needs some
   * attention. The goals we want to achieve are:
   *
   *   a) Maximize number of children taking their natural size.
   *   b) The allocated size of children should be a continuous
   *   function of c_gap.  That is, increasing the container size by
   *   one pixel should never make drastic changes in the distribution.
   *   c) If child i takes its natural size and child j doesn't,
   *   child j should have received at least as much gap as child i.
   *
   * The following code distributes the additional space by following
   * these rules.
   */

  /* Sort descending by gap and position. */
  g_qsort_with_data (spreading,
                     n_requested_sizes, sizeof (guint),
                     compare_gap, sizes);

  /* Distribute available space.
   * This master piece of a loop was conceived by Behdad Esfahbod.
   */
  for (i = n_requested_sizes - 1; extra_space > 0 && i >= 0; --i)
    {
      /* Divide remaining space by number of remaining children.
       * Sort order and reducing remaining space by assigned space
       * ensures that space is distributed equally.
       */
      gint glue = (extra_space + i) / (i + 1);
      gint gap = sizes[(spreading[i])].natural_size
               - sizes[(spreading[i])].minimum_size;

      gint extra = MIN (glue, gap);

      sizes[spreading[i]].minimum_size += extra;

      extra_space -= extra;
    }

  return extra_space;
}

/* Requires that the minimum and natural fields of lines
 * have been set, computes the allocation field of lines
 * by distributing total_size among lines.
 */
static void
clutter_grid_request_allocate (ClutterGridRequest *request,
                               ClutterOrientation  orientation,
                               gfloat              total_size)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridLineData *linedata;
  ClutterGridLines *lines;
  ClutterGridLine *line;
  gint nonempty;
  gint expand;
  gint i, j;
  RequestedSize *sizes;
  gint extra;
  gint rest;
  gint size;

  clutter_grid_request_compute_expand (request, orientation, &nonempty, &expand);

  if (nonempty == 0)
    return;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  size = total_size - (nonempty - 1) * linedata->spacing;

  if (linedata->homogeneous)
    {
      extra = size / nonempty;
      rest = size % nonempty;

      for (i = 0; i < lines->max - lines->min; i++)
        {
          line = &lines->lines[i];
          if (line->empty)
            continue;

          line->allocation = extra;
          if (rest > 0)
            {
              line->allocation += 1;
              rest -= 1;
            }
        }
    }
  else
    {
      sizes = g_newa (RequestedSize, nonempty);

      j = 0;
      for (i = 0; i < lines->max - lines->min; i++)
        {
          line = &lines->lines[i];
          if (line->empty)
            continue;

          size -= line->minimum;

          sizes[j].minimum_size = line->minimum;
          sizes[j].natural_size = line->natural;
          sizes[j].data = line;
          j++;
        }

      size = distribute_natural_allocation (MAX (0, size), nonempty, sizes);

      if (expand > 0)
        {
          extra = size / expand;
          rest = size % expand;
        }
      else
        {
          extra = 0;
          rest = 0;
        }

      j = 0;
      for (i = 0; i < lines->max - lines->min; i++)
        {
          line = &lines->lines[i];
          if (line->empty)
            continue;

          g_assert (line == sizes[j].data);

          line->allocation = sizes[j].minimum_size;
          if (line->expand)
            {
              line->allocation += extra;
              if (rest > 0)
                {
                  line->allocation += 1;
                  rest -= 1;
                }
            }

          j++;
        }
    }
}

/* Computes the position fields from allocation and spacing.
 */
static void
clutter_grid_request_position (ClutterGridRequest *request,
                               ClutterOrientation  orientation)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridLineData *linedata;
  ClutterGridLines *lines;
  ClutterGridLine *line;
  gfloat position;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];

  position = 0.f;
  for (i = 0; i < lines->max - lines->min; i++)
    {
      line = &lines->lines[i];
      if (!line->empty)
        {
          line->position = position;
          position += line->allocation + linedata->spacing;
        }
    }
}

static void
clutter_grid_child_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterGridChild *grid_child = CLUTTER_GRID_CHILD (gobject);
  ClutterLayoutManager *manager;

  manager = clutter_layout_meta_get_manager (CLUTTER_LAYOUT_META (gobject));

  switch (prop_id)
    {
    case PROP_CHILD_LEFT_ATTACH:
      CHILD_LEFT (grid_child) = g_value_get_int (value);
      clutter_layout_manager_layout_changed (manager);
      break;

    case PROP_CHILD_TOP_ATTACH:
      CHILD_TOP (grid_child) = g_value_get_int (value);
      clutter_layout_manager_layout_changed (manager);
      break;

    case PROP_CHILD_WIDTH:
      CHILD_WIDTH (grid_child) = g_value_get_int (value);
      clutter_layout_manager_layout_changed (manager);
      break;

    case PROP_CHILD_HEIGHT:
      CHILD_HEIGHT (grid_child) = g_value_get_int (value);
      clutter_layout_manager_layout_changed (manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_grid_child_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ClutterGridChild *grid_child = CLUTTER_GRID_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_LEFT_ATTACH:
      g_value_set_int (value, CHILD_LEFT (grid_child));
      break;

    case PROP_CHILD_TOP_ATTACH:
      g_value_set_int (value, CHILD_TOP (grid_child));
      break;

    case PROP_CHILD_WIDTH:
      g_value_set_int (value, CHILD_WIDTH (grid_child));
      break;

    case PROP_CHILD_HEIGHT:
      g_value_set_int (value, CHILD_HEIGHT (grid_child));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_grid_child_class_init (ClutterGridChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_grid_child_set_property;
  gobject_class->get_property = clutter_grid_child_get_property;

  child_props[PROP_CHILD_LEFT_ATTACH] =
    g_param_spec_int ("left-attach",
                      P_("Left attachment"),
                      P_("The column number to attach the left side of the "
                         "child to"),
                      -G_MAXINT, G_MAXINT, 0,
                      G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  child_props[PROP_CHILD_TOP_ATTACH] =
    g_param_spec_int ("top-attach",
                      P_("Top attachment"),
                      P_("The row number to attach the top side of a child "
                         "widget to"),
                      -G_MAXINT, G_MAXINT, 0,
                      G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  child_props[PROP_CHILD_WIDTH] =
    g_param_spec_int ("width",
                      P_("Width"),
                      P_("The number of columns that a child spans"),
                      -G_MAXINT, G_MAXINT, 1,
                      G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  child_props[PROP_CHILD_HEIGHT] =
    g_param_spec_int ("height",
                      P_("Height"),
                      P_("The number of rows that a child spans"),
                      -G_MAXINT, G_MAXINT, 1,
                      G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, PROP_CHILD_LAST,
                                     child_props);
}

static void
clutter_grid_child_init (ClutterGridChild *self)
{
  CHILD_LEFT (self) = -1;
  CHILD_TOP (self) = -1;
  CHILD_WIDTH (self) = 1;
  CHILD_HEIGHT (self) = 1;
}

static void
clutter_grid_layout_set_container (ClutterLayoutManager *self,
                                   ClutterContainer     *container)
{
  ClutterGridLayoutPrivate *priv = CLUTTER_GRID_LAYOUT (self)->priv;
  ClutterLayoutManagerClass *parent_class;

  priv->container = container;

  if (priv->container != NULL)
    {
      ClutterRequestMode request_mode;

      /* we need to change the :request-mode of the container
       * to match the orientation
       */
      request_mode = priv->orientation == CLUTTER_ORIENTATION_VERTICAL
                   ? CLUTTER_REQUEST_HEIGHT_FOR_WIDTH
                   : CLUTTER_REQUEST_WIDTH_FOR_HEIGHT;
      clutter_actor_set_request_mode (CLUTTER_ACTOR (priv->container),
                                      request_mode);
    }

  parent_class = CLUTTER_LAYOUT_MANAGER_CLASS (clutter_grid_layout_parent_class);
  parent_class->set_container (self, container);
}

static void
clutter_grid_layout_get_preferred_width (ClutterLayoutManager *self,
                                         ClutterContainer     *container,
                                         gfloat                for_height,
                                         gfloat               *min_width_p,
                                         gfloat               *nat_width_p)
{
  ClutterGridLayoutPrivate *priv = CLUTTER_GRID_LAYOUT (self)->priv;
  ClutterGridRequest request;
  ClutterGridLines *lines;

  if (min_width_p)
    *min_width_p = 0.0f;
  if (nat_width_p)
    *nat_width_p = 0.0f;

  request.grid = CLUTTER_GRID_LAYOUT (self);
  clutter_grid_request_update_attach (&request);
  clutter_grid_request_count_lines (&request);
  lines = &request.lines[priv->orientation];
  lines->lines = g_newa (ClutterGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (ClutterGridLine));

  clutter_grid_request_run (&request, priv->orientation, FALSE);
  clutter_grid_request_sum (&request, priv->orientation,
                            min_width_p, nat_width_p);
}

static void
clutter_grid_layout_get_preferred_height (ClutterLayoutManager *self,
                                          ClutterContainer     *container,
                                          gfloat                for_width,
                                          gfloat               *min_height_p,
                                          gfloat               *nat_height_p)
{
  ClutterGridLayoutPrivate *priv = CLUTTER_GRID_LAYOUT (self)->priv;
  ClutterGridRequest request;
  ClutterGridLines *lines;

  if (min_height_p)
    *min_height_p = 0.0f;
  if (nat_height_p)
    *nat_height_p = 0.0f;

  request.grid = CLUTTER_GRID_LAYOUT (self);
  clutter_grid_request_update_attach (&request);
  clutter_grid_request_count_lines (&request);
  lines = &request.lines[priv->orientation];
  lines->lines = g_newa (ClutterGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (ClutterGridLine));

  clutter_grid_request_run (&request, priv->orientation, FALSE);
  clutter_grid_request_sum (&request, priv->orientation,
                            min_height_p, nat_height_p);
}

static void
allocate_child (ClutterGridRequest *request,
                ClutterOrientation  orientation,
                ClutterGridChild   *child,
                gfloat             *position,
                gfloat             *size)
{
  ClutterGridLayoutPrivate *priv = request->grid->priv;
  ClutterGridLineData *linedata;
  ClutterGridLines *lines;
  ClutterGridLine *line;
  ClutterGridAttach *attach;
  gint i;

  linedata = &priv->linedata[orientation];
  lines = &request->lines[orientation];
  attach = &child->attach[orientation];

  *position = lines->lines[attach->pos - lines->min].position;

  *size = (attach->span - 1) * linedata->spacing;
  for (i = 0; i < attach->span; i++)
    {
      line = &lines->lines[attach->pos - lines->min + i];
      *size += line->allocation;
    }
}

#define GET_SIZE(allocation, orientation) \
  (orientation == CLUTTER_ORIENTATION_HORIZONTAL \
   ? clutter_actor_box_get_width ((allocation)) \
   : clutter_actor_box_get_height ((allocation)))

static void
clutter_grid_layout_allocate (ClutterLayoutManager   *layout,
                              ClutterContainer       *container,
                              const ClutterActorBox  *allocation,
                              ClutterAllocationFlags  flags)
{
  ClutterGridLayout *self = CLUTTER_GRID_LAYOUT (layout);
  ClutterGridLayoutPrivate *priv = self->priv;
  ClutterGridRequest request;
  ClutterGridLines *lines;
  ClutterActorIter iter;
  ClutterActor *child;

  request.grid = self;

  clutter_grid_request_update_attach (&request);
  clutter_grid_request_count_lines (&request);
  lines = &request.lines[0];
  lines->lines = g_newa (ClutterGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (ClutterGridLine));
  lines = &request.lines[1];
  lines->lines = g_newa (ClutterGridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (ClutterGridLine));

  clutter_grid_request_run (&request, 1 - priv->orientation, FALSE);
  clutter_grid_request_allocate (&request, 1 - priv->orientation, GET_SIZE (allocation, 1 - priv->orientation));
  clutter_grid_request_run (&request, priv->orientation, TRUE);
  clutter_grid_request_allocate (&request, priv->orientation, GET_SIZE (allocation, priv->orientation));

  clutter_grid_request_position (&request, 0);
  clutter_grid_request_position (&request, 1);

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      ClutterActorBox child_allocation;
      gfloat x, y, width, height;
      ClutterGridChild *grid_child;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      grid_child = GET_GRID_CHILD (self, child);
      allocate_child (&request, CLUTTER_ORIENTATION_HORIZONTAL, grid_child,
                      &x, &width);
      allocate_child (&request, CLUTTER_ORIENTATION_VERTICAL, grid_child,
                      &y, &height);
      x += allocation->x1;
      y += allocation->y1;

      CLUTTER_NOTE (LAYOUT, "Allocation for %s { %.2f, %.2f - %.2f x %.2f }",
                    _clutter_actor_get_debug_name (child),
                    x, y, width, height);

      child_allocation.x1 = x;
      child_allocation.y1 = y;
      child_allocation.x2 = child_allocation.x1 + width;
      child_allocation.y2 = child_allocation.y1 + height;

      clutter_actor_allocate (child, &child_allocation, flags);
    }
}

static GType
clutter_grid_layout_get_child_meta_type (ClutterLayoutManager *self)
{
  return CLUTTER_TYPE_GRID_CHILD;
}

static void
clutter_grid_layout_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ClutterGridLayout *self = CLUTTER_GRID_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      clutter_grid_layout_set_orientation (self, g_value_get_enum (value));
      break;

    case PROP_ROW_SPACING:
      clutter_grid_layout_set_row_spacing (self, g_value_get_uint (value));
      break;

    case PROP_COLUMN_SPACING:
      clutter_grid_layout_set_column_spacing (self, g_value_get_uint (value));
      break;

    case PROP_ROW_HOMOGENEOUS:
      clutter_grid_layout_set_row_homogeneous (self,
                                               g_value_get_boolean (value));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      clutter_grid_layout_set_column_homogeneous (self,
                                                  g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_grid_layout_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ClutterGridLayoutPrivate *priv = CLUTTER_GRID_LAYOUT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_ROW_SPACING:
      g_value_set_uint (value, COLUMNS (priv)->spacing);
      break;

    case PROP_COLUMN_SPACING:
      g_value_set_uint (value, ROWS (priv)->spacing);
      break;

    case PROP_ROW_HOMOGENEOUS:
      g_value_set_boolean (value, COLUMNS (priv)->homogeneous);
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      g_value_set_boolean (value, ROWS (priv)->homogeneous);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_grid_layout_class_init (ClutterGridLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterLayoutManagerClass *layout_class;

  layout_class = CLUTTER_LAYOUT_MANAGER_CLASS (klass);

  object_class->set_property = clutter_grid_layout_set_property;
  object_class->get_property = clutter_grid_layout_get_property;

  layout_class->set_container = clutter_grid_layout_set_container;
  layout_class->get_preferred_width = clutter_grid_layout_get_preferred_width;
  layout_class->get_preferred_height = clutter_grid_layout_get_preferred_height;
  layout_class->allocate = clutter_grid_layout_allocate;
  layout_class->get_child_meta_type = clutter_grid_layout_get_child_meta_type;

  /**
   * ClutterGridLayout:orientation:
   *
   * The orientation of the layout, either horizontal or vertical
   *
   * Since: 1.12
   */
  obj_props[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation",
                       P_("Orientation"),
                       P_("The orientation of the layout"),
                       CLUTTER_TYPE_ORIENTATION,
                       CLUTTER_ORIENTATION_HORIZONTAL,
                       G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  /**
   * ClutterGridLayout:row-spacing:
   *
   * The amount of space in pixels between two consecutive rows
   *
   * Since: 1.12
   */
  obj_props[PROP_ROW_SPACING] =
    g_param_spec_uint ("row-spacing",
                       P_("Row spacing"),
                       P_("The amount of space between two consecutive rows"),
                       0, G_MAXUINT, 0,
                       G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  /**
   * ClutterGridLayout:column-spacing:
   *
   * The amount of space in pixels between two consecutive columns
   *
   * Since: 1.12
   */
  obj_props[PROP_COLUMN_SPACING] =
    g_param_spec_uint ("column-spacing",
                       P_("Column spacing"),
                       P_("The amount of space between two consecutive "
                          "columns"),
                       0, G_MAXUINT, 0,
                       G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  /**
   * ClutterGridLayout:row-homogeneous:
   *
   * Whether all rows of the layout should have the same height
   *
   * Since: 1.12
   */
  obj_props[PROP_ROW_HOMOGENEOUS] =
    g_param_spec_boolean ("row-homogeneous",
                          P_("Row Homogeneous"),
                          P_("If TRUE, the rows are all the same height"),
                          FALSE,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  /**
   * ClutterGridLayout:column-homogeneous:
   *
   * Whether all columns of the layout should have the same width
   *
   * Since: 1.12
   */
  obj_props[PROP_COLUMN_HOMOGENEOUS] =
    g_param_spec_boolean ("column-homogeneous",
                          P_("Column Homogeneous"),
                          P_("If TRUE, the columns are all the same width"),
                          FALSE,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
clutter_grid_layout_init (ClutterGridLayout *self)
{
  self->priv = clutter_grid_layout_get_instance_private (self);

  self->priv->orientation = CLUTTER_ORIENTATION_HORIZONTAL;

  self->priv->linedata[0].spacing = 0;
  self->priv->linedata[1].spacing = 0;

  self->priv->linedata[0].homogeneous = FALSE;
  self->priv->linedata[1].homogeneous = FALSE;
}

/**
 * clutter_grid_layout_new:
 *
 * Creates a new #ClutterGridLayout
 *
 * Return value: the new #ClutterGridLayout
 */
ClutterLayoutManager *
clutter_grid_layout_new (void)
{
  return g_object_new (CLUTTER_TYPE_GRID_LAYOUT, NULL);
}

/**
 * clutter_grid_layout_attach:
 * @layout: a #ClutterGridLayout
 * @child: the #ClutterActor to add
 * @left: the column number to attach the left side of @child to
 * @top: the row number to attach the top side of @child to
 * @width: the number of columns that @child will span
 * @height: the number of rows that @child will span
 *
 * Adds a widget to the grid.
 *
 * The position of @child is determined by @left and @top. The
 * number of 'cells' that @child will occupy is determined by
 * @width and @height.
 *
 * Since: 1.12
 */
void
clutter_grid_layout_attach (ClutterGridLayout *layout,
                            ClutterActor      *child,
                            gint               left,
                            gint               top,
                            gint               width,
                            gint               height)
{
  ClutterGridLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));

  priv = layout->priv;

  if (!priv->container)
    return;

  grid_attach (layout, child, left, top, width, height);
  clutter_actor_add_child (CLUTTER_ACTOR (priv->container), child);
}

/**
 * clutter_grid_layout_attach_next_to:
 * @layout: a #ClutterGridLayout
 * @child: the actor to add
 * @sibling: (allow-none): the child of @layout that @child will be placed
 *     next to, or %NULL to place @child at the beginning or end
 * @side: the side of @sibling that @child is positioned next to
 * @width: the number of columns that @child will span
 * @height: the number of rows that @child will span
 *
 * Adds a actor to the grid.
 *
 * The actor is placed next to @sibling, on the side determined by
 * @side. When @sibling is %NULL, the actor is placed in row (for
 * left or right placement) or column 0 (for top or bottom placement),
 * at the end indicated by @side.
 *
 * Attaching widgets labeled [1], [2], [3] with @sibling == %NULL and
 * @side == %CLUTTER_GRID_POSITION_LEFT yields a layout of [3][2][1].
 *
 * Since: 1.12
 */
void
clutter_grid_layout_attach_next_to (ClutterGridLayout   *layout,
                                    ClutterActor        *child,
                                    ClutterActor        *sibling,
                                    ClutterGridPosition  side,
                                    gint                 width,
                                    gint                 height)
{
  ClutterGridLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (child));
  g_return_if_fail (clutter_actor_get_parent (child) == NULL);
  g_return_if_fail (sibling == NULL || CLUTTER_IS_ACTOR (sibling));
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  priv = layout->priv;

  if (!priv->container)
    return;

  grid_attach_next_to (layout, child, sibling, side, width, height);
  clutter_actor_add_child (CLUTTER_ACTOR (priv->container), child);
}

/**
 * clutter_grid_layout_set_orientation:
 * @layout: a #ClutterGridLayout
 * @orientation: the orientation of the #ClutterGridLayout
 *
 * Sets the orientation of the @layout
 *
 * Since: 1.12
 */
void
clutter_grid_layout_set_orientation (ClutterGridLayout *layout,
                                     ClutterOrientation orientation)
{
  ClutterGridLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));

  priv = layout->priv;

  if (priv->orientation != orientation)
    {
      priv->orientation = orientation;

      clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (layout));
      g_object_notify_by_pspec (G_OBJECT (layout), obj_props[PROP_ORIENTATION]);
    }
}

/**
 * clutter_grid_layout_get_child_at:
 * @layout: a #ClutterGridLayout
 * @left: the left edge of the cell
 * @top: the top edge of the cell
 *
 * Gets the child of @layout whose area covers the grid
 * cell whose upper left corner is at @left, @top.
 *
 * Returns: (transfer none): the child at the given position, or %NULL
 *
 * Since: 1.12
 */
ClutterActor *
clutter_grid_layout_get_child_at (ClutterGridLayout *layout,
                                  gint               left,
                                  gint               top)
{
  ClutterGridLayoutPrivate *priv;
  ClutterGridChild *grid_child;
  ClutterActorIter iter;
  ClutterActor *child;

  g_return_val_if_fail (CLUTTER_IS_GRID_LAYOUT (layout), NULL);

  priv = layout->priv;

  if (!priv->container)
    return NULL;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      grid_child = GET_GRID_CHILD (layout, child);

      if (CHILD_LEFT (grid_child) <= left &&
          CHILD_LEFT (grid_child) + CHILD_WIDTH (grid_child) > left &&
          CHILD_TOP (grid_child) <= top &&
          CHILD_TOP (grid_child) + CHILD_HEIGHT (grid_child) > top)
        return child;
    }

  return NULL;
}

/**
 * clutter_grid_layout_insert_row:
 * @layout: a #ClutterGridLayout
 * @position: the position to insert the row at
 *
 * Inserts a row at the specified position.
 *
 * Children which are attached at or below this position
 * are moved one row down. Children which span across this
 * position are grown to span the new row.
 *
 * Since: 1.12
 */
void
clutter_grid_layout_insert_row (ClutterGridLayout *layout,
                                gint               position)
{
  ClutterGridLayoutPrivate *priv;
  ClutterGridChild *grid_child;
  ClutterActorIter iter;
  ClutterActor *child;
  gint top, height;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));

  priv = layout->priv;

  if (!priv->container)
    return;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      grid_child = GET_GRID_CHILD (layout, child);

      top = CHILD_TOP (grid_child);
      height = CHILD_HEIGHT (grid_child);

      if (top >= position)
        {
          CHILD_TOP (grid_child) = top + 1;
          g_object_notify_by_pspec (G_OBJECT (grid_child),
                                    child_props[PROP_CHILD_TOP_ATTACH]);
        }
      else if (top + height > position)
        {
          CHILD_HEIGHT (grid_child) = height + 1;
          g_object_notify_by_pspec (G_OBJECT (grid_child),
                                    child_props[PROP_CHILD_HEIGHT]);
        }
    }
  clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (layout));
}

/**
 * clutter_grid_layout_insert_column:
 * @layout: a #ClutterGridLayout
 * @position: the position to insert the column at
 *
 * Inserts a column at the specified position.
 *
 * Children which are attached at or to the right of this position
 * are moved one column to the right. Children which span across this
 * position are grown to span the new column.
 *
 * Since: 1.12
 */
void
clutter_grid_layout_insert_column (ClutterGridLayout *layout,
                                   gint               position)
{
  ClutterGridLayoutPrivate *priv;
  ClutterGridChild *grid_child;
  ClutterActorIter iter;
  ClutterActor *child;
  gint left, width;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));

  priv = layout->priv;

  if (!priv->container)
    return;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (priv->container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      grid_child = GET_GRID_CHILD (layout, child);

      left = CHILD_LEFT (grid_child);
      width = CHILD_WIDTH (grid_child);

      if (left >= position)
        {
          CHILD_LEFT (grid_child) = left + 1;
          g_object_notify_by_pspec (G_OBJECT (grid_child),
                                    child_props[PROP_CHILD_LEFT_ATTACH]);
        }
      else if (left + width > position)
        {
          CHILD_WIDTH (grid_child) = width + 1;
          g_object_notify_by_pspec (G_OBJECT (grid_child),
                                    child_props[PROP_CHILD_WIDTH]);
        }
    }
  clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (layout));
}

/**
 * clutter_grid_layout_insert_next_to:
 * @layout: a #ClutterGridLayout
 * @sibling: the child of @layout that the new row or column will be
 *     placed next to
 * @side: the side of @sibling that @child is positioned next to
 *
 * Inserts a row or column at the specified position.
 *
 * The new row or column is placed next to @sibling, on the side
 * determined by @side. If @side is %CLUTTER_GRID_POSITION_LEFT or
 * %CLUTTER_GRID_POSITION_BOTTOM, a row is inserted. If @side is
 * %CLUTTER_GRID_POSITION_LEFT of %CLUTTER_GRID_POSITION_RIGHT,
 * a column is inserted.
 *
 * Since: 1.12
 */
void
clutter_grid_layout_insert_next_to (ClutterGridLayout   *layout,
                                    ClutterActor        *sibling,
                                    ClutterGridPosition  side)
{
  ClutterGridChild *grid_child;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (sibling));

  grid_child = GET_GRID_CHILD (layout, sibling);

  switch (side)
    {
    case CLUTTER_GRID_POSITION_LEFT:
      clutter_grid_layout_insert_column (layout, CHILD_LEFT (grid_child));
      break;

    case CLUTTER_GRID_POSITION_RIGHT:
      clutter_grid_layout_insert_column (layout, CHILD_LEFT (grid_child) +
                                         CHILD_WIDTH (grid_child));
      break;

    case CLUTTER_GRID_POSITION_TOP:
      clutter_grid_layout_insert_row (layout, CHILD_TOP (grid_child));
      break;

    case CLUTTER_GRID_POSITION_BOTTOM:
      clutter_grid_layout_insert_row (layout, CHILD_TOP (grid_child) +
                                      CHILD_HEIGHT (grid_child));
      break;

    default:
      g_assert_not_reached ();
    }
}

/**
 * clutter_grid_layout_get_orientation:
 * @layout: a #ClutterGridLayout
 *
 * Retrieves the orientation of the @layout.
 *
 * Return value: the orientation of the layout
 *
 * Since: 1.12
 */
ClutterOrientation
clutter_grid_layout_get_orientation (ClutterGridLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_GRID_LAYOUT (layout),
                        CLUTTER_ORIENTATION_HORIZONTAL);

  return layout->priv->orientation;
}

/**
 * clutter_grid_layout_set_row_spacing:
 * @layout: a #ClutterGridLayout
 * @spacing: the spacing between rows of the layout, in pixels
 *
 * Sets the spacing between rows of @layout
 *
 * Since: 1.12
 */
void
clutter_grid_layout_set_row_spacing (ClutterGridLayout *layout,
                                     guint              spacing)
{
  ClutterGridLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));

  priv = layout->priv;

  if (COLUMNS (priv)->spacing != spacing)
    {
      COLUMNS (priv)->spacing = spacing;

      clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (layout));
      g_object_notify_by_pspec (G_OBJECT (layout),
                                obj_props[PROP_ROW_SPACING]);
    }
}

/**
 * clutter_grid_layout_get_row_spacing:
 * @layout: a #ClutterGridLayout
 *
 * Retrieves the spacing set using clutter_grid_layout_set_row_spacing()
 *
 * Return value: the spacing between rows of @layout
 *
 * Since: 1.12
 */
guint
clutter_grid_layout_get_row_spacing (ClutterGridLayout *layout)
{
  ClutterGridLayoutPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_GRID_LAYOUT (layout), 0);

  priv = layout->priv;

  return COLUMNS (priv)->spacing;
}

/**
 * clutter_grid_layout_set_column_spacing:
 * @layout: a #ClutterGridLayout
 * @spacing: the spacing between columns of the layout, in pixels
 *
 * Sets the spacing between columns of @layout
 *
 * Since: 1.12
 */
void
clutter_grid_layout_set_column_spacing (ClutterGridLayout *layout,
                                        guint spacing)
{
  ClutterGridLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));

  priv = layout->priv;

  if (ROWS (priv)->spacing != spacing)
    {
      ROWS (priv)->spacing = spacing;

      clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (layout));
      g_object_notify_by_pspec (G_OBJECT (layout),
                                obj_props[PROP_COLUMN_SPACING]);
    }
}

/**
 * clutter_grid_layout_get_column_spacing:
 * @layout: a #ClutterGridLayout
 *
 * Retrieves the spacing set using clutter_grid_layout_set_column_spacing()
 *
 * Return value: the spacing between coluns of @layout
 *
 * Since: 1.12
 */
guint
clutter_grid_layout_get_column_spacing (ClutterGridLayout *layout)
{
  ClutterGridLayoutPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_GRID_LAYOUT (layout), 0);

  priv = layout->priv;

  return ROWS (priv)->spacing;
}

/**
 * clutter_grid_layout_set_column_homogeneous:
 * @layout: a #ClutterGridLayout
 * @homogeneous: %TRUE to make columns homogeneous
 *
 * Sets whether all columns of @layout will have the same width.
 *
 * Since: 1.12
 */
void
clutter_grid_layout_set_column_homogeneous (ClutterGridLayout *layout,
                                            gboolean           homogeneous)
{
  ClutterGridLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));

  priv = layout->priv;

  if (ROWS (priv)->homogeneous != homogeneous)
    {
      ROWS (priv)->homogeneous = homogeneous;

      clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (layout));
      g_object_notify_by_pspec (G_OBJECT (layout),
                                obj_props[PROP_COLUMN_HOMOGENEOUS]);
    }
}

/**
 * clutter_grid_layout_get_column_homogeneous:
 * @layout: a #ClutterGridLayout
 *
 * Returns whether all columns of @layout have the same width.
 *
 * Returns: whether all columns of @layout have the same width.
 */
gboolean
clutter_grid_layout_get_column_homogeneous (ClutterGridLayout *layout)
{
  ClutterGridLayoutPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_GRID_LAYOUT (layout), FALSE);

  priv = layout->priv;

  return ROWS (priv)->homogeneous;
}

/**
 * clutter_grid_layout_set_row_homogeneous:
 * @layout: a #ClutterGridLayout
 * @homogeneous: %TRUE to make rows homogeneous
 *
 * Sets whether all rows of @layout will have the same height.
 *
 * Since: 1.12
 */
void
clutter_grid_layout_set_row_homogeneous (ClutterGridLayout *layout,
                                         gboolean           homogeneous)
{
  ClutterGridLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_GRID_LAYOUT (layout));

  priv = layout->priv;

  if (COLUMNS (priv)->homogeneous != homogeneous)
    {
      COLUMNS (priv)->homogeneous = homogeneous;

      clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (layout));
      g_object_notify_by_pspec (G_OBJECT (layout),
                                obj_props[PROP_ROW_HOMOGENEOUS]);
    }
}

/**
 * clutter_grid_layout_get_row_homogeneous:
 * @layout: a #ClutterGridLayout
 *
 * Returns whether all rows of @layout have the same height.
 *
 * Returns: whether all rows of @layout have the same height.
 *
 * Since: 1.12
 */
gboolean
clutter_grid_layout_get_row_homogeneous (ClutterGridLayout *layout)
{
  ClutterGridLayoutPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_GRID_LAYOUT (layout), FALSE);

  priv = layout->priv;

  return COLUMNS (priv)->homogeneous;
}
