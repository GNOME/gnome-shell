/* Metacity window types/properties demo app */

/* 
 * Copyright (C) 2002 Havoc Pennington
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

static GtkWidget* do_appwindow (void);

static void
set_gdk_window_struts (GdkWindow *window,
                       int        left,
                       int        right,
                       int        top,
                       int        bottom)
{
  long vals[4];
  
  vals[0] = left;
  vals[1] = right;
  vals[2] = top;
  vals[3] = bottom;
  
  XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                   GDK_WINDOW_XWINDOW (window),
                   XInternAtom (GDK_WINDOW_XDISPLAY (window),
                                "_NET_WM_STRUT", False),
                   XA_CARDINAL, 32, PropModeReplace,
                   (guchar *)vals, 4);
}

static void
on_realize_set_struts (GtkWindow *window,
                       gpointer   data)
{
  int left;
  int right;
  int top;
  int bottom;

  g_return_if_fail (GTK_WIDGET_REALIZED (window));

  left = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "meta-strut-left"));
  right = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "meta-strut-right"));
  top = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "meta-strut-top"));
  bottom = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "meta-strut-bottom"));
  
  set_gdk_window_struts (GTK_WIDGET (window)->window,
                         left, right, top, bottom);
}

static void
set_gtk_window_struts (GtkWidget  *window,
                       int         left,
                       int         right,
                       int         top,
                       int         bottom)
{
  g_object_set_data (G_OBJECT (window), "meta-strut-left",
                     GINT_TO_POINTER (left));
  g_object_set_data (G_OBJECT (window), "meta-strut-right",
                     GINT_TO_POINTER (right));
  g_object_set_data (G_OBJECT (window), "meta-strut-top",
                     GINT_TO_POINTER (top));
  g_object_set_data (G_OBJECT (window), "meta-strut-bottom",
                     GINT_TO_POINTER (bottom));
  
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        on_realize_set_struts,
                                        NULL);
                                
  g_signal_connect_after (G_OBJECT (window),
                          "realize",
                          G_CALLBACK (on_realize_set_struts),
                          NULL);

  if (GTK_WIDGET_REALIZED (window))
    set_gdk_window_struts (GTK_WIDGET (window)->window,
                           left, right, top, bottom);
}

static void
set_gdk_window_type (GdkWindow  *window,
                     const char *type)
{
  Atom atoms[2] = { None, None };
  
  atoms[0] = XInternAtom (GDK_WINDOW_XDISPLAY (window),
                          type, False);
  
  XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                   GDK_WINDOW_XWINDOW (window),
                   XInternAtom (GDK_WINDOW_XDISPLAY (window), "_NET_WM_WINDOW_TYPE", False),
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *)atoms, 
                   1);
}

static void
on_realize_set_type (GtkWindow *window,
                     gpointer   data)
{
  const char *type;

  g_return_if_fail (GTK_WIDGET_REALIZED (window));

  type = g_object_get_data (G_OBJECT (window), "meta-window-type");

  g_return_if_fail (type != NULL);
  
  set_gdk_window_type (GTK_WIDGET (window)->window,
                       type);
}

static void
set_gtk_window_type (GtkWindow  *window,
                     const char *type)
{
  g_object_set_data (G_OBJECT (window), "meta-window-type", (char*) type);

  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        on_realize_set_type,
                                        NULL);
                                
  g_signal_connect_after (G_OBJECT (window),
                          "realize",
                          G_CALLBACK (on_realize_set_type),
                          NULL);

  if (GTK_WIDGET_REALIZED (window))
    set_gdk_window_type (GTK_WIDGET (window)->window,
                         type);
}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  do_appwindow ();

  gtk_main ();
  
  return 0;
}

static void
dialog_cb (gpointer             callback_data,
           guint                callback_action,
           GtkWidget           *widget)
{
  GtkWidget *dialog;
  
  dialog = gtk_message_dialog_new (GTK_WINDOW (callback_data),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "Here is a dialog");

  /* Close dialog on user response */
  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);
  
  gtk_widget_show (dialog);
}

static void
utility_cb (gpointer             callback_data,
            guint                callback_action,
            GtkWidget           *widget)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *button;
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  set_gtk_window_type (GTK_WINDOW (window), "_NET_WM_WINDOW_TYPE_UTILITY");
  gtk_window_set_title (GTK_WINDOW (window), "Utility");
  
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (callback_data));
  
  vbox = gtk_vbox_new (FALSE, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  button = gtk_button_new_with_mnemonic ("_A button");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic ("_B button");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic ("_C button");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic ("_D button");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  gtk_widget_show_all (window);
}

