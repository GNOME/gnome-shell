/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Neil Jagdish Patel <njp@o-hand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * NB: Inspiration for column storage taken from GtkListStore
 */

/**
 * SECTION:clutter-model
 * @short_description: A generic model implementation
 *
 * #ClutterModel is a generic list model which can be used to implement the
 * model-view-controller architectural pattern in Clutter.
 *
 * The #ClutterModel object is a list model which can accept most GObject 
 * types as a column type. Internally, it will keep a local copy of the data
 * passed in (such as a string or a boxed pointer).
 * 
 * Creating a simple clutter model:
 * <programlisting>
 * enum {
 *   COLUMN_INT,
 *   COLUMN_STRING.
 *   N_COLUMNS
 * };
 * 
 * {
 *   ClutterModel *model;
 *   gint i;
 *
 *   model = clutter_model_new (N_COLUMNS,
 *                              G_TYPE_INT,    "int",
 *                              G_TYPE_STRING, "string");
 *   for (i = 0; i < 10; i++)
 *     {
 *       gchar *string = g_strdup_printf ("String %d", i);
 *       clutter_model_append (model,
 *                             COLUMN_INT, i,
 *                             COLUMN_STRING, string,
 *                             -1);
 *       g_free (string);
 *     }
 *
 *   
 * }
 * </programlisting>
 *
 * Iterating through the model consists of retrieving a new #ClutterModelIter
 * pointing to the starting row, and calling #clutter_model_iter_next or
 * #clutter_model_iter_prev to move forward or backwards, repectively.
 *
 * A valid #ClutterModelIter represents the position between two rows in the
 * model. For example, the @begin iterator represents the gap immediately 
 * before the first row, and the @end iterator represents the gap immediately
 * after the last row. In an empty sequence, the begin and end iterators are
 * the same.
 *
 * Iterating a #ClutterModel:
  * <programlisting>
 * enum {
 *   COLUMN_INT,
 *   COLUMN_STRING.
 *
 *   N_COLUMNS
 * };
 * 
 * {
 *   ClutterModel *model;
 *   ClutterModelIter *iter = NULL;
 *
 *   /<!-- -->*  Fill the model *<!-- -->/
 *   model = populate_model ();
 *
 *   /<!-- -->* Get the first iter *<!-- -->/
 *   iter = clutter_model_get_first_iter (model);
 *   while (!clutter_model_iter_is_last (iter))
 *     {
 *       print_row (iter);
 *       
 *       iter = clutter_model_iter_next (iter);
 *     }
 *
 *   /<!-- -->* Make sure to unref the iter *<!-- -->/
 *   g_object_unref (iter);
 * }
 * </programlisting>
 *
 * #ClutterModel is available since Clutter 0.6
 */


#include "config.h"

#include <string.h>
#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-model.h"

#include "clutter-debug.h"


G_DEFINE_TYPE (ClutterModel, clutter_model, G_TYPE_OBJECT);

enum
{
  ROW_ADDED,
  ROW_REMOVED,
  ROW_CHANGED,

  SORT_CHANGED,
  FILTER_CHANGED,
  
  LAST_SIGNAL
};

static guint model_signals[LAST_SIGNAL] = { 0, };

#define CLUTTER_MODEL_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_MODEL, ClutterModelPrivate))

struct _ClutterModelPrivate
{
  GSequence              *sequence;

  GType                  *column_types;
  gchar                 **column_names;
  gint                    n_columns; 
  
  ClutterModelFilterFunc  filter;
  gpointer                filter_data;
  GDestroyNotify          filter_notify;

  ClutterModelSortFunc    sort;
  gint                    sort_column;
  gpointer                sort_data;
  GDestroyNotify          sort_notify;
};

/* Forwards */
static const gchar *      _model_get_column_name    (ClutterModel *model,
                                                     guint         column);
static GType              _model_get_column_type    (ClutterModel *model,
                                                     guint         column);
static ClutterModelIter * _model_get_iter_at_row    (ClutterModel *model, 
                                                     guint         row);
static void               clutter_model_real_remove (ClutterModel  *model,
                                                     GSequenceIter *iter);

static void 
clutter_model_finalize (GObject *object)
{
  ClutterModelPrivate *priv = CLUTTER_MODEL (object)->priv;
  GSequenceIter *iter;

  iter = g_sequence_get_end_iter (priv->sequence);
  while (!g_sequence_iter_is_end (iter))
    {
      clutter_model_real_remove (CLUTTER_MODEL (object), iter);
      iter = g_sequence_iter_next (iter);
    }
  g_sequence_free (priv->sequence);

  if (priv->sort_notify)
    priv->sort_notify (priv->sort_data);
  
  if (priv->filter_notify)
    priv->filter_notify (priv->filter_data);

  g_free (priv->column_types);
  g_strfreev (priv->column_names);

  G_OBJECT_CLASS (clutter_model_parent_class)->finalize (object);
}

