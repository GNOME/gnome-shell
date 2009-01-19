#include "shell-panel-window.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#define PANEL_HEIGHT 25

enum {
   PROP_0,

};

static void shell_panel_window_finalize (GObject *object);
static void shell_panel_window_size_request (GtkWidget *self, GtkRequisition *req);
static void shell_panel_window_size_allocate (GtkWidget *self, GtkAllocation *allocation);
static void shell_panel_window_realize (GtkWidget *self);
static void shell_panel_window_show (GtkWidget *self);
static void set_strut (ShellPanelWindow *self);
static void on_workarea_changed (ShellPanelWindow *self);
static void handle_new_workarea (ShellPanelWindow *self);
static GdkFilterReturn filter_func (GdkXEvent *xevent,
				    GdkEvent *event,
				    gpointer data);

G_DEFINE_TYPE(ShellPanelWindow, shell_panel_window, GTK_TYPE_WINDOW);

struct ShellPanelWindowPrivate {
  GtkAllocation workarea;
  guint width;
  guint height;
  Atom workarea_atom;
};

static void
shell_panel_window_class_init(ShellPanelWindowClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

    gobject_class->finalize = shell_panel_window_finalize;
    
    widget_class->realize = shell_panel_window_realize;
    widget_class->size_request = shell_panel_window_size_request;    
    widget_class->size_allocate = shell_panel_window_size_allocate;
    widget_class->show = shell_panel_window_show;
}

static void shell_panel_window_init (ShellPanelWindow *self)
{
  self->priv = g_new0 (ShellPanelWindowPrivate, 1);

  self->priv->workarea_atom = gdk_x11_get_xatom_by_name_for_display (gdk_display_get_default (), "_NET_WORKAREA");
  
  gtk_window_set_type_hint (GTK_WINDOW (self), GDK_WINDOW_TYPE_HINT_DOCK);
  gtk_window_set_focus_on_map (GTK_WINDOW (self), FALSE);
  gdk_window_add_filter (NULL, filter_func, self);
}

static void shell_panel_window_finalize (GObject *object)
{
    ShellPanelWindow *self = (ShellPanelWindow*)object;

    g_free (self->priv);
    g_signal_handlers_destroy(object);
    G_OBJECT_CLASS (shell_panel_window_parent_class)->finalize(object);
}

ShellPanelWindow* shell_panel_window_new(void) {
    return (ShellPanelWindow*) g_object_new(SHELL_TYPE_PANEL_WINDOW, 
        "type", GTK_WINDOW_TOPLEVEL, NULL);
}

static void
set_strut (ShellPanelWindow *self)
{
  long *buf;
  int strut_size;

  strut_size = GTK_WIDGET (self)->allocation.height;

  buf = g_new0 (long, 4);
  buf[0] = 0; /* left */
  buf[1] = 0; /* right */
  buf[2] = 0; /* top */
  buf[3] = strut_size; /* bottom */
  gdk_property_change (GTK_WIDGET (self)->window, gdk_atom_intern_static_string ("_NET_WM_STRUT"),
		       gdk_atom_intern_static_string ("CARDINAL"), 32,
		       GDK_PROP_MODE_REPLACE,
		       (guchar*) buf, 4);
  g_free (buf);
}

static void
shell_panel_window_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  ShellPanelWindow *self = SHELL_PANEL_WINDOW (widget);  
  GTK_WIDGET_CLASS (shell_panel_window_parent_class)->size_request(widget, requisition);  
  requisition->width = self->priv->width;
  requisition->height = PANEL_HEIGHT; 
}

static void
shell_panel_window_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  ShellPanelWindow *self = SHELL_PANEL_WINDOW (widget);
  GTK_WIDGET_CLASS (shell_panel_window_parent_class)->size_allocate(widget, allocation);
  if (GTK_WIDGET_REALIZED (self))
    set_strut (self);
}

