/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* gnome-shell-perf-helper: a program to create windows for performance tests
 *
 * Running performance tests with whatever windows a user has open results
 * in unreliable results, so instead we hide all other windows and talk
 * to this program over D-Bus to create just the windows we want.
 */

#include "config.h"

#include <math.h>

#include <gtk/gtk.h>

#define BUS_NAME "org.gnome.Shell.PerfHelper"

static void destroy_windows           (void);
static void finish_wait_windows       (void);
static void check_finish_wait_windows (void);

static const gchar introspection_xml[] =
	  "<node>"
	  "  <interface name='org.gnome.Shell.PerfHelper'>"
	  "    <method name='Exit'/>"
	  "    <method name='CreateWindow'>"
	  "      <arg type='i' name='width' direction='in'/>"
	  "      <arg type='i' name='height' direction='in'/>"
	  "      <arg type='b' name='alpha' direction='in'/>"
	  "      <arg type='b' name='maximized' direction='in'/>"
	  "      <arg type='b' name='redraws' direction='in'/>"
	  "      <arg type='b' name='text_input' direction='in'/>"
	  "    </method>"
	  "    <method name='WaitWindows'/>"
	  "    <method name='DestroyWindows'/>"
	  "  </interface>"
	"</node>";

typedef struct {
  GtkWidget *window;
  int width;
  int height;

  guint alpha : 1;
  guint maximized : 1;
  guint redraws : 1;
  guint mapped : 1;
  guint exposed : 1;
  guint pending : 1;

  gint64 start_time;
  gint64 time;
} WindowInfo;

static int opt_idle_timeout = 30;

static GOptionEntry opt_entries[] =
  {
    { "idle-timeout", 'r', 0, G_OPTION_ARG_INT, &opt_idle_timeout, "Exit after N seconds", "N" },
    { NULL }
  };

static guint timeout_id;
static GList *our_windows;
static GList *wait_windows_invocations;

static gboolean
on_timeout (gpointer data)
{
  timeout_id = 0;

  destroy_windows ();
  gtk_main_quit ();

  return FALSE;
}

static void
establish_timeout (void)
{
  g_clear_handle_id (&timeout_id, g_source_remove);

  timeout_id = g_timeout_add (opt_idle_timeout * 1000, on_timeout, NULL);
  g_source_set_name_by_id (timeout_id, "[gnome-shell] on_timeout");
}

static void
destroy_windows (void)
{
  GList *l;

  for (l = our_windows; l; l = l->next)
    {
      WindowInfo *info = l->data;
      gtk_widget_destroy (info->window);
      g_free (info);
    }

  g_list_free (our_windows);
  our_windows = NULL;

  check_finish_wait_windows ();
}

static gboolean
on_window_map_event (GtkWidget   *window,
                     GdkEventAny *event,
                     WindowInfo  *info)
{
  info->mapped = TRUE;

  return FALSE;
}

static gboolean
on_window_draw (GtkWidget  *window,
                cairo_t    *cr,
                WindowInfo *info)
{
  info->exposed = TRUE;

  if (info->exposed && info->mapped && info->pending)
    {
      info->pending = FALSE;
      check_finish_wait_windows ();
    }

  return FALSE;
}

static gboolean
on_child_draw (GtkWidget  *window,
               cairo_t    *cr,
               WindowInfo *info)
{
  cairo_rectangle_int_t allocation;
  double x_offset, y_offset;

  gtk_widget_get_allocation (window, &allocation);

  /* We draw an arbitrary pattern of red lines near the border of the
   * window to make it more clear than empty windows if something
   * is drastrically wrong.
   */

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

  if (info->alpha)
    cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
  else
    cairo_set_source_rgb (cr, 1, 1, 1);

  cairo_paint (cr);
  cairo_restore (cr);

  if (info->redraws)
    {
      double position = (info->time - info->start_time) / 1000000.;
      x_offset = 20 * cos (2 * M_PI * position);
      y_offset = 20 * sin (2 * M_PI * position);
    }
  else
    {
      x_offset = y_offset = 0;
    }

  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_set_line_width (cr, 10);
  cairo_move_to (cr, 0, 40 + y_offset);
  cairo_line_to (cr, allocation.width, 40 + y_offset);
  cairo_move_to (cr, 0, allocation.height - 40 + y_offset);
  cairo_line_to (cr, allocation.width, allocation.height - 40 + y_offset);
  cairo_move_to (cr, 40 + x_offset, 0);
  cairo_line_to (cr, 40 + x_offset, allocation.height);
  cairo_move_to (cr, allocation.width - 40 + x_offset, 0);
  cairo_line_to (cr, allocation.width - 40 + x_offset, allocation.height);
  cairo_stroke (cr);

  return FALSE;
}

static gboolean
tick_callback (GtkWidget     *widget,
               GdkFrameClock *frame_clock,
               gpointer       user_data)
{
  WindowInfo *info = user_data;

  if (info->start_time < 0)
    info->start_time = info->time = gdk_frame_clock_get_frame_time (frame_clock);
  else
    info->time = gdk_frame_clock_get_frame_time (frame_clock);

  gtk_widget_queue_draw (widget);

  return TRUE;
}