static void
clutter_model_class_init (ClutterModelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = clutter_model_finalize;

  klass->get_iter_at_row  = _model_get_iter_at_row;
  klass->get_column_name  = _model_get_column_name;
  klass->get_column_type  = _model_get_column_type;

   /**
   * ClutterModel::row-added:
   * @model: the #ClutterModel on which the signal is emitted
   * @iter: a #ClutterModelIter pointing to the new row
   *
   * The ::row-added signal is emitted when a new row has been added 
   *
   * Since 0.6
   */
  model_signals[ROW_ADDED] =
    g_signal_new ("row-added",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, row_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

   /**
   * ClutterModel::row-removed:
   * @model: the #ClutterModel on which the signal is emitted
   * @iter: a #ClutterModelIter pointing to the removed row
   *
   * The ::row-removed signal is emitted when a row has been removed
   *
   * Since 0.6
   */
  model_signals[ROW_REMOVED] =
    g_signal_new ("row-removed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, row_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

   /**
   * ClutterModel::row-changed:
   * @model: the #ClutterModel on which the signal is emitted
   * @iter: a #ClutterModelIter pointing to the changed row
   *
   * The ::row-removed signal is emitted when a row has been changed
   *
   * Since 0.6
   */
  model_signals[ROW_CHANGED] =
    g_signal_new ("row-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, row_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ClutterModel::sort-changed:
   * @model: the #ClutterModel on which the signal is emitted
   * 
   * The ::sort-changed signal is emitted after the model has been sorted
   *
   * Since 0.6
   */
  model_signals[SORT_CHANGED] =
    g_signal_new ("sort-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, sort_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

   /**
   * ClutterModel::filter-changed:
   * @model: the #ClutterModel on which the signal is emitted   
   *
   * The ::filter-changed signal is emitted when a new filter has been applied
   *
   * Since 0.6
   */
  model_signals[FILTER_CHANGED] =
    g_signal_new ("filter-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, filter_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (ClutterModelPrivate));
}

static void
clutter_model_init (ClutterModel *self)
{
  ClutterModelPrivate *priv;
  
  self->priv = priv = CLUTTER_MODEL_GET_PRIVATE (self);

  priv->sequence = g_sequence_new (NULL);

  priv->column_types = NULL;
  priv->n_columns = -1;

  priv->filter = NULL;
  priv->filter_data = NULL;
  priv->filter_notify = NULL;
  priv->sort = NULL;
  priv->sort_data = NULL;
  priv->sort_column = -1;
  priv->sort_notify = NULL;
}


static gboolean
clutter_model_check_type (type)
{
  gint i = 0;
  static const GType type_list[] =
    {
      G_TYPE_BOOLEAN,
      G_TYPE_CHAR,
      G_TYPE_UCHAR,
      G_TYPE_INT,
      G_TYPE_UINT,
      G_TYPE_LONG,
      G_TYPE_ULONG,
      G_TYPE_INT64,
      G_TYPE_UINT64,
      G_TYPE_ENUM,
      G_TYPE_FLAGS,
      G_TYPE_FLOAT,
      G_TYPE_DOUBLE,
      G_TYPE_STRING,
      G_TYPE_POINTER,
      G_TYPE_BOXED,
      G_TYPE_OBJECT,
      G_TYPE_INVALID
    };

  if (! G_TYPE_IS_VALUE_TYPE (type))
    return FALSE;


  while (type_list[i] != G_TYPE_INVALID)
    {
      if (g_type_is_a (type, type_list[i]))
	return TRUE;
      i++;
    }
  return FALSE;
}

static gint
_model_sort_func (GValueArray  *val_a, 
                  GValueArray  *val_b, 
                  ClutterModel *model)
{
  ClutterModelPrivate *priv;
  const GValue *a;
  const GValue *b;
  
  g_return_val_if_fail (CLUTTER_IS_MODEL (model), 0);
  g_return_val_if_fail (val_a, 0);
  g_return_val_if_fail (val_b, 0);
  priv = model->priv;
  
  a = g_value_array_get_nth (val_a, priv->sort_column);
  b = g_value_array_get_nth (val_b, priv->sort_column);

  return priv->sort (model, a, b, priv->sort_data);
}

static void
_model_sort (ClutterModel *model)
{
  ClutterModelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  if (!priv->sort)
    return;

  g_sequence_sort (priv->sequence, (GCompareDataFunc)_model_sort_func, model);
}


static gboolean
_model_filter (ClutterModel *model, ClutterModelIter *iter)
{
  ClutterModelPrivate *priv;
  gboolean res = TRUE;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), TRUE);
  priv = model->priv;

  if (!priv->filter)
    return TRUE;

  res = priv->filter (model, iter, priv->filter_data);

  return res;
}


static void
clutter_model_set_n_columns (ClutterModel *model,
                             gint          n_columns,
                             gboolean      set_types,
                             gboolean      set_names)
{
  ClutterModelPrivate *priv = model->priv;
  gboolean types_set, names_set;

  types_set = (priv->column_types != NULL);
  names_set = (priv->column_names != NULL);

  if (priv->n_columns > 0 && priv->n_columns != n_columns)
    return;

  priv->n_columns = n_columns;
  
  if (set_types && !priv->column_types)
    priv->column_types = g_new0 (GType,  n_columns);

  if (set_names && !priv->column_names)
    priv->column_names = g_new0 (gchar*, n_columns + 1);
}

static void
clutter_model_set_column_type (ClutterModel *model,
                               gint          column,
                               GType         type)
{
  ClutterModelPrivate *priv = model->priv;

  priv->column_types[column] = type;
}

static void
clutter_model_set_column_name (ClutterModel *model,
                               gint          column,
                               const gchar  *name)
{
  ClutterModelPrivate *priv = model->priv;

  priv->column_names[column] = g_strdup (name);
}

/**
 * clutter_model_new:
 * @n_columns: number of columns in the model
 * @Varargs: @n_columns number of #GType and string pairs
 *
 * Creates a new model with @n_columns columns with the types 
 * and names passed in.
 *
 * For example:
 * 
 * <informalexample><programlisting>
 * model = clutter_model_new (3,
 *                            G_TYPE_INT, "int column",
 *                            G_TYPE_STRING, "string column",
 *                            GDK_TYPE_PIXBUF, "pixbuf column");
 * </programlisting></informalexample>
 *
 * will create a new #ClutterModel with three columns of type int,
 * string and #GdkPixbuf respectively.
 *
 * Return value: a new #ClutterModel
 *
 * Since 0.6
 */
ClutterModel *
clutter_model_new (guint n_columns,
                   ...)
{
  ClutterModel *model;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  model = g_object_new (CLUTTER_TYPE_MODEL, NULL);
  clutter_model_set_n_columns (model, n_columns, TRUE, TRUE);

  va_start (args, n_columns);

  for (i = 0; i < n_columns; i++)
    { 
      GType type = va_arg (args, GType);
      const gchar *name = va_arg (args, gchar*);
 
      if (!clutter_model_check_type (type))
        {
          g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (type));
          g_object_unref (model);
          return NULL;
        }

      clutter_model_set_column_type (model, i, type);
      clutter_model_set_column_name (model, i, name);
    }

  va_end (args);

  return model;
}

/**
 * clutter_model_newv:
 * @n_columns: number of columns in the model
 * @types: an array of #GType types for the columns, from first to last
 * @names: an array of names for the columns, from first to last
 *
 * Non-vararg creation function. Used primarily by language bindings.
 *
 * Return value: a new #ClutterModel
 *
 * Since 0.6
 */
ClutterModel *
clutter_model_newv (guint                n_columns,
                    GType               *types,
                    const gchar * const  names[])
{
  ClutterModel *model;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  model = g_object_new (CLUTTER_TYPE_MODEL, NULL);
  clutter_model_set_n_columns (model, n_columns, TRUE, TRUE);

  for (i = 0; i < n_columns; i++)
    {
      if (!clutter_model_check_type (types[i]))
        {
          g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (types[i]));
          g_object_unref (model);
          return NULL;
        }

      clutter_model_set_column_type (model, i, types[i]);
      clutter_model_set_column_name (model, i, names[i]);
    }

  return model;
}

/**
 * clutter_model_set_types:
 * @model: a #ClutterModel
 * @n_columns: number of columns for the model
 * @types: an array of #GType types
 *
 * Sets the types of the columns inside a #ClutterModel.
 *
 * This function is meant primarily for #GObjects that inherit from
 * #ClutterModel, and should only be used when contructing a #ClutterModel.
 * It will not work after the initial creation of the #ClutterModel.
 *
 * Since 0.6
 */
void
clutter_model_set_types (ClutterModel *model,
                         guint         n_columns,
                         GType         *types)
{
  ClutterModelPrivate *priv;
  gint i;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  g_return_if_fail (n_columns > 0);

  priv = model->priv;

  g_return_if_fail (priv->n_columns < 0 || priv->n_columns == n_columns);
  g_return_if_fail (priv->column_types == NULL);

  clutter_model_set_n_columns (model, n_columns, TRUE, FALSE);

  for (i = 0; i < n_columns; i++)
    {
      if (!clutter_model_check_type (types[i]))
        {
          g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (types[i]));
          return;
        }

      clutter_model_set_column_type (model, i, types[i]);
    }
}

/**
 * clutter_model_set_names:
 * @model: a #ClutterModel
 * @n_columns: the number of column names
 * @names: an array of strings
 *
 * Assigns a name to the columns of a #ClutterModel.
 *
 * This function is meant primarily for #GObjects that inherit from
 * #ClutterModel, and should only be used when contructing a #ClutterModel.
 * It will not work after the initial creation of the #ClutterModel.
 *
 * Since: 0.6
 */
