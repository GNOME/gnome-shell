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
 * </itemizedlist>
 *
 * #ClutterFlowLayout is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-actor.h"
#include "clutter-animatable.h"
#include "clutter-child-meta.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-flow-layout.h"
#include "clutter-layout-meta.h"
#include "clutter-private.h"

#define CLUTTER_FLOW_LAYOUT_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_FLOW_LAYOUT, ClutterFlowLayoutPrivate))

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

  /* cache the preferred size; this way, if we get what we asked
   * or more then we don't have to recompute the layout
   */
  gfloat request_width;
  gfloat request_height;

  gint max_row_items;

  guint layout_wrap : 1;
};

enum
{
  PROP_0,

  PROP_ORIENTATION,

  PROP_COLUMN_SPACING,
  PROP_ROW_SPACING,

  PROP_MIN_COLUMN_WIDTH,
  PROP_MAX_COLUMN_WIDTH,
  PROP_MIN_ROW_HEGHT,
  PROP_MAX_ROW_HEIGHT,

  PROP_WRAP
};

G_DEFINE_TYPE (ClutterFlowLayout,
               clutter_flow_layout,
               CLUTTER_TYPE_LAYOUT_MANAGER);

static void
clutter_flow_layout_get_preferred_width (ClutterLayoutManager *manager,
                                         ClutterContainer     *container,
                                         gfloat                for_height,
                                         gfloat               *min_width_p,
                                         gfloat               *nat_width_p)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (manager)->priv;
  GList *l, *children = clutter_container_get_children (container);
  gfloat max_child_min_width, max_child_natural_width;
  gfloat row_natural_width;

  max_child_min_width = max_child_natural_width = 0;
  row_natural_width = 0;

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat child_min, child_natural;

      clutter_actor_get_preferred_width (child, for_height,
                                         &child_min,
                                         &child_natural);

      max_child_min_width = MAX (max_child_min_width, child_min);
      max_child_natural_width = MAX (max_child_natural_width, child_natural);

      if (priv->orientation == CLUTTER_FLOW_HORIZONTAL)
        row_natural_width += (child_natural + priv->col_spacing);
      else
        row_natural_width = MAX (row_natural_width, max_child_natural_width);
    }

  g_list_free (children);

  priv->request_width = row_natural_width;
  priv->col_width = max_child_natural_width;

  if (priv->max_col_width > 0 && priv->col_width > priv->max_col_width)
    priv->col_width = MAX (priv->max_col_width, max_child_min_width);

  if (priv->col_width < priv->min_col_width)
    priv->col_width = priv->min_col_width;

  if (min_width_p)
    *min_width_p = ceilf (max_child_min_width);

  if (nat_width_p)
    *nat_width_p = ceilf (row_natural_width);
}

static void
clutter_flow_layout_get_preferred_height (ClutterLayoutManager *manager,
                                          ClutterContainer     *container,
                                          gfloat                for_width,
                                          gfloat               *min_height_p,
                                          gfloat               *nat_height_p)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (manager)->priv;
  GList *l, *children = clutter_container_get_children (container);
  gfloat max_child_min_height, max_child_natural_height;
  gfloat col_natural_height;

  max_child_min_height = max_child_natural_height = 0;
  col_natural_height = 0;

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat child_min, child_natural;

      clutter_actor_get_preferred_height (child, for_width,
                                          &child_min,
                                          &child_natural);

      max_child_min_height = MAX (max_child_min_height, child_min);
      max_child_natural_height = MAX (max_child_natural_height, child_natural);

      if (priv->orientation == CLUTTER_FLOW_VERTICAL)
        col_natural_height += (child_natural + priv->row_spacing);
      else
        col_natural_height = MAX (col_natural_height, max_child_natural_height);
    }

  g_list_free (children);

  priv->request_height = col_natural_height;
  priv->row_height = max_child_natural_height;

  if (priv->max_row_height > 0 && priv->row_height > priv->max_row_height)
    priv->row_height = MAX (priv->max_row_height, max_child_min_height);

  if (priv->row_height < priv->min_row_height)
    priv->row_height = priv->min_row_height;

  if (min_height_p)
    *min_height_p = ceilf (max_child_min_height);

  if (nat_height_p)
    *nat_height_p = ceilf (col_natural_height);
}

static gint
compute_lines (ClutterFlowLayout *self,
               const GList       *children,
               gfloat             avail_width,
               gfloat             avail_height)
{
  ClutterFlowLayoutPrivate *priv = self->priv;
  gint items_per_line;

  if (priv->orientation == CLUTTER_FLOW_HORIZONTAL)
    items_per_line = avail_width / (priv->col_width + priv->col_spacing);
  else
    items_per_line = avail_height / (priv->row_height + priv->row_spacing);

  return items_per_line;
}