static gboolean
focus_in_event_cb (GtkWidget *window,
                   GdkEvent  *event,
                   gpointer   data)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (data);

  gtk_label_set_text (GTK_LABEL (widget), "Has focus");
}


static gboolean
focus_out_event_cb (GtkWidget *window,
                    GdkEvent  *event,
                    gpointer   data)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (data);

  gtk_label_set_text (GTK_LABEL (widget), "Not focused");
}

static GtkWidget*
focus_label (GtkWidget *window)
{
  GtkWidget *label;
  
  label = gtk_label_new ("Not focused");

  g_signal_connect (G_OBJECT (window), "focus_in_event",
                    G_CALLBACK (focus_in_event_cb), label);

  g_signal_connect (G_OBJECT (window), "focus_out_event",
                    G_CALLBACK (focus_out_event_cb), label);
  
  return label;
}

static void
splashscreen_cb (gpointer             callback_data,
                 guint                callback_action,
                 GtkWidget           *widget)
{
  GtkWidget *window;
  GtkWidget *image;
  GtkWidget *vbox;
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  set_gtk_window_type (GTK_WINDOW (window), "_NET_WM_WINDOW_TYPE_SPLASHSCREEN");
  gtk_window_set_title (GTK_WINDOW (window), "Splashscreen");
  
  vbox = gtk_vbox_new (FALSE, 0);
  
  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (vbox), image, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (vbox), focus_label (window), FALSE, FALSE, 0);  
  
  gtk_container_add (GTK_CONTAINER (window), vbox);
  
  gtk_widget_show_all (window);
}

enum
{
  DOCK_TOP = 1,
  DOCK_BOTTOM = 2,
  DOCK_LEFT = 3,
  DOCK_RIGHT = 4,
  DOCK_ALL = 5
};

static void
make_dock (int type)
{
  GtkWidget *window;
  GtkWidget *image;
  GtkWidget *box;
  GtkWidget *button;

  g_return_if_fail (type != DOCK_ALL);

  box = NULL;
  switch (type)
    {
    case DOCK_LEFT:
    case DOCK_RIGHT:      
      box = gtk_vbox_new (FALSE, 0);
      break;
    case DOCK_TOP:
    case DOCK_BOTTOM:
      box = gtk_hbox_new (FALSE, 0);
      break;
    case DOCK_ALL:
      break;
    }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  set_gtk_window_type (GTK_WINDOW (window), "_NET_WM_WINDOW_TYPE_DOCK");
  
  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);  
  
  gtk_box_pack_start (GTK_BOX (box), focus_label (window), FALSE, FALSE, 0);  

  button = gtk_button_new_with_label ("Close");
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (gtk_widget_destroy), window);
  
  gtk_container_add (GTK_CONTAINER (window), box);

#define DOCK_SIZE 48
  switch (type)
    {
    case DOCK_LEFT:
      gtk_widget_set_size_request (window, DOCK_SIZE, gdk_screen_height ());
      gtk_window_move (GTK_WINDOW (window), 0, 0);
      set_gtk_window_struts (window, DOCK_SIZE, 0, 0, 0);
      gtk_window_set_title (GTK_WINDOW (window), "LeftDock");
      break;
    case DOCK_RIGHT:      
      gtk_widget_set_size_request (window, DOCK_SIZE, gdk_screen_height ());
      gtk_window_move (GTK_WINDOW (window), gdk_screen_width () - DOCK_SIZE, 0);
      set_gtk_window_struts (window, 0, DOCK_SIZE, 0, 0);
      gtk_window_set_title (GTK_WINDOW (window), "RightDock");
      break;
    case DOCK_TOP:
      gtk_widget_set_size_request (window, gdk_screen_width (), DOCK_SIZE);
      gtk_window_move (GTK_WINDOW (window), 0, 0);
      set_gtk_window_struts (window, 0, 0, DOCK_SIZE, 0);
      gtk_window_set_title (GTK_WINDOW (window), "TopDock");
      break;
    case DOCK_BOTTOM:
      gtk_widget_set_size_request (window, gdk_screen_width (), DOCK_SIZE);
      gtk_window_move (GTK_WINDOW (window), 0, gdk_screen_height () - DOCK_SIZE);
      set_gtk_window_struts (window, 0, 0, 0, DOCK_SIZE);
      gtk_window_set_title (GTK_WINDOW (window), "BottomDock");
      break;
    case DOCK_ALL:
      break;
    }
  
  gtk_widget_show_all (window);
}