void
clutter_model_set_names (ClutterModel        *model,
                         guint                n_columns,
                         const gchar * const  names[])
{
  ClutterModelPrivate *priv;
  gint i;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  g_return_if_fail (n_columns > 0);

  priv = model->priv;

  g_return_if_fail (priv->n_columns < 0 || priv->n_columns == n_columns);
  g_return_if_fail (priv->column_names == NULL);

  clutter_model_set_n_columns (model, n_columns, FALSE, TRUE);

  for (i = 0; i < n_columns; i++)
    clutter_model_set_column_name (model, i, names[i]);
}

/**
 * clutter_model_get_n_columns:
 * @model: a #ClutterModel
 *
 * Retrieves the number of columns inside @model.
 *
 * Return value: the number of columns
 *
 * Since: 0.6
 */
guint
clutter_model_get_n_columns (ClutterModel *model)
{
  g_return_val_if_fail (CLUTTER_IS_MODEL (model), 0);

  return model->priv->n_columns;
}

static GValueArray *
clutter_model_new_row (ClutterModel *model)
{
  ClutterModelPrivate *priv;
  GValueArray *row;
  gint i;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), NULL);
  priv = model->priv;
  
  row = g_value_array_new (priv->n_columns);

  for (i = 0; i < priv->n_columns; i++)
   {
      GValue value = { 0, };

      g_value_init (&value, priv->column_types[i]);
      g_value_array_append (row, &value);
      g_value_unset (&value);
    }

  return row;
}

/**
 * clutter_model_append_value:
 * @model: a #ClutterModel
 * @column: the column to set
 * @value: the value of the cell
 *
 * Creates and appends a new row to the #ClutterModel, setting the row
 * value for the given @column upon creation.
 *
 * Since 0.6
 */
void
clutter_model_append_value (ClutterModel *model,
                            guint         column,
                            const GValue *value)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  GSequenceIter *seq_iter;
  GValueArray *row;

  g_return_if_fail (CLUTTER_IS_MODEL (model));

  priv = model->priv;

  row = clutter_model_new_row (model);
  seq_iter = g_sequence_append (priv->sequence, row);

  iter = g_object_new (CLUTTER_TYPE_MODEL_ITER,
                       "model", model,
                       "iter", seq_iter,
                       NULL);

  clutter_model_iter_set_value (iter, column, value);

  if (priv->sort_column == column)
    _model_sort (model);

  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter);
  g_object_unref (iter);
}

/**
 * clutter_model_append:
 * @model: a #ClutterModel
 * @Varargs: pairs of column number and value, terminated with -1
 *
 * Creates and appends a new row to the #ClutterModel, setting the
 * row values upon creation. For example, to append a new row where
 * column 0 is type %G_TYPE_INT and column 1 is of type %G_TYPE_STRING:
 *
 * <informalexample><programlisting>
 *   ClutterModel *model;
 *   model = clutter_model_new (2,
 *                              G_TYPE_INT,    "My integers",
 *                              G_TYPE_STRING, "My strings");
 *   clutter_model_append (model, 0, 42, 1, "string", -1);
 * </programlisting></informalexample>
 *
 * Since 0.6
 */
void
clutter_model_append (ClutterModel *model,
                      ...)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  GSequenceIter *seq_iter;
  GValueArray *row;
  va_list args;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  row = clutter_model_new_row (model);
  seq_iter = g_sequence_append (priv->sequence, row);

  iter = g_object_new (CLUTTER_TYPE_MODEL_ITER,
                       "model", model,
                       "iter", seq_iter,
                       NULL);

  va_start (args, model);
  clutter_model_iter_set_valist (iter, args);
  va_end (args);

  /*FIXME: Sort the model if necessary */
  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter);
  g_object_unref (iter);
}

/**
 * clutter_model_prepend_value:
 * @model: a #ClutterModel
 * @column: column to set
 * @value: new value for the cell
 *
 * Creates and prepends a new row to the #ClutterModel, setting the row
 * value for the given @column upon creation.
 *
 * Since 0.6
 */
void
clutter_model_prepend_value (ClutterModel *model,
                             guint         column,
                             const GValue *value)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  GSequenceIter *seq_iter;
  GValueArray *row;

  g_return_if_fail (CLUTTER_IS_MODEL (model));

  priv = model->priv;

  row = clutter_model_new_row (model);
  seq_iter = g_sequence_prepend (priv->sequence, row);

  iter = g_object_new (CLUTTER_TYPE_MODEL_ITER,
                       "model", model,
                       "iter", seq_iter,
                       NULL);

  clutter_model_iter_set_value (iter, column, value);

  if (priv->sort_column == column)
    _model_sort (model);

  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter); 
  g_object_unref (iter);
}

/**
 * clutter_model_prepend:
 * @model: a #ClutterModel
 * @Varargs: pairs of column number and value, terminated with -1
 *
 * Creates and prepends a new row to the #ClutterModel, setting the row
 * values upon creation. For example, to prepend a new row where column 0
 * is type %G_TYPE_INT and column 1 is of type %G_TYPE_STRING:
 *
 * <informalexample><programlisting>
 *   ClutterModel *model;
 *   model = clutter_model_new (2,
 *                              G_TYPE_INT,    "My integers",
 *                              G_TYPE_STRING, "My strings");
 *   clutter_model_prepend (model, 0, 42, 1, "string", -1);
 * </programlisting></informalexample>
 *
 * Since 0.6
 */
void
clutter_model_prepend (ClutterModel *model,
                       ...)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  GSequenceIter *seq_iter;
  GValueArray *row;
  va_list args;

  g_return_if_fail (CLUTTER_IS_MODEL (model));

  priv = model->priv;

  row = clutter_model_new_row (model);
  seq_iter = g_sequence_prepend (priv->sequence, row);

  iter = g_object_new (CLUTTER_TYPE_MODEL_ITER,
                       "model", model,
                       "iter", seq_iter,
                       NULL);
  va_start (args, model);
  clutter_model_iter_set_valist (iter, args);
  va_end (args);

  /*FIXME: Sort the model if necessary */
  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter); 
  g_object_unref (iter);
}


/**
 * clutter_model_insert:
 * @model: a #ClutterModel
 * @row: the position to insert the new row
 * @Varargs: pairs of column number and value, terminated with -1
 *
 * Inserts a new row to the #ClutterModel at @row, setting the row
 * values upon creation. For example, to insert a new row at index 100,
 * where column 0 is type %G_TYPE_INT and column 1 is of type
 * %G_TYPE_STRING:
 *
 * <informalexample><programlisting>
 *   ClutterModel *model;
 *   model = clutter_model_new (2,
 *                              G_TYPE_INT,    "My integers",
 *                              G_TYPE_STRING, "My strings");
 *   clutter_model_insert (model, 100, 0, 42, 1, "string", -1);
 * </programlisting></informalexample>
 *
 * Since 0.6
 */