static void
clutter_flow_layout_allocate (ClutterLayoutManager   *manager,
                              ClutterContainer       *container,
                              const ClutterActorBox  *allocation,
                              ClutterAllocationFlags  flags)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (manager)->priv;
  GList *l, *children = clutter_container_get_children (container);
  gfloat avail_width, avail_height;
  gfloat item_x, item_y;
  gint line_items_count;
  gint items_per_line;
  gint line_index;

  if (children == NULL)
    return;

  clutter_actor_box_get_size (allocation, &avail_width, &avail_height);

  items_per_line = compute_lines (CLUTTER_FLOW_LAYOUT (manager),
                                  children,
                                  avail_width, avail_height);

  item_x = item_y = 0;

  line_items_count = 0;
  line_index = 0;

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      ClutterActorBox child_alloc;
      gfloat item_width, item_height;
      gfloat child_min, child_natural;

      if (line_items_count == items_per_line)
        {
          if (priv->orientation == CLUTTER_FLOW_HORIZONTAL)
            {
              item_x = 0;
              item_y += priv->row_height + priv->row_spacing;
            }
          else
            {
              item_x += priv->col_width + priv->col_spacing;
              item_y = 0;
            }

          line_items_count = 0;
          line_index += 1;
        }

      clutter_actor_get_preferred_width (child, priv->row_height,
                                         &child_min,
                                         &child_natural);
      item_width = MIN (child_natural, priv->col_width);

      clutter_actor_get_preferred_height (child, item_width,
                                          &child_min,
                                          &child_natural);
      item_height = MIN (child_natural, priv->row_height);

      CLUTTER_NOTE (LAYOUT,
                    "flow[line:%d, item:%d/%d] = { %.2f, %.2f, %.2f, %.2f }",
                    line_index, line_items_count, items_per_line,
                    item_x, item_y, item_width, item_height);

      child_alloc.x1 = ceil (item_x);
      child_alloc.y1 = ceil (item_y);
      child_alloc.x2 = ceil (child_alloc.x1 + item_width);
      child_alloc.y2 = ceil (child_alloc.y1 + item_height);
      clutter_actor_allocate (child, &child_alloc, flags);

      if (priv->orientation == CLUTTER_FLOW_HORIZONTAL)
        item_x += item_width;
      else
        item_y += item_height;

      line_items_count += 1;
    }

  g_list_free (children);
}

