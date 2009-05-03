/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <gdk/gdkx.h>
#include <clutter/x11/clutter-x11.h>

#include "shell-embedded-window-private.h"

/* This type is a subclass of GtkWindow that ties the window to a
 * ShellGtkEmbed; the window is reparented into the stage
 * window for the actor and the resizing logic is bound to the clutter
 * logic.
 *
 * The typical usage we might expect is
 *
 *  - ShellEmbeddedWindow is created and filled with content
 *  - ShellEmbeddedWindow is shown with gtk_widget_show_all()
 *  - ShellGtkEmbed is created for the ShellEmbeddedWindow
 *  - actor is added to a stage
 *
 * Ideally, the way it would work is that the GtkWindow is mapped
 * if and only if both:
 *
 * - GTK_WIDGET_VISIBLE (window) [widget has been shown]
 * - Actor is mapped [actor and all parents visible, actor in stage]
 *
 * Implementing this perfectly is not currently possible, due to problems
 * in Clutter, see:
 *
 * http://bugzilla.openedhand.com/show_bug.cgi?id=1138
 *
 * So until that is fixed we use the "realized" state of the ClutterActor
 * as a stand-in for the ideal mapped state, this will work as long
 * as the ClutterActor and all its parents are in fact visible.
 */

G_DEFINE_TYPE (ShellEmbeddedWindow, shell_embedded_window, GTK_TYPE_WINDOW);

struct _ShellEmbeddedWindowPrivate {
  ShellGtkEmbed *actor;

  GdkRectangle position;
};

/*
 * The normal gtk_window_show() starts all of the complicated asynchronous
 * window resizing code running; we don't want or need any of that.
 * Bypassing the normal code does mean that the extra geometry management
 * available on GtkWindow: gridding, maximum sizes, etc, is ignored; we
 * don't really want that anyways - we just want a way of embedding a
 * GtkWidget into a Clutter stage.
 */
static void
shell_embedded_window_show (GtkWidget *widget)
{
  ShellEmbeddedWindow *window = SHELL_EMBEDDED_WINDOW (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);

  if (window->priv->actor)
    {
      /* Size is 0x0 if the GtkWindow is not shown */
      clutter_actor_queue_relayout (CLUTTER_ACTOR (window->priv->actor));

      if (CLUTTER_ACTOR_IS_REALIZED (window->priv->actor))
        gtk_widget_map (widget);
    }
}

static void
shell_embedded_window_hide (GtkWidget *widget)
{
  ShellEmbeddedWindow *window = SHELL_EMBEDDED_WINDOW (widget);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (window->priv->actor));

  GTK_WIDGET_CLASS (shell_embedded_window_parent_class)->hide (widget);
}

static void
shell_embedded_window_realize (GtkWidget *widget)
{
  ShellEmbeddedWindow *window = SHELL_EMBEDDED_WINDOW (widget);
  ClutterActor *stage;
  Window stage_xwindow;

  GTK_WIDGET_CLASS (shell_embedded_window_parent_class)->realize (widget);

  stage = clutter_actor_get_stage (CLUTTER_ACTOR (window->priv->actor));

  /* Clutter is buggy and will realize an actor when it has a parent
   * but no grandparent; this is a workaround until
   * http://bugzilla.openedhand.com/show_bug.cgi?id=1138 is fixed - we
   * only have one stage in the shell in any case.
   */
  if (!stage)
    stage = clutter_stage_get_default ();

  stage_xwindow = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

  /* Using XReparentWindow() is simpler than using gdk_window_reparent(),
   * since it avoids maybe having to create a new foreign GDK window for
   * the stage. However, GDK will be left thinking that the parent of
   * window->window is the root window - it's not immediately clear
   * to me whether that is more or less likely to cause problems than
   * modifying the GDK hierarchy.
   */
  XReparentWindow (GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (widget)),
                   GDK_WINDOW_XWINDOW (widget->window),
                   stage_xwindow,
                   window->priv->position.x, window->priv->position.y);
}