void
clutter_model_insert (ClutterModel *model,
                      guint         row,
                      ...)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  GValueArray *row_array;
  GSequenceIter *seq_iter;
  va_list args;

  g_return_if_fail (CLUTTER_IS_MODEL (model));

  priv = model->priv;

  row_array = clutter_model_new_row (model);

  seq_iter = g_sequence_get_iter_at_pos (priv->sequence, row);
  seq_iter = g_sequence_insert_before (seq_iter, row_array);

  iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, 
                       "model", model,
                       "iter", seq_iter,
                       NULL);

  va_start (args, row);
  clutter_model_iter_set_valist (iter, args);
  va_end (args);

  /* FIXME: Sort? */

  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter);
  g_object_unref (iter);
}

/**
 * clutter_model_insert_value:
 * @model: a #ClutterModel
 * @row: position of the row to modify
 * @column: column to modify
 * @value: new value for the cell
 *
 * Sets the data in the cell specified by @iter and @column. The type of 
 * @value must be convertable to the type of the column.
 *
 * Since 0.6
 */
void
clutter_model_insert_value (ClutterModel *model,
                            guint         row,
                            guint         column,
                            const GValue *value)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  GSequenceIter *seq_iter;
  
  g_return_if_fail (CLUTTER_IS_MODEL (model));

  priv = model->priv;

  seq_iter = g_sequence_get_iter_at_pos (priv->sequence, row);
  
  iter = g_object_new (CLUTTER_TYPE_MODEL_ITER,
                       "model", model,
                       "iter", seq_iter,
                       NULL);

  clutter_model_iter_set_value (iter, column, value);

  if (priv->sort_column == column)
    _model_sort (model);

  g_signal_emit (model, model_signals[ROW_CHANGED], 0, iter);  
  g_object_unref (iter);
}

static void
clutter_model_real_remove (ClutterModel  *model,
                           GSequenceIter *iter)
{
  ClutterModelPrivate *priv;
  GValueArray *value_array;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  value_array = g_sequence_get (iter);
  g_value_array_free (value_array);
}

/**
 * clutter_model_remove:
 * @model: a #ClutterModel
 * @row: position of row to remove
 *
 * Removes the row at the given position from the model.
 *
 * Since 0.6
 */
void
clutter_model_remove (ClutterModel *model,
                      guint         row)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  GSequenceIter *seq_iter;
  gint i = 0;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  seq_iter = g_sequence_get_begin_iter (priv->sequence);
  iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, "model", model, NULL);

  while (!g_sequence_iter_is_end (seq_iter))
    {
      g_object_set (iter, "iter", seq_iter, NULL);
      if (_model_filter (model, iter))
        {
          if (i == row)
            {  
              g_signal_emit (model, model_signals[ROW_REMOVED], 0, iter);
              clutter_model_real_remove (model, seq_iter);
              g_sequence_remove (seq_iter); 
              break;
            }
          i++;
        }
      seq_iter = g_sequence_iter_next (seq_iter);
    }
  g_object_unref (iter);
}

static const gchar *
_model_get_column_name (ClutterModel *model,
                        guint         column)
{
  ClutterModelPrivate *priv = model->priv;

  if (column < 0 || column >= priv->n_columns)
    return NULL;

  if (priv->column_names && priv->column_names[column])
    return priv->column_names[column];

  return g_type_name (priv->column_types[column]);
}

/**
 * clutter_model_get_column_name:
 * @model: #ClutterModel
 * @column: the column number
 *
 * Retrieves the name of the @column
 *
 * Return value: the name of the column. The model holds the returned
 *   string, and it should not be modified or freed
 *
 * Since: 0.6
 */
G_CONST_RETURN gchar *
clutter_model_get_column_name (ClutterModel *model,
                               guint         column)
{
  ClutterModelPrivate *priv;
  ClutterModelClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), NULL);

  priv = model->priv;
  if (column < 0 || column > priv->n_columns)
    {
      g_warning ("%s: Invalid column id value %d\n", G_STRLOC, column);
      return NULL;
    }

  klass = CLUTTER_MODEL_GET_CLASS (model);
  if (klass->get_column_name)
    return klass->get_column_name (model, column);

  return NULL;
}

static GType
_model_get_column_type (ClutterModel *model,
                        guint         column)
{
  ClutterModelPrivate *priv = model->priv;

  if (column < 0 || column >= priv->n_columns)
    return G_TYPE_INVALID;

  return priv->column_types[column];
}

/**
 * clutter_model_get_column_type:
 * @model: #ClutterModel
 * @column: the column number
 *
 * Retrieves the type of the @column.
 *
 * Return value: the type of the column.
 *
 * Since: 0.6
 */
GType
clutter_model_get_column_type (ClutterModel *model,
                               guint         column)
{
  ClutterModelPrivate *priv;
  ClutterModelClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), G_TYPE_INVALID);

  priv = model->priv;
  if (column < 0 || column > priv->n_columns)
    {
      g_warning ("%s: Invalid column id value %d\n", G_STRLOC, column);
      return G_TYPE_INVALID;
    }

  klass = CLUTTER_MODEL_GET_CLASS (model);
  if (klass->get_column_type)
    return klass->get_column_type (model, column);

  return G_TYPE_INVALID;
}

static ClutterModelIter * 
_model_get_iter_at_row (ClutterModel *model, 
                        guint         row)
{
  return g_object_new (CLUTTER_TYPE_MODEL_ITER,
                       "model", model,
                       "row", row,
                       NULL);
}

/**
 * clutter_model_get_iter_at_row:
 * @model: a #ClutterModel
 * @row: position of the row to retrieve
 *
 * Retrieves a #ClutterModelIter representing the row at the given position.
 *
 * Return value: A new #ClutterModelIter, or %NULL if @row was out-of-bounds
 *
 * Since 0.6
 **/
ClutterModelIter * 
clutter_model_get_iter_at_row (ClutterModel *model,
                               guint         row)
{
  ClutterModelClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), NULL);

  klass = CLUTTER_MODEL_GET_CLASS (model);
  if (klass->get_iter_at_row)
    return klass->get_iter_at_row (model, row);

  return NULL;
}


/**
 * clutter_model_get_first_iter:
 * @model: a #ClutterModel
 *
 * Retrieves a #ClutterModelIter representing the first row in @model.
 *
 * Return value: A new #ClutterModelIter
 *
 * Since 0.6
 **/
ClutterModelIter *
clutter_model_get_first_iter (ClutterModel *model)
{
  g_return_val_if_fail (CLUTTER_IS_MODEL (model), NULL);

  return clutter_model_get_iter_at_row (model, 0);
}

/**
 * clutter_model_get_last_iter:
 * @model: a #ClutterModel
 *
 * Retrieves a #ClutterModelIter representing the last row in @model.
 *
 * Return value: A new #ClutterModelIter
 *
 * Since 0.6
 **/
ClutterModelIter *
clutter_model_get_last_iter (ClutterModel *model)
{
  guint length;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), NULL);

  length = clutter_model_get_n_rows (model);

  return clutter_model_get_iter_at_row (model, length-1);
}

/**
 * clutter_model_get_n_rows:
 * @model: a #ClutterModel
 *
 * Retrieves the number of rows inside @model.
 *
 * Return value: The length of the @model. If there is a filter set, then
 *   thelength of the filtered @model is returned.
 *
 * Since 0.6
 */
