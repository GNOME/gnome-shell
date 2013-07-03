/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-flow-layout
 * @short_description: A reflowing layout manager
 *
 * #ClutterFlowLayout is a layout manager which implements the following
 * policy:
 *
 * <itemizedlist>
 *   <listitem><para>the preferred natural size depends on the value
 *   of the #ClutterFlowLayout:orientation property; the layout will try
 *   to maintain all its children on a single row or
 *   column;</para></listitem>
 *   <listitem><para>if either the width or the height allocated are
 *   smaller than the preferred ones, the layout will wrap; in this case,
 *   the preferred height or width, respectively, will take into account
 *   the amount of columns and rows;</para></listitem>
 *   <listitem><para>each line (either column or row) in reflowing will
 *   have the size of the biggest cell on that line; if the
 *   #ClutterFlowLayout:homogeneous property is set to %FALSE the actor
 *   will be allocated within that area, and if set to %TRUE instead the
 *   actor will be given exactly that area;</para></listitem>
 *   <listitem><para>the size of the columns or rows can be controlled
 *   for both minimum and maximum; the spacing can also be controlled
 *   in both columns and rows.</para></listitem>
 * </itemizedlist>
 *
 * <figure id="flow-layout-image">
 *   <title>Horizontal flow layout</title>
 *   <para>The image shows a #ClutterFlowLayout with the
 *   #ClutterFlowLayout:orientation propert set to
 *   %CLUTTER_FLOW_HORIZONTAL.</para>
 *   <graphic fileref="flow-layout.png" format="PNG"/>
 * </figure>
 *
 * <informalexample>
 *  <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/flow-layout.c">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 *  </programlisting>
 * </informalexample>
 *
 * #ClutterFlowLayout is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-container.h"

#include "clutter-actor.h"
#include "clutter-animatable.h"
#include "clutter-child-meta.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-flow-layout.h"
#include "clutter-layout-meta.h"
#include "clutter-private.h"

struct _ClutterFlowLayoutPrivate
{
  ClutterContainer *container;

  ClutterFlowOrientation orientation;

  gfloat col_spacing;
  gfloat row_spacing;

  gfloat min_col_width;
  gfloat max_col_width;
  gfloat col_width;

  gfloat min_row_height;
  gfloat max_row_height;
  gfloat row_height;

  /* per-line size */
  GArray *line_min;
  GArray *line_natural;
  gfloat req_width;
  gfloat req_height;

  guint line_count;

  guint is_homogeneous : 1;
  guint snap_to_grid : 1;
};

enum
{
  PROP_0,

  PROP_ORIENTATION,

  PROP_HOMOGENEOUS,

  PROP_COLUMN_SPACING,
  PROP_ROW_SPACING,

  PROP_MIN_COLUMN_WIDTH,
  PROP_MAX_COLUMN_WIDTH,
  PROP_MIN_ROW_HEGHT,
  PROP_MAX_ROW_HEIGHT,

  PROP_SNAP_TO_GRID,

  N_PROPERTIES
};

static GParamSpec *flow_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (ClutterFlowLayout,
                            clutter_flow_layout,
                            CLUTTER_TYPE_LAYOUT_MANAGER)

static gint
get_columns (ClutterFlowLayout *self,
             gfloat             for_width)
{
  ClutterFlowLayoutPrivate *priv = self->priv;
  gint n_columns;

  if (for_width < 0)
    return 1;

  if (priv->col_width == 0)
    return 1;

  n_columns = (gint) (for_width + priv->col_spacing)
            / (priv->col_width + priv->col_spacing);

  if (n_columns == 0)
    return 1;

  return n_columns;
}

static gint
get_rows (ClutterFlowLayout *self,
          gfloat             for_height)
{
  ClutterFlowLayoutPrivate *priv = self->priv;
  gint n_rows;

  if (for_height < 0)
    return 1;

  if (priv->row_height == 0)
    return 1;

  n_rows = (gint) (for_height + priv->row_spacing)
         / (priv->row_height + priv->row_spacing);

  if (n_rows == 0)
    return 1;

  return n_rows;
}

static gint
compute_lines (ClutterFlowLayout *self,
               gfloat             avail_width,
               gfloat             avail_height)
{
  ClutterFlowLayoutPrivate *priv = self->priv;

  if (priv->orientation == CLUTTER_FLOW_HORIZONTAL)
    return get_columns (self, avail_width);
  else
    return get_rows (self, avail_height);
}