static void
create_window (int      width,
	       int      height,
               gboolean alpha,
               gboolean maximized,
               gboolean redraws,
               gboolean text_input)
{
  WindowInfo *info;
  GtkWidget *child;

  info = g_new0 (WindowInfo, 1);
  info->width = width;
  info->height = height;
  info->alpha = alpha;
  info->maximized = maximized;
  info->redraws = redraws;
  info->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  if (alpha)
    gtk_widget_set_visual (info->window, gdk_screen_get_rgba_visual (gdk_screen_get_default ()));
  if (maximized)
    gtk_window_maximize (GTK_WINDOW (info->window));
  info->pending = TRUE;
  info->start_time = -1;

  if (text_input)
    {
      child = gtk_entry_new ();
      gtk_widget_show (child);
    }
  else
    {
      child = g_object_new (GTK_TYPE_BOX, "visible", TRUE, "app-paintable", TRUE, NULL);
      gtk_widget_set_app_paintable (info->window, TRUE);
      g_signal_connect (child, "draw", G_CALLBACK (on_child_draw), info);
    }

  gtk_container_add (GTK_CONTAINER (info->window), child);

  g_signal_connect (info->window, "draw", G_CALLBACK (on_window_draw), info);
  g_signal_connect (info->window, "map-event", G_CALLBACK (on_window_map_event), info);

  gtk_widget_set_size_request (info->window, width, height);
  gtk_widget_show (info->window);

  if (info->redraws)
    gtk_widget_add_tick_callback (info->window, tick_callback,
                                  info, NULL);

  our_windows = g_list_prepend (our_windows, info);
}

static void
finish_wait_windows (void)
{
  GList *l;

  for (l = wait_windows_invocations; l; l = l->next)
    g_dbus_method_invocation_return_value (l->data, NULL);

  g_list_free (wait_windows_invocations);
  wait_windows_invocations = NULL;
}

static void
check_finish_wait_windows (void)
{
  GList *l;
  gboolean have_pending = FALSE;

  for (l = our_windows; l; l = l->next)
    {
      WindowInfo *info = l->data;
      if (info->pending)
        have_pending = TRUE;
    }

  if (!have_pending)
    finish_wait_windows ();
}

static void
handle_method_call (GDBusConnection       *connection,
		    const gchar           *sender,
		    const gchar           *object_path,
		    const gchar           *interface_name,
		    const gchar           *method_name,
		    GVariant              *parameters,
		    GDBusMethodInvocation *invocation,
		    gpointer               user_data)
{
  /* Push off the idle timeout */
  establish_timeout ();

  if (g_strcmp0 (method_name, "Exit") == 0)
    {
      destroy_windows ();

      g_dbus_method_invocation_return_value (invocation, NULL);
      g_dbus_connection_flush_sync (connection, NULL, NULL);

      gtk_main_quit ();
    }
  else if (g_strcmp0 (method_name, "CreateWindow") == 0)
    {
      int width, height;
      gboolean alpha, maximized, redraws, text_input;

      g_variant_get (parameters, "(iibbbb)",
                     &width, &height,
                     &alpha, &maximized, &redraws, &text_input);

      create_window (width, height, alpha, maximized, redraws, text_input);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "WaitWindows") == 0)
    {
      wait_windows_invocations = g_list_prepend (wait_windows_invocations, invocation);
      check_finish_wait_windows ();
    }
  else if (g_strcmp0 (method_name, "DestroyWindows") == 0)
    {
      destroy_windows ();
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL
};

static void
on_bus_acquired (GDBusConnection *connection,
		 const gchar     *name,
		 gpointer         user_data)
{
  GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

  g_dbus_connection_register_object (connection,
				     "/org/gnome/Shell/PerfHelper",
				     introspection_data->interfaces[0],
				     &interface_vtable,
				     NULL,  /* user_data */
				     NULL,  /* user_data_free_func */
				     NULL); /* GError** */
}

static void
on_name_acquired (GDBusConnection *connection,
		  const gchar     *name,
		  gpointer         user_data)
{
}

static void
on_name_lost  (GDBusConnection *connection,
	       const gchar     *name,
	       gpointer         user_data)
{
  destroy_windows ();
  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GOptionContext *context;
  GError *error = NULL;

  /* Since we depend on this, avoid the possibility of lt-gnome-shell-perf-helper */
  g_set_prgname ("gnome-shell-perf-helper");

  context = g_option_context_new (" - server to create windows for performance testing");
  g_option_context_add_main_entries (context, opt_entries, NULL);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      return 1;
    }

  g_bus_own_name (G_BUS_TYPE_SESSION,
                  BUS_NAME,
                  G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                  G_BUS_NAME_OWNER_FLAGS_REPLACE,
                  on_bus_acquired,
                  on_name_acquired,
                  on_name_lost,
                  NULL,
                  NULL);

  establish_timeout ();

  gtk_main ();

  return 0;
}