guint
clutter_model_get_n_rows (ClutterModel *model)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  GSequenceIter *seq_iter;
  guint i = 0;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), 0);
  priv = model->priv;

  seq_iter = g_sequence_get_begin_iter (priv->sequence);
  iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, "model", model, NULL);

  while (!g_sequence_iter_is_end (seq_iter))
    {
      g_object_set (iter, "iter", seq_iter, NULL);
      if (_model_filter (model, iter))
        {
          i++;
        }
      seq_iter = g_sequence_iter_next (seq_iter);
    }

  g_object_unref (iter);
  
  return i;
}


/**
 * clutter_model_set_sorting_column:
 * @model: a #ClutterModel
 * @column: the column of the @model to sort, or -1
 *
 * Sets the model to sort by @column. If @column is a negative value
 * the sorting column will be unset.
 *
 * Since 0.6
 */
void               
clutter_model_set_sorting_column (ClutterModel *model,
                                  gint          column)
{
  ClutterModelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  if (column > priv->n_columns)
    {
      g_warning ("%s: Invalid column id value %d\n", G_STRLOC, column);
      return;
    }

  priv->sort_column = column;

  if (priv->sort_column > 0)
    _model_sort (model);

  g_signal_emit (model, model_signals[SORT_CHANGED], 0);
}

/**
 * clutter_model_get_sorting_column:
 * @model: a #ClutterModel
 *
 * Retrieves the number of column used for sorting the @model.
 *
 * Return value: a column number, or -1 if the model is not sorted
 *
 * Since 0.6
 */
gint
clutter_model_get_sorting_column (ClutterModel *model)
{
  g_return_val_if_fail (CLUTTER_IS_MODEL (model), -1);

  return model->priv->sort_column;
}

/**
 * clutter_model_foreach:
 * @model: a #ClutterModel
 * @func: a #ClutterModelForeachFunc
 * @user_data: user data to pass to @func
 *
 * Calls @func for each row in the model. 
 *
 * Since 0.6
 */
void
clutter_model_foreach (ClutterModel            *model,
                       ClutterModelForeachFunc  func,
                       gpointer                 user_data)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  GSequenceIter *seq_iter;
  
  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  seq_iter = g_sequence_get_begin_iter (priv->sequence);
  iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, "model", model, NULL);

  while (!g_sequence_iter_is_end (seq_iter))
    {
      g_object_set (iter, "iter", seq_iter, NULL);
      if (_model_filter (model, iter))
        {
          if (!func (model, iter, user_data))
            break;
        }
      seq_iter = g_sequence_iter_next (seq_iter);
    }

  g_object_unref (iter);
}

/**
 * clutter_model_set_sort:
 * @model: a #ClutterModel
 * @column: the column to sort on
 * @func: a #ClutterModelSortFunc, or #NULL
 * @user_data: user data to pass to @func, or #NULL
 * @notify: destroy notifier of @user_data, or #NULL
 *
 * Sorts the @model using @func.
 *
 * Since 0.6
 */
void
clutter_model_set_sort (ClutterModel         *model,
                        guint                 column,
                        ClutterModelSortFunc  func,
                        gpointer              user_data,
                        GDestroyNotify        notify)
{
  ClutterModelPrivate *priv;
    
  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  if (priv->sort_data && priv->sort_notify)
    priv->sort_notify (priv->sort_data);

  priv->sort = func;
  priv->sort_data = user_data;
  priv->sort_notify = notify;
  
  /* This takes care of calling _model_sort & emitting the signal*/
  clutter_model_set_sorting_column (model, column);
}

/**
 * clutter_model_set_filter:
 * @model: a #ClutterModel
 * @func: a #ClutterModelFilterFunc, or #NULL
 * @user_data: user data to pass to @func, or #NULL
 * @notify: destroy notifier of @user_data, or #NULL
 *
 * Filters the @model using @func.
 *
 * Since 0.6
 */
void
clutter_model_set_filter (ClutterModel           *model,
                          ClutterModelFilterFunc  func,
                          gpointer                user_data,
                          GDestroyNotify          notify)
{
  ClutterModelPrivate *priv;
    
  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  if (priv->filter_data && priv->filter_notify)
    priv->filter_notify (priv->filter_data);

  priv->filter = func;
  priv->filter_data = user_data;
  priv->filter_notify = notify;

  g_signal_emit (model, model_signals[FILTER_CHANGED], 0);
}

/*
 * ClutterModelIter Object 
 */

/**
 * SECTION:clutter-model-iter
 * @short_description: Iterates through a model
 *
 * #ClutterModelIter is an object used for iterating through all the rows
 * of a #ClutterModel. It allows setting and getting values on the row
 * which is currently pointing at.
 *
 * A #ClutterModelIter represents a position between two elements
 * of the sequence. For example, the iterator returned by
 * clutter_model_get_first_iter() represents the gap immediately before
 * the first row of the #ClutterModel, and the iterator returned by
 * clutter_model_get_last_iter() represents the gap immediately after the
 * last row.
 *
 * A #ClutterModelIter can only be created by a #ClutterModel and it is
 * valid as long as the model does not change.
 *
 * #ClutterModelIter is available since Clutter 0.6
 */

G_DEFINE_TYPE (ClutterModelIter, clutter_model_iter, G_TYPE_OBJECT);

#define CLUTTER_MODEL_ITER_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
  CLUTTER_TYPE_MODEL_ITER, ClutterModelIterPrivate))

struct _ClutterModelIterPrivate
{
  ClutterModel  *model;
  GSequenceIter *seq_iter;

  gboolean ignore_sort;
};

enum
{
  ITER_PROP_0,

  ITER_PROP_MODEL,
  ITER_PROP_ROW,
  ITER_PROP_ITER,
};

/* Forwards */
static void               _model_iter_get_value (ClutterModelIter *iter,
                                                 guint             column,
                                                 GValue           *value);
static void               _model_iter_set_value (ClutterModelIter *iter,
                                                 guint             column,
                                                 const GValue     *value);
static gboolean           _model_iter_is_first  (ClutterModelIter *iter);
static gboolean           _model_iter_is_last   (ClutterModelIter *iter);
static ClutterModelIter * _model_iter_next      (ClutterModelIter *iter);
static ClutterModelIter * _model_iter_prev      (ClutterModelIter *iter);
static ClutterModel     * _model_iter_get_model (ClutterModelIter *iter);
static guint              _model_iter_get_row   (ClutterModelIter *iter);
static void               _model_iter_set_row   (ClutterModelIter *iter,
                                                 guint             row);