static void
clutter_flow_layout_get_preferred_width (ClutterLayoutManager *manager,
                                         ClutterContainer     *container,
                                         gfloat                for_height,
                                         gfloat               *min_width_p,
                                         gfloat               *nat_width_p)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (manager)->priv;
  gint n_rows, line_item_count, line_count;
  gfloat total_min_width, total_natural_width;
  gfloat line_min_width, line_natural_width;
  gfloat max_min_width, max_natural_width;
  ClutterActor *actor, *child;
  ClutterActorIter iter;
  gfloat item_y;

  n_rows = get_rows (CLUTTER_FLOW_LAYOUT (manager), for_height);

  total_min_width = 0;
  total_natural_width = 0;

  line_min_width = 0;
  line_natural_width = 0;

  line_item_count = 0;
  line_count = 0;

  item_y = 0;

  actor = CLUTTER_ACTOR (container);

  /* clear the line width arrays */
  if (priv->line_min != NULL)
    g_array_free (priv->line_min, TRUE);

  if (priv->line_natural != NULL)
    g_array_free (priv->line_natural, TRUE);

  priv->line_min = g_array_sized_new (FALSE, FALSE,
                                      sizeof (gfloat),
                                      16);
  priv->line_natural = g_array_sized_new (FALSE, FALSE,
                                          sizeof (gfloat),
                                          16);

  if (clutter_actor_get_n_children (actor) != 0)
    line_count = 1;

  max_min_width = max_natural_width = 0;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat child_min, child_natural;
      gfloat new_y, item_height;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      if (priv->orientation == CLUTTER_FLOW_VERTICAL && for_height > 0)
        {
          clutter_actor_get_preferred_height (child, -1,
                                              &child_min,
                                              &child_natural);

          if ((priv->snap_to_grid && line_item_count == n_rows) ||
              (!priv->snap_to_grid && item_y + child_natural > for_height))
            {
              total_min_width += line_min_width;
              total_natural_width += line_natural_width;

              g_array_append_val (priv->line_min,
                                  line_min_width);
              g_array_append_val (priv->line_natural,
                                  line_natural_width);

              line_min_width = line_natural_width = 0;

              line_item_count = 0;
              line_count += 1;
              item_y = 0;
            }

          if (priv->snap_to_grid)
            {
              new_y = ((line_item_count + 1) * (for_height + priv->row_spacing))
                    / n_rows;
              item_height = new_y - item_y - priv->row_spacing;
            }
          else
            {
              new_y = item_y + child_natural + priv->row_spacing;
              item_height = child_natural;
            }

          clutter_actor_get_preferred_width (child, item_height,
                                             &child_min,
                                             &child_natural);

          line_min_width = MAX (line_min_width, child_min);
          line_natural_width = MAX (line_natural_width, child_natural);

          item_y = new_y;
          line_item_count += 1;

          max_min_width = MAX (max_min_width, line_min_width);
          max_natural_width = MAX (max_natural_width, line_natural_width);
        }
      else
        {
          clutter_actor_get_preferred_width (child, for_height,
                                             &child_min,
                                             &child_natural);

          max_min_width = MAX (max_min_width, child_min);
          max_natural_width = MAX (max_natural_width, child_natural);

          total_min_width += max_min_width;
          total_natural_width += max_natural_width;
          line_count += 1;
        }
    }

  priv->col_width = max_natural_width;

  if (priv->max_col_width > 0 && priv->col_width > priv->max_col_width)
    priv->col_width = MAX (priv->max_col_width, max_min_width);

  if (priv->col_width < priv->min_col_width)
    priv->col_width = priv->min_col_width;

  if (priv->orientation == CLUTTER_FLOW_VERTICAL && for_height > 0)
    {
      /* if we have a non-full row we need to add it */
      if (line_item_count > 0)
        {
          total_min_width += line_min_width;
          total_natural_width += line_natural_width;

          g_array_append_val (priv->line_min,
                              line_min_width);
          g_array_append_val (priv->line_natural,
                              line_natural_width);
        }

      priv->line_count = line_count;

      if (priv->line_count > 0)
        {
          gfloat total_spacing;

          total_spacing = priv->col_spacing * (priv->line_count - 1);

          total_min_width += total_spacing;
          total_natural_width += total_spacing;
        }
    }
  else
    {
      g_array_append_val (priv->line_min, line_min_width);
      g_array_append_val (priv->line_natural, line_natural_width);

      priv->line_count = line_count;

      if (priv->line_count > 0)
        {
          gfloat total_spacing;

          total_spacing = priv->col_spacing * (priv->line_count - 1);

          total_min_width += total_spacing;
          total_natural_width += total_spacing;
        }
    }

  CLUTTER_NOTE (LAYOUT,
                "Flow[w]: %d lines (%d per line): w [ %.2f, %.2f ] for h %.2f",
                n_rows, priv->line_count,
                total_min_width,
                total_natural_width,
                for_height);

  priv->req_height = for_height;

  if (min_width_p)
    *min_width_p = max_min_width;

  if (nat_width_p)
    *nat_width_p = total_natural_width;
}

