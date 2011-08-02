#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

GtkWidget *main_window;
GtkWidget *noinput_window, *passive_window, *local_window;
GtkWidget *global_window, *lame_window, *grabby_window, *dying_window;

static void
clear_on_destroy (GtkWidget *widget, gpointer user_data)
{
  GtkWidget **widget_pointer = user_data;

  *widget_pointer = NULL;
}

static void
disable_take_focus (GtkWidget *window)
{
  GdkDisplay *display;
  GdkWindow *gdkwindow;
  Atom *protocols, wm_take_focus;
  int n_protocols, i;

  gtk_widget_realize (window);
  gdkwindow = gtk_widget_get_window (window);
  display = gdk_window_get_display (gdkwindow);

  wm_take_focus = gdk_x11_get_xatom_by_name_for_display (display, "WM_TAKE_FOCUS");
  XGetWMProtocols (GDK_DISPLAY_XDISPLAY (display),
                   GDK_WINDOW_XID (gdkwindow),
                   &protocols, &n_protocols);
  for (i = 0; i < n_protocols; i++)
    {
      if (protocols[i] == wm_take_focus)
        {
          protocols[i] = protocols[n_protocols - 1];
          n_protocols--;
          break;
        }
    }
  XSetWMProtocols (GDK_DISPLAY_XDISPLAY (display),
                   GDK_WINDOW_XID (gdkwindow),
                   protocols, n_protocols);
  XFree (protocols);
}

static void
clear_input_hint (GtkWidget *window)
{
  /* This needs to be called after gtk_widget_show, otherwise
   * GTK+ will overwrite it. */
  GdkWindow *gdkwindow = gtk_widget_get_window (window);
  XWMHints *wm_hints;

  wm_hints = XGetWMHints (GDK_DISPLAY_XDISPLAY (gdk_window_get_display (gdkwindow)),
                          GDK_WINDOW_XID (gdkwindow));

  wm_hints->flags |= InputHint;
  wm_hints->input = False;

  XSetWMHints (GDK_DISPLAY_XDISPLAY (gdk_window_get_display (gdkwindow)),
               GDK_WINDOW_XID (gdkwindow),
               wm_hints);

  XFree (wm_hints);
}

static void
active_notify (GObject    *obj,
               GParamSpec *pspec,
               gpointer    user_data)
{
  GtkLabel *label = user_data;

  if (gtk_window_is_active (GTK_WINDOW (obj)))
    gtk_label_set_text (label, "Focused");
  else
    gtk_label_set_text (label, "Not focused");
}

static void
make_focused_label (GtkWidget *toplevel,
                    GtkWidget *parent)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_widget_show (label);

  gtk_container_add (GTK_CONTAINER (parent), label);

  g_signal_connect (toplevel, "notify::is-active",
                    G_CALLBACK (active_notify), label);
  active_notify (G_OBJECT (toplevel), NULL, label);
}

static void
setup_test_dialog (GtkWidget *toplevel)
{
  make_focused_label (toplevel, toplevel);
  gtk_widget_set_size_request (toplevel, 200, 200);
}

static void
noinput_clicked (GtkButton *button, gpointer user_data)
{
  if (noinput_window)
    gtk_window_present_with_time (GTK_WINDOW (noinput_window), gtk_get_current_event_time ());
  else
    {
      noinput_window = g_object_new (GTK_TYPE_WINDOW,
                                     "type", GTK_WINDOW_TOPLEVEL,
                                     "title", "No Input",
                                     "accept-focus", FALSE,
                                     NULL);
      setup_test_dialog (noinput_window);
      g_signal_connect (noinput_window, "destroy",
                        G_CALLBACK (clear_on_destroy), &noinput_window);
      disable_take_focus (noinput_window);
      gtk_widget_show (noinput_window);
    }
}