static void
dock_cb (gpointer             callback_data,
         guint                callback_action,
         GtkWidget           *widget)
{
  if (callback_action == DOCK_ALL)
    {
      make_dock (DOCK_TOP);
      make_dock (DOCK_BOTTOM);
      make_dock (DOCK_LEFT);
      make_dock (DOCK_RIGHT);
    }
  else
    {
      make_dock (callback_action);
    }
}

static void
desktop_cb (gpointer             callback_data,
            guint                callback_action,
            GtkWidget           *widget)
{
  GtkWidget *window;
  GtkWidget *label;
  GdkColor desktop_color;
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  set_gtk_window_type (GTK_WINDOW (window), "_NET_WM_WINDOW_TYPE_DESKTOP");
  gtk_window_set_title (GTK_WINDOW (window), "Desktop");
  gtk_widget_set_size_request (window,
                               gdk_screen_width (), gdk_screen_height ());
  gtk_window_move (GTK_WINDOW (window), 0, 0);
  
  desktop_color.red = 0x5144;
  desktop_color.green = 0x75D6;
  desktop_color.blue = 0xA699;

  gtk_widget_modify_bg (window, GTK_STATE_NORMAL, &desktop_color);
  
  label = focus_label (window);
  
  gtk_container_add (GTK_CONTAINER (window), label);
  
  gtk_widget_show_all (window);
}

static GtkItemFactoryEntry menu_items[] =
{
  { "/_Windows",              NULL,         NULL,                     0, "<Branch>" },
  { "/Windows/tearoff",       NULL,         NULL,                     0, "<Tearoff>" },
  { "/Windows/_Dialog",       "<control>d",  dialog_cb,               0, NULL },
  { "/Windows/_Utility",      "<control>u",  utility_cb,              0, NULL },
  { "/Windows/_Splashscreen", "<control>s",  splashscreen_cb,         0, NULL },
  { "/Windows/_Top dock",     NULL,          dock_cb,                 DOCK_TOP, NULL },
  { "/Windows/_Bottom dock",  NULL,          dock_cb,                 DOCK_BOTTOM, NULL },
  { "/Windows/_Left dock",    NULL,          dock_cb,                 DOCK_LEFT, NULL },
  { "/Windows/_Right dock",   NULL,          dock_cb,                 DOCK_RIGHT, NULL },
  { "/Windows/_All docks",    NULL,          dock_cb,                 DOCK_ALL, NULL },
  { "/Windows/Des_ktop",      NULL,          desktop_cb,              0, NULL }
};

static void
toolbar_cb (GtkWidget *button,
            gpointer   data)
{
  GtkWidget *dialog;
  
  dialog = gtk_message_dialog_new (GTK_WINDOW (data),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "Clicking the toolbar buttons doesn't do anything");

  /* Close dialog on user response */
  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);
  
  gtk_widget_show (dialog);
}

static void
update_statusbar (GtkTextBuffer *buffer,
                  GtkStatusbar  *statusbar)
{
  gchar *msg;
  gint row, col;
  gint count;
  GtkTextIter iter;
  
  gtk_statusbar_pop (statusbar, 0); /* clear any previous message, underflow is allowed */

  count = gtk_text_buffer_get_char_count (buffer);

  gtk_text_buffer_get_iter_at_mark (buffer,
                                    &iter,
                                    gtk_text_buffer_get_insert (buffer));

  row = gtk_text_iter_get_line (&iter);
  col = gtk_text_iter_get_line_offset (&iter);

  msg = g_strdup_printf ("Cursor at row %d column %d - %d chars in document",
                         row, col, count);

  gtk_statusbar_push (statusbar, 0, msg);

  g_free (msg);
}

static void
mark_set_callback (GtkTextBuffer     *buffer,
                   const GtkTextIter *new_location,
                   GtkTextMark       *mark,
                   gpointer           data)
{
  update_statusbar (buffer, GTK_STATUSBAR (data));
}

static int window_count = 0;

static void
destroy_cb (GtkWidget *w, gpointer data)
{
  --window_count;
  if (window_count == 0)
    gtk_main_quit ();
}