static void
clutter_flow_layout_get_preferred_height (ClutterLayoutManager *manager,
                                          ClutterContainer     *container,
                                          gfloat                for_width,
                                          gfloat               *min_height_p,
                                          gfloat               *nat_height_p)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (manager)->priv;
  gint n_columns, line_item_count, line_count;
  gfloat total_min_height, total_natural_height;
  gfloat line_min_height, line_natural_height;
  gfloat max_min_height, max_natural_height;
  ClutterActor *actor, *child;
  ClutterActorIter iter;
  gfloat item_x;

  n_columns = get_columns (CLUTTER_FLOW_LAYOUT (manager), for_width);

  total_min_height = 0;
  total_natural_height = 0;

  line_min_height = 0;
  line_natural_height = 0;

  line_item_count = 0;
  line_count = 0;

  item_x = 0;

  actor = CLUTTER_ACTOR (container);

  /* clear the line height arrays */
  if (priv->line_min != NULL)
    g_array_free (priv->line_min, TRUE);

  if (priv->line_natural != NULL)
    g_array_free (priv->line_natural, TRUE);

  priv->line_min = g_array_sized_new (FALSE, FALSE,
                                      sizeof (gfloat),
                                      16);
  priv->line_natural = g_array_sized_new (FALSE, FALSE,
                                          sizeof (gfloat),
                                          16);

  if (clutter_actor_get_n_children (actor) != 0)
    line_count = 1;

  max_min_height = max_natural_height = 0;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat child_min, child_natural;
      gfloat new_x, item_width;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      if (priv->orientation == CLUTTER_FLOW_HORIZONTAL && for_width > 0)
        {
          clutter_actor_get_preferred_width (child, -1,
                                             &child_min,
                                             &child_natural);

          if ((priv->snap_to_grid && line_item_count == n_columns) ||
              (!priv->snap_to_grid && item_x + child_natural > for_width))
            {
              total_min_height += line_min_height;
              total_natural_height += line_natural_height;

              g_array_append_val (priv->line_min,
                                  line_min_height);
              g_array_append_val (priv->line_natural,
                                  line_natural_height);

              line_min_height = line_natural_height = 0;

              line_item_count = 0;
              line_count += 1;
              item_x = 0;
            }

          if (priv->snap_to_grid)
            {
              new_x = ((line_item_count + 1) * (for_width + priv->col_spacing))
                    / n_columns;
              item_width = new_x - item_x - priv->col_spacing;
            }
          else
            {
              new_x = item_x + child_natural + priv->col_spacing;
              item_width = child_natural;
            }

          clutter_actor_get_preferred_height (child, item_width,
                                              &child_min,
                                              &child_natural);

          line_min_height = MAX (line_min_height, child_min);
          line_natural_height = MAX (line_natural_height, child_natural);

          item_x = new_x;
          line_item_count += 1;

          max_min_height = MAX (max_min_height, line_min_height);
          max_natural_height = MAX (max_natural_height, line_natural_height);
        }
      else
        {
          clutter_actor_get_preferred_height (child, for_width,
                                              &child_min,
                                              &child_natural);

          max_min_height = MAX (max_min_height, child_min);
          max_natural_height = MAX (max_natural_height, child_natural);

          total_min_height += max_min_height;
          total_natural_height += max_natural_height;

          line_count += 1;
        }
    }

  priv->row_height = max_natural_height;

  if (priv->max_row_height > 0 && priv->row_height > priv->max_row_height)
    priv->row_height = MAX (priv->max_row_height, max_min_height);

  if (priv->row_height < priv->min_row_height)
    priv->row_height = priv->min_row_height;

  if (priv->orientation == CLUTTER_FLOW_HORIZONTAL && for_width > 0)
    {
      /* if we have a non-full row we need to add it */
      if (line_item_count > 0)
        {
          total_min_height += line_min_height;
          total_natural_height += line_natural_height;

          g_array_append_val (priv->line_min,
                              line_min_height);
          g_array_append_val (priv->line_natural,
                              line_natural_height);
        }

      priv->line_count = line_count;
      if (priv->line_count > 0)
        {
          gfloat total_spacing;

          total_spacing = priv->row_spacing * (priv->line_count - 1);

          total_min_height += total_spacing;
          total_natural_height += total_spacing;
        }
    }
  else
    {
      g_array_append_val (priv->line_min, line_min_height);
      g_array_append_val (priv->line_natural, line_natural_height);

      priv->line_count = line_count;

      if (priv->line_count > 0)
        {
          gfloat total_spacing;

          total_spacing = priv->col_spacing * priv->line_count;

          total_min_height += total_spacing;
          total_natural_height += total_spacing;
        }
    }

  CLUTTER_NOTE (LAYOUT,
                "Flow[h]: %d lines (%d per line): w [ %.2f, %.2f ] for h %.2f",
                n_columns, priv->line_count,
                total_min_height,
                total_natural_height,
                for_width);

  priv->req_width = for_width;

  if (min_height_p)
    *min_height_p = max_min_height;

  if (nat_height_p)
    *nat_height_p = total_natural_height;
}