/* GObject stuff */
static void
clutter_model_iter_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ClutterModelIter *iter = CLUTTER_MODEL_ITER (object);
  ClutterModelIterPrivate *priv;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (object));
  priv = iter->priv;

  switch (prop_id)
    {
      case ITER_PROP_MODEL:
        g_value_set_object (value, priv->model);
        break;
      case ITER_PROP_ROW:
        g_value_set_uint (value, clutter_model_iter_get_row (iter));
        break;
      case ITER_PROP_ITER:
        g_value_set_pointer (value, priv->seq_iter);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clutter_model_iter_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterModelIter *iter = CLUTTER_MODEL_ITER (object);
  ClutterModelIterPrivate *priv;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (object));
  priv = iter->priv;

  switch (prop_id)
    {
      case ITER_PROP_MODEL:
        priv->model = g_value_get_object (value);
        break;
      case ITER_PROP_ROW:
        _model_iter_set_row (iter, g_value_get_uint (value));
        break;
      case ITER_PROP_ITER:
        priv->seq_iter = g_value_get_pointer (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clutter_model_iter_class_init (ClutterModelIterClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = clutter_model_iter_get_property;
  gobject_class->set_property = clutter_model_iter_set_property;

  klass->get_value = _model_iter_get_value;
  klass->set_value = _model_iter_set_value;
  klass->is_first  = _model_iter_is_first;
  klass->is_last   = _model_iter_is_last;
  klass->next      = _model_iter_next;
  klass->prev      = _model_iter_prev;
  klass->get_model = _model_iter_get_model;
  klass->get_row   = _model_iter_get_row;

  /* Properties */

  /**
   * ClutterModelIter:model:
   *
   * A reference to the #ClutterModel that this iter belongs to.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
    ITER_PROP_MODEL,
    g_param_spec_object ("model",
                          "Model",
                          "A ClutterModel",
                          CLUTTER_TYPE_MODEL,
                          G_PARAM_READWRITE));

   /**
   * ClutterModelIter:row:
   *
   * A reference to the row number that this iter represents.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
    ITER_PROP_ROW,
    g_param_spec_uint ("row",
                       "Row",
                       "The row number",
                       0, 65535, 0,
                       G_PARAM_READWRITE));
  
   /**
   * ClutterModelIter:iter:
   *
   * An internal iter reference. This property should only be used when 
   * sub-classing #ClutterModelIter.
   *
   * Since: 0.6
   */  
  g_object_class_install_property (gobject_class,
    ITER_PROP_ITER,
    g_param_spec_pointer ("iter",
                          "Iter",
                          "The internal iter reference",
                          G_PARAM_READWRITE));
  
  g_type_class_add_private (gobject_class, sizeof (ClutterModelIterPrivate));
}

static void
clutter_model_iter_init (ClutterModelIter *self)
{
  ClutterModelIterPrivate *priv;
  
  self->priv = priv = CLUTTER_MODEL_ITER_GET_PRIVATE (self);

  priv->seq_iter = NULL;
  priv->model = NULL;
  priv->ignore_sort = FALSE;
}

static void
_model_iter_get_value (ClutterModelIter *iter,
                       guint             column,
                       GValue           *value)
{
  ClutterModelIterPrivate *priv;
  GValueArray *value_array;
  GValue *iter_value;
  GValue real_value = { 0, };
  gboolean converted = FALSE;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));
  priv = iter->priv;
  g_return_if_fail (CLUTTER_IS_MODEL (priv->model));
  g_return_if_fail (priv->seq_iter);

  value_array = g_sequence_get (priv->seq_iter);
  iter_value = g_value_array_get_nth (value_array, column);

  if (!g_type_is_a (G_VALUE_TYPE (value), G_VALUE_TYPE (iter_value)))
    {
      if (!g_value_type_compatible (G_VALUE_TYPE (value), 
                                    G_VALUE_TYPE (iter_value)) &&
          !g_value_type_compatible (G_VALUE_TYPE (iter_value), 
                                    G_VALUE_TYPE (value)))
        {
          g_warning ("%s: Unable to convert from %s to %s\n",
                     G_STRLOC,
                     g_type_name (G_VALUE_TYPE (value)),
                     g_type_name (G_VALUE_TYPE (iter_value)));
          return;
        }
      if (!g_value_transform (value, &real_value))
        {
          g_warning ("%s: Unable to make conversion from %s to %s\n",
                     G_STRLOC, 
                     g_type_name (G_VALUE_TYPE (value)),
                     g_type_name (G_VALUE_TYPE (iter_value)));
          g_value_unset (&real_value);
        }
      converted = TRUE;
    }
  
  if (converted)
    g_value_copy (&real_value, value);
  else
    g_value_copy (iter_value, value);
}

static void
_model_iter_set_value (ClutterModelIter *iter,
                       guint             column,
                       const GValue     *value)
{
  ClutterModelPrivate *model_priv;
  ClutterModelIterPrivate *priv;
  GValueArray *value_array;
  GValue *iter_value;
  GValue real_value = { 0, };
  gboolean converted = FALSE;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));
  priv = iter->priv;
  g_return_if_fail (CLUTTER_IS_MODEL (priv->model));
  g_return_if_fail (priv->seq_iter);

  model_priv = priv->model->priv;

  value_array = g_sequence_get (priv->seq_iter);
  iter_value = g_value_array_get_nth (value_array, column);

  if (!g_type_is_a (G_VALUE_TYPE (value), G_VALUE_TYPE (iter_value)))
    {
      if (!g_value_type_compatible (G_VALUE_TYPE (value), 
                                    G_VALUE_TYPE (iter_value)) &&
          !g_value_type_compatible (G_VALUE_TYPE (iter_value), 
                                    G_VALUE_TYPE (value)))
        {
          g_warning ("%s: Unable to convert from %s to %s\n",
                     G_STRLOC,
                     g_type_name (G_VALUE_TYPE (value)),
                     g_type_name (G_VALUE_TYPE (iter_value)));
          return;
        }
      if (!g_value_transform (value, &real_value))
        {
          g_warning ("%s: Unable to make conversion from %s to %s\n",
                     G_STRLOC, 
                     g_type_name (G_VALUE_TYPE (value)),
                     g_type_name (G_VALUE_TYPE (iter_value)));
          g_value_unset (&real_value);
        }
      converted = TRUE;
    }
 
  if (converted)
    {
      g_value_copy (&real_value, iter_value);
      g_value_unset (&real_value);
    }
  else
    g_value_copy (value, iter_value);

  /* Check if we need to sort */
  if (!priv->ignore_sort)
    {
      if (model_priv->sort_column == column && model_priv->sort)
        _model_sort (priv->model);

      g_signal_emit (priv->model, model_signals[ROW_CHANGED], 0, iter);
    }
}

static gboolean
_model_iter_is_first (ClutterModelIter *iter)
{
  ClutterModel *model = NULL;
  ClutterModelPrivate *model_priv;
  ClutterModelIterPrivate *iter_priv;
  GSequenceIter *begin, *end;
  ClutterModelIter *temp_iter;
  
  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), TRUE);
  iter_priv = iter->priv;
  model = iter_priv->model;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), TRUE);
  model_priv = model->priv;

  begin = g_sequence_get_begin_iter (model_priv->sequence);
  end  = iter_priv->seq_iter;
  
  temp_iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, "model", model, NULL);

  while (!g_sequence_iter_is_begin (begin))
    {
      temp_iter->priv->seq_iter = begin;
      if (_model_filter (model, temp_iter))
      {
        end = begin;
        break;
      }
      begin = g_sequence_iter_prev (begin);
    }
  /* This is because the 'begin_iter' is always *before* the last valid iter.
   * Otherwise we'd have endless loops 
   */
  end = g_sequence_iter_prev (end);

  g_object_unref (temp_iter);
  return iter_priv->seq_iter == end;
}

