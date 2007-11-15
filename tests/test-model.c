#include <clutter/clutter.h>
#include <string.h>

static void
print_iter (ClutterModelIter *iter, const gchar *text)
{
  gint i;
  gchar *string;

  clutter_model_iter_get (iter, 0, &i, 1, &string, -1);
  g_print ("%s: %d, %s\n", text, i, string);
  g_free (string);
}

static gboolean
foreach_func (ClutterModel *model, ClutterModelIter *iter, gpointer null)
{
  gint i;
  gchar *string;

  clutter_model_iter_get (iter, 0, &i, 1, &string, -1);
  g_print ("Foreach: %d: %s\n", i, string);
  
  g_free (string);
  return TRUE;
}

static gboolean
filter_func (ClutterModel *model, ClutterModelIter *iter, gpointer null)
{
  gint i = 0;

  clutter_model_iter_get (iter, 0, &i, -1);

  return !(i % 2);
}

static gint
sort_func (ClutterModel *model,
           const GValue *a,
           const GValue *b,
           gpointer null)
{
  return -1 * strcmp (g_value_get_string (a), g_value_get_string (b)); 
}

static void
on_row_changed (ClutterModel *model, ClutterModelIter *iter)
{
  print_iter (iter, "Changed");
}

static void
filter_model (ClutterModel *model)
{
  ClutterModelIter *iter;

  clutter_model_set_filter (model, filter_func, NULL, NULL);

  iter = clutter_model_get_first_iter (model);

  while (!clutter_model_iter_is_last (iter))
  {
    print_iter (iter, "Filtered Forward Iteration");
    iter = clutter_model_iter_next (iter);
  }
  g_object_unref (iter);

  clutter_model_set_sort (model, 1, sort_func, NULL, NULL);

  g_signal_connect (model, "row-changed",
                    G_CALLBACK (on_row_changed), NULL);
  
  iter = clutter_model_get_iter_at_row (model, 0);
  clutter_model_iter_set (iter, 1, "Changed string of 0th row, automatically"
                                   " gets sorted", -1);
  g_object_unref (iter);

  clutter_model_foreach (model, foreach_func, NULL);

  clutter_model_set_filter (model, NULL, NULL, NULL);
  while (clutter_model_get_n_rows (model))
  {
    clutter_model_remove (model, 0);
  }
  
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
                            0, i,
                            1, string,
                            -1);
      g_free (string);
    }

  clutter_model_foreach (model, foreach_func, NULL);
  iterate (model);

  return FALSE;
}

static void
on_row_added (ClutterModel *model, ClutterModelIter *iter, gpointer null)
{
  gint i;
  gchar *string;

  clutter_model_iter_get (iter, 0, &i, 1, &string, -1);

  g_print ("Added: %d, %s\n", i, string);

  g_free (string);
}

static void
on_row_removed (ClutterModel *model, ClutterModelIter *iter, gpointer null)
{
  print_iter (iter, "Removed");
}

static void
on_sort_changed (ClutterModel *model)
{
  g_print ("\nSort Changed\n\n");
  clutter_model_foreach (model, foreach_func, NULL);
}

static void
on_filter_changed (ClutterModel *model)
{
  g_print ("\nFilter Changed\n\n");
}
 
int
main (int argc, char *argv[])
{
  ClutterModel    *model;

  clutter_init (&argc, &argv);

  model = clutter_model_new (2, G_TYPE_INT, G_TYPE_STRING);

  g_timeout_add (1000, (GSourceFunc)populate_model, model);

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