static void
clutter_flow_layout_allocate (ClutterLayoutManager   *manager,
                              ClutterContainer       *container,
                              const ClutterActorBox  *allocation,
                              ClutterAllocationFlags  flags)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (manager)->priv;
  ClutterActor *actor, *child;
  ClutterActorIter iter;
  gfloat x_off, y_off;
  gfloat avail_width, avail_height;
  gfloat item_x, item_y;
  gint line_item_count;
  gint items_per_line;
  gint line_index;

  actor = CLUTTER_ACTOR (container);
  if (clutter_actor_get_n_children (actor) == 0)
    return;

  clutter_actor_box_get_origin (allocation, &x_off, &y_off);
  clutter_actor_box_get_size (allocation, &avail_width, &avail_height);

  /* blow the cached preferred size and re-compute with the given
   * available size in case the FlowLayout wasn't given the exact
   * size it requested
   */
  if ((priv->req_width >= 0 && avail_width != priv->req_width) ||
      (priv->req_height >= 0 && avail_height != priv->req_height))
    {
      clutter_flow_layout_get_preferred_width (manager, container,
                                               avail_height,
                                               NULL, NULL);
      clutter_flow_layout_get_preferred_height (manager, container,
                                                avail_width,
                                                NULL, NULL);
    }

  items_per_line = compute_lines (CLUTTER_FLOW_LAYOUT (manager),
                                  avail_width, avail_height);

  item_x = x_off;
  item_y = y_off;

  line_item_count = 0;
  line_index = 0;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      ClutterActorBox child_alloc;
      gfloat item_width, item_height;
      gfloat new_x, new_y;
      gfloat child_min, child_natural;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      new_x = new_y = 0;

      if (!priv->snap_to_grid)
        clutter_actor_get_preferred_size (child,
                                          NULL, NULL,
                                          &item_width,
                                          &item_height);

      if (priv->orientation == CLUTTER_FLOW_HORIZONTAL)
        {
          if ((priv->snap_to_grid &&
               line_item_count == items_per_line && line_item_count > 0) ||
              (!priv->snap_to_grid && item_x + item_width > avail_width))
            {
              item_y += g_array_index (priv->line_natural,
                                       gfloat,
                                       line_index);

              if (line_index >= 0)
                item_y += priv->row_spacing;

              line_item_count = 0;
              line_index += 1;

              item_x = x_off;
            }

          if (priv->snap_to_grid)
            {
              new_x = x_off + ((line_item_count + 1) * (avail_width + priv->col_spacing))
                    / items_per_line;
              item_width = new_x - item_x - priv->col_spacing;
            }
          else
            {
              new_x = item_x + item_width + priv->col_spacing;
            }

          item_height = g_array_index (priv->line_natural,
                                       gfloat,
                                       line_index);

        }
      else
        {
          if ((priv->snap_to_grid &&
               line_item_count == items_per_line && line_item_count > 0) ||
              (!priv->snap_to_grid && item_y + item_height > avail_height))
            {
              item_x += g_array_index (priv->line_natural,
                                       gfloat,
                                       line_index);

              if (line_index >= 0)
                item_x += priv->col_spacing;

              line_item_count = 0;
              line_index += 1;

              item_y = y_off;
            }

          if (priv->snap_to_grid)
            {
              new_y = y_off + ((line_item_count + 1) * (avail_height + priv->row_spacing))
                    / items_per_line;
              item_height = new_y - item_y - priv->row_spacing;
            }
          else
            {
              new_y = item_y + item_height + priv->row_spacing;
            }

          item_width = g_array_index (priv->line_natural,
                                      gfloat,
                                      line_index);
        }

      if (!priv->is_homogeneous &&
          !clutter_actor_needs_expand (child,
                                       CLUTTER_ORIENTATION_HORIZONTAL))
        {
          clutter_actor_get_preferred_width (child, item_height,
                                             &child_min,
                                             &child_natural);
          item_width = MIN (item_width, child_natural);
        }

      if (!priv->is_homogeneous &&
          !clutter_actor_needs_expand (child,
                                       CLUTTER_ORIENTATION_VERTICAL))
        {
          clutter_actor_get_preferred_height (child, item_width,
                                              &child_min,
                                              &child_natural);
          item_height = MIN (item_height, child_natural);
        }

      CLUTTER_NOTE (LAYOUT,
                    "flow[line:%d, item:%d/%d] ="
                    "{ %.2f, %.2f, %.2f, %.2f }",
                    line_index, line_item_count + 1, items_per_line,
                    item_x, item_y, item_width, item_height);

      child_alloc.x1 = ceil (item_x);
      child_alloc.y1 = ceil (item_y);
      child_alloc.x2 = ceil (child_alloc.x1 + item_width);
      child_alloc.y2 = ceil (child_alloc.y1 + item_height);
      clutter_actor_allocate (child, &child_alloc, flags);

      if (priv->orientation == CLUTTER_FLOW_HORIZONTAL)
        item_x = new_x;
      else
        item_y = new_y;

      line_item_count += 1;
    }
}

static void
clutter_flow_layout_set_container (ClutterLayoutManager *manager,
                                   ClutterContainer     *container)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (manager)->priv;
  ClutterLayoutManagerClass *parent_class;

  priv->container = container;

  if (priv->container != NULL)
    {
      ClutterRequestMode request_mode;

      /* we need to change the :request-mode of the container
       * to match the orientation
       */
      request_mode = (priv->orientation == CLUTTER_FLOW_HORIZONTAL)
                   ? CLUTTER_REQUEST_HEIGHT_FOR_WIDTH
                   : CLUTTER_REQUEST_WIDTH_FOR_HEIGHT;
      clutter_actor_set_request_mode (CLUTTER_ACTOR (priv->container),
                                      request_mode);
    }

  parent_class = CLUTTER_LAYOUT_MANAGER_CLASS (clutter_flow_layout_parent_class);
  parent_class->set_container (manager, container);
}

