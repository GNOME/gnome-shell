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
 *   Jose Dapena Paz <jdapena@igalia.com>
 *
 * Based on the MX MxTable actor by:
 *   Thomas Wood <thomas.wood@intel.com>
 * and ClutterBoxLayout by:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-table-layout
 * @title: ClutterTableLayout
 * @short_description: A layout manager arranging children in rows
 *   and columns
 *
 * The #ClutterTableLayout is a #ClutterLayoutManager implementing the
 * following layout policy:
 *
 * <itemizedlist>
 *   <listitem><para>children are arranged in a table</para></listitem>
 *   <listitem><para>each child specifies the specific row and column
 *   cell to appear;</para></listitem>
 *   <listitem><para>a child can also set a span, and this way, take
 *   more than one cell both horizontally and vertically;</para></listitem>
 *   <listitem><para>each child will be allocated to its natural
 *   size or, if set to expand, the available size;</para></listitem>
 *   <listitem><para>if a child is set to fill on either (or both)
 *   axis, its allocation will match all the available size; the
 *   fill layout property only makes sense if the expand property is
 *   also set;</para></listitem>
 *   <listitem><para>if a child is set to expand but not to fill then
 *   it is possible to control the alignment using the horizontal and
 *   vertical alignment layout properties.</para></listitem>
 * </itemizedlist>
 *
 * It is possible to control the spacing between children of a
 * #ClutterTableLayout by using clutter_table_layout_set_row_spacing()
 * and clutter_table_layout_set_column_spacing().
 *
 * In order to set the layout properties when packing an actor inside a
 * #ClutterTableLayout you should use the clutter_table_layout_pack()
 * function.
 *
 * A #ClutterTableLayout can use animations to transition between different
 * values of the layout management properties; the easing mode and duration
 * used for the animations are controlled by the
 * #ClutterTableLayout:easing-mode and #ClutterTableLayout:easing-duration
 * properties and their accessor functions.
 *
 * <figure id="table-layout-image">
 *   <title>Table layout</title>
 *   <para>The image shows a #ClutterTableLayout.</para>
 *   <graphic fileref="table-layout.png" format="PNG"/>
 * </figure>
 *
 * #ClutterTableLayout is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-container.h"
#include "deprecated/clutter-alpha.h"

#include "clutter-table-layout.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-layout-meta.h"
#include "clutter-private.h"
#include "clutter-types.h"

#define CLUTTER_TYPE_TABLE_CHILD          (clutter_table_child_get_type ())
#define CLUTTER_TABLE_CHILD(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TABLE_CHILD, ClutterTableChild))
#define CLUTTER_IS_TABLE_CHILD(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TABLE_CHILD))

typedef struct _ClutterTableChild         ClutterTableChild;
typedef struct _ClutterLayoutMetaClass    ClutterTableChildClass;

typedef struct _DimensionData {
  gfloat min_size;
  gfloat pref_size;
  gfloat final_size;

  guint expand  : 1;
  guint visible : 1;
} DimensionData;

struct _ClutterTableLayoutPrivate
{
  ClutterContainer *container;

  guint col_spacing;
  guint row_spacing;

  gint n_rows;
  gint n_cols;
  gint active_row;
  gint active_col;
  gint visible_rows;
  gint visible_cols;

  GArray *columns;
  GArray *rows;

  gulong easing_mode;
  guint easing_duration;

  guint is_animating   : 1;
  guint use_animations : 1;
};

struct _ClutterTableChild
{
  ClutterLayoutMeta parent_instance;

  gint col;
  gint row;

  gint col_span;
  gint row_span;

  ClutterTableAlignment x_align;
  ClutterTableAlignment y_align;

  guint x_expand            : 1;
  guint y_expand            : 1;
  guint x_fill              : 1;
  guint y_fill              : 1;
};

enum
{
  PROP_CHILD_0,

  PROP_CHILD_ROW,
  PROP_CHILD_COLUMN,
  PROP_CHILD_ROW_SPAN,
  PROP_CHILD_COLUMN_SPAN,
  PROP_CHILD_X_ALIGN,
  PROP_CHILD_Y_ALIGN,
  PROP_CHILD_X_FILL,
  PROP_CHILD_Y_FILL,
  PROP_CHILD_X_EXPAND,
  PROP_CHILD_Y_EXPAND
};

enum
{
  PROP_0,

  PROP_ROW_SPACING,
  PROP_COLUMN_SPACING,
  PROP_USE_ANIMATIONS,
  PROP_EASING_MODE,
  PROP_EASING_DURATION
};

GType clutter_table_child_get_type (void);

G_DEFINE_TYPE (ClutterTableChild, clutter_table_child, CLUTTER_TYPE_LAYOUT_META)

G_DEFINE_TYPE_WITH_PRIVATE (ClutterTableLayout, clutter_table_layout, CLUTTER_TYPE_LAYOUT_MANAGER)

/*
 * ClutterBoxChild
 */

static void
table_child_set_position (ClutterTableChild *self,
                          gint               col,
                          gint               row)
{
  gboolean row_changed = FALSE, col_changed = FALSE;

  if (self->col != col)
    {
      self->col = col;

      col_changed = TRUE;
    }

  if (self->row != row)
    {
      self->row = row;

      row_changed = TRUE;
    }

  if (row_changed || col_changed)
    {
      ClutterLayoutManager *layout;

      layout = clutter_layout_meta_get_manager (CLUTTER_LAYOUT_META (self));
      clutter_layout_manager_layout_changed (layout);

      g_object_freeze_notify (G_OBJECT (self));

      if (row_changed)
        g_object_notify (G_OBJECT (self), "row");

      if (col_changed)
        g_object_notify (G_OBJECT (self), "column");

      g_object_thaw_notify (G_OBJECT (self));
    }
}

static void
table_child_set_span (ClutterTableChild  *self,
                      gint                col_span,
                      gint                row_span)
{
  gboolean row_changed = FALSE, col_changed = FALSE;

  if (self->col_span != col_span)
    {
      self->col_span = col_span;

      col_changed = TRUE;
    }

  if (self->row_span != row_span)
    {
      self->row_span = row_span;

      row_changed = TRUE;
    }

  if (row_changed || col_changed)
    {
      ClutterLayoutManager *layout;

      layout = clutter_layout_meta_get_manager (CLUTTER_LAYOUT_META (self));
      clutter_layout_manager_layout_changed (layout);

      if (row_changed)
        g_object_notify (G_OBJECT (self), "row-span");

      if (col_changed)
        g_object_notify (G_OBJECT (self), "column-span");
    }
}

static void
table_child_set_align (ClutterTableChild     *self,
                       ClutterTableAlignment  x_align,
                       ClutterTableAlignment  y_align)
{
  gboolean x_changed = FALSE, y_changed = FALSE;

  if (self->x_align != x_align)
    {
      self->x_align = x_align;

      x_changed = TRUE;
    }

  if (self->y_align != y_align)
    {
      self->y_align = y_align;

      y_changed = TRUE;
    }

  if (x_changed || y_changed)
    {
      ClutterLayoutManager *layout;

      layout = clutter_layout_meta_get_manager (CLUTTER_LAYOUT_META (self));
      clutter_layout_manager_layout_changed (layout);

      g_object_freeze_notify (G_OBJECT (self));

      if (x_changed)
        g_object_notify (G_OBJECT (self), "x-align");

      if (y_changed)
        g_object_notify (G_OBJECT (self), "y-align");

      g_object_thaw_notify (G_OBJECT (self));
    }
}