static void
passive_clicked (GtkButton *button, gpointer user_data)
{
  if (passive_window)
    gtk_window_present_with_time (GTK_WINDOW (passive_window), gtk_get_current_event_time ());
  else
    {
      passive_window = g_object_new (GTK_TYPE_WINDOW,
                                     "type", GTK_WINDOW_TOPLEVEL,
                                     "title", "Passive Input",
                                     "accept-focus", TRUE,
                                     NULL);
      setup_test_dialog (passive_window);
      g_signal_connect (passive_window, "destroy",
                        G_CALLBACK (clear_on_destroy), &passive_window);
      disable_take_focus (passive_window);
      gtk_widget_show (passive_window);
    }
}

static void
local_clicked (GtkButton *button, gpointer user_data)
{
  if (local_window)
    gtk_window_present_with_time (GTK_WINDOW (local_window), gtk_get_current_event_time ());
  else
    {
      local_window = g_object_new (GTK_TYPE_WINDOW,
                                   "type", GTK_WINDOW_TOPLEVEL,
                                   "title", "Locally Active Input",
                                   "accept-focus", TRUE,
                                   NULL);
      setup_test_dialog (local_window);
      g_signal_connect (local_window, "destroy",
                        G_CALLBACK (clear_on_destroy), &local_window);
      gtk_widget_show (local_window);
    }
}

static void
global_clicked (GtkButton *button, gpointer user_data)
{
  if (global_window)
    gtk_window_present_with_time (GTK_WINDOW (global_window), gtk_get_current_event_time ());
  else
    {
      /* gtk will only process WM_TAKE_FOCUS messages if accept-focus
       * is TRUE. So we set that property and then manually clear the
       * Input WMHint.
       */
      global_window = g_object_new (GTK_TYPE_WINDOW,
                                    "type", GTK_WINDOW_TOPLEVEL,
                                    "title", "Globally Active Input",
                                    "accept-focus", TRUE,
                                    NULL);
      setup_test_dialog (global_window);
      g_signal_connect (global_window, "destroy",
                        G_CALLBACK (clear_on_destroy), &global_window);
      gtk_widget_show (global_window);
      clear_input_hint (global_window);
    }
}

static void
lame_clicked (GtkButton *button, gpointer user_data)
{
  if (lame_window)
    gtk_window_present_with_time (GTK_WINDOW (lame_window), gtk_get_current_event_time ());
  else
    {
      lame_window = g_object_new (GTK_TYPE_WINDOW,
                                  "type", GTK_WINDOW_TOPLEVEL,
                                  "title", "Lame Globally Active Input",
                                  "accept-focus", FALSE,
                                  NULL);
      setup_test_dialog (lame_window);
      g_signal_connect (lame_window, "destroy",
                        G_CALLBACK (clear_on_destroy), &lame_window);
      gtk_widget_show (lame_window);
    }
}

static void
grabby_active_changed (GObject *object, GParamSpec *param, gpointer user_data)
{
  if (gtk_window_is_active (GTK_WINDOW (grabby_window)))
    {
      GdkWindow *gdkwindow = gtk_widget_get_window (grabby_window);
      guint32 now = gdk_x11_get_server_time (gdkwindow);

      gtk_window_present_with_time (GTK_WINDOW (main_window), now - 1);
      XSetInputFocus (GDK_DISPLAY_XDISPLAY (gdk_window_get_display (gdkwindow)),
                      GDK_WINDOW_XID (gdkwindow),
                      RevertToParent,
                      now);
    }
}

static void
grabby_clicked (GtkButton *button, gpointer user_data)
{
  if (grabby_window)
    gtk_window_present_with_time (GTK_WINDOW (grabby_window), gtk_get_current_event_time ());
  else
    {
      grabby_window = g_object_new (GTK_TYPE_WINDOW,
                                    "type", GTK_WINDOW_TOPLEVEL,
                                    "title", "Focus-grabbing Window",
                                    "accept-focus", TRUE,
                                    /* Because mutter maps windows
                                     * asynchronously, our trick won't
                                     * work if we try to do it when the
                                     * window is first mapped.
                                     */
                                    "focus-on-map", FALSE,
                                    NULL);
      setup_test_dialog (grabby_window);
      g_signal_connect (grabby_window, "destroy",
                        G_CALLBACK (clear_on_destroy), &grabby_window);
      g_signal_connect (grabby_window, "notify::is-active",
                        G_CALLBACK (grabby_active_changed), NULL);
      gtk_widget_show (grabby_window);
    }
}