static gboolean
_model_iter_is_last (ClutterModelIter *iter)
{
  ClutterModel *model = NULL;
  ClutterModelPrivate *model_priv;
  ClutterModelIterPrivate *iter_priv;
  GSequenceIter *begin, *end;
  ClutterModelIter *temp_iter;
  
  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), TRUE);
  iter_priv = iter->priv;
  model = iter_priv->model;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), TRUE);
  model_priv = model->priv;

  if (g_sequence_iter_is_end (iter_priv->seq_iter))
    return TRUE;

  begin = g_sequence_get_end_iter (model_priv->sequence);
  begin = g_sequence_iter_prev (begin);
  end  = iter_priv->seq_iter;
  
  temp_iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, 
                            "model", model, 
                            "iter", begin,
                            NULL);

  while (!g_sequence_iter_is_begin (begin))
    {
      temp_iter->priv->seq_iter = begin;
      if (_model_filter (model, temp_iter))
        {
          end = begin;
          break;
        }

      begin = g_sequence_iter_prev (begin);
    }
  /* This is because the 'end_iter' is always *after* the last valid iter.
   * Otherwise we'd have endless loops 
   */
  end = g_sequence_iter_next (end);

  g_object_unref (temp_iter);
  return iter_priv->seq_iter == end;
}

static ClutterModelIter *
_model_iter_next (ClutterModelIter *iter)
{
  ClutterModel *model = NULL;
  ClutterModelPrivate *model_priv;
  ClutterModelIterPrivate *iter_priv;
  GSequenceIter *filter_next;
  ClutterModelIter *temp_iter;
  
  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), iter);
  iter_priv = iter->priv;
  model = iter_priv->model;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), iter);
  model_priv = model->priv;

  filter_next = g_sequence_iter_next (iter_priv->seq_iter);
  temp_iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, "model", model, NULL);

  while (!g_sequence_iter_is_end (filter_next))
    {
      g_object_set (temp_iter, "iter", filter_next, NULL);
      if (_model_filter (model, temp_iter))
        break;

      filter_next = g_sequence_iter_next (filter_next);
    }
  
  g_object_unref (temp_iter);

  /* We do this because the 'end_iter' is always *after* the last valid iter.
   * Otherwise loops will go on forever
   */
  if (filter_next == iter_priv->seq_iter)
    filter_next = g_sequence_iter_next (filter_next);
  
  g_object_set (iter, "iter", filter_next, NULL);
  return iter;
}

static ClutterModelIter *
_model_iter_prev (ClutterModelIter *iter)
{
  ClutterModel *model = NULL;
  ClutterModelPrivate *model_priv;
  ClutterModelIterPrivate *iter_priv;
  GSequenceIter *filter_prev;
  ClutterModelIter *temp_iter;
  
  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), iter);
  iter_priv = iter->priv;
  model = iter_priv->model;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), iter);
  model_priv = model->priv;

  filter_prev = g_sequence_iter_prev (iter_priv->seq_iter);
  temp_iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, "model", model, NULL);

  while (!g_sequence_iter_is_begin (filter_prev))
    {
      g_object_set (temp_iter, "iter", filter_prev, NULL);
      if (_model_filter (model, temp_iter))
        break;

      filter_prev = g_sequence_iter_prev (filter_prev);
    }
  
  /* We do this because the 'end_iter' is always *after* the last valid iter.
   * Otherwise loops will go on forever
   */
  if (filter_prev == iter_priv->seq_iter)
    filter_prev = g_sequence_iter_prev (filter_prev);
  
  g_object_set (iter, "iter", filter_prev, NULL);
  
  g_object_unref (temp_iter);
  
  return iter;
}

static ClutterModel *
_model_iter_get_model (ClutterModelIter *iter)
{
  ClutterModelIterPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), NULL);
  priv = iter->priv;
  g_return_val_if_fail (CLUTTER_IS_MODEL (priv->model), NULL);
  
  return priv->model;
}

static guint
_model_iter_get_row (ClutterModelIter *iter)
{
  ClutterModel *model = NULL;
  ClutterModelPrivate *model_priv;
  ClutterModelIterPrivate *iter_priv;
  GSequenceIter *filter_next;
  ClutterModelIter *temp_iter;
  guint row = 0;
  
  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), row);
  iter_priv = iter->priv;
  model = iter_priv->model;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), row);
  model_priv = model->priv;

  filter_next = g_sequence_iter_next (iter_priv->seq_iter);
  temp_iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, 
                            "model", model, NULL);

  while (!g_sequence_iter_is_end (filter_next))
    {
      g_object_set (temp_iter, "iter", filter_next, NULL);
      if (_model_filter (model, temp_iter))
        {
          if (iter_priv->seq_iter == temp_iter->priv->seq_iter)
            break;

          row++;
        }
      filter_next = g_sequence_iter_next (filter_next);
    }
  
  g_object_unref (temp_iter);

  return row;
}

static void
_model_iter_set_row (ClutterModelIter *iter, guint row)
{
  ClutterModel *model = NULL;
  ClutterModelPrivate *model_priv;
  ClutterModelIterPrivate *iter_priv;
  GSequenceIter *filter_next;
  ClutterModelIter *temp_iter;
  guint i = 0;
  
  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));
  iter_priv = iter->priv;
  model = iter_priv->model;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  model_priv = model->priv;

  temp_iter = g_object_new (CLUTTER_TYPE_MODEL_ITER, 
                            "model", model,
                            NULL);

  filter_next = g_sequence_get_begin_iter (model_priv->sequence);
  while (!g_sequence_iter_is_end (filter_next))
    {
      g_object_set (temp_iter, "iter", filter_next, NULL);
      if (_model_filter (model, temp_iter))
        {
          if (i == row)
            {
              iter_priv->seq_iter = filter_next;             
              break;
            }

          i++;
        }

      filter_next = g_sequence_iter_next (filter_next);
    }

  g_object_unref (temp_iter);
}

/*
 *  Public functions
 */

/**
 * clutter_model_iter_set_valist:
 * @iter: a #ClutterModelIter
 * @args: va_list of column/value pairs, terminiated by -1
 *
 * See #clutter_model_iter_set; this version takes a va_list for language
 * bindings.
 *
 * Since 0.6
 */
void 
clutter_model_iter_set_valist (ClutterModelIter *iter,
                               va_list           args)
{
  ClutterModel *model;
  ClutterModelPrivate *model_priv;
  ClutterModelIterPrivate *priv;
  guint column = 0;
  gboolean sort = FALSE;
  
  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));
  priv = iter->priv;
  model = priv->model;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  model_priv = model->priv;
 
  column = va_arg (args, gint);
  
  /* Don't want to sort while setting lots of fields, leave that till the end
   */
  priv->ignore_sort = TRUE;
  while (column != -1)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column < 0 || column >= model_priv->n_columns)
        { 
          g_warning ("%s: Invalid column number %d added to iter "
                     "(remember to end you list of columns with a -1)",
                     G_STRLOC, column);
          break;
        }
      g_value_init (&value, model_priv->column_types[column]);

      G_VALUE_COLLECT (&value, args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);

          /* Leak value as it might not be in a sane state */
          break;
        }

      clutter_model_iter_set_value (iter, column, &value);
      
      g_value_unset (&value);
      
      if (column == model_priv->sort_column)
        sort = TRUE;
      
      column = va_arg (args, gint);
    }

  priv->ignore_sort = FALSE;
  if (sort)
    _model_sort (model);

  g_signal_emit (model, model_signals[ROW_CHANGED], 0, iter);
}