static void
table_child_set_fill (ClutterTableChild *self,
                      gboolean           x_fill,
                      gboolean           y_fill)
{
  gboolean x_changed = FALSE, y_changed = FALSE;

  x_fill = !!x_fill;
  y_fill = !!y_fill;

  if (self->x_fill != x_fill)
    {
      self->x_fill = x_fill;

      x_changed = TRUE;
    }

  if (self->y_fill != y_fill)
    {
      self->y_fill = y_fill;

      y_changed = TRUE;
    }

  if (x_changed || y_changed)
    {
      ClutterLayoutManager *layout;

      layout = clutter_layout_meta_get_manager (CLUTTER_LAYOUT_META (self));
      clutter_layout_manager_layout_changed (layout);

      g_object_freeze_notify (G_OBJECT (self));

      if (x_changed)
        g_object_notify (G_OBJECT (self), "x-fill");

      if (y_changed)
        g_object_notify (G_OBJECT (self), "y-fill");

      g_object_thaw_notify (G_OBJECT (self));
    }
}

static void
table_child_set_expand (ClutterTableChild *self,
                        gboolean           x_expand,
                        gboolean           y_expand)
{
  gboolean x_changed = FALSE, y_changed = FALSE;

  x_expand = !!x_expand;
  y_expand = !!y_expand;

  if (self->x_expand != x_expand)
    {
      self->x_expand = x_expand;

      x_changed = TRUE;
    }

  if (self->y_expand != y_expand)
    {
      self->y_expand = y_expand;

      y_changed = TRUE;
    }

  if (x_changed || y_changed)
    {
      ClutterLayoutManager *layout;

      layout = clutter_layout_meta_get_manager (CLUTTER_LAYOUT_META (self));
      clutter_layout_manager_layout_changed (layout);

      g_object_freeze_notify (G_OBJECT (self));

      if (x_changed)
        g_object_notify (G_OBJECT (self), "x-expand");

      if (y_changed)
        g_object_notify (G_OBJECT (self), "y-expand");

      g_object_thaw_notify (G_OBJECT (self));
    }
}

