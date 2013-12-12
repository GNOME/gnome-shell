#include <stdlib.h>
#include <string.h>
#include <clutter/clutter.h>

typedef struct _ModelData
{
  ClutterModel *model;

  guint n_row;
} ModelData;

typedef struct _ChangedData
{
  ClutterModel *model;

  ClutterModelIter *iter;

  guint row;
  guint n_emissions;

  gint value_check;
} ChangedData;

enum
{
  COLUMN_FOO,   /* G_TYPE_STRING */
  COLUMN_BAR,   /* G_TYPE_INT */

  N_COLUMNS
};

static const struct {
  const gchar *expected_foo;
  gint expected_bar;
} base_model[] = {
  { "String 1", 1 },
  { "String 2", 2 },
  { "String 3", 3 },
  { "String 4", 4 },
  { "String 5", 5 },
  { "String 6", 6 },
  { "String 7", 7 },
  { "String 8", 8 },
  { "String 9", 9 },
};

static const struct {
  const gchar *expected_foo;
  gint expected_bar;
} forward_base[] = {
  { "String 1", 1 },
  { "String 2", 2 },
  { "String 3", 3 },
  { "String 4", 4 },
  { "String 5", 5 },
  { "String 6", 6 },
  { "String 7", 7 },
  { "String 8", 8 },
  { "String 9", 9 },
};

static const struct {
  const gchar *expected_foo;
  gint expected_bar;
} backward_base[] = {
  { "String 9", 9 },
  { "String 8", 8 },
  { "String 7", 7 },
  { "String 6", 6 },
  { "String 5", 5 },
  { "String 4", 4 },
  { "String 3", 3 },
  { "String 2", 2 },
  { "String 1", 1 },
};

static const struct {
  const gchar *expected_foo;
  gint expected_bar;
} filter_odd[] = {
  { "String 1", 1 },
  { "String 3", 3 },
  { "String 5", 5 },
  { "String 7", 7 },
  { "String 9", 9 },
};

static const struct {
  const gchar *expected_foo;
  gint expected_bar;
} filter_even[] = {
  { "String 8", 8 },
  { "String 6", 6 },
  { "String 4", 4 },
  { "String 2", 2 },
};

static inline void
compare_iter (ClutterModelIter *iter,
              const gint        expected_row,
              const gchar      *expected_foo,
              const gint        expected_bar)
{
  gchar *foo = NULL;
  gint bar = 0;
  gint row = 0;

  row = clutter_model_iter_get_row (iter);
  clutter_model_iter_get (iter,
                          COLUMN_FOO, &foo,
                          COLUMN_BAR, &bar,
                          -1);

  if (g_test_verbose ())
    g_print ("Row %d => %d: Got [ '%s', '%d' ], expected [ '%s', '%d' ]\n",
             row, expected_row,
             foo, bar,
             expected_foo, expected_bar);

  g_assert_cmpint (row, ==, expected_row);
  g_assert_cmpstr (foo, ==, expected_foo);
  g_assert_cmpint (bar, ==, expected_bar);

  g_free (foo);
}

static void
on_row_added (ClutterModel     *model,
              ClutterModelIter *iter,
              gpointer          data)
{
  ModelData *model_data = data;

  compare_iter (iter,
                model_data->n_row,
                base_model[model_data->n_row].expected_foo,
                base_model[model_data->n_row].expected_bar);

  model_data->n_row += 1;
}

static gboolean
filter_even_rows (ClutterModel     *model,
                  ClutterModelIter *iter,
                  gpointer          dummy G_GNUC_UNUSED)
{
  gint bar_value;

  clutter_model_iter_get (iter, COLUMN_BAR, &bar_value, -1);

  if (bar_value % 2 == 0)
    return TRUE;

  return FALSE;
}

static gboolean
filter_odd_rows (ClutterModel     *model,
                 ClutterModelIter *iter,
                 gpointer          dummy G_GNUC_UNUSED)
{
  gint bar_value;

  clutter_model_iter_get (iter, COLUMN_BAR, &bar_value, -1);

  if (bar_value % 2 != 0)
    return TRUE;

  return FALSE;
}