static void
clutter_flow_layout_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ClutterFlowLayout *self = CLUTTER_FLOW_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      clutter_flow_layout_set_orientation (self, g_value_get_enum (value));
      break;

    case PROP_HOMOGENEOUS:
      clutter_flow_layout_set_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_COLUMN_SPACING:
      clutter_flow_layout_set_column_spacing (self, g_value_get_float (value));
      break;

    case PROP_ROW_SPACING:
      clutter_flow_layout_set_row_spacing (self, g_value_get_float (value));
      break;

    case PROP_MIN_COLUMN_WIDTH:
      clutter_flow_layout_set_column_width (self,
                                            g_value_get_float (value),
                                            self->priv->max_col_width);
      break;

    case PROP_MAX_COLUMN_WIDTH:
      clutter_flow_layout_set_column_width (self,
                                            self->priv->min_col_width,
                                            g_value_get_float (value));
      break;

    case PROP_MIN_ROW_HEGHT:
      clutter_flow_layout_set_row_height (self,
                                          g_value_get_float (value),
                                          self->priv->max_row_height);
      break;

    case PROP_MAX_ROW_HEIGHT:
      clutter_flow_layout_set_row_height (self,
                                          self->priv->min_row_height,
                                          g_value_get_float (value));
      break;

    case PROP_SNAP_TO_GRID:
      clutter_flow_layout_set_snap_to_grid (self,
                                            g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_flow_layout_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, priv->is_homogeneous);
      break;

    case PROP_COLUMN_SPACING:
      g_value_set_float (value, priv->col_spacing);
      break;

    case PROP_ROW_SPACING:
      g_value_set_float (value, priv->row_spacing);
      break;

    case PROP_MIN_COLUMN_WIDTH:
      g_value_set_float (value, priv->min_col_width);
      break;

    case PROP_MAX_COLUMN_WIDTH:
      g_value_set_float (value, priv->max_col_width);
      break;

    case PROP_MIN_ROW_HEGHT:
      g_value_set_float (value, priv->min_row_height);
      break;

    case PROP_MAX_ROW_HEIGHT:
      g_value_set_float (value, priv->max_row_height);
      break;

    case PROP_SNAP_TO_GRID:
      g_value_set_boolean (value, priv->snap_to_grid);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_flow_layout_finalize (GObject *gobject)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (gobject)->priv;

  if (priv->line_min != NULL)
    g_array_free (priv->line_min, TRUE);

  if (priv->line_natural != NULL)
    g_array_free (priv->line_natural, TRUE);

  G_OBJECT_CLASS (clutter_flow_layout_parent_class)->finalize (gobject);
}