static void
clutter_table_child_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ClutterTableChild *self = CLUTTER_TABLE_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_COLUMN:
      table_child_set_position (self,
                                g_value_get_int (value),
                                self->row);
      break;

    case PROP_CHILD_ROW:
      table_child_set_position (self,
                                self->col,
                                g_value_get_int (value));
      break;

    case PROP_CHILD_COLUMN_SPAN:
      table_child_set_span (self,
                            g_value_get_int (value),
                            self->row_span);
      break;

    case PROP_CHILD_ROW_SPAN:
      table_child_set_span (self,
                            self->col_span,
                            g_value_get_int (value));
      break;

    case PROP_CHILD_X_ALIGN:
      table_child_set_align (self,
                             g_value_get_enum (value),
                             self->y_align);
      break;

    case PROP_CHILD_Y_ALIGN:
      table_child_set_align (self,
                             self->x_align,
                             g_value_get_enum (value));
      break;

    case PROP_CHILD_X_FILL:
      table_child_set_fill (self,
                            g_value_get_boolean (value),
                            self->y_fill);
      break;

    case PROP_CHILD_Y_FILL:
      table_child_set_fill (self,
                            self->x_fill,
                            g_value_get_boolean (value));
      break;

    case PROP_CHILD_X_EXPAND:
      table_child_set_expand (self,
                              g_value_get_boolean (value),
                              self->y_expand);
      break;

    case PROP_CHILD_Y_EXPAND:
      table_child_set_expand (self,
                              self->x_expand,
                              g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_table_child_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ClutterTableChild *self = CLUTTER_TABLE_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_ROW:
      g_value_set_int (value, self->row);
      break;

    case PROP_CHILD_COLUMN:
      g_value_set_int (value, self->col);
      break;

    case PROP_CHILD_ROW_SPAN:
      g_value_set_int (value, self->row_span);
      break;

    case PROP_CHILD_COLUMN_SPAN:
      g_value_set_int (value, self->col_span);
      break;

    case PROP_CHILD_X_ALIGN:
      g_value_set_enum (value, self->x_align);
      break;

    case PROP_CHILD_Y_ALIGN:
      g_value_set_enum (value, self->y_align);
      break;

    case PROP_CHILD_X_FILL:
      g_value_set_boolean (value, self->x_fill);
      break;

    case PROP_CHILD_Y_FILL:
      g_value_set_boolean (value, self->y_fill);
      break;

    case PROP_CHILD_X_EXPAND:
      g_value_set_boolean (value, self->x_expand);
      break;

    case PROP_CHILD_Y_EXPAND:
      g_value_set_boolean (value, self->y_expand);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_table_child_class_init (ClutterTableChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_table_child_set_property;
  gobject_class->get_property = clutter_table_child_get_property;

  pspec = g_param_spec_int ("column",
                            P_("Column Number"),
                            P_("The column the widget resides in"),
                            0, G_MAXINT,
                            0,
                            CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_COLUMN, pspec);

  pspec = g_param_spec_int ("row",
                            P_("Row Number"),
                            P_("The row the widget resides in"),
                            0, G_MAXINT,
                            0,
                            CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_ROW, pspec);

  pspec = g_param_spec_int ("column-span",
                            P_("Column Span"),
                            P_("The number of columns the widget should span"),
                            1, G_MAXINT,
                            1,
                            CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_COLUMN_SPAN, pspec);

  pspec = g_param_spec_int ("row-span",
                            P_("Row Span"),
                            P_("The number of rows the widget should span"),
                            1, G_MAXINT,
                            1,
                            CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_ROW_SPAN, pspec);

  pspec = g_param_spec_boolean ("x-expand",
                                P_("Horizontal Expand"),
                                P_("Allocate extra space for the child in horizontal axis"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_X_EXPAND, pspec);

  pspec = g_param_spec_boolean ("y-expand",
                                P_("Vertical Expand"),
                                P_("Allocate extra space for the child in vertical axis"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_Y_EXPAND, pspec);

  pspec = g_param_spec_boolean ("x-fill",
                                P_("Horizontal Fill"),
                                P_("Whether the child should receive priority when the container is allocating spare space on the horizontal axis"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_X_FILL, pspec);

  pspec = g_param_spec_boolean ("y-fill",
                                P_("Vertical Fill"),
                                P_("Whether the child should receive priority when the container is allocating spare space on the vertical axis"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_Y_FILL, pspec);

  /**
   * ClutterTableLayout:x-align:
   *
   * The horizontal alignment of the actor within the cell
   *
   * Since: 1.4
   */
  pspec = g_param_spec_enum ("x-align",
                             P_("Horizontal Alignment"),
                             P_("Horizontal alignment of the actor within the cell"),
                             CLUTTER_TYPE_TABLE_ALIGNMENT,
                             CLUTTER_TABLE_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_X_ALIGN, pspec);

  /**
   * ClutterTableLayout:y-align:
   *
   * The vertical alignment of the actor within the cell
   *
   * Since: 1.4
   */
  pspec = g_param_spec_enum ("y-align",
                             P_("Vertical Alignment"),
                             P_("Vertical alignment of the actor within the cell"),
                             CLUTTER_TYPE_TABLE_ALIGNMENT,
                             CLUTTER_TABLE_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_Y_ALIGN, pspec);
}

static void
clutter_table_child_init (ClutterTableChild *self)
{
  self->col_span = 1;
  self->row_span = 1;

  self->x_align = CLUTTER_TABLE_ALIGNMENT_CENTER;
  self->y_align = CLUTTER_TABLE_ALIGNMENT_CENTER;

  self->x_expand = TRUE;
  self->y_expand = TRUE;

  self->x_fill = TRUE;
  self->y_fill = TRUE;
}

static GType
clutter_table_layout_get_child_meta_type (ClutterLayoutManager *manager)
{
  return CLUTTER_TYPE_TABLE_CHILD;
}

static void
clutter_table_layout_set_container (ClutterLayoutManager *layout,
                                    ClutterContainer     *container)
{
  ClutterTableLayoutPrivate *priv = CLUTTER_TABLE_LAYOUT (layout)->priv;

  priv->container = container;
}


static void
update_row_col (ClutterTableLayout *layout,
                ClutterContainer   *container)
{
  ClutterTableLayoutPrivate *priv = layout->priv;
  ClutterLayoutManager *manager = CLUTTER_LAYOUT_MANAGER (layout);
  ClutterActor *actor, *child;
  gint n_cols, n_rows;

  n_cols = n_rows = 0;

  if (container == NULL)
    goto out;

  actor = CLUTTER_ACTOR (container);
  for (child = clutter_actor_get_first_child (actor);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      ClutterTableChild *meta;

      meta =
        CLUTTER_TABLE_CHILD (clutter_layout_manager_get_child_meta (manager,
                                                                    container,
                                                                    child));

      n_cols = MAX (n_cols, meta->col + meta->col_span);
      n_rows = MAX (n_rows, meta->row + meta->row_span);
    }

out:
  priv->n_cols = n_cols;
  priv->n_rows = n_rows;

}

static void
calculate_col_widths (ClutterTableLayout *self,
                      ClutterContainer   *container,
                      gint                for_width)
{
  ClutterTableLayoutPrivate *priv = self->priv;
  ClutterLayoutManager *manager = CLUTTER_LAYOUT_MANAGER (self);
  ClutterActor *actor, *child;
  gint i;
  DimensionData *columns;
  ClutterOrientation orientation = CLUTTER_ORIENTATION_HORIZONTAL;

  update_row_col (self, container);
  g_array_set_size (priv->columns, 0);
  g_array_set_size (priv->columns, priv->n_cols);
  columns = (DimensionData *) (void *) priv->columns->data;

  /* reset the visibility of all columns */
  priv->visible_cols = 0;
  for (i = 0; i < priv->n_cols; i++)
    {
      columns[i].expand = FALSE;
      columns[i].visible = FALSE;
    }

  actor = CLUTTER_ACTOR (container);

  /* STAGE ONE: calculate column widths for non-spanned children */
  for (child = clutter_actor_get_first_child (actor);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      ClutterTableChild *meta;
      DimensionData *col;
      gfloat c_min, c_pref;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      meta =
        CLUTTER_TABLE_CHILD (clutter_layout_manager_get_child_meta (manager,
                                                                    container,
                                                                    child));

      if (meta->col_span > 1)
        continue;

      col = &columns[meta->col];

      if (!col->visible)
        {
          col->visible = TRUE;
          priv->visible_cols += 1;
        }

      clutter_actor_get_preferred_width (child, -1, &c_min, &c_pref);

      col->min_size = MAX (col->min_size, c_min);
      col->pref_size = MAX (col->pref_size, c_pref);

      if (!col->expand)
        {
          col->expand = clutter_actor_needs_expand (child, orientation) ||
            meta->x_expand;
        }
    }

  /* STAGE TWO: take spanning children into account */
  for (child = clutter_actor_get_first_child (actor);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      ClutterTableChild *meta;
      gfloat c_min, c_pref;
      gfloat min_width, pref_width;
      gint start_col, end_col;
      gint n_expand;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      meta =
        CLUTTER_TABLE_CHILD (clutter_layout_manager_get_child_meta (manager,
                                                                    container,
                                                                    child));

      if (meta->col_span < 2)
        continue;

      start_col = meta->col;
      end_col = meta->col + meta->col_span - 1;

      clutter_actor_get_preferred_width (child, -1, &c_min, &c_pref);

      /* check there is enough room for this actor */
      min_width = 0;
      pref_width = 0;
      n_expand = 0;
      for (i = start_col; i <= end_col; i++)
        {
          min_width += columns[i].min_size;
          pref_width += columns[i].pref_size;

          if (columns[i].expand)
            n_expand++;

          if (!columns[i].visible)
            {
              columns[i].visible = TRUE;
              priv->visible_cols += 1;
            }

          if (!columns[i].expand)
            {
              columns[i].expand = clutter_actor_needs_expand (child,
                                                              orientation) ||
                meta->x_expand;
            }
        }
      min_width += priv->col_spacing * (meta->col_span - 1);
      pref_width += priv->col_spacing * (meta->col_span - 1);

      /* see calculate_row_heights() for comments */
      /* (1) */
      if (c_min > min_width)
        {

          /* (2) */
          /* we can start from preferred width and decrease */
          if (pref_width > c_min)
            {
              for (i = start_col; i <= end_col; i++)
                columns[i].final_size = columns[i].pref_size;

              while (pref_width > c_min)
                {
                  for (i = start_col; i <= end_col; i++)
                    {
                      if (columns[i].final_size > columns[i].min_size)
                        {
                          columns[i].final_size--;
                          pref_width--;
                        }
                    }
                }

              for (i = start_col; i <= end_col; i++)
                columns[i].min_size = columns[i].final_size;
            }
          else
            {
              /* (3) */
              /* we can expand from preferred size */
              gfloat expand_by;

              expand_by = c_pref - pref_width;

              for (i = start_col; i <= end_col; i++)
                {
                  if (n_expand)
                    {
                      if (columns[i].expand)
                        columns[i].min_size = columns[i].pref_size
                                            + expand_by / n_expand;
                    }
                  else
                    columns[i].min_size = columns[i].pref_size
                                        + expand_by / meta->col_span;
                }
            }
        }
    }

  /* calculate final widths */
  if (for_width >= 0)
    {
      gfloat min_width, pref_width;
      gint n_expand;

      min_width = 0;
      pref_width = 0;
      n_expand = 0;

      for (i = 0; i < self->priv->n_cols; i++)
        {
          pref_width += columns[i].pref_size;
          min_width += columns[i].min_size;
          if (columns[i].expand)
            n_expand++;
        }

      pref_width += priv->col_spacing * (priv->n_cols - 1);
      min_width += priv->col_spacing * (priv->n_cols - 1);

      if (for_width <= min_width)
        {
          /* erk, we can't shrink this! */
          for (i = 0; i < priv->n_cols; i++)
            columns[i].final_size = columns[i].min_size;

          return;
        }

      if (for_width == pref_width)
        {
          /* perfect! */
          for (i = 0; i < self->priv->n_cols; i++)
            columns[i].final_size = columns[i].pref_size;

          return;
        }

      /* for_width is between min_width and pref_width */
      if (for_width < pref_width && for_width > min_width)
        {
          gfloat width;

          /* shrink columns until they reach min_width */

          /* start with all columns at preferred size */
          for (i = 0; i < self->priv->n_cols; i++)
            columns[i].final_size = columns[i].pref_size;

          width = pref_width;

          while (width > for_width)
            {
              for (i = 0; i < self->priv->n_cols; i++)
                {
                  if (columns[i].final_size > columns[i].min_size)
                    {
                      columns[i].final_size--;
                      width--;
                    }
                }
            }

          return;
        }

      /* expand columns */
      if (for_width > pref_width)
        {
          gfloat extra_width = for_width - pref_width;
          gint remaining;

          if (n_expand)
            remaining = (gint) extra_width % n_expand;
          else
            remaining = (gint) extra_width % priv->n_cols;

          for (i = 0; i < self->priv->n_cols; i++)
            {
              if (columns[i].expand)
                {
                  columns[i].final_size = columns[i].pref_size
                                        + (extra_width / n_expand);
                }
              else
                columns[i].final_size = columns[i].pref_size;
            }

          /* distribute the remainder among children */
          i = 0;
          while (remaining)
            {
              columns[i].final_size++;
              i++;
              remaining--;
            }
        }
    }

}

static void
calculate_row_heights (ClutterTableLayout *self,
                       ClutterContainer   *container,
                       gint                for_height)
{
  ClutterTableLayoutPrivate *priv = self->priv;
  ClutterLayoutManager *manager = CLUTTER_LAYOUT_MANAGER (self);
  ClutterActor *actor, *child;
  gint i;
  DimensionData *rows, *columns;
  ClutterOrientation orientation = CLUTTER_ORIENTATION_VERTICAL;

  update_row_col (self, container);
  g_array_set_size (priv->rows, 0);
  g_array_set_size (priv->rows, self->priv->n_rows);

  rows = (DimensionData *) (void *) priv->rows->data;
  columns = (DimensionData *) (void *) priv->columns->data;

  /* reset the visibility of all rows */
  priv->visible_rows = 0;
  for (i = 0; i < priv->n_rows; i++)
    {
      rows[i].expand = FALSE;
      rows[i].visible = FALSE;
    }

  actor = CLUTTER_ACTOR (container);

  /* STAGE ONE: calculate row heights for non-spanned children */
  for (child = clutter_actor_get_first_child (actor);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      ClutterTableChild *meta;
      DimensionData *row;
      gfloat c_min, c_pref;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      meta =
        CLUTTER_TABLE_CHILD (clutter_layout_manager_get_child_meta (manager,
                                                                    container,
                                                                    child));

      if (meta->row_span > 1)
        continue;

      row = &rows[meta->row];

      if (!row->visible)
        {
          row->visible = TRUE;
          priv->visible_rows += 1;
        }

      clutter_actor_get_preferred_height (child, columns[meta->col].final_size,
                                          &c_min, &c_pref);

      row->min_size = MAX (row->min_size, c_min);
      row->pref_size = MAX (row->pref_size, c_pref);

      if (!row->expand)
        {
          row->expand = clutter_actor_needs_expand (child, orientation) ||
            meta->y_expand;
        }
    }

  /* STAGE TWO: take spanning children into account */
  for (child = clutter_actor_get_first_child (actor);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      ClutterTableChild *meta;
      gfloat c_min, c_pref;
      gfloat min_height, pref_height;
      gint start_row, end_row;
      gint n_expand;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      meta =
        CLUTTER_TABLE_CHILD (clutter_layout_manager_get_child_meta (manager,
                                                                    container,
                                                                    child));

      if (meta->row_span < 2)
        continue;

      start_row = meta->row;
      end_row = meta->row + meta->row_span - 1;

      clutter_actor_get_preferred_height (child, columns[meta->col].final_size,
                                         &c_min, &c_pref);


      /* check there is enough room for this actor */
      min_height = 0;
      pref_height = 0;
      n_expand = 0;
      for (i = start_row; i <= end_row; i++)
        {
          min_height += rows[i].min_size;
          pref_height += rows[i].pref_size;

          if (rows[i].expand)
            n_expand++;

          if (!rows[i].visible)
            {
              rows[i].visible = TRUE;
              priv->visible_rows += 1;
            }

          if (!rows[i].expand)
            {
              rows[i].expand = clutter_actor_needs_expand (child, orientation) ||
                meta->y_expand;
            }
        }

      min_height += priv->row_spacing * (meta->row_span - 1);
      pref_height += priv->row_spacing * (meta->row_span - 1);

      /* 1) If the minimum height of the rows spanned is less than the
       *    minimum height of the child that is spanning them, then we
       *    must increase the minimum height of the rows spanned.
       *
       * 2) If the preferred height of the spanned rows is more than
       *    the minimum height of the spanning child, then we can start
       *    at this size and decrease each row evenly.
       *
       * 3) If the preferred height of the rows is more than the minimum
       *    height of the spanned child, then we can start at the preferred
       *    height and expand.
       */

      /* (1) */
      if (c_min > min_height)
        {

          /* (2) */
          /* we can start from preferred height and decrease */
          if (pref_height > c_min)
            {
              for (i = start_row; i <= end_row; i++)
                rows[i].final_size = rows[i].pref_size;

              while (pref_height > c_min)
                {
                  for (i = start_row; i <= end_row; i++)
                    {
                      if (rows[i].final_size > rows[i].min_size)
                        {
                          rows[i].final_size--;
                          pref_height--;
                        }
                    }
                }

              for (i = start_row; i <= end_row; i++)
                rows[i].min_size = rows[i].final_size;
            }
          else
            {
              /* (3) */
              /* we can expand from preferred size */
              gfloat expand_by = c_pref - pref_height;

              for (i = start_row; i <= end_row; i++)
                {
                  if (n_expand)
                    {
                      if (rows[i].expand)
                        rows[i].min_size = rows[i].pref_size
                                         + expand_by / n_expand;
                    }
                  else
                    rows[i].min_size = rows[i].pref_size
                                     + expand_by / meta->row_span;
                }
            }
        }
    }

  /* calculate final heights */
  if (for_height >= 0)
    {
      gfloat min_height, pref_height;
      gint n_expand;

      min_height = 0;
      pref_height = 0;
      n_expand = 0;

      for (i = 0; i < self->priv->n_rows; i++)
        {
          pref_height += rows[i].pref_size;
          min_height += rows[i].min_size;
          if (rows[i].expand)
            n_expand++;
        }

      pref_height += priv->row_spacing * (priv->n_rows - 1);
      min_height += priv->row_spacing * (priv->n_rows - 1);

      if (for_height <= min_height)
        {
          /* erk, we can't shrink this! */
          for (i = 0; i < self->priv->n_rows; i++)
            rows[i].final_size = rows[i].min_size;

          return;
        }

      if (for_height == pref_height)
        {
          /* perfect! */
          for (i = 0; i < self->priv->n_rows; i++)
            rows[i].final_size = rows[i].pref_size;

          return;
        }

      /* for_height is between min_height and pref_height */
      if (for_height < pref_height && for_height > min_height)
        {
          gfloat height;

          /* shrink rows until they reach min_height */

          /* start with all rows at preferred size */
          for (i = 0; i < self->priv->n_rows; i++)
            rows[i].final_size = rows[i].pref_size;

          height = pref_height;

          while (height > for_height)
            {
              for (i = 0; i < priv->n_rows; i++)
                {
                  if (rows[i].final_size > rows[i].min_size)
                    {
                      rows[i].final_size--;
                      height--;
                    }
                }
            }

          return;
        }

      /* expand rows */
      if (for_height > pref_height)
        {
          gfloat extra_height = for_height - pref_height;
          gint remaining;

          if (n_expand)
            remaining = (gint) extra_height % n_expand;
          else
            remaining = (gint) extra_height % self->priv->n_rows;

          for (i = 0; i < self->priv->n_rows; i++)
            {
              if (rows[i].expand)
                {
                  rows[i].final_size = rows[i].pref_size
                                     + (extra_height / n_expand);
                }
              else
                rows[i].final_size = rows[i].pref_size;
            }

          /* distribute the remainder among children */
          i = 0;
          while (remaining)
            {
              rows[i].final_size++;
              i++;
              remaining--;
            }
        }
    }

}

static void
calculate_table_dimensions (ClutterTableLayout *self,
                            ClutterContainer   *container,
                            gfloat              for_width,
                            gfloat              for_height)
{
  calculate_col_widths (self, container, for_width);
  calculate_row_heights (self, container, for_height);
}

static void
clutter_table_layout_get_preferred_width (ClutterLayoutManager *layout,
                                          ClutterContainer     *container,
                                          gfloat                for_height,
                                          gfloat               *min_width_p,
                                          gfloat               *natural_width_p)
{
  ClutterTableLayout *self = CLUTTER_TABLE_LAYOUT (layout);
  ClutterTableLayoutPrivate *priv = self->priv;
  gfloat total_min_width, total_pref_width;
  DimensionData *columns;
  gint i;

  update_row_col (self, container);
  if (priv->n_cols < 1)
    {
      *min_width_p = 0;
      *natural_width_p = 0;
      return;
    }

  calculate_table_dimensions (self, container, -1, for_height);
  columns = (DimensionData *) (void *) priv->columns->data;

  total_min_width = (priv->visible_cols - 1) * (float) priv->col_spacing;
  total_pref_width = total_min_width;

  for (i = 0; i < priv->n_cols; i++)
    {
      total_min_width += columns[i].min_size;
      total_pref_width += columns[i].pref_size;
    }

  if (min_width_p)
    *min_width_p = total_min_width;

  if (natural_width_p)
    *natural_width_p = total_pref_width;
}

static void
clutter_table_layout_get_preferred_height (ClutterLayoutManager *layout,
                                           ClutterContainer     *container,
                                           gfloat                for_width,
                                           gfloat               *min_height_p,
                                           gfloat               *natural_height_p)
{
  ClutterTableLayout *self = CLUTTER_TABLE_LAYOUT (layout);
  ClutterTableLayoutPrivate *priv = self->priv;
  gfloat total_min_height, total_pref_height;
  DimensionData *rows;
  gint i;

  update_row_col (self, container);
  if (priv->n_rows < 1)
    {
      *min_height_p = 0;
      *natural_height_p = 0;
      return;
    }

  calculate_table_dimensions (self, container, for_width, -1);
  rows = (DimensionData *) (void *) priv->rows->data;

  total_min_height = (priv->visible_rows - 1) * (float) priv->row_spacing;
  total_pref_height = total_min_height;

  for (i = 0; i < self->priv->n_rows; i++)
    {
      total_min_height += rows[i].min_size;
      total_pref_height += rows[i].pref_size;
    }

  if (min_height_p)
    *min_height_p = total_min_height;

  if (natural_height_p)
    *natural_height_p = total_pref_height;
}

static gdouble
get_table_alignment_factor (ClutterTableAlignment alignment)
{
  switch (alignment)
    {
    case CLUTTER_TABLE_ALIGNMENT_START:
      return 0.0;

    case CLUTTER_TABLE_ALIGNMENT_CENTER:
      return 0.5;

    case CLUTTER_TABLE_ALIGNMENT_END:
      return 1.0;
    }

  return 0.0;
}

static void
clutter_table_layout_allocate (ClutterLayoutManager   *layout,
                               ClutterContainer       *container,
                               const ClutterActorBox  *box,
                               ClutterAllocationFlags  flags)
{
  ClutterTableLayout *self = CLUTTER_TABLE_LAYOUT (layout);
  ClutterTableLayoutPrivate *priv = self->priv;
  ClutterActor *actor, *child;
  gint row_spacing, col_spacing;
  gint i;
  DimensionData *rows, *columns;

  update_row_col (self, container);
  if (priv->n_cols < 1 || priv->n_rows < 1)
    return;

  actor = CLUTTER_ACTOR (container);

  if (clutter_actor_get_n_children (actor) == 0)
    return;

  col_spacing = (priv->col_spacing);
  row_spacing = (priv->row_spacing);

  calculate_table_dimensions (self, container,
                              box->x2 - box->x1,
                              box->y2 - box->y1);

  rows = (DimensionData *) (void *) priv->rows->data;
  columns = (DimensionData *) (void *) priv->columns->data;

  for (child = clutter_actor_get_first_child (actor);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      gint row, col, row_span, col_span;
      gint col_width, row_height;
      ClutterTableChild *meta;
      ClutterActorBox childbox;
      gint child_x, child_y;
      gdouble x_align, y_align;
      gboolean x_fill, y_fill;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      meta =
        CLUTTER_TABLE_CHILD (clutter_layout_manager_get_child_meta (layout,
                                                                    container,
                                                                    child));

      /* get child properties */
      col = meta->col;
      row = meta->row;
      row_span = meta->row_span;
      col_span = meta->col_span;
      x_align = get_table_alignment_factor (meta->x_align);
      y_align = get_table_alignment_factor (meta->y_align);
      x_fill = meta->x_fill;
      y_fill = meta->y_fill;

      /* initialise the width and height */
      col_width = columns[col].final_size;
      row_height = rows[row].final_size;

      /* Add the widths of the spanned columns:
       *
       * First check that we have a non-zero span. Then we loop over each of
       * the columns that we're spanning but we stop short if we go past the
       * number of columns in the table. This is necessary to avoid accessing
       * uninitialised memory. We add the spacing in here too since we only
       * want to add as much spacing as times we successfully span.
       */
      if (col + col_span > priv->n_cols)
        g_warning (G_STRLOC ": column-span exceeds number of columns");
      if (row + row_span > priv->n_rows)
        g_warning (G_STRLOC ": row-span exceeds number of rows");

      if (col_span > 1)
        {
          for (i = col + 1; i < col + col_span && i < priv->n_cols; i++)
            {
              col_width += columns[i].final_size;
              col_width += col_spacing;
            }
        }

      /* add the height of the spanned rows */
      if (row_span > 1)
        {
          for (i = row + 1; i < row + row_span && i < priv->n_rows; i++)
            {
              row_height += rows[i].final_size;
              row_height += row_spacing;
            }
        }

      /* calculate child x */
      child_x = clutter_actor_box_get_x (box);
      for (i = 0; i < col; i++)
        {
          if (columns[i].visible)
            {
              child_x += columns[i].final_size;
              child_x += col_spacing;
            }
        }

      /* calculate child y */
      child_y = clutter_actor_box_get_y (box);
      for (i = 0; i < row; i++)
        {
          if (rows[i].visible)
            {
              child_y += rows[i].final_size;
              child_y += row_spacing;
            }
        }

      /* set up childbox */
      childbox.x1 = (float) child_x;
      childbox.x2 = (float) MAX (0, child_x + col_width);

      childbox.y1 = (float) child_y;
      childbox.y2 = (float) MAX (0, child_y + row_height);

      if (priv->use_animations)
        {
          clutter_actor_save_easing_state (child);
          clutter_actor_set_easing_mode (child, priv->easing_mode);
          clutter_actor_set_easing_duration (child, priv->easing_duration);
        }

      if (clutter_actor_needs_expand (child, CLUTTER_ORIENTATION_HORIZONTAL) ||
          clutter_actor_needs_expand (child, CLUTTER_ORIENTATION_VERTICAL))
        clutter_actor_allocate (child, &childbox, flags);
      else
        clutter_actor_allocate_align_fill (child, &childbox,
                                           x_align, y_align,
                                           x_fill, y_fill,
                                           flags);

      if (priv->use_animations)
        clutter_actor_restore_easing_state (child);
    }
}

static void
clutter_table_layout_set_property (GObject      *gobject,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ClutterTableLayout *self = CLUTTER_TABLE_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_COLUMN_SPACING:
      clutter_table_layout_set_column_spacing (self, g_value_get_uint (value));
      break;

    case PROP_ROW_SPACING:
      clutter_table_layout_set_row_spacing (self, g_value_get_uint (value));
      break;

    case PROP_USE_ANIMATIONS:
      clutter_table_layout_set_use_animations (self, g_value_get_boolean (value));
      break;

    case PROP_EASING_MODE:
      clutter_table_layout_set_easing_mode (self, g_value_get_ulong (value));
      break;

    case PROP_EASING_DURATION:
      clutter_table_layout_set_easing_duration (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_table_layout_get_property (GObject    *gobject,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ClutterTableLayoutPrivate *priv = CLUTTER_TABLE_LAYOUT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ROW_SPACING:
      g_value_set_uint (value, priv->row_spacing);
      break;

    case PROP_COLUMN_SPACING:
      g_value_set_uint (value, priv->col_spacing);
      break;

    case PROP_USE_ANIMATIONS:
      g_value_set_boolean (value, priv->use_animations);
      break;

    case PROP_EASING_MODE:
      g_value_set_ulong (value, priv->easing_mode);
      break;

    case PROP_EASING_DURATION:
      g_value_set_uint (value, priv->easing_duration);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_table_layout_finalize (GObject *gobject)
{
  ClutterTableLayoutPrivate *priv = CLUTTER_TABLE_LAYOUT (gobject)->priv;

  g_array_free (priv->columns, TRUE);
  g_array_free (priv->rows, TRUE);

  G_OBJECT_CLASS (clutter_table_layout_parent_class)->finalize (gobject);
}

static void
clutter_table_layout_class_init (ClutterTableLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterLayoutManagerClass *layout_class;
  GParamSpec *pspec;

  layout_class = CLUTTER_LAYOUT_MANAGER_CLASS (klass);

  gobject_class->set_property = clutter_table_layout_set_property;
  gobject_class->get_property = clutter_table_layout_get_property;
  gobject_class->finalize = clutter_table_layout_finalize;

  layout_class->get_preferred_width =
    clutter_table_layout_get_preferred_width;
  layout_class->get_preferred_height =
    clutter_table_layout_get_preferred_height;
  layout_class->allocate = clutter_table_layout_allocate;
  layout_class->set_container = clutter_table_layout_set_container;
  layout_class->get_child_meta_type =
    clutter_table_layout_get_child_meta_type;

  /**
   * ClutterTableLayout:column-spacing:
   *
   * The spacing between columns of the #ClutterTableLayout, in pixels
   *
   * Since: 1.4
   */
  pspec = g_param_spec_uint ("column-spacing",
                             P_("Column Spacing"),
                             P_("Spacing between columns"),
                             0, G_MAXUINT, 0,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_COLUMN_SPACING, pspec);

  /**
   * ClutterTableLayout:row-spacing:
   *
   * The spacing between rows of the #ClutterTableLayout, in pixels
   *
   * Since: 1.4
   */
  pspec = g_param_spec_uint ("row-spacing",
                             P_("Row Spacing"),
                             P_("Spacing between rows"),
                             0, G_MAXUINT, 0,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ROW_SPACING, pspec);

  /**
   * ClutterTableLayout:use-animations:
   *
   * Whether the #ClutterTableLayout should animate changes in the
   * layout properties.
   *
   * By default, #ClutterTableLayout will honour the easing state of
   * the children when allocating them. Setting this property to
   * %TRUE will override the easing state with the layout manager's
   * #ClutterTableLayout:easing-mode and #ClutterTableLayout:easing-duration
   * properties.
   *
   * Since: 1.4
   *
   * Deprecated: 1.12: #ClutterTableLayout will honour the easing state
   *   of the children when allocating them
   */
  pspec = g_param_spec_boolean ("use-animations",
                                P_("Use Animations"),
                                P_("Whether layout changes should be animated"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_USE_ANIMATIONS, pspec);

  /**
   * ClutterTableLayout:easing-mode:
   *
   * The easing mode for the animations, in case
   * #ClutterTableLayout:use-animations is set to %TRUE.
   *
   * The easing mode has the same semantics of #ClutterAnimation:mode: it can
   * either be a value from the #ClutterAnimationMode enumeration, like
   * %CLUTTER_EASE_OUT_CUBIC, or a logical id as returned by
   * clutter_alpha_register_func().
   *
   * The default value is %CLUTTER_EASE_OUT_CUBIC.
   *
   * Since: 1.4
   *
   * Deprecated: 1.12: #ClutterTableLayout will honour the easing state
   *   of the children when allocating them
   */
  pspec = g_param_spec_ulong ("easing-mode",
                              P_("Easing Mode"),
                              P_("The easing mode of the animations"),
                              0, G_MAXULONG,
                              CLUTTER_EASE_OUT_CUBIC,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_EASING_MODE, pspec);

  /**
   * ClutterTableLayout:easing-duration:
   *
   * The duration of the animations, in case #ClutterTableLayout:use-animations
   * is set to %TRUE.
   *
   * The duration is expressed in milliseconds.
   *
   * Since: 1.4
   *
   * Deprecated: 1.12: #ClutterTableLayout will honour the easing state
   *   of the children when allocating them
   */
  pspec = g_param_spec_uint ("easing-duration",
                             P_("Easing Duration"),
                             P_("The duration of the animations"),
                             0, G_MAXUINT,
                             500,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_EASING_DURATION, pspec);
}

static void
clutter_table_layout_init (ClutterTableLayout *layout)
{
  ClutterTableLayoutPrivate *priv;

  layout->priv = priv = clutter_table_layout_get_instance_private (layout);

  priv->row_spacing = 0;
  priv->col_spacing = 0;

  priv->use_animations = FALSE;
  priv->easing_mode = CLUTTER_EASE_OUT_CUBIC;
  priv->easing_duration = 500;

  priv->columns = g_array_new (FALSE, TRUE, sizeof (DimensionData));
  priv->rows = g_array_new (FALSE, TRUE, sizeof (DimensionData));
}

/**
 * clutter_table_layout_new:
 *
 * Creates a new #ClutterTableLayout layout manager
 *
 * Return value: the newly created #ClutterTableLayout
 *
 * Since: 1.4
 */
ClutterLayoutManager *
clutter_table_layout_new (void)
{
  return g_object_new (CLUTTER_TYPE_TABLE_LAYOUT, NULL);
}

/**
 * clutter_table_layout_set_column_spacing:
 * @layout: a #ClutterTableLayout
 * @spacing: the spacing between columns of the layout, in pixels
 *
 * Sets the spacing between columns of @layout
 *
 * Since: 1.4
 */
void
clutter_table_layout_set_column_spacing (ClutterTableLayout *layout,
                                         guint               spacing)
{
  ClutterTableLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));

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

/**
 * clutter_table_layout_get_column_spacing:
 * @layout: a #ClutterTableLayout
 *
 * Retrieves the spacing set using clutter_table_layout_set_column_spacing()
 *
 * Return value: the spacing between columns of the #ClutterTableLayout
 *
 * Since: 1.4
 */
guint
clutter_table_layout_get_column_spacing (ClutterTableLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout), 0);

  return layout->priv->col_spacing;
}

/**
 * clutter_table_layout_set_row_spacing:
 * @layout: a #ClutterTableLayout
 * @spacing: the spacing between rows of the layout, in pixels
 *
 * Sets the spacing between rows of @layout
 *
 * Since: 1.4
 */
void
clutter_table_layout_set_row_spacing (ClutterTableLayout *layout,
                                      guint               spacing)
{
  ClutterTableLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));

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

/**
 * clutter_table_layout_get_row_spacing:
 * @layout: a #ClutterTableLayout
 *
 * Retrieves the spacing set using clutter_table_layout_set_row_spacing()
 *
 * Return value: the spacing between rows of the #ClutterTableLayout
 *
 * Since: 1.4
 */
guint
clutter_table_layout_get_row_spacing (ClutterTableLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout), 0);

  return layout->priv->row_spacing;
}

/**
 * clutter_table_layout_pack:
 * @layout: a #ClutterTableLayout
 * @actor: a #ClutterActor
 * @column: the column the @actor should be put, or -1 to append
 * @row: the row the @actor should be put, or -1 to append
 *
 * Packs @actor inside the #ClutterContainer associated to @layout
 * at the given row and column.
 *
 * Since: 1.4
 */
void
clutter_table_layout_pack (ClutterTableLayout  *layout,
                           ClutterActor        *actor,
                           gint                 column,
                           gint                 row)
{
  ClutterTableLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before adding children",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  update_row_col (CLUTTER_TABLE_LAYOUT (layout), priv->container);

  clutter_container_add_actor (priv->container, actor);

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  g_assert (CLUTTER_IS_TABLE_CHILD (meta));

  if (row < 0)
    row = priv->n_rows;

  if (column < 0)
    column = priv->n_cols;

  table_child_set_position (CLUTTER_TABLE_CHILD (meta), column, row);
}

/**
 * clutter_table_layout_set_span:
 * @layout: a #ClutterTableLayout
 * @actor: a #ClutterActor child of @layout
 * @column_span: Column span for @actor
 * @row_span: Row span for @actor
 *
 * Sets the row and column span for @actor
 * inside @layout
 *
 * Since: 1.4
 */
void
clutter_table_layout_set_span (ClutterTableLayout *layout,
                               ClutterActor       *actor,
                               gint                column_span,
                               gint                row_span)
{
  ClutterTableLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_TABLE_CHILD (meta));

  table_child_set_span (CLUTTER_TABLE_CHILD (meta), column_span, row_span);
}

/**
 * clutter_table_layout_get_span:
 * @layout: a #ClutterTableLayout
 * @actor: a #ClutterActor child of @layout
 * @column_span: (out): return location for the col span
 * @row_span: (out): return location for the row span
 *
 * Retrieves the row and column span for @actor as set using
 * clutter_table_layout_pack() or clutter_table_layout_set_span()
 *
 * Since: 1.4
 */
void
clutter_table_layout_get_span (ClutterTableLayout *layout,
                               ClutterActor       *actor,
                               gint               *column_span,
                               gint               *row_span)
{
  ClutterTableLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_TABLE_CHILD (meta));

  if (column_span)
    *column_span = CLUTTER_TABLE_CHILD (meta)->col_span;

  if (row_span)
    *row_span = CLUTTER_TABLE_CHILD (meta)->row_span;
}

/**
 * clutter_table_layout_set_alignment:
 * @layout: a #ClutterTableLayout
 * @actor: a #ClutterActor child of @layout
 * @x_align: Horizontal alignment policy for @actor
 * @y_align: Vertical alignment policy for @actor
 *
 * Sets the horizontal and vertical alignment policies for @actor
 * inside @layout
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: Use clutter_actor_set_x_align() and
 *   clutter_actor_set_y_align() instead.
 */
void
clutter_table_layout_set_alignment (ClutterTableLayout    *layout,
                                    ClutterActor          *actor,
                                    ClutterTableAlignment  x_align,
                                    ClutterTableAlignment  y_align)
{
  ClutterTableLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_TABLE_CHILD (meta));

  table_child_set_align (CLUTTER_TABLE_CHILD (meta), x_align, y_align);
}

