#include <gtk/gtk.h>

enum {
  DESTROY_PARENT,
  DETACH,
  ATTACH_1,
  ATTACH_2
};

GtkWidget *window1, *window2;

static void
dialog_response (GtkDialog *dialog, int response, gpointer user_data)
{
  if (response == DESTROY_PARENT)
    {
      GtkWidget *window = GTK_WIDGET (gtk_window_get_transient_for (GTK_WINDOW (dialog)));

      if (window == window1)
	{
	  gtk_dialog_set_response_sensitive (dialog, ATTACH_1, FALSE);
	  window1 = NULL;
	}
      else
	{
	  gtk_dialog_set_response_sensitive (dialog, ATTACH_2, FALSE);
	  window2 = NULL;
	}

      gtk_dialog_set_response_sensitive (dialog, DESTROY_PARENT, FALSE);
      gtk_dialog_set_response_sensitive (dialog, DETACH, FALSE);
      gtk_widget_destroy (window);
    }
  else if (response == DETACH)
    {
      gtk_window_set_transient_for (GTK_WINDOW (dialog), NULL);
      gtk_dialog_set_response_sensitive (dialog, DESTROY_PARENT, FALSE);
      gtk_dialog_set_response_sensitive (dialog, DETACH, FALSE);
      gtk_dialog_set_response_sensitive (dialog, ATTACH_1, window1 != NULL);
      gtk_dialog_set_response_sensitive (dialog, ATTACH_2, window2 != NULL);
    }
  else if (response == ATTACH_1)
    {
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window1));
      gtk_dialog_set_response_sensitive (dialog, DESTROY_PARENT, TRUE);
      gtk_dialog_set_response_sensitive (dialog, DETACH, TRUE);
      gtk_dialog_set_response_sensitive (dialog, ATTACH_1, FALSE);
      gtk_dialog_set_response_sensitive (dialog, ATTACH_2, window2 != NULL);
    }
  else if (response == ATTACH_2)
    {
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window2));
      gtk_dialog_set_response_sensitive (dialog, DESTROY_PARENT, TRUE);
      gtk_dialog_set_response_sensitive (dialog, DETACH, TRUE);
      gtk_dialog_set_response_sensitive (dialog, ATTACH_1, window1 != NULL);
      gtk_dialog_set_response_sensitive (dialog, ATTACH_2, FALSE);
    }
  else if (response == GTK_RESPONSE_CLOSE)
    gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GtkWidget *dialog;

  gtk_init (&argc, &argv);

  window1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window1), "Parent 1");
  gtk_widget_show (window1);

  window2 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window2), "Parent 2");
  gtk_widget_show (window2);

  dialog = gtk_dialog_new_with_buttons ("Child",
					NULL,
					GTK_DIALOG_MODAL,
					"Destroy Parent",
					DESTROY_PARENT,
					"Detach",
					DETACH,
					"Attach to 1",
					ATTACH_1,
					"Attach to 2",
					ATTACH_2,
					GTK_STOCK_QUIT,
					GTK_RESPONSE_CLOSE,
					NULL);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), DESTROY_PARENT, FALSE);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), DETACH, FALSE);

  g_signal_connect (dialog, "response", G_CALLBACK (dialog_response), NULL);
  gtk_widget_show (dialog);

  gtk_main ();

  return 0;
}
