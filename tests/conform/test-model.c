#include <stdlib.h>
#include <string.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

typedef struct _ModelData
{
  ClutterModel *model;

  guint n_row;
} ModelData;

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

void
test_list_model_filter (TestConformSimpleFixture *fixture,
                        gconstpointer             data)
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

  g_object_unref (test_data.model);
}

void
test_list_model_iterate (TestConformSimpleFixture *fixture,
                         gconstpointer             data)
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

void
test_list_model_populate (TestConformSimpleFixture *fixture,
                          gconstpointer             data)
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

void
test_list_model_from_script (TestConformSimpleFixture *fixture,
                             gconstpointer dummy)
{
  ClutterScript *script = clutter_script_new ();
  GObject *model;
  GError *error = NULL;
  gchar *test_file;
  const gchar *name;
  GType type;

  test_file = clutter_test_get_data_file ("test-script-model.json");
  clutter_script_load_from_file (script, test_file, &error);
  if (g_test_verbose () && error)
    g_print ("Error: %s", error->message);
  g_assert (error == NULL);

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
  g_assert (type == CLUTTER_TYPE_RECTANGLE);
}