static void
shell_panel_window_realize (GtkWidget *widget)
{
  ShellPanelWindow *self = SHELL_PANEL_WINDOW (widget);
  GTK_WIDGET_CLASS (shell_panel_window_parent_class)->realize(widget);
  set_strut (self);
}

static void
shell_panel_window_show (GtkWidget *widget)
{
  ShellPanelWindow *self = SHELL_PANEL_WINDOW (widget);
  on_workarea_changed (self);
  GTK_WIDGET_CLASS (shell_panel_window_parent_class)->show(widget);
}

static void
handle_new_workarea (ShellPanelWindow *self)
{
  GtkRequisition requisition;
  int x, y;
  int width;
  int height;
  int x_target, y_target;

  gtk_widget_size_request (GTK_WIDGET (self), &requisition);

  /* If we don't have a workarea, just use monitor */
  if (self->priv->workarea.width == 0) 
    {
      int monitor;
      GdkRectangle monitor_geometry;      
      
      monitor = gdk_screen_get_monitor_at_point (gdk_screen_get_default (),
                                                 0, 0);
      gdk_screen_get_monitor_geometry (gdk_screen_get_default (),
                                       monitor, &monitor_geometry);      
      x = monitor_geometry.x;
      y = monitor_geometry.y;
      width = monitor_geometry.width;
      height = monitor_geometry.height;
    }
  else
    {
      x = self->priv->workarea.x;
      y = self->priv->workarea.y;
      width = self->priv->workarea.width;
      height = self->priv->workarea.height;
    }
  
  x_target = x;
  y_target = y + height - requisition.height;

  self->priv->width = width;
  self->priv->height = height;
  gtk_widget_set_size_request (GTK_WIDGET (self), width - x_target, PANEL_HEIGHT);
  gtk_window_move (GTK_WINDOW (self), x_target, y_target);
}

static void
on_workarea_changed (ShellPanelWindow *self)
{
  gulong bytes_after, nitems;
  Atom type;
  gint format;
  guchar *data;
  long *data32;
  Atom workarea = gdk_x11_get_xatom_by_name_for_display (gdk_display_get_default (), "_NET_WORKAREA");

  XGetWindowProperty (GDK_DISPLAY(), GDK_ROOT_WINDOW(),
		      workarea,
		      0, 4, FALSE, workarea,
		      &type, &format, &nitems, &bytes_after, &data);
  if ((format == 32) && (nitems == 4) && (bytes_after == 0))
    {
      int x, y, width, height;
      data32 = (long*) data;
      x = data32[0];
      y = data32[1];
      width = data32[2];
      height = data32[3];
      if (x == self->priv->workarea.x && y == self->priv->workarea.y 
          && width == self->priv->workarea.width 
          && height == self->priv->workarea.height)
        return;

      self->priv->workarea.x = x;
      self->priv->workarea.y = y;
      self->priv->workarea.width = width;
      self->priv->workarea.height = height;

      handle_new_workarea (self);
    }
  else if (nitems == 0)
    {
      /* We have no workarea set; assume there are no other panels at this time */
      self->priv->workarea.x = self->priv->workarea.y = 0;
      self->priv->workarea.width = self->priv->workarea.height = 0;
      handle_new_workarea (self);
    }
  else
    {
      g_printerr ("unexpected return from XGetWindowProperty: %d %ld %ld\n",
          format, nitems, bytes_after);
    }
}

static GdkFilterReturn
filter_func (GdkXEvent *gdk_xevent,
	     GdkEvent *event,
	     gpointer data)
{
  ShellPanelWindow *self = SHELL_PANEL_WINDOW (data);
  GdkFilterReturn ret = GDK_FILTER_CONTINUE;
  XEvent *xevent = (XEvent *) event;

  switch (xevent->type) 
  {
  case PropertyNotify:
    {
      if (xevent->xproperty.atom != self->priv->workarea_atom)
	break;
      on_workarea_changed (self);
    }
    break;
  default:
    break;
  }
  return ret;
}