/**
 * clutter_table_layout_get_alignment:
 * @layout: a #ClutterTableLayout
 * @actor: a #ClutterActor child of @layout
 * @x_align: (out): return location for the horizontal alignment policy
 * @y_align: (out): return location for the vertical alignment policy
 *
 * Retrieves the horizontal and vertical alignment policies for @actor
 * as set using clutter_table_layout_pack() or
 * clutter_table_layout_set_alignment().
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: Use clutter_actor_get_x_align() and
 *   clutter_actor_get_y_align() instead.
 */
void
clutter_table_layout_get_alignment (ClutterTableLayout    *layout,
                                    ClutterActor          *actor,
                                    ClutterTableAlignment *x_align,
                                    ClutterTableAlignment *y_align)
{
  ClutterTableLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_TABLE_CHILD (meta));

  if (x_align)
    *x_align = CLUTTER_TABLE_CHILD (meta)->x_align;

  if (y_align)
    *y_align = CLUTTER_TABLE_CHILD (meta)->y_align;
}

/**
 * clutter_table_layout_set_fill:
 * @layout: a #ClutterTableLayout
 * @actor: a #ClutterActor child of @layout
 * @x_fill: whether @actor should fill horizontally the allocated space
 * @y_fill: whether @actor should fill vertically the allocated space
 *
 * Sets the horizontal and vertical fill policies for @actor
 * inside @layout
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: Use clutter_actor_set_x_align() and
 *   clutter_actor_set_y_align() instead.
 */