static void
clutter_flow_layout_class_init (ClutterFlowLayoutClass *klass)
{
  GObjectClass *gobject_class;
  ClutterLayoutManagerClass *layout_class;

  gobject_class = G_OBJECT_CLASS (klass);
  layout_class = CLUTTER_LAYOUT_MANAGER_CLASS (klass);

  layout_class->get_preferred_width =
    clutter_flow_layout_get_preferred_width;
  layout_class->get_preferred_height =
    clutter_flow_layout_get_preferred_height;
  layout_class->allocate = clutter_flow_layout_allocate;
  layout_class->set_container = clutter_flow_layout_set_container;

  /**
   * ClutterFlowLayout:orientation:
   *
   * The orientation of the #ClutterFlowLayout. The children
   * of the layout will be layed out following the orientation.
   *
   * This property also controls the overflowing directions
   *
   * Since: 1.2
   */
  flow_properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation",
                       P_("Orientation"),
                       P_("The orientation of the layout"),
                       CLUTTER_TYPE_FLOW_ORIENTATION,
                       CLUTTER_FLOW_HORIZONTAL,
                       CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /**
   * ClutterFlowLayout:homogeneous:
   *
   * Whether each child inside the #ClutterFlowLayout should receive
   * the same allocation
   *
   * Since: 1.2
   */
  flow_properties[PROP_HOMOGENEOUS] =
    g_param_spec_boolean ("homogeneous",
                          P_("Homogeneous"),
                          P_("Whether each item should receive the same allocation"),
                          FALSE,
                          CLUTTER_PARAM_READWRITE);

  /**
   * ClutterFlowLayout:column-spacing:
   *
   * The spacing between columns, in pixels; the value of this
   * property is honoured by horizontal non-overflowing layouts
   * and by vertical overflowing layouts
   *
   * Since: 1.2
   */
  flow_properties[PROP_COLUMN_SPACING] =
    g_param_spec_float ("column-spacing",
                        P_("Column Spacing"),
                        P_("The spacing between columns"),
                        0.0, G_MAXFLOAT,
                        0.0,
                        CLUTTER_PARAM_READWRITE);

  /**
   * ClutterFlowLayout:row-spacing:
   *
   * The spacing between rows, in pixels; the value of this
   * property is honoured by vertical non-overflowing layouts and
   * by horizontal overflowing layouts
   *
   * Since: 1.2
   */
  flow_properties[PROP_ROW_SPACING] =
    g_param_spec_float ("row-spacing",
                        P_("Row Spacing"),
                        P_("The spacing between rows"),
                        0.0, G_MAXFLOAT,
                        0.0,
                        CLUTTER_PARAM_READWRITE);

  /**
   * ClutterFlowLayout:min-column-width:
   *
   * Minimum width for each column in the layout, in pixels
   *
   * Since: 1.2
   */
  flow_properties[PROP_MIN_COLUMN_WIDTH] =
    g_param_spec_float ("min-column-width",
                        P_("Minimum Column Width"),
                        P_("Minimum width for each column"),
                        0.0, G_MAXFLOAT,
                        0.0,
                        CLUTTER_PARAM_READWRITE);

  /**
   * ClutterFlowLayout:max-column-width:
   *
   * Maximum width for each column in the layout, in pixels. If
   * set to -1 the width will be the maximum child width
   *
   * Since: 1.2
   */
  flow_properties[PROP_MAX_COLUMN_WIDTH] =
    g_param_spec_float ("max-column-width",
                        P_("Maximum Column Width"),
                        P_("Maximum width for each column"),
                        -1.0, G_MAXFLOAT,
                        -1.0,
                        CLUTTER_PARAM_READWRITE);

  /**
   * ClutterFlowLayout:min-row-height:
   *
   * Minimum height for each row in the layout, in pixels
   *
   * Since: 1.2
   */
  flow_properties[PROP_MIN_ROW_HEGHT] =
    g_param_spec_float ("min-row-height",
                        P_("Minimum Row Height"),
                        P_("Minimum height for each row"),
                        0.0, G_MAXFLOAT,
                        0.0,
                        CLUTTER_PARAM_READWRITE);

  /**
   * ClutterFlowLayout:max-row-height:
   *
   * Maximum height for each row in the layout, in pixels. If
   * set to -1 the width will be the maximum child height
   *
   * Since: 1.2
   */
  flow_properties[PROP_MAX_ROW_HEIGHT] =
    g_param_spec_float ("max-row-height",
                        P_("Maximum Row Height"),
                        P_("Maximum height for each row"),
                        -1.0, G_MAXFLOAT,
                        -1.0,
                        CLUTTER_PARAM_READWRITE);

  /**
   * ClutterFlowLayout:snap-to-grid:
   *
   * Whether the #ClutterFlowLayout should arrange its children
   * on a grid
   *
   * Since: 1.16
   */
  flow_properties[PROP_SNAP_TO_GRID] =
    g_param_spec_boolean ("snap-to-grid",
                          P_("Snap to grid"),
                          P_("Snap to grid"),
                          TRUE,
                          CLUTTER_PARAM_READWRITE);

  gobject_class->finalize = clutter_flow_layout_finalize;
  gobject_class->set_property = clutter_flow_layout_set_property;
  gobject_class->get_property = clutter_flow_layout_get_property;
  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     flow_properties);
}

static void
clutter_flow_layout_init (ClutterFlowLayout *self)
{
  ClutterFlowLayoutPrivate *priv;

  self->priv = priv = clutter_flow_layout_get_instance_private (self);

  priv->orientation = CLUTTER_FLOW_HORIZONTAL;

  priv->col_spacing = 0;
  priv->row_spacing = 0;

  priv->min_col_width = priv->min_row_height = 0;
  priv->max_col_width = priv->max_row_height = -1;

  priv->line_min = NULL;
  priv->line_natural = NULL;
  priv->snap_to_grid = TRUE;
}

/**
 * clutter_flow_layout_new:
 * @orientation: the orientation of the flow layout
 *
 * Creates a new #ClutterFlowLayout with the given @orientation
 *
 * Return value: the newly created #ClutterFlowLayout
 *
 * Since: 1.2
 */
ClutterLayoutManager *
clutter_flow_layout_new (ClutterFlowOrientation orientation)
{
  return g_object_new (CLUTTER_TYPE_FLOW_LAYOUT,
                       "orientation", orientation,
                       NULL);
}

/**
 * clutter_flow_layout_set_orientation:
 * @layout: a #ClutterFlowLayout
 * @orientation: the orientation of the layout
 *
 * Sets the orientation of the flow layout
 *
 * The orientation controls the direction used to allocate
 * the children: either horizontally or vertically. The
 * orientation also controls the direction of the overflowing
 *
 * Since: 1.2
 */
void
clutter_flow_layout_set_orientation (ClutterFlowLayout      *layout,
                                     ClutterFlowOrientation  orientation)
{
  ClutterFlowLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  priv = layout->priv;

  if (priv->orientation != orientation)
    {
      ClutterLayoutManager *manager;

      priv->orientation = orientation;

      if (priv->container != NULL)
        {
          ClutterRequestMode request_mode;

          /* we need to change the :request-mode of the container
           * to match the orientation
           */
          request_mode = (priv->orientation == CLUTTER_FLOW_HORIZONTAL)
                       ? CLUTTER_REQUEST_HEIGHT_FOR_WIDTH
                       : CLUTTER_REQUEST_WIDTH_FOR_HEIGHT;
          clutter_actor_set_request_mode (CLUTTER_ACTOR (priv->container),
                                          request_mode);
        }

      manager = CLUTTER_LAYOUT_MANAGER (layout);
      clutter_layout_manager_layout_changed (manager);

      g_object_notify_by_pspec (G_OBJECT (layout),
                                flow_properties[PROP_ORIENTATION]);
    }
}