/**
 * clutter_model_iter_get:
 * @iter: a #ClutterModelIter
 * @Varargs: va_list of column/return location pairs, terminiated by -1
 *
 * Gets the value of one or more cells in the row referenced by @iter. The
 * variable argument list should contain integer column numbers, each column
 * column number followed by a place to store the value being retrieved. The
 * list is terminated by a -1. For example, to get a value from column 0
 * with type G_TYPE_STRING, you would write: <literal>clutter_model_iter_get
 * (iter, 0, &place_string_here, -1);</literal>, where place_string_here is
 * a gchar* to be filled with the string. If appropriate, the returned values
 * have to be freed or unreferenced.
 *
 * Since 0.6
 */
void
clutter_model_iter_get (ClutterModelIter *iter,
                        ...)
{
  va_list args;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));

  va_start (args, iter);
  clutter_model_iter_get_valist (iter, args);
  va_end (args);
}

/**
 * clutter_model_iter_get_value:
 * @iter: a #ClutterModelIter
 * @column: column number to retrieve the value from
 * @value: an empty #GValue to set
 *
 * Sets an initializes @value to that at @column. When done with @value, 
 * #g_value_unset() needs to be called to free any allocated memory.
 *
 * Since 0.6
 */
void
clutter_model_iter_get_value (ClutterModelIter *iter,
                              guint             column,
                              GValue           *value)
{
  ClutterModelIterClass *klass;
  ClutterModel *model;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  g_return_if_fail (klass->get_value != NULL);

  model = iter->priv->model;
  g_value_init (value, clutter_model_get_column_type (model, column));
  klass->get_value (iter, column, value);
}

/**
 * clutter_model_iter_get_valist:
 * @iter: a #ClutterModelIter
 * @args: va_list of column/return location pairs, terminiated by -1
 *
 * See #clutter_model_iter_get; this version takes a va_list for language
 * bindings.
 *
 * Since 0.6
 */
void 
clutter_model_iter_get_valist (ClutterModelIter *iter,
                               va_list           args)
{
  ClutterModel *model;
  ClutterModelPrivate *model_priv;
  ClutterModelIterPrivate *priv;
  guint column = 0;
  
  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));
  priv = iter->priv;
  model = priv->model;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  model_priv = model->priv;
 
  column = va_arg (args, gint);

  while (column != -1)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column < 0 || column >= model_priv->n_columns)
        { 
          g_warning ("%s: Invalid column number %d added to iter "
                     "(remember to end you list of columns with a -1)",
                     G_STRLOC, column);
          break;
        }

      /* this one will take care of initialising value to the
       * correct type
       */
      clutter_model_iter_get_value (iter, column, &value);

      G_VALUE_LCOPY (&value, args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);

          /* Leak value as it might not be in a sane state */
          break;
        }
     
      g_value_unset (&value);
      
      column = va_arg (args, gint);
    }
}

/**
 * clutter_model_iter_set:
 * @iter: a #ClutterModelIter
 * @Varargs: va_list of column/return location pairs, terminiated by -1
 *
 * Sets the value of one or more cells in the row referenced by @iter. The
 * variable argument list should contain integer column numbers, each column
 * column number followed by the value to be set. The  list is terminated by a
 * -1. For example, to set column 0 with type G_TYPE_STRING, you would write:
 * <literal>clutter_model_iter_set (iter, 0, "foo", -1);</literal>.
 *
 * Since 0.6
 */
void
clutter_model_iter_set (ClutterModelIter *iter,
                        ...)
{
  va_list args;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));

  va_start (args, iter);
  clutter_model_iter_set_valist (iter, args);
  va_end (args);
}


/**
 * clutter_model_iter_set_value:
 * @iter: a #ClutterModelIter
 * @column: column number to retrieve the value from
 * @value: new value for the cell
 *
 * Sets the data in the cell specified by @iter and @column. The type of @value
 * must be convertable to the type of the column.
 *
 * Since 0.6
 */
void
clutter_model_iter_set_value (ClutterModelIter *iter,
                              guint             column,
                              const GValue     *value)
{
  ClutterModelIterClass *klass;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  g_return_if_fail (klass->set_value != NULL);
  
  klass->set_value (iter, column, value);
}

/**
 * clutter_model_iter_is_first:
 * @iter: a #ClutterModelIter
 *
 * Gets whether the current iterator is at the beginning of the model
 * to which it belongs.
 *
 * Return value: #TRUE if @iter is the first iter in the filtered model
 *
 * Since 0.6
 */
gboolean
clutter_model_iter_is_first (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), FALSE);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  g_return_val_if_fail (klass->is_first != NULL, FALSE);
  
  return klass->is_first (iter);
}

/**
 * clutter_model_iter_is_last:
 * @iter: a #ClutterModelIter
 * 
 * Gets whether the iterator is at the end of the model to which it
 * belongs.
 *
 * Return value: #TRUE if @iter is the last iter in the filtered model.
 *
 * Since 0.6
 */
gboolean
clutter_model_iter_is_last (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), FALSE);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  g_return_val_if_fail (klass->is_last != NULL, FALSE);
  
  return klass->is_last (iter);
}

/**
 * clutter_model_iter_next:
 * @iter: a #ClutterModelIter
 * 
 * Sets the @iter to point at the next position in the model after @iter.
 *
 * Return value: @iter, pointing at the next position.
 *
 * Since 0.6
 */
ClutterModelIter *
clutter_model_iter_next (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), NULL);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  g_return_val_if_fail (klass->next != NULL, NULL);
  
  return klass->next (iter);
}

/**
 * clutter_model_iter_prev:
 * @iter: a #ClutterModelIter
 * 
 * Sets the @iter to point at the previous position in the model after @iter.
 *
 * Return value: @iter, pointing at the previous position.
 *
 * Since 0.6
 */
ClutterModelIter *
clutter_model_iter_prev (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), NULL);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  g_return_val_if_fail (klass->prev != NULL, NULL);
  
  return klass->prev (iter);
}

/**
 * clutter_model_iter_get_model:
 * @iter: a #ClutterModelIter
 * 
 * Returns a pointer to the #ClutterModel that this iter is part of.
 *
 * Return value: a pointer to a #ClutterModel.
 *
 * Since 0.6
 */
ClutterModel *
clutter_model_iter_get_model (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), NULL);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  g_return_val_if_fail (klass->get_model != NULL, NULL);
  
  return klass->get_model (iter);
}

/**
 * clutter_model_iter_get_row:
 * @iter: a #ClutterModelIter
 * 
 * Returns the position of the row that the @iter points to.
 *
 * Return value: the position of the @iter in the model.
 *
 * Since 0.6
 */
guint
clutter_model_iter_get_row (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), -1);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  g_return_val_if_fail (klass->get_row != NULL, -1);
  
  return klass->get_row (iter);
}