void
clutter_table_layout_set_fill (ClutterTableLayout *layout,
                               ClutterActor       *actor,
                               gboolean            x_fill,
                               gboolean            y_fill)
{
  ClutterTableLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_TABLE_CHILD (meta));

  table_child_set_fill (CLUTTER_TABLE_CHILD (meta), x_fill, y_fill);
}

/**
 * clutter_table_layout_get_fill:
 * @layout: a #ClutterTableLayout
 * @actor: a #ClutterActor child of @layout
 * @x_fill: (out): return location for the horizontal fill policy
 * @y_fill: (out): return location for the vertical fill policy
 *
 * Retrieves the horizontal and vertical fill policies for @actor
 * as set using clutter_table_layout_pack() or clutter_table_layout_set_fill()
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: Use clutter_actor_get_x_align() and
 *   clutter_actor_get_y_align() instead.
 */
void
clutter_table_layout_get_fill (ClutterTableLayout *layout,
                               ClutterActor       *actor,
                               gboolean           *x_fill,
                               gboolean           *y_fill)
{
  ClutterTableLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_TABLE_CHILD (meta));

  if (x_fill)
    *x_fill = CLUTTER_TABLE_CHILD (meta)->x_fill;

  if (y_fill)
    *y_fill = CLUTTER_TABLE_CHILD (meta)->y_fill;
}