/**
 * clutter_flow_layout_get_orientation:
 * @layout: a #ClutterFlowLayout
 *
 * Retrieves the orientation of the @layout
 *
 * Return value: the orientation of the #ClutterFlowLayout
 *
 * Since: 1.2
 */
ClutterFlowOrientation
clutter_flow_layout_get_orientation (ClutterFlowLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout),
                        CLUTTER_FLOW_HORIZONTAL);

  return layout->priv->orientation;
}

/**
 * clutter_flow_layout_set_homogeneous:
 * @layout: a #ClutterFlowLayout
 * @homogeneous: whether the layout should be homogeneous or not
 *
 * Sets whether the @layout should allocate the same space for
 * each child
 *
 * Since: 1.2
 */
void
clutter_flow_layout_set_homogeneous (ClutterFlowLayout *layout,
                                     gboolean           homogeneous)
{
  ClutterFlowLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  priv = layout->priv;

  if (priv->is_homogeneous != homogeneous)
    {
      ClutterLayoutManager *manager;

      priv->is_homogeneous = homogeneous;

      manager = CLUTTER_LAYOUT_MANAGER (layout);
      clutter_layout_manager_layout_changed (manager);

      g_object_notify_by_pspec (G_OBJECT (layout),
                                flow_properties[PROP_HOMOGENEOUS]);
    }
}

/**
 * clutter_flow_layout_get_homogeneous:
 * @layout: a #ClutterFlowLayout
 *
 * Retrieves whether the @layout is homogeneous
 *
 * Return value: %TRUE if the #ClutterFlowLayout is homogeneous
 *
 * Since: 1.2
 */
gboolean
clutter_flow_layout_get_homogeneous (ClutterFlowLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout), FALSE);

  return layout->priv->is_homogeneous;
}

/**
 * clutter_flow_layout_set_column_spacing:
 * @layout: a #ClutterFlowLayout
 * @spacing: the space between columns
 *
 * Sets the space between columns, in pixels
 *
 * Since: 1.2
 */
void
clutter_flow_layout_set_column_spacing (ClutterFlowLayout *layout,
                                        gfloat             spacing)
{
  ClutterFlowLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  priv = layout->priv;

  if (priv->col_spacing != spacing)
    {
      ClutterLayoutManager *manager;

      priv->col_spacing = spacing;

      manager = CLUTTER_LAYOUT_MANAGER (layout);
      clutter_layout_manager_layout_changed (manager);

      g_object_notify_by_pspec (G_OBJECT (layout),
                                flow_properties[PROP_COLUMN_SPACING]);
    }
}

/**
 * clutter_flow_layout_get_column_spacing:
 * @layout: a #ClutterFlowLayout
 *
 * Retrieves the spacing between columns
 *
 * Return value: the spacing between columns of the #ClutterFlowLayout,
 *   in pixels
 *
 * Since: 1.2
 */
gfloat
clutter_flow_layout_get_column_spacing (ClutterFlowLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout), 0.0);

  return layout->priv->col_spacing;
}

/**
 * clutter_flow_layout_set_row_spacing:
 * @layout: a #ClutterFlowLayout
 * @spacing: the space between rows
 *
 * Sets the spacing between rows, in pixels
 *
 * Since: 1.2
 */
void
clutter_flow_layout_set_row_spacing (ClutterFlowLayout *layout,
                                     gfloat             spacing)
{
  ClutterFlowLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  priv = layout->priv;

  if (priv->row_spacing != spacing)
    {
      ClutterLayoutManager *manager;

      priv->row_spacing = spacing;

      manager = CLUTTER_LAYOUT_MANAGER (layout);
      clutter_layout_manager_layout_changed (manager);

      g_object_notify_by_pspec (G_OBJECT (layout),
                                flow_properties[PROP_ROW_SPACING]);
    }
}

/**
 * clutter_flow_layout_get_row_spacing:
 * @layout: a #ClutterFlowLayout
 *
 * Retrieves the spacing between rows
 *
 * Return value: the spacing between rows of the #ClutterFlowLayout,
 *   in pixels
 *
 * Since: 1.2
 */
gfloat
clutter_flow_layout_get_row_spacing (ClutterFlowLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout), 0.0);

  return layout->priv->row_spacing;
}

/**
 * clutter_flow_layout_set_column_width:
 * @layout: a #ClutterFlowLayout
 * @min_width: minimum width of a column
 * @max_width: maximum width of a column
 *
 * Sets the minimum and maximum widths that a column can have
 *
 * Since: 1.2
 */