static void
list_model_filter (void)
{
  ModelData test_data = { NULL, 0 };
  ClutterModelIter *iter;
  gint i;

  test_data.model = clutter_list_model_new (N_COLUMNS,
                                            G_TYPE_STRING, "Foo",
                                            G_TYPE_INT,    "Bar");
  test_data.n_row = 0;

  for (i = 1; i < 10; i++)
    {
      gchar *foo = g_strdup_printf ("String %d", i);

      clutter_model_append (test_data.model,
                            COLUMN_FOO, foo,
                            COLUMN_BAR, i,
                            -1);

      g_free (foo);
    }

  if (g_test_verbose ())
    g_print ("Forward iteration (filter odd)...\n");

  clutter_model_set_filter (test_data.model, filter_odd_rows, NULL, NULL);

  iter = clutter_model_get_first_iter (test_data.model);
  g_assert (iter != NULL);

  i = 0;
  while (!clutter_model_iter_is_last (iter))
    {
      compare_iter (iter, i,
                    filter_odd[i].expected_foo,
                    filter_odd[i].expected_bar);

      iter = clutter_model_iter_next (iter);
      i += 1;
    }

  g_object_unref (iter);

  if (g_test_verbose ())
    g_print ("Backward iteration (filter even)...\n");

  clutter_model_set_filter (test_data.model, filter_even_rows, NULL, NULL);

  iter = clutter_model_get_last_iter (test_data.model);
  g_assert (iter != NULL);

  i = 0;
  do
    {
      compare_iter (iter, G_N_ELEMENTS (filter_even) - i - 1,
                    filter_even[i].expected_foo,
                    filter_even[i].expected_bar);

      iter = clutter_model_iter_prev (iter);
      i += 1;
    }
  while (!clutter_model_iter_is_first (iter));

  g_object_unref (iter);

  if (g_test_verbose ())
    g_print ("get_iter_at_row...\n");

  clutter_model_set_filter (test_data.model, filter_odd_rows, NULL, NULL);

  for (i = 0; i < 5; i++)
    {
      iter = clutter_model_get_iter_at_row (test_data.model, i);
      compare_iter (iter, i ,
                    filter_odd[i].expected_foo,
                    filter_odd[i].expected_bar);
      g_object_unref (iter);
    }

  iter = clutter_model_get_iter_at_row (test_data.model, 5);
  g_assert (iter == NULL);

  g_object_unref (test_data.model);
}

static void
list_model_iterate (void)
{
  ModelData test_data = { NULL, 0 };
  ClutterModelIter *iter;
  gint i;

  test_data.model = clutter_list_model_new (N_COLUMNS,
                                            G_TYPE_STRING, "Foo",
                                            G_TYPE_INT,    "Bar");
  test_data.n_row = 0;

  g_signal_connect (test_data.model, "row-added",
                    G_CALLBACK (on_row_added),
                    &test_data);

  for (i = 1; i < 10; i++)
    {
      gchar *foo = g_strdup_printf ("String %d", i);

      clutter_model_append (test_data.model,
                            COLUMN_FOO, foo,
                            COLUMN_BAR, i,
                            -1);

      g_free (foo);
    }

  if (g_test_verbose ())
    g_print ("Forward iteration...\n");

  iter = clutter_model_get_first_iter (test_data.model);
  g_assert (iter != NULL);

  i = 0;
  while (!clutter_model_iter_is_last (iter))
    {
      compare_iter (iter, i,
                    forward_base[i].expected_foo,
                    forward_base[i].expected_bar);

      iter = clutter_model_iter_next (iter);
      i += 1;
    }

  g_object_unref (iter);

  if (g_test_verbose ())
    g_print ("Backward iteration...\n");

  iter = clutter_model_get_last_iter (test_data.model);
  g_assert (iter != NULL);

  i = 0;
  do
    {
      compare_iter (iter, G_N_ELEMENTS (backward_base) - i - 1,
                    backward_base[i].expected_foo,
                    backward_base[i].expected_bar);

      iter = clutter_model_iter_prev (iter);
      i += 1;
    }
  while (!clutter_model_iter_is_first (iter));

  compare_iter (iter, G_N_ELEMENTS (backward_base) - i - 1,
                backward_base[i].expected_foo,
                backward_base[i].expected_bar);

  g_object_unref (iter);

  g_object_unref (test_data.model);
}

static void
list_model_populate (void)
{
  ModelData test_data = { NULL, 0 };
  gint i;

  test_data.model = clutter_list_model_new (N_COLUMNS,
                                            G_TYPE_STRING, "Foo",
                                            G_TYPE_INT,    "Bar");
  test_data.n_row = 0;

  g_signal_connect (test_data.model, "row-added",
                    G_CALLBACK (on_row_added),
                    &test_data);

  for (i = 1; i < 10; i++)
    {
      gchar *foo = g_strdup_printf ("String %d", i);

      clutter_model_append (test_data.model,
                            COLUMN_FOO, foo,
                            COLUMN_BAR, i,
                            -1);

      g_free (foo);
    }

  g_object_unref (test_data.model);
}