static void
clutter_flow_layout_set_container (ClutterLayoutManager *manager,
                                   ClutterContainer     *container)
{
  ClutterFlowLayoutPrivate *priv = CLUTTER_FLOW_LAYOUT (manager)->priv;

  priv->container = container;
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

    case PROP_WRAP:
      clutter_flow_layout_set_wrap (self, g_value_get_boolean (value));
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

    case PROP_WRAP:
      g_value_set_boolean (value, priv->layout_wrap);
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_flow_layout_class_init (ClutterFlowLayoutClass *klass)
{
  GObjectClass *gobject_class;
  ClutterLayoutManagerClass *layout_class;
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (ClutterFlowLayoutPrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  layout_class = CLUTTER_LAYOUT_MANAGER_CLASS (klass);

  gobject_class->set_property = clutter_flow_layout_set_property;
  gobject_class->get_property = clutter_flow_layout_get_property;

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
   * If #ClutterFlowLayout:wrap is set to %TRUE then this property
   * will control the primary direction of the layout before
   * wrapping takes place
   *
   * Since: 1.2
   */
  pspec = g_param_spec_enum ("orientation",
                             "Orientation",
                             "The orientation of the layout",
                             CLUTTER_TYPE_FLOW_ORIENTATION,
                             CLUTTER_FLOW_HORIZONTAL,
                             CLUTTER_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT);
  g_object_class_install_property (gobject_class, PROP_ORIENTATION, pspec);

  /**
   * ClutterFlowLayout:wrap:
   *
   * Whether the layout should wrap the children to fit them
   * in the allocation. A non-wrapping layout has a preferred
   * size of the biggest child in the direction opposite to the
   * #ClutterFlowLayout:orientation property, and the sum of
   * the preferred sizes (taking into account spacing) of the
   * children in the direction of the orientation
   *
   * Since: 1.2
   */
  pspec = g_param_spec_boolean ("wrap",
                                "Wrap",
                                "Whether the layout should wrap",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_WRAP, pspec);

  /**
   * ClutterFlowLayout:column-spacing:
   *
   * The spacing between columns, in pixels; the value of this
   * property is honoured by horizontal non-wrapping layouts and
   * by vertical wrapping layouts
   *
   * Since: 1.2
   */
  pspec = g_param_spec_float ("column-spacing",
                              "Column Spacing",
                              "The spacing between columns",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_COLUMN_SPACING,
                                   pspec);

  /**
   * ClutterFlowLayout:row-spacing:
   *
   * The spacing between rows, in pixels; the value of this
   * property is honoured by vertical non-wrapping layouts and
   * by horizontal wrapping layouts
   *
   * Since: 1.2
   */
  pspec = g_param_spec_float ("row-spacing",
                              "Row Spacing",
                              "The spacing between rows",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_ROW_SPACING,
                                   pspec);

  /**
   * ClutterFlowLayout:min-column-width:
   *
   * Minimum width for each column in the layout, in pixels
   *
   * Since: 1.2
   */
  pspec = g_param_spec_float ("min-column-width",
                              "Minimum Column Width",
                              "Minimum width for each column",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_MIN_COLUMN_WIDTH,
                                   pspec);

  /**
   * ClutterFlowLayout:max-column-width:
   *
   * Maximum width for each column in the layout, in pixels. If
   * set to -1 the width will be the maximum child width
   *
   * Since: 1.2
   */
  pspec = g_param_spec_float ("max-column-width",
                              "Maximum Column Width",
                              "Maximum width for each column",
                              -1.0, G_MAXFLOAT,
                              -1.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_COLUMN_WIDTH,
                                   pspec);

  /**
   * ClutterFlowLayout:min-row-height:
   *
   * Minimum height for each row in the layout, in pixels
   *
   * Since: 1.2
   */
  pspec = g_param_spec_float ("min-row-height",
                              "Minimum Row Height",
                              "Minimum height for each row",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_MIN_ROW_HEGHT,
                                   pspec);

  /**
   * ClutterFlowLayout:max-row-height:
   *
   * Maximum height for each row in the layout, in pixels. If
   * set to -1 the width will be the maximum child height
   *
   * Since: 1.2
   */
  pspec = g_param_spec_float ("max-row-height",
                              "Maximum Row Height",
                              "Maximum height for each row",
                              -1.0, G_MAXFLOAT,
                              -1.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_ROW_HEIGHT,
                                   pspec);
}

static void
clutter_flow_layout_init (ClutterFlowLayout *self)
{
  ClutterFlowLayoutPrivate *priv;

  self->priv = priv = CLUTTER_FLOW_LAYOUT_GET_PRIVATE (self);

  priv->orientation = CLUTTER_FLOW_HORIZONTAL;

  priv->col_spacing = 0;
  priv->row_spacing = 0;

  priv->min_col_width = priv->min_row_height = 0;
  priv->max_col_width = priv->max_row_height = -1;
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

      manager = CLUTTER_LAYOUT_MANAGER (layout);
      clutter_layout_manager_layout_changed (manager);

      g_object_notify (G_OBJECT (layout), "orientation");
    }
}

ClutterFlowOrientation
clutter_flow_layout_get_orientation (ClutterFlowLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout),
                        CLUTTER_FLOW_HORIZONTAL);

  return layout->priv->orientation;
}

void
clutter_flow_layout_set_wrap (ClutterFlowLayout *layout,
                              gboolean           wrap)
{
  ClutterFlowLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout));

  priv = layout->priv;

  if (priv->layout_wrap != wrap)
    {
      ClutterLayoutManager *manager;

      priv->layout_wrap = wrap;

      manager = CLUTTER_LAYOUT_MANAGER (layout);
      clutter_layout_manager_layout_changed (manager);

      g_object_notify (G_OBJECT (layout), "wrap");
    }
}

gboolean
clutter_flow_layout_get_wrap (ClutterFlowLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout), FALSE);

  return layout->priv->layout_wrap;
}

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

      g_object_notify (G_OBJECT (layout), "column-spacing");
    }
}

gfloat
clutter_flow_layout_get_column_spacing (ClutterFlowLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout), 0.0);

  return layout->priv->col_spacing;
}

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

      g_object_notify (G_OBJECT (layout), "row-spacing");
    }
}

gfloat
clutter_flow_layout_get_row_spacing (ClutterFlowLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_FLOW_LAYOUT (layout), 0.0);

  return layout->priv->row_spacing;
}

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

  if (notify_min || notify_max)
    {
      ClutterLayoutManager *manager = CLUTTER_LAYOUT_MANAGER (layout);

      clutter_layout_manager_layout_changed (manager);
    }

  if (notify_min)
    g_object_notify (G_OBJECT (layout), "min-column-width");

  if (notify_max)
    g_object_notify (G_OBJECT (layout), "max-column-width");
}

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

  if (notify_min || notify_max)
    {
      ClutterLayoutManager *manager = CLUTTER_LAYOUT_MANAGER (layout);

      clutter_layout_manager_layout_changed (manager);
    }

  if (notify_min)
    g_object_notify (G_OBJECT (layout), "min-row-height");

  if (notify_max)
    g_object_notify (G_OBJECT (layout), "max-row-height");
}

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
