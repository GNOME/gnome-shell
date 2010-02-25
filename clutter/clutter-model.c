/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Neil Jagdish Patel <njp@o-hand.com>
 *             Emmanuele Bassi <ebassi@openedhand.com>
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
 * #ClutterModel is a generic list model API which can be used to implement
 * the model-view-controller architectural pattern in Clutter.
 *
 * The #ClutterModel class is a list model which can accept most GObject 
 * types as a column type.
 * 
 * Creating a simple clutter model:
 * <informalexample><programlisting>
 * enum
 * {
 *   COLUMN_INT,
 *   COLUMN_STRING,
 *
 *   N_COLUMNS
 * };
 * 
 * {
 *   ClutterModel *model;
 *   gint i;
 *
 *   model = clutter_model_default_new (N_COLUMNS,
 *                                      /<!-- -->* column type, column title *<!-- -->/
 *                                      G_TYPE_INT,     "my integers",
 *                                      G_TYPE_STRING,  "my strings");
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
 * </programlisting></informalexample>
 *
 * Iterating through the model consists of retrieving a new #ClutterModelIter
 * pointing to the starting row, and calling clutter_model_iter_next() or
 * clutter_model_iter_prev() to move forward or backwards, repectively.
 *
 * A valid #ClutterModelIter represents the position between two rows in the
 * model. For example, the "first" iterator represents the gap immediately 
 * before the first row, and the "last" iterator represents the gap immediately
 * after the last row. In an empty sequence, the first and last iterators are
 * the same.
 *
 * Iterating a #ClutterModel:
 * <informalexample><programlisting>
 * enum
 * {
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
 * </programlisting></informalexample>
 *
 * #ClutterModel is an abstract class. Clutter provides a list model
 * implementation called #ClutterListModel which has been optimised
 * for insertion and look up in sorted lists.
 *
 * <refsect2 id="ClutterModel-script">
 *   <title>ClutterModel custom properties for #ClutterScript</title>
 *   <para>#ClutterModel defines a custom property "columns" for #ClutterScript
 *   which allows defining the column names and types.</para>
 *   <example id=ClutterModel-script-column-example">
 *     <title>Example of the "columns" custom property</title>
 *     <para>The definition below will create a #ClutterListModel with three
 *     columns: the first one with name "Name" and containing strings; the
 *     second one with name "Score" and containing integers; the third one with
 *     name "Icon" and containing #ClutterTexture<!-- -->s.</para>
 *     <programlisting>
 *  {
 *    "type" : "ClutterListModel",
 *    "id" : "teams-model",
 *    "columns" : [
 *      [ "Name", "gchararray" ],
 *      [ "Score", "gint" ],
 *      [ "Icon", "ClutterTexture" ]
 *    ]
 *  }
 *     </programlisting>
 *   </example>
 * </refsect2>
 *
 * #ClutterModel is available since Clutter 0.6
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-model.h"
#include "clutter-model-private.h"

#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-scriptable.h"

static void clutter_scriptable_iface_init (ClutterScriptableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE
        (ClutterModel, clutter_model, G_TYPE_OBJECT,
         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                clutter_scriptable_iface_init));

enum
{
  PROP_0,

  PROP_FILTER_SET
};

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
  GType                  *column_types;
  gchar                 **column_names;
  gint                    n_columns; 

  ClutterModelFilterFunc  filter_func;
  gpointer                filter_data;
  GDestroyNotify          filter_notify;

  gint                    sort_column;
  ClutterModelSortFunc    sort_func;
  gpointer                sort_data;
  GDestroyNotify          sort_notify;
};

static GType
clutter_model_real_get_column_type (ClutterModel *model,
                                    guint         column)
{
  ClutterModelPrivate *priv = model->priv;

  if (column < 0 || column >= clutter_model_get_n_columns (model))
    return G_TYPE_INVALID;

  return priv->column_types[column];
}

static const gchar *
clutter_model_real_get_column_name (ClutterModel *model,
                                    guint         column)
{
  ClutterModelPrivate *priv = model->priv;

  if (column < 0 || column >= clutter_model_get_n_columns (model))
    return NULL;

  if (priv->column_names && priv->column_names[column])
    return priv->column_names[column];

  return g_type_name (priv->column_types[column]);
}

static guint
clutter_model_real_get_n_columns (ClutterModel *model)
{
  return model->priv->n_columns;
}

static void 
clutter_model_finalize (GObject *object)
{
  ClutterModelPrivate *priv = CLUTTER_MODEL (object)->priv;
  gint i;

  if (priv->sort_notify)
    priv->sort_notify (priv->sort_data);
  
  if (priv->filter_notify)
    priv->filter_notify (priv->filter_data);

  g_free (priv->column_types);

  if (priv->column_names != NULL)
    {
      /* the column_names vector might have holes in it, so we need
       * to use the columns number to clear up everything
       */
      for (i = 0; i < priv->n_columns; i++)
        g_free (priv->column_names[i]);

      g_free (priv->column_names);
    }

  G_OBJECT_CLASS (clutter_model_parent_class)->finalize (object);
}

static void
clutter_model_get_property (GObject    *gobject,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ClutterModelPrivate *priv = CLUTTER_MODEL (gobject)->priv;

  switch (prop_id)
    {
    case PROP_FILTER_SET:
      g_value_set_boolean (value, priv->filter_func != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_model_class_init (ClutterModelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->get_property = clutter_model_get_property;
  gobject_class->finalize = clutter_model_finalize;

  g_type_class_add_private (gobject_class, sizeof (ClutterModelPrivate));

  klass->get_column_name  = clutter_model_real_get_column_name;
  klass->get_column_type  = clutter_model_real_get_column_type;
  klass->get_n_columns    = clutter_model_real_get_n_columns;

  /**
   * ClutterModel:filter-set:
   *
   * Whether the #ClutterModel has a filter set
   *
   * This property is set to %TRUE if a filter function has been
   * set using clutter_model_set_filter()
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("filter-set",
                                "Filter Set",
                                "Whether the model has a filter",
                                FALSE,
                                CLUTTER_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_FILTER_SET, pspec);

   /**
   * ClutterModel::row-added:
   * @model: the #ClutterModel on which the signal is emitted
   * @iter: a #ClutterModelIter pointing to the new row
   *
   * The ::row-added signal is emitted when a new row has been added.
   * The data on the row has already been set when the ::row-added signal
   * has been emitted.
   *
   * Since: 0.6
   */
  model_signals[ROW_ADDED] =
    g_signal_new ("row-added",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, row_added),
                  NULL, NULL,
                  clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_MODEL_ITER);
   /**
   * ClutterModel::row-removed:
   * @model: the #ClutterModel on which the signal is emitted
   * @iter: a #ClutterModelIter pointing to the removed row
   *
   * The ::row-removed signal is emitted when a row has been removed.
   * The data on the row pointed by the passed iterator is still valid
   * when the ::row-removed signal has been emitted.
   *
   * Since: 0.6
   */
  model_signals[ROW_REMOVED] =
    g_signal_new ("row-removed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, row_removed),
                  NULL, NULL,
                  clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_MODEL_ITER);
   /**
   * ClutterModel::row-changed:
   * @model: the #ClutterModel on which the signal is emitted
   * @iter: a #ClutterModelIter pointing to the changed row
   *
   * The ::row-removed signal is emitted when a row has been changed.
   * The data on the row has already been updated when the ::row-changed
   * signal has been emitted.
   *
   * Since: 0.6
   */
  model_signals[ROW_CHANGED] =
    g_signal_new ("row-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, row_changed),
                  NULL, NULL,
                  clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_MODEL_ITER);
  /**
   * ClutterModel::sort-changed:
   * @model: the #ClutterModel on which the signal is emitted
   * 
   * The ::sort-changed signal is emitted after the model has been sorted
   *
   * Since: 0.6
   */
  model_signals[SORT_CHANGED] =
    g_signal_new ("sort-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, sort_changed),
                  NULL, NULL,
                  clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
   /**
   * ClutterModel::filter-changed:
   * @model: the #ClutterModel on which the signal is emitted   
   *
   * The ::filter-changed signal is emitted when a new filter has been applied
   *
   * Since: 0.6
   */
  model_signals[FILTER_CHANGED] =
    g_signal_new ("filter-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterModelClass, filter_changed),
                  NULL, NULL,
                  clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
clutter_model_init (ClutterModel *self)
{
  ClutterModelPrivate *priv;
  
  self->priv = priv = CLUTTER_MODEL_GET_PRIVATE (self);

  priv->n_columns = -1;
  priv->column_types = NULL;
  priv->column_names = NULL;

  priv->filter_func = NULL;
  priv->filter_data = NULL;
  priv->filter_notify = NULL;

  priv->sort_column = -1;
  priv->sort_func = NULL;
  priv->sort_data = NULL;
  priv->sort_notify = NULL;
}

/* XXX - is this whitelist really necessary? we accept every fundamental
 * type.
 */
gboolean
clutter_model_check_type (GType gtype)
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

  if (! G_TYPE_IS_VALUE_TYPE (gtype))
    return FALSE;

  while (type_list[i] != G_TYPE_INVALID)
    {
      if (g_type_is_a (gtype, type_list[i]))
	return TRUE;
      i++;
    }
  return FALSE;
}


typedef struct {
    gchar *name;
    GType  type;
} ColumnInfo;

static gboolean
clutter_model_parse_custom_node (ClutterScriptable *scriptable,
                                 ClutterScript     *script,
                                 GValue            *value,
                                 const gchar       *name,
                                 JsonNode          *node)
{
  GSList *columns = NULL;
  GList *elements, *l;

  if (strcmp (name, "columns") != 0)
    return FALSE;

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    return FALSE;

  elements = json_array_get_elements (json_node_get_array (node));

  for (l = elements; l != NULL; l = l->next)
    {
      JsonNode *child_node = l->data;
      JsonArray *array = json_node_get_array (child_node);
      ColumnInfo *cinfo;
      const gchar *column_name;
      const gchar *type_name;

      if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY ||
          json_array_get_length (array) != 2)
        {
          g_warning ("A column must be an array of "
                     "[\"column-name\", \"GType-name\"] pairs");
          return FALSE;
        }

      column_name = json_array_get_string_element (array, 0);
      type_name = json_array_get_string_element (array, 1);

      cinfo = g_slice_new0 (ColumnInfo);
      cinfo->name = g_strdup (column_name);
      cinfo->type = clutter_script_get_type_from_name (script, type_name);

      columns = g_slist_prepend (columns, cinfo);
    }

  g_list_free (elements);

  g_value_init (value, G_TYPE_POINTER);
  g_value_set_pointer (value, g_slist_reverse (columns));

  return TRUE;
}

static void
clutter_model_set_custom_property (ClutterScriptable *scriptable,
                                   ClutterScript     *script,
                                   const gchar       *name,
                                   const GValue      *value)
{
  if (strcmp (name, "columns") == 0)
    {
      ClutterModel *model = CLUTTER_MODEL (scriptable);
      GSList *columns, *l;
      guint n_columns;
      gint i;

      columns = g_value_get_pointer (value);
      n_columns = g_slist_length (columns);

      clutter_model_set_n_columns (model, n_columns, TRUE, TRUE);

      for (i = 0, l = columns; l != NULL; l = l->next, i++)
        {
          ColumnInfo *cinfo = l->data;

          clutter_model_set_column_name (model, i, cinfo->name);
          clutter_model_set_column_type (model, i, cinfo->type);

          g_free (cinfo->name);
          g_slice_free (ColumnInfo, cinfo);
        }

      g_slist_free (columns);
    }
}


static void
clutter_scriptable_iface_init (ClutterScriptableIface *iface)
{
  iface->parse_custom_node = clutter_model_parse_custom_node;
  iface->set_custom_property = clutter_model_set_custom_property;
}

/**
 * clutter_model_resort:
 * @model: a #ClutterModel
 *
 * Force a resort on the @model. This function should only be
 * used by subclasses of #ClutterModel.
 *
 * Since: 0.6
 */
void
clutter_model_resort (ClutterModel *model)
{
  ClutterModelPrivate *priv;
  ClutterModelClass *klass;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  klass = CLUTTER_MODEL_GET_CLASS (model);

  if (klass->resort)
    klass->resort (model, priv->sort_func, priv->sort_data);
}

/**
 * clutter_model_filter_row:
 * @model: a #ClutterModel
 * @row: the row to filter
 *
 * Checks whether @row should be filtered or not using the
 * filtering function set on @model.
 *
 * This function should be used only by subclasses of #ClutterModel.
 *
 * Return value: %TRUE if the row should be displayed,
 *   %FALSE otherwise
 *
 * Since: 0.6
 */
gboolean
clutter_model_filter_row (ClutterModel *model,
                          guint         row)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  gboolean res = TRUE;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), TRUE);

  priv = model->priv;

  if (!priv->filter_func)
    return TRUE;

  iter = clutter_model_get_iter_at_row (model, row);
  if (iter == NULL)
    return FALSE;

  res = priv->filter_func (model, iter, priv->filter_data);

  g_object_unref (iter);

  return res;
}

/**
 * clutter_model_filter_iter:
 * @model: a #ClutterModel
 * @iter: the row to filter
 *
 * Checks whether the row pointer by @iter should be filtered or not using
 * the filtering function set on @model.
 *
 * This function should be used only by subclasses of #ClutterModel.
 *
 * Return value: %TRUE if the row should be displayed,
 *   %FALSE otherwise
 *
 * Since: 0.6
 */
gboolean
clutter_model_filter_iter (ClutterModel     *model,
                           ClutterModelIter *iter)
{
  ClutterModelPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), TRUE);
  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), TRUE);

  priv = model->priv;

  if (!priv->filter_func)
    return TRUE;

  return priv->filter_func (model, iter, priv->filter_data);
}