/**
 * clutter_table_layout_set_expand:
 * @layout: a #ClutterTableLayout
 * @actor: a #ClutterActor child of @layout
 * @x_expand: whether @actor should allocate extra space horizontally
 * @y_expand: whether @actor should allocate extra space vertically
 *
 * Sets the horizontal and vertical expand policies for @actor
 * inside @layout
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: Use clutter_actor_set_x_expand() or
 *   clutter_actor_set_y_expand() instead.
 */
void
clutter_table_layout_set_expand (ClutterTableLayout *layout,
                                 ClutterActor       *actor,
                                 gboolean            x_expand,
                                 gboolean            y_expand)
{
  ClutterTableLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_TABLE_CHILD (meta));

  table_child_set_expand (CLUTTER_TABLE_CHILD (meta), x_expand, y_expand);
}

/**
 * clutter_table_layout_get_expand:
 * @layout: a #ClutterTableLayout
 * @actor: a #ClutterActor child of @layout
 * @x_expand: (out): return location for the horizontal expand policy
 * @y_expand: (out): return location for the vertical expand policy
 *
 * Retrieves the horizontal and vertical expand policies for @actor
 * as set using clutter_table_layout_pack() or clutter_table_layout_set_expand()
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: Use clutter_actor_get_x_expand() and
 *   clutter_actor_get_y_expand() instead.
 */