static void
dying_clicked (GtkButton *button, gpointer user_data)
{
  if (dying_window)
    {
      gtk_window_present_with_time (GTK_WINDOW (dying_window), gtk_get_current_event_time ());
      gtk_widget_destroy (dying_window);
    }
  else
    {
      GtkWidget *label;

      dying_window = g_object_new (GTK_TYPE_WINDOW,
                                   "type", GTK_WINDOW_TOPLEVEL,
                                   "title", "Dying Window",
                                   "accept-focus", TRUE,
                                   /* As with grabby window */
                                   "focus-on-map", FALSE,
                                   NULL);
      setup_test_dialog (dying_window);
      g_signal_connect (dying_window, "destroy",
                        G_CALLBACK (clear_on_destroy), &dying_window);

      label = gtk_label_new ("Click button again to test");
      gtk_container_add (GTK_CONTAINER (dying_window), label);
      gtk_widget_set_size_request (dying_window, 200, 200);

      gtk_widget_show_all (dying_window);
    }
}

static void
main_window_destroyed (GtkWidget *widget, gpointer user_data)
{
  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GtkWidget *vbox, *button;

  gtk_init (&argc, &argv);

  main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (main_window), "Focus Tester");

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_set_homogeneous (GTK_BOX (vbox), 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_container_add (GTK_CONTAINER (main_window), vbox);

  make_focused_label (main_window, vbox);

  /* ICCCM "No Input" mode; Input hint False, WM_TAKE_FOCUS absent */
  button = gtk_button_new_with_label ("No Input Window");
  g_signal_connect (button, "clicked", G_CALLBACK (noinput_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  /* ICCCM "Passive" mode; Input hint True, WM_TAKE_FOCUS absent */
  button = gtk_button_new_with_label ("Passive Input Window");
  g_signal_connect (button, "clicked", G_CALLBACK (passive_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  /* ICCCM "Locally Active" mode; Input hint True, WM_TAKE_FOCUS present.
   * This is the behavior of GtkWindows with accept_focus==TRUE.
   */
  button = gtk_button_new_with_label ("Locally Active Window");
  g_signal_connect (button, "clicked", G_CALLBACK (local_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  /* ICCCM "Globally Active" mode; Input hint False, WM_TAKE_FOCUS present,
   * and the window responds to WM_TAKE_FOCUS by calling XSetInputFocus.
   */
  button = gtk_button_new_with_label ("Globally Active Window");
  g_signal_connect (button, "clicked", G_CALLBACK (global_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  /* "Lame" Globally Active mode; like "Globally Active", except that
   * the window does not respond to WM_TAKE_FOCUS. This is the
   * behavior of GtkWindows with accept_focus==FALSE.
   */
  button = gtk_button_new_with_label ("Globally Lame Window");
  g_signal_connect (button, "clicked", G_CALLBACK (lame_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  /* "Grabby" window; when you activate the window, it asks the wm to
   * focus the main window, but then forcibly grabs focus back with
   * a newer timestamp, causing the wm to be temporarily confused
   * about what window is focused and triggering the "Earlier attempt
   * to focus ... failed" codepath.
   */
  button = gtk_button_new_with_label ("Grabby Window");
  g_signal_connect (button, "clicked", G_CALLBACK (grabby_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  /* "Dying" window; we create the window on the first click, then
   * activate it and destroy it on the second click, causing mutter to
   * do an XSetInputFocus but not receive the corresponding FocusIn.
   */
  button = gtk_button_new_with_label ("Dying Window");
  g_signal_connect (button, "clicked", G_CALLBACK (dying_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  gtk_widget_show_all (main_window);

  g_signal_connect (main_window, "destroy",
                    G_CALLBACK (main_window_destroyed), NULL);

  gtk_main ();

  return 0;
}