/*
 * clutter_model_set_n_columns:
 * @model: a #ClutterModel
 * @n_columns: number of columns
 * @set_types: set the columns type
 * @set_names: set the columns name
 *
 * Sets the number of columns in @model to @n_columns. If @set_types
 * or @set_names are %TRUE, initialises the columns type and name
 * arrays as well.
 *
 * This function can only be called once.
 */
void
clutter_model_set_n_columns (ClutterModel *model,
                             gint          n_columns,
                             gboolean      set_types,
                             gboolean      set_names)
{
  ClutterModelPrivate *priv = model->priv;

  if (priv->n_columns > 0 && priv->n_columns != n_columns)
    return;

  priv->n_columns = n_columns;
  
  if (set_types && !priv->column_types)
    priv->column_types = g_new0 (GType,  n_columns);

  if (set_names && !priv->column_names)
    priv->column_names = g_new0 (gchar*, n_columns);
}

/*
 * clutter_model_set_column_type:
 * @model: a #ClutterModel
 * @column: column index
 * @gtype: type of the column
 *
 * Sets the type of @column inside @model
 */
void
clutter_model_set_column_type (ClutterModel *model,
                               gint          column,
                               GType         gtype)
{
  ClutterModelPrivate *priv = model->priv;

  priv->column_types[column] = gtype;
}

