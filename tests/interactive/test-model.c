#include <gmodule.h>
#include <clutter/clutter.h>
#include <string.h>

enum
{
  COLUMN_FOO,
  COLUMN_BAR,

  N_COLUMNS
};

static void
print_iter (ClutterModelIter *iter,
            const gchar      *text)
{
  ClutterModel *model;
  gint i;
  gchar *string;

  model = clutter_model_iter_get_model (iter);

  clutter_model_iter_get (iter, COLUMN_FOO, &i, COLUMN_BAR, &string, -1);

  g_print ("[row:%02d]: %s: (%s: %d), (%s: %s)\n",
           clutter_model_iter_get_row (iter),
           text,
           clutter_model_get_column_name (model, COLUMN_FOO), i,
           clutter_model_get_column_name (model, COLUMN_BAR), string);

  g_free (string);
}

static gboolean
foreach_func (ClutterModel     *model,
              ClutterModelIter *iter,
              gpointer          dummy)
{
  gint i;
  gchar *string;

  clutter_model_iter_get (iter, COLUMN_FOO, &i, COLUMN_BAR, &string, -1);

  g_print ("[row:%02d]: Foreach: %d, %s\n",
           clutter_model_iter_get_row (iter),
           i, string);
  
  g_free (string);

  return TRUE;
}

static gboolean
filter_func (ClutterModel     *model,
             ClutterModelIter *iter,
             gpointer          dummy)
{
  gint i = 0;

  clutter_model_iter_get (iter, COLUMN_FOO, &i, -1);

  return !(i % 2);
}

static gint
sort_func (ClutterModel *model,
           const GValue *a,
           const GValue *b,
           gpointer      dummy)
{
  return -1 * strcmp (g_value_get_string (a), g_value_get_string (b)); 
}

static void
on_row_changed (ClutterModel     *model,
                ClutterModelIter *iter)
{
  print_iter (iter, "Changed");
}

static void
filter_model (ClutterModel *model)
{
  ClutterModelIter *iter;

  g_print ("\n* Filter function: even rows\n");
  clutter_model_set_filter (model, filter_func, NULL, NULL);

  iter = clutter_model_get_first_iter (model);
  while (!clutter_model_iter_is_last (iter))
    {
      print_iter (iter, "Filtered Forward Iteration");

      iter = clutter_model_iter_next (iter);
    }
  g_object_unref (iter);

  g_print ("\n* Sorting function: reverse alpha\n");
  clutter_model_set_sort (model, COLUMN_BAR, sort_func, NULL, NULL);

  g_signal_connect (model, "row-changed", G_CALLBACK (on_row_changed), NULL);
  
  iter = clutter_model_get_iter_at_row (model, 0);
  clutter_model_iter_set (iter, COLUMN_BAR, "Changed string of 0th row, "
                                            "automatically gets sorted",
                                -1);
  g_object_unref (iter);

  clutter_model_foreach (model, foreach_func, NULL);

  g_print ("\n* Unset filter\n");
  clutter_model_set_filter (model, NULL, NULL, NULL);

  while (clutter_model_get_n_rows (model))
    clutter_model_remove (model, 0);
  
  clutter_main_quit ();
}

static void
iterate (ClutterModel *model)
{
  ClutterModelIter *iter;
  
  iter = clutter_model_get_first_iter (model);

  while (!clutter_model_iter_is_last (iter))
    {
      print_iter (iter, "Forward Iteration");
      iter = clutter_model_iter_next (iter);
    }
  g_object_unref (iter);

  iter = clutter_model_get_last_iter (model);  
  do
    {
      print_iter (iter, "Reverse Iteration");
      iter = clutter_model_iter_prev (iter);
    }
  while (!clutter_model_iter_is_first (iter));
  
  print_iter (iter, "Reverse Iteration");
  g_object_unref (iter);

  filter_model (model);
}


static gboolean
populate_model (ClutterModel *model)
{
  gint i;

  for (i = 0; i < 10; i++)
    {
      gchar *string = g_strdup_printf ("String %d", i);

      clutter_model_append (model,
                            COLUMN_FOO, i,
                            COLUMN_BAR, string,
                            -1);
      g_free (string);
    }

  clutter_model_foreach (model, foreach_func, NULL);
  iterate (model);

  return FALSE;
}

static void
on_row_added (ClutterModel     *model,
              ClutterModelIter *iter,
              gpointer          dummy)
{
  gint i;
  gchar *string;

  clutter_model_iter_get (iter, COLUMN_FOO, &i, COLUMN_BAR, &string, -1);

  g_print ("[row:%02d]: Added: %d, %s\n",
           clutter_model_iter_get_row (iter),
           i, string);

  g_free (string);
}

static void
on_row_removed (ClutterModel     *model,
                ClutterModelIter *iter,
                gpointer          dummy)
{
  print_iter (iter, "Removed");
}

static void
on_sort_changed (ClutterModel *model)
{
  g_print ("*** Sort Changed   ***\n\n");
  clutter_model_foreach (model, foreach_func, NULL);
}

static void
on_filter_changed (ClutterModel *model)
{
  g_print ("*** Filter Changed ***\n\n");
}
 
G_MODULE_EXPORT int
test_model_main (int argc, char *argv[])
{
  ClutterModel    *model;

  clutter_init (&argc, &argv);

  model = clutter_list_model_new (N_COLUMNS,
                                  G_TYPE_INT,    "Foo",
                                  G_TYPE_STRING, "Bar");

  g_timeout_add (1000, (GSourceFunc) populate_model, model);

  g_signal_connect (model, "row-added",
                    G_CALLBACK (on_row_added), NULL);
  g_signal_connect (model, "row-removed",
                    G_CALLBACK (on_row_removed), NULL);
  g_signal_connect (model, "sort-changed",
                    G_CALLBACK (on_sort_changed), NULL);
  g_signal_connect (model, "filter-changed",
                    G_CALLBACK (on_filter_changed), NULL);

  clutter_main();
  
  g_object_unref (model);

  return 0;
}