static GtkWidget *
do_appwindow (void)
{
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *toolbar;
  GtkWidget *handlebox;
  GtkWidget *statusbar;
  GtkWidget *contents;
  GtkWidget *sw;
  GtkTextBuffer *buffer;
  GtkAccelGroup *accel_group;      
  GtkItemFactory *item_factory;
      
  /* Create the toplevel window
   */

  ++window_count;
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Application Window");
  
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (destroy_cb), NULL);
      
  table = gtk_table_new (1, 4, FALSE);
      
  gtk_container_add (GTK_CONTAINER (window), table);
      
  /* Create the menubar
   */
      
  accel_group = gtk_accel_group_new ();
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  g_object_unref (accel_group);
      
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);

  /* Set up item factory to go away with the window */
  g_object_ref (item_factory);
  gtk_object_sink (GTK_OBJECT (item_factory));
  g_object_set_data_full (G_OBJECT (window),
                          "<main>",
                          item_factory,
                          (GDestroyNotify) g_object_unref);

  /* create menu items */
  gtk_item_factory_create_items (item_factory, G_N_ELEMENTS (menu_items),
                                 menu_items, window);

  gtk_table_attach (GTK_TABLE (table),
                    gtk_item_factory_get_widget (item_factory, "<main>"),
                    /* X direction */          /* Y direction */
                    0, 1,                      0, 1,
                    GTK_EXPAND | GTK_FILL,     0,
                    0,                         0);

  /* Create the toolbar
   */
  toolbar = gtk_toolbar_new ();

  gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
                            GTK_STOCK_NEW,
                            "Open another one of these windows",
                            NULL,
                            G_CALLBACK (do_appwindow),
                            window, /* user data for callback */
                            -1);  /* -1 means "append" */
  
  gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
                            GTK_STOCK_OPEN,
                            "This is a demo button with an 'open' icon",
                            NULL,
                            G_CALLBACK (toolbar_cb),
                            window, /* user data for callback */
                            -1);  /* -1 means "append" */

  gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
                            GTK_STOCK_QUIT,
                            "This is a demo button with a 'quit' icon",
                            NULL,
                            G_CALLBACK (toolbar_cb),
                            window, /* user data for callback */
                            -1);  /* -1 means "append" */

  handlebox = gtk_handle_box_new ();

  gtk_container_add (GTK_CONTAINER (handlebox), toolbar);
      
  gtk_table_attach (GTK_TABLE (table),
                    handlebox,
                    /* X direction */       /* Y direction */
                    0, 1,                   1, 2,
                    GTK_EXPAND | GTK_FILL,  0,
                    0,                      0);

  /* Create document
   */

  sw = gtk_scrolled_window_new (NULL, NULL);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                       GTK_SHADOW_IN);
      
  gtk_table_attach (GTK_TABLE (table),
                    sw,
                    /* X direction */       /* Y direction */
                    0, 1,                   2, 3,
                    GTK_EXPAND | GTK_FILL,  GTK_EXPAND | GTK_FILL,
                    0,                      0);

  gtk_window_set_default_size (GTK_WINDOW (window),
                               200, 200);
      
  contents = gtk_text_view_new ();
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (contents),
                               PANGO_WRAP_WORD);
      
  gtk_container_add (GTK_CONTAINER (sw),
                     contents);

  /* Create statusbar */

  statusbar = gtk_statusbar_new ();
  gtk_table_attach (GTK_TABLE (table),
                    statusbar,
                    /* X direction */       /* Y direction */
                    0, 1,                   3, 4,
                    GTK_EXPAND | GTK_FILL,  0,
                    0,                      0);

  /* Show text widget info in the statusbar */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (contents));

  gtk_text_buffer_set_text (buffer,
                            "This demo demonstrates various kinds of windows that "
                            "window managers and window manager themes should handle. "
                            "Be sure to tear off the menu and toolbar, those are also "
                            "a special kind of window.",
                            -1);
      
  g_signal_connect_object (buffer,
                           "changed",
                           G_CALLBACK (update_statusbar),
                           statusbar,
                           0);

  g_signal_connect_object (buffer,
                           "mark_set", /* cursor moved */
                           G_CALLBACK (mark_set_callback),
                           statusbar,
                           0);
      
  update_statusbar (buffer, GTK_STATUSBAR (statusbar));

  gtk_widget_show_all (window);

  return window;
}