/*
 * clutter_model_set_column_name:
 * @model: a #ClutterModel
 * @column: column index
 * @name: name of the column, or %NULL
 *
 * Sets the name of @column inside @model
 */
void
clutter_model_set_column_name (ClutterModel *model,
                               gint          column,
                               const gchar  *name)
{
  ClutterModelPrivate *priv = model->priv;

  priv->column_names[column] = g_strdup (name);
}

/**
 * clutter_model_set_types:
 * @model: a #ClutterModel
 * @n_columns: number of columns for the model
 * @types: (array length=n_columns): an array of #GType types
 *
 * Sets the types of the columns inside a #ClutterModel.
 *
 * This function is meant primarily for #GObjects that inherit from
 * #ClutterModel, and should only be used when contructing a #ClutterModel.
 * It will not work after the initial creation of the #ClutterModel.
 *
 * Since: 0.6
 */
void
clutter_model_set_types (ClutterModel *model,
                         guint         n_columns,
                         GType        *types)
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
 * @names: (array length=n_columns): an array of strings
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

  return CLUTTER_MODEL_GET_CLASS (model)->get_n_columns (model);
}

/**
 * clutter_model_appendv:
 * @model: a #ClutterModel
 * @n_columns: the number of columns and values
 * @columns: (array length=n_columns): a vector with the columns to set
 * @values: (array length=n_columns): a vector with the values
 *
 * Creates and appends a new row to the #ClutterModel, setting the row
 * values for the given @columns upon creation.
 *
 * Since: 0.6
 */
