/* Metacity control panel */

/*
 * Copyright (C) 2002 Sun Microsystems, Inc.
 * Copyright (C) 2002 Red Hat, Inc.
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

#include <config.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <libintl.h>
#include <string.h>
#include <gconf/gconf-client.h>

void update_config (GtkWidget *widget, gpointer user_data);

static GConfClient *gconf_client;
static GtkWidget *click_radio;
static GtkWidget *point_radio;
static GtkWidget *autoraise_check;

#define KEY_DIR	"/apps/metacity/general"
#define KEY_FOCUS_MODE "/apps/metacity/general/focus_mode"
#define KEY_AUTO_RAISE "/apps/metacity/general/auto_raise"

static void 
update_ui (void)
{
  char *focus_mode;

  focus_mode = gconf_client_get_string (gconf_client,
                                        KEY_FOCUS_MODE,
                                        NULL);

  if (focus_mode == NULL)
    focus_mode = g_strdup ("click");

  if (strcmp (focus_mode, "click") == 0)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (click_radio),
                                    TRUE);
      gtk_widget_set_sensitive(autoraise_check, FALSE);
    }
  else 
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (point_radio),
                                    TRUE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (autoraise_check),
                                    gconf_client_get_bool (gconf_client,
                                                           KEY_AUTO_RAISE,
                                                           NULL));
      gtk_widget_set_sensitive(autoraise_check, TRUE);
    }

  g_free (focus_mode);
}

static void
key_change_cb (GConfClient *client, guint cnxn_id,
               GConfEntry *entry, gpointer user_data)
{
  update_ui ();
}

void
update_config (GtkWidget *widget, gpointer user_data)
{
  const char *focus_mode = NULL;

  if (GTK_TOGGLE_BUTTON (click_radio)->active == TRUE)
    {
      focus_mode = "click";
    }
  else
    {
      focus_mode = "sloppy";
    }

  gconf_client_set_string (gconf_client,
                           KEY_FOCUS_MODE,
                           focus_mode, 
                           NULL);

  gconf_client_set_bool (gconf_client, KEY_AUTO_RAISE,
                         GTK_TOGGLE_BUTTON (autoraise_check)->active, NULL);
}

int
main (int argc, char **argv)
{
  GladeXML *xml;
  GdkPixbuf *pixbuf;
  GtkWidget *window;

  bindtextdomain (GETTEXT_PACKAGE, METACITY_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
	
  gtk_init (&argc, &argv);

  xml = glade_xml_new (METACITY_PROPS_GLADEDIR
                       "/metacity-properties.glade", NULL, NULL);

  click_radio = glade_xml_get_widget (xml, "Clickfocus");
  point_radio = glade_xml_get_widget (xml, "Pointfocus");
  autoraise_check = glade_xml_get_widget (xml, "Autoraise");
  window = glade_xml_get_widget (xml, "Mainwindow");

  pixbuf = gdk_pixbuf_new_from_file (METACITY_PROPS_ICON_DIR 
                                     "/metacity-properties.png", NULL);
	
  gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
  g_object_unref (G_OBJECT (pixbuf));

  gconf_client = gconf_client_get_default ();
  gconf_client_add_dir (gconf_client, 
                        KEY_DIR,
                        GCONF_CLIENT_PRELOAD_NONE,
                        NULL);
  gconf_client_notify_add (gconf_client, 
                           KEY_FOCUS_MODE,
                           key_change_cb,
                           NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client,
                           KEY_AUTO_RAISE, 
                           key_change_cb,
                           NULL, NULL, NULL);

  update_ui ();

  glade_xml_signal_autoconnect (xml);

  gtk_widget_show_all (window);
  
  gtk_main ();
		
  return 0;
}