static gboolean
shell_embedded_window_configure_event (GtkWidget         *widget,
                                       GdkEventConfigure *event)
{
  /* Normally a configure event coming back from X triggers the
   * resizing logic inside GtkWindow; we just ignore them
   * since we are handling the resizing logic separately.
   */
  return FALSE;
}

static void
shell_embedded_window_check_resize (GtkContainer *container)
{
  ShellEmbeddedWindow *window = SHELL_EMBEDDED_WINDOW (container);

  /* Check resize is called when a resize is queued on something
   * inside the GtkWindow; we need to make sure that in response
   * to this gtk_widget_size_request() and then
   * gtk_widget_size_allocate() are called; we defer to the Clutter
   * logic and assume it will do the right thing.
   */
  if (window->priv->actor)
    clutter_actor_queue_relayout (CLUTTER_ACTOR (window->priv->actor));
}

static void
shell_embedded_window_class_init (ShellEmbeddedWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ShellEmbeddedWindowPrivate));

  widget_class->show            = shell_embedded_window_show;
  widget_class->hide            = shell_embedded_window_hide;
  widget_class->realize         = shell_embedded_window_realize;
  widget_class->configure_event = shell_embedded_window_configure_event;

  container_class->check_resize    = shell_embedded_window_check_resize;
}

static void
shell_embedded_window_init (ShellEmbeddedWindow *window)
{
  window->priv = G_TYPE_INSTANCE_GET_PRIVATE (window, SHELL_TYPE_EMBEDDED_WINDOW,
                                              ShellEmbeddedWindowPrivate);

  /* Setting the resize mode to immediate means that calling queue_resize()
   * on a widget within the window will immmediately call check_resize()
   * to be called, instead of having it queued to an idle. From our perspective,
   * this is ideal since we just are going to queue a resize to Clutter's
   * idle resize anyways.
   */
  g_object_set (G_OBJECT (window),
                "resize-mode", GTK_RESIZE_IMMEDIATE,
                "type", GTK_WINDOW_POPUP,
                NULL);
}

/*
 * Private routines called by ShellGtkEmbed
 */

void
_shell_embedded_window_set_actor (ShellEmbeddedWindow  *window,
                                  ShellGtkEmbed        *actor)

{
  g_return_if_fail (SHELL_IS_EMBEDDED_WINDOW (window));

  window->priv->actor = actor;

  if (GTK_WIDGET_VISIBLE (window) && CLUTTER_ACTOR_IS_REALIZED (actor))
    gtk_widget_map (GTK_WIDGET (window));
}

void
_shell_embedded_window_allocate (ShellEmbeddedWindow *window,
                                 int                  x,
                                 int                  y,
                                 int                  width,
                                 int                  height)
{
  GtkAllocation allocation;

  g_return_if_fail (SHELL_IS_EMBEDDED_WINDOW (window));

  window->priv->position.x = x;
  window->priv->position.y = y;
  window->priv->position.width = width;
  window->priv->position.height = height;

  if (GTK_WIDGET_REALIZED (window))
    gdk_window_move_resize (GTK_WIDGET (window)->window,
                            x, y, width, height);

  allocation.x = 0;
  allocation.y = 0;
  allocation.width = width;
  allocation.height = height;

  gtk_widget_size_allocate (GTK_WIDGET (window), &allocation);
}

void
_shell_embedded_window_realize (ShellEmbeddedWindow *window)
{
  g_return_if_fail (SHELL_IS_EMBEDDED_WINDOW (window));

  if (GTK_WIDGET_VISIBLE (window))
    gtk_widget_map (GTK_WIDGET (window));
}

void
_shell_embedded_window_unrealize (ShellEmbeddedWindow *window)
{
  g_return_if_fail (SHELL_IS_EMBEDDED_WINDOW (window));

  gtk_widget_unmap (GTK_WIDGET (window));
}

/*
 * Public API
 */
GtkWidget *
shell_embedded_window_new (void)
{
  return g_object_new (SHELL_TYPE_EMBEDDED_WINDOW,
                       NULL);
}