void
clutter_model_appendv (ClutterModel *model,
                       guint         n_columns,
                       guint        *columns,
                       GValue       *values)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  gint i;
  gboolean resort = FALSE;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  g_return_if_fail (n_columns <= clutter_model_get_n_columns (model));
  g_return_if_fail (columns != NULL);
  g_return_if_fail (values != NULL);

  priv = model->priv;

  iter = CLUTTER_MODEL_GET_CLASS (model)->insert_row (model, -1);
  g_assert (CLUTTER_IS_MODEL_ITER (iter));

  for (i = 0; i < n_columns; i++)
    {
      if (priv->sort_column == columns[i])
        resort = TRUE;

      clutter_model_iter_set_value (iter, columns[i], &values[i]);
    }

  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter);

  if (resort)
    clutter_model_resort (model);

  g_object_unref (iter);
}

/* forward declaration */
static void clutter_model_iter_set_internal_valist (ClutterModelIter *iter,
                                                    va_list           args);

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
 *   model = clutter_model_default_new (2,
 *                                      G_TYPE_INT,    "Score",
 *                                      G_TYPE_STRING, "Team");
 *   clutter_model_append (model, 0, 42, 1, "Team #1", -1);
 * </programlisting></informalexample>
 *
 * Since: 0.6
 */
void
clutter_model_append (ClutterModel *model,
                      ...)
{
  ClutterModelIter *iter;
  va_list args;

  g_return_if_fail (CLUTTER_IS_MODEL (model));

  iter = CLUTTER_MODEL_GET_CLASS (model)->insert_row (model, -1);
  g_assert (CLUTTER_IS_MODEL_ITER (iter));

  /* do not emit the ::row-changed signal */
  va_start (args, model);
  clutter_model_iter_set_internal_valist (iter, args);
  va_end (args);

  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter);

  g_object_unref (iter);
}

/**
 * clutter_model_prependv:
 * @model: a #ClutterModel
 * @n_columns: the number of columns and values to set
 * @columns: (array length=n_columns): a vector containing the columns to set
 * @values: (array length=n_columns): a vector containing the values for the cells
 *
 * Creates and prepends a new row to the #ClutterModel, setting the row
 * values for the given @columns upon creation.
 *
 * Since: 0.6
 */
void
clutter_model_prependv (ClutterModel *model,
                        guint         n_columns,
                        guint        *columns,
                        GValue       *values)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  gint i;
  gboolean resort = FALSE;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  g_return_if_fail (n_columns <= clutter_model_get_n_columns (model));
  g_return_if_fail (columns != NULL);
  g_return_if_fail (values != NULL);

  priv = model->priv;

  iter = CLUTTER_MODEL_GET_CLASS (model)->insert_row (model, 0);
  g_assert (CLUTTER_IS_MODEL_ITER (iter));

  for (i = 0; i < n_columns; i++)
    {
      if (priv->sort_column == columns[i])
        resort = TRUE;

      clutter_model_iter_set_value (iter, columns[i], &values[i]);
    }

  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter);

  if (resort)
    clutter_model_resort (model);

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
 *   model = clutter_model_default_new (2,
 *                                      G_TYPE_INT,    "Score",
 *                                      G_TYPE_STRING, "Team");
 *   clutter_model_prepend (model, 0, 42, 1, "Team #1", -1);
 * </programlisting></informalexample>
 *
 * Since: 0.6
 */
void
clutter_model_prepend (ClutterModel *model,
                       ...)
{
  ClutterModelIter *iter;
  va_list args;

  g_return_if_fail (CLUTTER_IS_MODEL (model));

  iter = CLUTTER_MODEL_GET_CLASS (model)->insert_row (model, 0);
  g_assert (CLUTTER_IS_MODEL_ITER (iter));

  va_start (args, model);
  clutter_model_iter_set_internal_valist (iter, args);
  va_end (args);

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
 *   model = clutter_model_default_new (2,
 *                                      G_TYPE_INT,    "Score",
 *                                      G_TYPE_STRING, "Team");
 *   clutter_model_insert (model, 3, 0, 42, 1, "Team #1", -1);
 * </programlisting></informalexample>
 *
 * Since: 0.6
 */
void
clutter_model_insert (ClutterModel *model,
                      guint         row,
                      ...)
{
  ClutterModelIter *iter;
  va_list args;

  g_return_if_fail (CLUTTER_IS_MODEL (model));

  iter = CLUTTER_MODEL_GET_CLASS (model)->insert_row (model, row);
  g_assert (CLUTTER_IS_MODEL_ITER (iter));

  /* set_valist() will call clutter_model_resort() if one of the
   * passed columns matches the model sorting column index
   */
  va_start (args, row);
  clutter_model_iter_set_internal_valist (iter, args);
  va_end (args);

  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter);

  g_object_unref (iter);
}

/**
 * clutter_model_insertv:
 * @model: a #ClutterModel
 * @row: row index
 * @n_columns: the number of columns and values to set
 * @columns: (array length=n_columns): a vector containing the columns to set
 * @values: (array length=n_columns): a vector containing the values for the cells
 *
 * Inserts data at @row into the #ClutterModel, setting the row
 * values for the given @columns upon creation.
 *
 * Since: 0.6
 */