void
clutter_table_layout_get_expand (ClutterTableLayout *layout,
                                 ClutterActor       *actor,
                                 gboolean           *x_expand,
                                 gboolean           *y_expand)
{
  ClutterTableLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_TABLE_CHILD (meta));

  if (x_expand)
    *x_expand = CLUTTER_TABLE_CHILD (meta)->x_expand;

  if (y_expand)
    *y_expand = CLUTTER_TABLE_CHILD (meta)->y_expand;
}

/**
 * clutter_table_layout_set_use_animations:
 * @layout: a #ClutterTableLayout
 * @animate: %TRUE if the @layout should use animations
 *
 * Sets whether @layout should animate changes in the layout properties
 *
 * The duration of the animations is controlled by
 * clutter_table_layout_set_easing_duration(); the easing mode to be used
 * by the animations is controlled by clutter_table_layout_set_easing_mode()
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: #ClutterTableLayout will honour the easing state
 *   of the children when allocating them
 */
void
clutter_table_layout_set_use_animations (ClutterTableLayout *layout,
                                         gboolean            animate)
{
  ClutterTableLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));

  priv = layout->priv;

  animate = !!animate;
  if (priv->use_animations != animate)
    {
      priv->use_animations = animate;

      g_object_notify (G_OBJECT (layout), "use-animations");
    }
}

