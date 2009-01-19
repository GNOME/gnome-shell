#include "shell-panel-window.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#define PANEL_HEIGHT 25

enum {
   PROP_0,

};

static void shell_panel_window_dispose (GObject *object);
static void shell_panel_window_finalize (GObject *object);
static void shell_panel_window_set_property ( GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec );
static void shell_panel_window_get_property( GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec );
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
};

static void
shell_panel_window_class_init(ShellPanelWindowClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

    gobject_class->dispose = shell_panel_window_dispose;
    gobject_class->finalize = shell_panel_window_finalize;
    gobject_class->set_property = shell_panel_window_set_property;
    gobject_class->get_property = shell_panel_window_get_property;

    widget_class->realize = shell_panel_window_realize;
    widget_class->size_request = shell_panel_window_size_request;    
    widget_class->size_allocate = shell_panel_window_size_allocate;
    widget_class->show = shell_panel_window_show;
}

static void shell_panel_window_init (ShellPanelWindow *self)
{
  self->priv = g_new0 (ShellPanelWindowPrivate, 1);

  gtk_window_set_type_hint (GTK_WINDOW (self), GDK_WINDOW_TYPE_HINT_DOCK);
  gtk_window_set_focus_on_map (GTK_WINDOW (self), FALSE);
  gdk_window_add_filter (NULL, filter_func, self);
}

static void shell_panel_window_dispose (GObject *object)
{
    ShellPanelWindow *self = (ShellPanelWindow*)object;

    G_OBJECT_CLASS (shell_panel_window_parent_class)->dispose(object);
}

static void shell_panel_window_finalize (GObject *object)
{
    ShellPanelWindow *self = (ShellPanelWindow*)object;

    g_free (self->priv);
    g_signal_handlers_destroy(object);
    G_OBJECT_CLASS (shell_panel_window_parent_class)->finalize(object);
}

static void shell_panel_window_set_property ( GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec ) 
{
   ShellPanelWindow* self = SHELL_PANEL_WINDOW(object);
    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}


static void shell_panel_window_get_property ( GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec ) 
{
   ShellPanelWindow* self = SHELL_PANEL_WINDOW(object);
    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

ShellPanelWindow* shell_panel_window_new(void)
{
    return (ShellPanelWindow*) g_object_new(SHELL_TYPE_PANEL_WINDOW, 
        "type", GTK_WINDOW_TOPLEVEL, NULL);
}


static void
set_strut (ShellPanelWindow *self)
{
  long *buf;
  int x, y;
  int strut_size;

  strut_size = GTK_WIDGET (self)->allocation.height;

  gtk_window_get_position (GTK_WINDOW (self), &x, &y);
  
  buf = g_new0(long, 4);
  buf[3] = strut_size;
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
  int monitor;
  GtkRequisition requisition;
  GdkRectangle monitor_geometry;
  int x, y;
  int width;

  monitor = gdk_screen_get_monitor_at_point (gdk_screen_get_default (),
					     self->priv->workarea.x,
					     self->priv->workarea.y);
  gdk_screen_get_monitor_geometry (gdk_screen_get_default (),
				   monitor, &monitor_geometry);
  gtk_widget_size_request (GTK_WIDGET (self), &requisition);

  x = self->priv->workarea.x;
  y = monitor_geometry.y + monitor_geometry.height - requisition.height;
  width = monitor_geometry.width - x;

  self->priv->width = width;
  gtk_widget_set_size_request (GTK_WIDGET (self), width, PANEL_HEIGHT);
  gtk_window_move (GTK_WINDOW (self), x, y);
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
  if ((format == 32) && (nitems == 4) && (bytes_after == 0)) {
    int x, y, width, height;
    data32 = (long*)data;
    x = data32[0]; y = data32[1];
    width = data32[2]; height = data32[3];
    if (x == self->priv->workarea.x &&
	y == self->priv->workarea.y &&
	width == self->priv->workarea.width &&
	height == self->priv->workarea.height)
      return;

    self->priv->workarea.x = x;
    self->priv->workarea.y = y;
    self->priv->workarea.width = width;
    self->priv->workarea.height = height;

    handle_new_workarea (self);
  } else if (nitems == 0) {
    self->priv->workarea.x = self->priv->workarea.y = 0;
    self->priv->workarea.width = self->priv->workarea.height = -1;
    handle_new_workarea (self);
  } else {
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
  Atom workarea = gdk_x11_get_xatom_by_name_for_display (gdk_display_get_default (), "_NET_WORKAREA");

  switch (xevent->type) {
  case PropertyNotify:
    {
      if (xevent->xproperty.atom != workarea)
	break;

      on_workarea_changed (self);
    }
    break;
  default:
    break;
  }
  return ret;
}