void
clutter_model_insertv (ClutterModel *model,
                       guint         row,
                       guint         n_columns,
                       guint        *columns,
                       GValue       *values)
{
  ClutterModelPrivate *priv;
  ClutterModelIter *iter;
  gint i;
  gboolean resort = FALSE;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  g_return_if_fail (n_columns <= clutter_model_get_n_columns (model));
  g_return_if_fail (columns != NULL);
  g_return_if_fail (values != NULL);

  priv = model->priv;

  iter = CLUTTER_MODEL_GET_CLASS (model)->insert_row (model, row);
  g_assert (CLUTTER_IS_MODEL_ITER (iter));

  for (i = 0; i < n_columns; i++)
    {
      if (priv->sort_column == columns[i])
        resort = TRUE;

      clutter_model_iter_set_value (iter, columns[i], &values[i]);
    }

  g_signal_emit (model, model_signals[ROW_ADDED], 0, iter);

  if (resort)
    clutter_model_resort (model);

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
 * @value must be convertable to the type of the column. If the row does
 * not exist then it is created.
 *
 * Since: 0.6
 */
void
clutter_model_insert_value (ClutterModel *model,
                            guint         row,
                            guint         column,
                            const GValue *value)
{
  ClutterModelPrivate *priv;
  ClutterModelClass *klass;
  ClutterModelIter *iter;
  gboolean added = FALSE;
  
  g_return_if_fail (CLUTTER_IS_MODEL (model));

  priv = model->priv;
  klass = CLUTTER_MODEL_GET_CLASS (model);

  iter = klass->get_iter_at_row (model, row);
  if (!iter)
    {
      iter = klass->insert_row (model, row);
      added = TRUE;
    }

  g_assert (CLUTTER_IS_MODEL_ITER (iter));

  clutter_model_iter_set_value (iter, column, value);

  if (added)
    g_signal_emit (model, model_signals[ROW_ADDED], 0, iter);

  if (priv->sort_column == column)
    clutter_model_resort (model);

  g_object_unref (iter);
}

/**
 * clutter_model_remove:
 * @model: a #ClutterModel
 * @row: position of row to remove
 *
 * Removes the row at the given position from the model.
 *
 * Since: 0.6
 */
void
clutter_model_remove (ClutterModel *model,
                      guint         row)
{
  ClutterModelClass *klass;

  g_return_if_fail (CLUTTER_IS_MODEL (model));

  klass = CLUTTER_MODEL_GET_CLASS (model);
  if (klass->remove_row)
    klass->remove_row (model, row);
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
  ClutterModelClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), NULL);

  if (column < 0 || column >= clutter_model_get_n_columns (model))
    {
      g_warning ("%s: Invalid column id value %d\n", G_STRLOC, column);
      return NULL;
    }

  klass = CLUTTER_MODEL_GET_CLASS (model);
  if (klass->get_column_name)
    return klass->get_column_name (model, column);

  return NULL;
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
  ClutterModelClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), G_TYPE_INVALID);

  if (column < 0 || column >= clutter_model_get_n_columns (model))
    {
      g_warning ("%s: Invalid column id value %d\n", G_STRLOC, column);
      return G_TYPE_INVALID;
    }

  klass = CLUTTER_MODEL_GET_CLASS (model);
  if (klass->get_column_type)
    return klass->get_column_type (model, column);

  return G_TYPE_INVALID;
}

/**
 * clutter_model_get_iter_at_row:
 * @model: a #ClutterModel
 * @row: position of the row to retrieve
 *
 * Retrieves a #ClutterModelIter representing the row at the given index.
 *
 * If a filter function has been set using clutter_model_set_filter()
 * then the @model implementation will return the first non filtered
 * row.
 *
 * Return value: (transfer full): A new #ClutterModelIter, or %NULL if @row was
 *   out of bounds. When done using the iterator object, call g_object_unref()
 *   to deallocate its resources
 *
 * Since: 0.6
 */
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
 * Retrieves a #ClutterModelIter representing the first non-filtered
 * row in @model.
 *
 * Return value: (transfer full): A new #ClutterModelIter.
 *   Call g_object_unref() when done using it
 *
 * Since: 0.6
 */
ClutterModelIter *
clutter_model_get_first_iter (ClutterModel *model)
{
  ClutterModelIter *retval;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), NULL);

  retval = clutter_model_get_iter_at_row (model, 0);
  if (retval != NULL)
    {
      g_assert (clutter_model_filter_iter (model, retval) != FALSE);
      g_assert (clutter_model_iter_get_row (retval) == 0);
    }

  return retval;
}

/**
 * clutter_model_get_last_iter:
 * @model: a #ClutterModel
 *
 * Retrieves a #ClutterModelIter representing the last non-filtered
 * row in @model.
 *
 * Return value: (transfer full): A new #ClutterModelIter.
 *   Call g_object_unref() when done using it
 *
 * Since: 0.6
 */
ClutterModelIter *
clutter_model_get_last_iter (ClutterModel *model)
{
  ClutterModelIter *retval;
  guint length;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), NULL);

  length = clutter_model_get_n_rows (model);
  retval = clutter_model_get_iter_at_row (model, length - 1);
  if (retval != NULL)
    g_assert (clutter_model_filter_iter (model, retval) != FALSE);

  return retval;
}

/**
 * clutter_model_get_n_rows:
 * @model: a #ClutterModel
 *
 * Retrieves the number of rows inside @model, eventually taking
 * into account any filtering function set using clutter_model_set_filter().
 *
 * Return value: The length of the @model. If there is a filter set, then
 *   the length of the filtered @model is returned.
 *
 * Since: 0.6
 */