static void
list_model_from_script (void)
{
  ClutterScript *script = clutter_script_new ();
  GObject *model;
  GError *error = NULL;
  gchar *test_file;
  const gchar *name;
  GType type;
  ClutterModelIter *iter;
  GValue value = { 0, };

  test_file = g_test_build_filename (G_TEST_DIST, "scripts", "test-script-model.json", NULL);
  clutter_script_load_from_file (script, test_file, &error);
  if (g_test_verbose () && error)
    g_print ("Error: %s", error->message);

  g_assert_no_error (error);

  model = clutter_script_get_object (script, "test-model");

  g_assert (CLUTTER_IS_MODEL (model));
  g_assert (clutter_model_get_n_columns (CLUTTER_MODEL (model)) == 3);

  name = clutter_model_get_column_name (CLUTTER_MODEL (model), 0);
  type = clutter_model_get_column_type (CLUTTER_MODEL (model), 0);

  if (g_test_verbose ())
    g_print ("column[0]: %s, type: %s\n", name, g_type_name (type));

  g_assert (strcmp (name, "text-column") == 0);
  g_assert (type == G_TYPE_STRING);

  name = clutter_model_get_column_name (CLUTTER_MODEL (model), 2);
  type = clutter_model_get_column_type (CLUTTER_MODEL (model), 2);

  if (g_test_verbose ())
    g_print ("column[2]: %s, type: %s\n", name, g_type_name (type));

  g_assert (strcmp (name, "actor-column") == 0);
  g_assert (g_type_is_a (type, CLUTTER_TYPE_ACTOR));

  g_assert (clutter_model_get_n_rows (CLUTTER_MODEL (model)) == 3);

  iter = clutter_model_get_iter_at_row (CLUTTER_MODEL (model), 0);
  clutter_model_iter_get_value (iter, 0, &value);
  g_assert (G_VALUE_HOLDS_STRING (&value));
  g_assert (strcmp (g_value_get_string (&value), "text-row-1") == 0);
  g_value_unset (&value);

  clutter_model_iter_get_value (iter, 1, &value);
  g_assert (G_VALUE_HOLDS_INT (&value));
  g_assert (g_value_get_int (&value) == 1);
  g_value_unset (&value);

  clutter_model_iter_get_value (iter, 2, &value);
  g_assert (G_VALUE_HOLDS_OBJECT (&value));
  g_assert (g_value_get_object (&value) == NULL);
  g_value_unset (&value);

  iter = clutter_model_iter_next (iter);
  clutter_model_iter_get_value (iter, 2, &value);
  g_assert (G_VALUE_HOLDS_OBJECT (&value));
  g_assert (CLUTTER_IS_ACTOR (g_value_get_object (&value)));
  g_value_unset (&value);

  iter = clutter_model_iter_next (iter);
  clutter_model_iter_get_value (iter, 2, &value);
  g_assert (G_VALUE_HOLDS_OBJECT (&value));
  g_assert (CLUTTER_IS_ACTOR (g_value_get_object (&value)));
  g_assert (strcmp (clutter_actor_get_name (g_value_get_object (&value)),
                    "actor-row-3") == 0);
  g_value_unset (&value);
  g_object_unref (iter);
}

static void
on_row_changed (ClutterModel *model,
                ClutterModelIter *iter,
                ChangedData *data)
{
  gint value = -1;

  clutter_model_iter_get (iter, COLUMN_BAR, &value, -1);

  if (g_test_verbose ())
    g_print ("row-changed value-check: %d, expected: %d\n",
             value, data->value_check);

  g_assert_cmpint (value, ==, data->value_check);

  data->n_emissions += 1;
}

static void
list_model_row_changed (void)
{
  ChangedData test_data = { NULL, NULL, 0, 0 };
  GValue value = { 0, };
  gint i;

  test_data.model = clutter_list_model_new (N_COLUMNS,
                                            G_TYPE_STRING, "Foo",
                                            G_TYPE_INT,    "Bar");
  for (i = 1; i < 10; i++)
    {
      gchar *foo = g_strdup_printf ("String %d", i);

      clutter_model_append (test_data.model,
                            COLUMN_FOO, foo,
                            COLUMN_BAR, i,
                            -1);

      g_free (foo);
    }

  g_signal_connect (test_data.model, "row-changed",
                    G_CALLBACK (on_row_changed),
                    &test_data);

  test_data.row = g_random_int_range (0, 9);
  test_data.iter = clutter_model_get_iter_at_row (test_data.model,
                                                  test_data.row);
  g_assert (CLUTTER_IS_MODEL_ITER (test_data.iter));

  test_data.value_check = 47;

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, test_data.value_check);

  clutter_model_iter_set_value (test_data.iter, COLUMN_BAR, &value);

  g_value_unset (&value);

  if (g_test_verbose ())
    g_print ("iter.set_value() emissions: %d, expected: 1\n",
             test_data.n_emissions);

  g_assert_cmpint (test_data.n_emissions, ==, 1);

  test_data.n_emissions = 0;
  test_data.value_check = 42;

  clutter_model_iter_set (test_data.iter,
                          COLUMN_FOO, "changed",
                          COLUMN_BAR, test_data.value_check,
                          -1);

  if (g_test_verbose ())
    g_print ("iter.set() emissions: %d, expected: 1\n",
             test_data.n_emissions);

  g_assert_cmpint (test_data.n_emissions, ==, 1);

  g_object_unref (test_data.iter);
  g_object_unref (test_data.model);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/list-model/populate", list_model_populate)
  CLUTTER_TEST_UNIT ("/list-model/iterate", list_model_iterate)
  CLUTTER_TEST_UNIT ("/list-model/filter", list_model_filter)
  CLUTTER_TEST_UNIT ("/list-model/row-changed", list_model_row_changed)
  CLUTTER_TEST_UNIT ("/list-model/from-script", list_model_from_script)
)