void
clutter_flow_layout_set_column_width (ClutterFlowLayout *layout,
                                      gfloat             min_width,
                                      gfloat             max_width)
{
  ClutterFlowLayoutPrivate *priv;
  gboolean notify_min = FALSE, notify_max = FALSE;

  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  priv = layout->priv;

  if (priv->min_col_width != min_width)
    {
      priv->min_col_width = min_width;

      notify_min = TRUE;
    }

  if (priv->max_col_width != max_width)
    {
      priv->max_col_width = max_width;

      notify_max = TRUE;
    }

  g_object_freeze_notify (G_OBJECT (layout));

  if (notify_min || notify_max)
    {
      ClutterLayoutManager *manager = CLUTTER_LAYOUT_MANAGER (layout);

      clutter_layout_manager_layout_changed (manager);
    }

  if (notify_min)
    g_object_notify_by_pspec (G_OBJECT (layout),
                              flow_properties[PROP_MIN_COLUMN_WIDTH]);

  if (notify_max)
    g_object_notify_by_pspec (G_OBJECT (layout),
                              flow_properties[PROP_MAX_COLUMN_WIDTH]);

  g_object_thaw_notify (G_OBJECT (layout));
}

/**
 * clutter_flow_layout_get_column_width:
 * @layout: a #ClutterFlowLayout
 * @min_width: (out): return location for the minimum column width, or %NULL
 * @max_width: (out): return location for the maximum column width, or %NULL
 *
 * Retrieves the minimum and maximum column widths
 *
 * Since: 1.2
 */
void
clutter_flow_layout_get_column_width (ClutterFlowLayout *layout,
                                      gfloat            *min_width,
                                      gfloat            *max_width)
{
  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  if (min_width)
    *min_width = layout->priv->min_col_width;

  if (max_width)
    *max_width = layout->priv->max_col_width;
}

/**
 * clutter_flow_layout_set_row_height:
 * @layout: a #ClutterFlowLayout
 * @min_height: the minimum height of a row
 * @max_height: the maximum height of a row
 *
 * Sets the minimum and maximum heights that a row can have
 *
 * Since: 1.2
 */
void
clutter_flow_layout_set_row_height (ClutterFlowLayout *layout,
                                    gfloat              min_height,
                                    gfloat              max_height)
{
  ClutterFlowLayoutPrivate *priv;
  gboolean notify_min = FALSE, notify_max = FALSE;

  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  priv = layout->priv;

  if (priv->min_row_height != min_height)
    {
      priv->min_row_height = min_height;

      notify_min = TRUE;
    }

  if (priv->max_row_height != max_height)
    {
      priv->max_row_height = max_height;

      notify_max = TRUE;
    }

  g_object_freeze_notify (G_OBJECT (layout));

  if (notify_min || notify_max)
    {
      ClutterLayoutManager *manager = CLUTTER_LAYOUT_MANAGER (layout);

      clutter_layout_manager_layout_changed (manager);
    }

  if (notify_min)
    g_object_notify_by_pspec (G_OBJECT (layout),
                              flow_properties[PROP_MIN_ROW_HEGHT]);

  if (notify_max)
    g_object_notify_by_pspec (G_OBJECT (layout),
                              flow_properties[PROP_MAX_ROW_HEIGHT]);

  g_object_thaw_notify (G_OBJECT (layout));
}

/**
 * clutter_flow_layout_get_row_height:
 * @layout: a #ClutterFlowLayout
 * @min_height: (out): return location for the minimum row height, or %NULL
 * @max_height: (out): return location for the maximum row height, or %NULL
 *
 * Retrieves the minimum and maximum row heights
 *
 * Since: 1.2
 */
void
clutter_flow_layout_get_row_height (ClutterFlowLayout *layout,
                                    gfloat            *min_height,
                                    gfloat            *max_height)
{
  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  if (min_height)
    *min_height = layout->priv->min_row_height;

  if (max_height)
    *max_height = layout->priv->max_row_height;
}

/**
 * clutter_flow_layout_set_snap_to_grid:
 * @layout: a #ClutterFlowLayout
 * @snap_to_grid: %TRUE if @layout should place its children on a grid
 *
 * Whether the @layout should place its children on a grid.
 *
 * Since: 1.16
 */
void
clutter_flow_layout_set_snap_to_grid (ClutterFlowLayout *layout,
                                      gboolean           snap_to_grid)
{
  ClutterFlowLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  priv = layout->priv;

  if (priv->snap_to_grid != snap_to_grid)
    {
      priv->snap_to_grid = snap_to_grid;

      clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (layout));

      g_object_notify_by_pspec (G_OBJECT (layout),
                                flow_properties[PROP_SNAP_TO_GRID]);
    }
}

/**
 * clutter_flow_layout_get_snap_to_grid:
 * @layout: a #ClutterFlowLayout
 *
 * Retrieves the value of #ClutterFlowLayout:snap-to-grid property
 *
 * Return value: %TRUE if the @layout is placing its children on a grid
 *
 * Since: 1.16
 */
gboolean
clutter_flow_layout_get_snap_to_grid (ClutterFlowLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout), FALSE);

  return layout->priv->snap_to_grid;
}