/**
 * clutter_table_layout_get_use_animations:
 * @layout: a #ClutterTableLayout
 *
 * Retrieves whether @layout should animate changes in the layout properties
 *
 * Since clutter_table_layout_set_use_animations()
 *
 * Return value: %TRUE if the animations should be used, %FALSE otherwise
 *
 * Since: 1.4
 *
 * Deprecated: 1.12
 */
gboolean
clutter_table_layout_get_use_animations (ClutterTableLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout), FALSE);

  return layout->priv->use_animations;
}

/**
 * clutter_table_layout_set_easing_mode:
 * @layout: a #ClutterTableLayout
 * @mode: an easing mode, either from #ClutterAnimationMode or a logical id
 *   from clutter_alpha_register_func()
 *
 * Sets the easing mode to be used by @layout when animating changes in layout
 * properties
 *
 * Use clutter_table_layout_set_use_animations() to enable and disable the
 * animations
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: #ClutterTableLayout will honour the easing state
 *   of the children when allocating them
 */
void
clutter_table_layout_set_easing_mode (ClutterTableLayout *layout,
                                      gulong              mode)
{
  ClutterTableLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));

  priv = layout->priv;

  if (priv->easing_mode != mode)
    {
      priv->easing_mode = mode;

      g_object_notify (G_OBJECT (layout), "easing-mode");
    }
}

/**
 * clutter_table_layout_get_easing_mode:
 * @layout: a #ClutterTableLayout
 *
 * Retrieves the easing mode set using clutter_table_layout_set_easing_mode()
 *
 * Return value: an easing mode
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: #ClutterTableLayout will honour the easing state
 *   of the children when allocating them
 */
gulong
clutter_table_layout_get_easing_mode (ClutterTableLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout),
                        CLUTTER_EASE_OUT_CUBIC);

  return layout->priv->easing_mode;
}

/**
 * clutter_table_layout_set_easing_duration:
 * @layout: a #ClutterTableLayout
 * @msecs: the duration of the animations, in milliseconds
 *
 * Sets the duration of the animations used by @layout when animating changes
 * in the layout properties
 *
 * Use clutter_table_layout_set_use_animations() to enable and disable the
 * animations
 *
 * Since: 1.4
 *
 * Deprecated: 1.12: #ClutterTableLayout will honour the easing state
 *   of the children when allocating them
 */
void
clutter_table_layout_set_easing_duration (ClutterTableLayout *layout,
                                          guint               msecs)
{
  ClutterTableLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout));

  priv = layout->priv;

  if (priv->easing_duration != msecs)
    {
      priv->easing_duration = msecs;

      g_object_notify (G_OBJECT (layout), "easing-duration");
    }
}

/**
 * clutter_table_layout_get_easing_duration:
 * @layout: a #ClutterTableLayout
 *
 * Retrieves the duration set using clutter_table_layout_set_easing_duration()
 *
 * Return value: the duration of the animations, in milliseconds
 *
 * Since: 1.4
 *
 * Deprecated: 1.12
 */
guint
clutter_table_layout_get_easing_duration (ClutterTableLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout), 500);

  return layout->priv->easing_duration;
}


/**
 * clutter_table_layout_get_row_count:
 * @layout: A #ClutterTableLayout
 *
 * Retrieve the current number rows in the @layout
 *
 * Returns: the number of rows
 *
 * Since: 1.4
 */
gint
clutter_table_layout_get_row_count (ClutterTableLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout), -1);

  update_row_col (layout, layout->priv->container);
  return CLUTTER_TABLE_LAYOUT (layout)->priv->n_rows;
}

/**
 * clutter_table_layout_get_column_count:
 * @layout: A #ClutterTableLayout
 *
 * Retrieve the current number of columns in @layout
 *
 * Returns: the number of columns
 *
 * Since: 1.4
 */
gint
clutter_table_layout_get_column_count (ClutterTableLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_TABLE_LAYOUT (layout), -1);

  update_row_col (layout, layout->priv->container);
  return CLUTTER_TABLE_LAYOUT (layout)->priv->n_cols;
}