guint
clutter_model_get_n_rows (ClutterModel *model)
{
  ClutterModelClass *klass;
  guint row_count;

  g_return_val_if_fail (CLUTTER_IS_MODEL (model), 0);

  klass = CLUTTER_MODEL_GET_CLASS (model);
  if (klass->get_n_rows)
    row_count = klass->get_n_rows (model);
  else
    {
      ClutterModelIter *iter;

      iter = clutter_model_get_first_iter (model);
      if (iter == NULL)
        return 0;

      row_count = 0;
      while (!clutter_model_iter_is_last (iter))
        {
          if (clutter_model_filter_iter (model, iter))
            row_count += 1;

          iter = clutter_model_iter_next (iter);
        }

      g_object_unref (iter);
    }

  return row_count;
}


/**
 * clutter_model_set_sorting_column:
 * @model: a #ClutterModel
 * @column: the column of the @model to sort, or -1
 *
 * Sets the model to sort by @column. If @column is a negative value
 * the sorting column will be unset.
 *
 * Since: 0.6
 */
void               
clutter_model_set_sorting_column (ClutterModel *model,
                                  gint          column)
{
  ClutterModelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_MODEL (model));
  priv = model->priv;

  /* The extra comparison for >= 0 is because column gets promoted to
     unsigned in the second comparison */
  if (column >= 0 && column >= clutter_model_get_n_columns (model))
    {
      g_warning ("%s: Invalid column id value %d\n", G_STRLOC, column);
      return;
    }

  priv->sort_column = column;

  if (priv->sort_column >= 0)
    clutter_model_resort (model);

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
 * Since: 0.6
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
 * Since: 0.6
 */
void
clutter_model_foreach (ClutterModel            *model,
                       ClutterModelForeachFunc  func,
                       gpointer                 user_data)
{
  ClutterModelIter *iter;
  
  g_return_if_fail (CLUTTER_IS_MODEL (model));

  iter = clutter_model_get_first_iter (model);
  if (!iter)
    return;

  while (!clutter_model_iter_is_last (iter))
    {
      if (clutter_model_filter_iter (model, iter))
        {
          if (!func (model, iter, user_data))
            break;
        }

      iter = clutter_model_iter_next (iter);
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
 * Sorts @model using the given sorting function.
 *
 * Since: 0.6
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
  g_return_if_fail ((func != NULL && column >= 0) ||
                    (func == NULL && column == -1));

  priv = model->priv;

  if (priv->sort_notify)
    priv->sort_notify (priv->sort_data);

  priv->sort_func = func;
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
 * Filters the @model using the given filtering function.
 *
 * Since: 0.6
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

  if (priv->filter_notify)
    priv->filter_notify (priv->filter_data);

  priv->filter_func = func;
  priv->filter_data = user_data;
  priv->filter_notify = notify;

  g_signal_emit (model, model_signals[FILTER_CHANGED], 0);
  g_object_notify (G_OBJECT (model), "filter-set");
}

/**
 * clutter_model_get_filter_set:
 * @model: a #ClutterModel
 *
 * Returns whether the @model has a filter in place, set
 * using clutter_model_set_filter()
 *
 * Return value: %TRUE if a filter is set
 *
 * Since: 1.0
 */
gboolean
clutter_model_get_filter_set (ClutterModel *model)
{
  g_return_val_if_fail (CLUTTER_IS_MODEL (model), FALSE);

  return model->priv->filter_func != NULL;
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
 * A #ClutterModelIter can only be created by a #ClutterModel implementation
 * and it is valid as long as the model does not change.
 *
 * #ClutterModelIter is available since Clutter 0.6
 */

G_DEFINE_ABSTRACT_TYPE (ClutterModelIter, clutter_model_iter, G_TYPE_OBJECT);

#define CLUTTER_MODEL_ITER_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
  CLUTTER_TYPE_MODEL_ITER, ClutterModelIterPrivate))

struct _ClutterModelIterPrivate
{
  ClutterModel  *model;

  gint row;

  guint ignore_sort : 1;
};

enum
{
  ITER_PROP_0,

  ITER_PROP_MODEL,
  ITER_PROP_ROW
};

static ClutterModel *
clutter_model_iter_real_get_model (ClutterModelIter *iter)
{
  return iter->priv->model;
}

static guint
clutter_model_iter_real_get_row (ClutterModelIter *iter)
{
  return iter->priv->row;
}

/* private function */
void
clutter_model_iter_set_row (ClutterModelIter *iter,
                            guint             row)
{
  iter->priv->row = row;
}

static void
clutter_model_iter_get_value_unimplemented (ClutterModelIter *iter,
                                            guint             column,
                                            GValue           *value)
{
  g_warning ("%s: Iterator of type '%s' does not implement the "
             "ClutterModelIter::get_value() virtual function",
             G_STRLOC,
             g_type_name (G_OBJECT_TYPE (iter)));
}

static void
clutter_model_iter_set_value_unimplemented (ClutterModelIter *iter,
                                            guint             column,
                                            const GValue     *value)
{
  g_warning ("%s: Iterator of type '%s' does not implement the "
             "ClutterModelIter::set_value() virtual function",
             G_STRLOC,
             g_type_name (G_OBJECT_TYPE (iter)));
}

static gboolean
clutter_model_iter_is_first_unimplemented (ClutterModelIter *iter)
{
  g_warning ("%s: Iterator of type '%s' does not implement the "
             "ClutterModelIter::is_first() virtual function",
             G_STRLOC,
             g_type_name (G_OBJECT_TYPE (iter)));
  return FALSE;
}

static gboolean
clutter_model_iter_is_last_unimplemented (ClutterModelIter *iter)
{
  g_warning ("%s: Iterator of type '%s' does not implement the "
             "ClutterModelIter::is_last() virtual function",
             G_STRLOC,
             g_type_name (G_OBJECT_TYPE (iter)));
  return FALSE;
}

static ClutterModelIter *
clutter_model_iter_next_unimplemented (ClutterModelIter *iter)
{
  g_warning ("%s: Iterator of type '%s' does not implement the "
             "ClutterModelIter::next() virtual function",
             G_STRLOC,
             g_type_name (G_OBJECT_TYPE (iter)));
  return NULL;
}

static ClutterModelIter *
clutter_model_iter_prev_unimplemented (ClutterModelIter *iter)
{
  g_warning ("%s: Iterator of type '%s' does not implement the "
             "ClutterModelIter::prev() virtual function",
             G_STRLOC,
             g_type_name (G_OBJECT_TYPE (iter)));
  return NULL;
}

static ClutterModelIter *
clutter_model_iter_copy_unimplemented (ClutterModelIter *iter)
{
  g_warning ("%s: Iterator of type '%s' does not implement the "
             "ClutterModelIter::copy() virtual function",
             G_STRLOC,
             g_type_name (G_OBJECT_TYPE (iter)));
  return NULL;
}

static void
clutter_model_iter_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ClutterModelIter *iter = CLUTTER_MODEL_ITER (object);
  ClutterModelIterPrivate *priv = iter->priv;

  switch (prop_id)
    {
    case ITER_PROP_MODEL:
      g_value_set_object (value, priv->model);
      break;
    case ITER_PROP_ROW:
      g_value_set_uint (value, priv->row);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_model_iter_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterModelIter *iter = CLUTTER_MODEL_ITER (object);
  ClutterModelIterPrivate *priv = iter->priv;

  switch (prop_id)
    {
    case ITER_PROP_MODEL:
      priv->model = g_value_get_object (value);
      break;
    case ITER_PROP_ROW:
      priv->row = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_model_iter_class_init (ClutterModelIterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->get_property = clutter_model_iter_get_property;
  gobject_class->set_property = clutter_model_iter_set_property;

  klass->get_model = clutter_model_iter_real_get_model;
  klass->get_row   = clutter_model_iter_real_get_row;
  klass->is_first  = clutter_model_iter_is_first_unimplemented;
  klass->is_last   = clutter_model_iter_is_last_unimplemented;
  klass->next      = clutter_model_iter_next_unimplemented;
  klass->prev      = clutter_model_iter_prev_unimplemented;
  klass->get_value = clutter_model_iter_get_value_unimplemented;
  klass->set_value = clutter_model_iter_set_value_unimplemented;
  klass->copy      = clutter_model_iter_copy_unimplemented;

  /* Properties */

  /**
   * ClutterModelIter:model:
   *
   * A reference to the #ClutterModel that this iter belongs to.
   *
   * Since: 0.6
   */
  pspec = g_param_spec_object ("model",
                               "Model",
                               "The model to which the iterator belongs to",
                               CLUTTER_TYPE_MODEL,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, ITER_PROP_MODEL, pspec);

  /**
   * ClutterModelIter:row:
   *
   * The row number to which this iter points to.
   *
   * Since: 0.6
   */
  pspec = g_param_spec_uint ("row",
                             "Row",
                             "The row to which the iterator points to",
                             0, G_MAXUINT, 0,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, ITER_PROP_ROW, pspec);
  
  g_type_class_add_private (gobject_class, sizeof (ClutterModelIterPrivate));
}

static void
clutter_model_iter_init (ClutterModelIter *self)
{
  ClutterModelIterPrivate *priv;
  
  self->priv = priv = CLUTTER_MODEL_ITER_GET_PRIVATE (self);

  priv->model = NULL;
  priv->row = 0;

  priv->ignore_sort = FALSE;
}

/*
 *  Public functions
 */

static void
clutter_model_iter_set_internal_valist (ClutterModelIter *iter,
                                        va_list           args)
{
  ClutterModel *model;
  ClutterModelIterPrivate *priv;
  guint column = 0;
  gboolean sort = FALSE;
  
  priv = iter->priv;
  model = priv->model;
  g_assert (CLUTTER_IS_MODEL (model));

  column = va_arg (args, gint);
  
  /* Don't want to sort while setting lots of fields, leave that till the end
   */
  priv->ignore_sort = TRUE;
  while (column != -1)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column < 0 || column >= clutter_model_get_n_columns (model))
        { 
          g_warning ("%s: Invalid column number %d added to iter "
                     "(remember to end you list of columns with a -1)",
                     G_STRLOC, column);
          break;
        }
      g_value_init (&value, clutter_model_get_column_type (model, column));

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
      
      if (column == clutter_model_get_sorting_column (model))
        sort = TRUE;
      
      column = va_arg (args, gint);
    }

  priv->ignore_sort = FALSE;
  if (sort)
    clutter_model_resort (model);
}

/**
 * clutter_model_iter_set_valist:
 * @iter: a #ClutterModelIter
 * @args: va_list of column/value pairs, terminiated by -1
 *
 * See clutter_model_iter_set(); this version takes a va_list for language
 * bindings.
 *
 * Since: 0.6
 */
void 
clutter_model_iter_set_valist (ClutterModelIter *iter,
                               va_list           args)
{
  ClutterModelIterPrivate *priv;
  ClutterModel *model;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));

  clutter_model_iter_set_internal_valist (iter, args);

  priv = iter->priv;
  model = priv->model;
  g_assert (CLUTTER_IS_MODEL (model));

  g_signal_emit (model, model_signals[ROW_CHANGED], 0, iter);
}

/**
 * clutter_model_iter_get:
 * @iter: a #ClutterModelIter
 * @Varargs: a list of column/return location pairs, terminated by -1
 *
 * Gets the value of one or more cells in the row referenced by @iter. The
 * variable argument list should contain integer column numbers, each column
 * column number followed by a place to store the value being retrieved. The
 * list is terminated by a -1.
 *
 * For example, to get a value from column 0 with type %G_TYPE_STRING use:
 * <informalexample><programlisting>
 *   clutter_model_iter_get (iter, 0, &place_string_here, -1);
 * </programlisting></informalexample>
 * 
 * where place_string_here is a gchar* to be filled with the string. If
 * appropriate, the returned values have to be freed or unreferenced.
 *
 * Since: 0.6
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
 * g_value_unset() needs to be called to free any allocated memory.
 *
 * Since: 0.6
 */
void
clutter_model_iter_get_value (ClutterModelIter *iter,
                              guint             column,
                              GValue           *value)
{
  ClutterModelIterClass *klass;
  ClutterModel *model;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));
  
  model = iter->priv->model;

  g_value_init (value, clutter_model_get_column_type (model, column));

  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  if (klass && klass->get_value)
    klass->get_value (iter, column, value);
}

/**
 * clutter_model_iter_get_valist:
 * @iter: a #ClutterModelIter
 * @args: a list of column/return location pairs, terminated by -1
 *
 * See clutter_model_iter_get(). This version takes a va_list for language
 * bindings.
 *
 * Since: 0.6
 */
void 
clutter_model_iter_get_valist (ClutterModelIter *iter,
                               va_list           args)
{
  ClutterModelIterPrivate *priv;
  ClutterModel *model;
  guint column = 0;
  
  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));

  priv = iter->priv;
  model = priv->model;
  g_assert (CLUTTER_IS_MODEL (model));

  column = va_arg (args, gint);

  while (column != -1)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column < 0 || column >= clutter_model_get_n_columns (model))
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
 * @Varargs: a list of column/return location pairs, terminated by -1
 *
 * Sets the value of one or more cells in the row referenced by @iter. The
 * variable argument list should contain integer column numbers, each column
 * column number followed by the value to be set. The  list is terminated by a
 * -1.
 *
 * For example, to set column 0 with type %G_TYPE_STRING, use:
 * <informalexample><programlisting>
 *   clutter_model_iter_set (iter, 0, "foo", -1);
 * </programlisting></informalexample>
 *
 * Since: 0.6
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
 * Sets the data in the cell specified by @iter and @column. The type of
 * @value must be convertable to the type of the column.
 *
 * Since: 0.6
 */
void
clutter_model_iter_set_value (ClutterModelIter *iter,
                              guint             column,
                              const GValue     *value)
{
  ClutterModelIterClass *klass;

  g_return_if_fail (CLUTTER_IS_MODEL_ITER (iter));
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  if (klass && klass->set_value)
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
 * Since: 0.6
 */
gboolean
clutter_model_iter_is_first (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), FALSE);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  if (klass && klass->is_first)
    return klass->is_first (iter);

  return FALSE;
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
 * Since: 0.6
 */
gboolean
clutter_model_iter_is_last (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), FALSE);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  if (klass && klass->is_last)
    return klass->is_last (iter);

  return FALSE;
}

/**
 * clutter_model_iter_next:
 * @iter: a #ClutterModelIter
 * 
 * Updates the @iter to point at the next position in the model.
 * The model implementation should take into account the presence of
 * a filter function.
 *
 * Return value: (transfer none): The passed iterator, updated to point at the next
 *   row in the model.
 *
 * Since: 0.6
 */
ClutterModelIter *
clutter_model_iter_next (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), NULL);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  if (klass && klass->next)
    return klass->next (iter);

  return NULL;
}

/**
 * clutter_model_iter_prev:
 * @iter: a #ClutterModelIter
 * 
 * Sets the @iter to point at the previous position in the model.
 * The model implementation should take into account the presence of
 * a filter function.
 *
 * Return value: (transfer none): The passed iterator, updated to point at the previous
 *   row in the model.
 *
 * Since: 0.6
 */
ClutterModelIter *
clutter_model_iter_prev (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), NULL);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  if (klass && klass->prev)
    return klass->prev (iter);

  return NULL;
}

/**
 * clutter_model_iter_get_model:
 * @iter: a #ClutterModelIter
 * 
 * Retrieves a pointer to the #ClutterModel that this iter is part of.
 *
 * Return value: (transfer none): a pointer to a #ClutterModel.
 *
 * Since: 0.6
 */
ClutterModel *
clutter_model_iter_get_model (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), NULL);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  if (klass && klass->get_model)
    return klass->get_model (iter);

  return NULL;
}

/**
 * clutter_model_iter_get_row:
 * @iter: a #ClutterModelIter
 * 
 * Retrieves the position of the row that the @iter points to.
 *
 * Return value: the position of the @iter in the model
 *
 * Since: 0.6
 */
guint
clutter_model_iter_get_row (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), 0);
  
  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  if (klass && klass->get_row)
    return klass->get_row (iter);

  return 0;
}

/**
 * clutter_model_iter_copy:
 * @iter: a #ClutterModelIter
 *
 * Copies the passed iterator.
 *
 * Return value: a copy of the iterator, or %NULL
 *
 * Since: 0.8
 */
ClutterModelIter *
clutter_model_iter_copy (ClutterModelIter *iter)
{
  ClutterModelIterClass *klass;

  g_return_val_if_fail (CLUTTER_IS_MODEL_ITER (iter), NULL);

  klass = CLUTTER_MODEL_ITER_GET_CLASS (iter);
  if (klass->copy)
    return klass->copy (iter);

  return NULL;
}
