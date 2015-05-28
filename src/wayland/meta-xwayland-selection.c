/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* The file is loosely based on xwayland/selection.c from Weston */

#include "config.h"

#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glib-unix.h>
#include <gio/gunixoutputstream.h>
#include <gio/gunixinputstream.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <meta/errors.h>
#include "meta-xwayland-private.h"
#include "meta-wayland-data-device.h"

#define INCR_CHUNK_SIZE (128 * 1024)

typedef struct {
  MetaXWaylandSelection *selection_data;
  GInputStream *stream;
  GCancellable *cancellable;
  MetaWindow *window;
  XSelectionRequestEvent request_event;
  guchar buffer[INCR_CHUNK_SIZE];
  gsize buffer_len;
  guint incr : 1;
} WaylandSelectionData;

typedef struct {
  MetaXWaylandSelection *selection_data;
  GOutputStream *stream;
  GCancellable *cancellable;
  gchar *mime_type;
  guint incr : 1;
} X11SelectionData;

typedef struct {
  Atom selection_atom;
  Window window;
  Window owner;
  Time timestamp;
  const MetaWaylandDataSource *source;
  WaylandSelectionData *wayland_selection;
  X11SelectionData *x11_selection;

  struct wl_listener ownership_listener;
} MetaSelectionBridge;

struct _MetaXWaylandSelection {
  MetaSelectionBridge clipboard;
};

static MetaSelectionBridge *
atom_to_selection_bridge (MetaWaylandCompositor *compositor,
                          Atom                   selection_atom)
{
  MetaXWaylandSelection *selection_data = compositor->xwayland_manager.selection_data;

  if (selection_atom == selection_data->clipboard.selection_atom)
    return &selection_data->clipboard;
  else
    return NULL;
}

static X11SelectionData *
x11_selection_data_new (MetaXWaylandSelection *selection_data,
                        int                    fd,
                        const char            *mime_type)
{
  X11SelectionData *data;

  data = g_slice_new0 (X11SelectionData);
  data->selection_data = selection_data;
  data->stream = g_unix_output_stream_new (fd, TRUE);
  data->cancellable = g_cancellable_new ();
  data->mime_type = g_strdup (mime_type);

  return data;
}

static void
x11_selection_data_free (X11SelectionData *data)
{
  g_cancellable_cancel (data->cancellable);
  g_object_unref (data->cancellable);
  g_object_unref (data->stream);
  g_free (data->mime_type);
  g_slice_free (X11SelectionData, data);
}

static void
x11_data_write_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  MetaSelectionBridge *selection = user_data;
  X11SelectionData *data = selection->x11_selection;
  GError *error = NULL;

  g_output_stream_write_finish (G_OUTPUT_STREAM (object), res, &error);

  if (data->incr)
    {
      Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
      XDeleteProperty (xdisplay, selection->window,
                       gdk_x11_get_xatom_by_name ("_META_SELECTION"));
    }

  if (error)
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error writing from X11 selection: %s\n", error->message);

      g_error_free (error);
    }

  if (!data->incr)
    {
      g_clear_pointer (&selection->x11_selection,
                       (GDestroyNotify) x11_selection_data_free);
    }
}

static void
x11_selection_data_write (MetaSelectionBridge *selection,
                          guchar              *buffer,
                          gulong               len)
{
  X11SelectionData *data = selection->x11_selection;

  g_output_stream_write_async (data->stream, buffer, len,
                               G_PRIORITY_DEFAULT, data->cancellable,
                               x11_data_write_cb, selection);
}

static MetaWaylandDataSource *
data_device_get_active_source_for_atom (MetaWaylandDataDevice *data_device,
                                        Atom                   selection_atom)
{
  if (selection_atom == gdk_x11_get_xatom_by_name ("CLIPBOARD"))
    return data_device->selection_data_source;
  else
    return NULL;
}

static WaylandSelectionData *
wayland_selection_data_new (XSelectionRequestEvent *request_event,
                            MetaWaylandCompositor  *compositor)
{
  MetaWaylandDataDevice *data_device;
  MetaWaylandDataSource *wayland_source;
  MetaSelectionBridge *selection;
  WaylandSelectionData *data;
  const gchar *mime_type;
  GError *error = NULL;
  int p[2];

  selection = atom_to_selection_bridge (compositor, request_event->selection);

  if (!selection)
    return NULL;

  if (!g_unix_open_pipe (p, FD_CLOEXEC, &error))
    {
      g_critical ("Failed to open pipe: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  data_device = &compositor->seat->data_device;
  mime_type = gdk_x11_get_xatom_name (request_event->target);

  if (!g_unix_set_fd_nonblocking (p[0], TRUE, &error) ||
      !g_unix_set_fd_nonblocking (p[1], TRUE, &error))
    {
      if (error)
        {
          g_critical ("Failed to make fds non-blocking: %s\n", error->message);
          g_error_free (error);
        }

      close (p[0]);
      close (p[1]);
      return NULL;
    }

  wayland_source = data_device_get_active_source_for_atom (data_device,
                                                           selection->selection_atom),
  meta_wayland_data_source_send (wayland_source, mime_type, p[1]);

  data = g_slice_new0 (WaylandSelectionData);
  data->request_event = *request_event;
  data->cancellable = g_cancellable_new ();
  data->stream = g_unix_input_stream_new (p[0], TRUE);

  data->window = meta_display_lookup_x_window (meta_get_display (),
                                               data->request_event.requestor);

  if (!data->window)
    {
      /* Not a managed window, set the PropertyChangeMask
       * for INCR deletion notifications.
       */
      XSelectInput (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                    data->request_event.requestor, PropertyChangeMask);
    }

  return data;
}

static void
reply_selection_request (XSelectionRequestEvent *request_event,
                         gboolean                accepted)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  XSelectionEvent event;

  memset(&event, 0, sizeof (XSelectionEvent));
  event.type = SelectionNotify;
  event.time = request_event->time;
  event.requestor = request_event->requestor;
  event.selection = request_event->selection;
  event.target = request_event->target;
  event.property = accepted ? request_event->property : None;

  XSendEvent (xdisplay, request_event->requestor,
              False, NoEventMask, (XEvent *) &event);
}

static void
wayland_selection_data_free (WaylandSelectionData *data)
{
  if (!data->window)
    {
      MetaDisplay *display = meta_get_display ();

      meta_error_trap_push (display);
      XSelectInput (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                    data->request_event.requestor, NoEventMask);
      meta_error_trap_pop (display);
    }

  g_cancellable_cancel (data->cancellable);
  g_object_unref (data->cancellable);
  g_object_unref (data->stream);
  g_slice_free (WaylandSelectionData, data);
}

static void
wayland_selection_update_x11_property (WaylandSelectionData *data)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  XChangeProperty (xdisplay,
                   data->request_event.requestor,
                   data->request_event.property,
                   data->request_event.target,
                   8, PropModeReplace,
                   data->buffer, data->buffer_len);
  data->buffer_len = 0;
}

static void
wayland_data_read_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  MetaSelectionBridge *selection = user_data;
  WaylandSelectionData *data = selection->wayland_selection;
  GError *error = NULL;
  gsize bytes_read;

  bytes_read = g_input_stream_read_finish (G_INPUT_STREAM (object),
                                           res, &error);
  if (error)
    {
      g_warning ("Error transfering wayland clipboard to X11: %s\n",
                 error->message);
      g_error_free (error);

      if (data)
        {
          reply_selection_request (&data->request_event, FALSE);
          g_clear_pointer (&selection->wayland_selection,
                           (GDestroyNotify) wayland_selection_data_free);
        }

      return;
    }

  data->buffer_len = bytes_read;

  if (bytes_read == INCR_CHUNK_SIZE)
    {
      if (!data->incr)
        {
          Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
          guint32 incr_chunk_size = INCR_CHUNK_SIZE;

          /* Not yet in incr */
          data->incr = TRUE;
          XChangeProperty (xdisplay,
                           data->request_event.requestor,
                           data->request_event.property,
                           gdk_x11_get_xatom_by_name ("INCR"),
                           32, PropModeReplace,
                           (guchar *) &incr_chunk_size, 1);
          reply_selection_request (&data->request_event, TRUE);
        }
      else
        wayland_selection_update_x11_property (data);
    }
  else
    {
      if (!data->incr)
        {
          /* Non-incr transfer finished */
          wayland_selection_update_x11_property (data);
          reply_selection_request (&data->request_event, TRUE);
        }
      else if (data->incr)
        {
          /* Incr transfer complete, setting a new property */
          wayland_selection_update_x11_property (data);

          if (bytes_read > 0)
            return;
        }

      g_clear_pointer (&selection->wayland_selection,
                       (GDestroyNotify) wayland_selection_data_free);
    }
}

static void
wayland_selection_data_read (MetaSelectionBridge *selection)
{
  WaylandSelectionData *data = selection->wayland_selection;

  g_input_stream_read_async (data->stream, data->buffer,
                             INCR_CHUNK_SIZE, G_PRIORITY_DEFAULT,
                             data->cancellable,
                             wayland_data_read_cb, selection);
}

static void
meta_xwayland_selection_get_incr_chunk (MetaWaylandCompositor *compositor,
                                        MetaSelectionBridge   *selection)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  gulong nitems_ret, bytes_after_ret;
  guchar *prop_ret;
  int format_ret;
  Atom type_ret;

  XGetWindowProperty (xdisplay,
                      selection->window,
                      gdk_x11_get_xatom_by_name ("_META_SELECTION"),
                      0, /* offset */
                      0x1fffffff, /* length */
                      False, /* delete */
                      AnyPropertyType,
                      &type_ret,
                      &format_ret,
                      &nitems_ret,
                      &bytes_after_ret,
                      &prop_ret);

  if (nitems_ret > 0)
    {
      x11_selection_data_write (selection, prop_ret, nitems_ret);
    }
  else
    {
      /* Transfer has completed */
      g_clear_pointer (&selection->x11_selection,
                       (GDestroyNotify) x11_selection_data_free);
    }

  XFree (prop_ret);
}

static void
meta_x11_source_send (MetaWaylandDataSource *source,
                      const gchar           *mime_type,
                      gint                   fd)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  MetaSelectionBridge *selection = source->user_data;
  Atom type_atom;

  if (strcmp (mime_type, "text/plain;charset=utf-8") == 0)
    type_atom = gdk_x11_get_xatom_by_name ("UTF8_STRING");
  else
    type_atom = gdk_x11_get_xatom_by_name (mime_type);

  g_clear_pointer (&selection->x11_selection,
                   (GDestroyNotify) x11_selection_data_free);

  /* Takes ownership of fd */
  selection->x11_selection =
    x11_selection_data_new (compositor->xwayland_manager.selection_data,
                            fd, mime_type);

  XConvertSelection (xdisplay,
                     selection->selection_atom, type_atom,
                     gdk_x11_get_xatom_by_name ("_META_SELECTION"),
                     selection->window,
                     CurrentTime);
  XFlush (xdisplay);
}

static void
meta_x11_source_target (MetaWaylandDataSource *source,
                        const gchar           *mime_type)
{
}

static void
meta_x11_source_cancel (MetaWaylandDataSource *source)
{
  MetaSelectionBridge *selection = source->user_data;

  g_clear_pointer (&selection->x11_selection,
                   (GDestroyNotify) x11_selection_data_free);
}

static const MetaWaylandDataSourceFuncs meta_x11_source_funcs = {
  meta_x11_source_send,
  meta_x11_source_target,
  meta_x11_source_cancel
};

static gboolean
meta_xwayland_data_source_fetch_mimetype_list (MetaWaylandDataSource *source,
                                               Window                 window,
                                               Atom                   prop)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  gulong nitems_ret, bytes_after_ret, i;
  Atom *atoms, type_ret, utf8_string;
  int format_ret;

  utf8_string = gdk_x11_get_xatom_by_name ("UTF8_STRING");
  XGetWindowProperty (xdisplay, window, prop,
                      0, /* offset */
                      0x1fffffff, /* length */
                      True, /* delete */
                      AnyPropertyType,
                      &type_ret,
                      &format_ret,
                      &nitems_ret,
                      &bytes_after_ret,
                      (guchar **) &atoms);

  if (nitems_ret == 0 || type_ret != XA_ATOM)
    {
      XFree (atoms);
      return FALSE;
    }

  for (i = 0; i < nitems_ret; i++)
    {
      const gchar *mime_type;

      if (atoms[i] == utf8_string)
        mime_type = "text/plain;charset=utf-8";
      else
        mime_type = gdk_x11_get_xatom_name (atoms[i]);

      meta_wayland_data_source_add_mime_type (source, mime_type);
    }

  XFree (atoms);

  return TRUE;
}

static void
meta_xwayland_selection_get_x11_targets (MetaWaylandCompositor *compositor,
                                         MetaSelectionBridge   *selection)
{
  MetaWaylandDataSource *data_source;

  data_source = meta_wayland_data_source_new (&meta_x11_source_funcs,
                                              NULL, selection);

  if (meta_xwayland_data_source_fetch_mimetype_list (data_source,
                                                     selection->window,
                                                     gdk_x11_get_xatom_by_name ("_META_SELECTION")))
    {
      selection->source = data_source;

      if (selection->selection_atom == gdk_x11_get_xatom_by_name ("CLIPBOARD"))
        {
          meta_wayland_data_device_set_selection (&compositor->seat->data_device, data_source,
                                                  wl_display_next_serial (compositor->wayland_display));
        }
    }
  else
    {
      meta_wayland_data_source_free (data_source);
    }
}

static void
meta_xwayland_selection_get_x11_data (MetaWaylandCompositor *compositor,
                                      MetaSelectionBridge   *selection)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  gulong nitems_ret, bytes_after_ret;
  guchar *prop_ret;
  int format_ret;
  Atom type_ret;

  if (!selection->x11_selection)
    return;

  XGetWindowProperty (xdisplay,
                      selection->window,
                      gdk_x11_get_xatom_by_name ("_META_SELECTION"),
                      0, /* offset */
                      0x1fffffff, /* length */
                      True, /* delete */
                      AnyPropertyType,
                      &type_ret,
                      &format_ret,
                      &nitems_ret,
                      &bytes_after_ret,
                      &prop_ret);

  selection->x11_selection->incr = (type_ret == gdk_x11_get_xatom_by_name ("INCR"));

  if (selection->x11_selection->incr)
    return;

  if (type_ret == gdk_x11_get_xatom_by_name (selection->x11_selection->mime_type))
    x11_selection_data_write (selection, prop_ret, nitems_ret);

  XFree (prop_ret);
}

static gboolean
meta_xwayland_selection_handle_selection_notify (MetaWaylandCompositor *compositor,
                                                 XEvent                *xevent)
{
  XSelectionEvent *event = (XSelectionEvent *) xevent;
  MetaSelectionBridge *selection;

  selection = atom_to_selection_bridge (compositor, event->selection);

  if (!selection)
    return FALSE;

  /* convert selection failed */
  if (event->property == None)
    {
      g_clear_pointer (&selection->x11_selection,
                       (GDestroyNotify) x11_selection_data_free);
      return FALSE;
    }

  if (event->target == gdk_x11_get_xatom_by_name ("TARGETS"))
    meta_xwayland_selection_get_x11_targets (compositor, selection);
  else
    meta_xwayland_selection_get_x11_data (compositor, selection);

  return TRUE;
}

static void
meta_xwayland_selection_send_targets (MetaWaylandCompositor       *compositor,
                                      const MetaWaylandDataSource *data_source,
                                      Window                       requestor,
                                      Atom                         property)
{
  Atom *targets;
  gchar **p;
  int i = 0;

  if (!data_source)
    return;

  if (data_source->mime_types.size == 0)
    return;

  /* Make extra room for TIMESTAMP/TARGETS */
  targets = g_new (Atom, data_source->mime_types.size + 2);

  wl_array_for_each (p, &data_source->mime_types)
    {
      targets[i++] = gdk_x11_get_xatom_by_name (*p);
    }

  targets[i++] = gdk_x11_get_xatom_by_name ("TIMESTAMP");
  targets[i++] = gdk_x11_get_xatom_by_name ("TARGETS");

  XChangeProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                   requestor, property,
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *) targets, i);

  g_free (targets);
}

static void
meta_xwayland_selection_send_timestamp (MetaWaylandCompositor *compositor,
                                        Window                 requestor,
                                        Atom                   property,
                                        Time                   timestamp)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  XChangeProperty (xdisplay, requestor, property,
                   XA_INTEGER, 32,
                   PropModeReplace,
                   (guchar *) &timestamp, 1);
}

static void
meta_xwayland_selection_send_incr_chunk (MetaWaylandCompositor *compositor,
                                         MetaSelectionBridge   *selection)
{
  if (!selection->wayland_selection)
    return;

  if (selection->wayland_selection->buffer_len > 0)
    wayland_selection_update_x11_property (selection->wayland_selection);
  else
    wayland_selection_data_read (selection);
}

static gboolean
handle_incr_chunk (MetaWaylandCompositor *compositor,
                   MetaSelectionBridge   *selection,
                   XPropertyEvent        *event)
{
  if (selection->x11_selection &&
      selection->x11_selection->incr &&
      event->window == selection->owner &&
      event->state == PropertyNewValue &&
      event->atom == gdk_x11_get_xatom_by_name ("_META_SELECTION"))
    {
      /* X11 to Wayland */
      meta_xwayland_selection_get_incr_chunk (compositor, selection);
      return TRUE;
    }
  else if (selection->wayland_selection &&
           selection->wayland_selection->incr &&
           event->window == selection->window &&
           event->state == PropertyDelete &&
           event->atom == selection->wayland_selection->request_event.property)
    {
      /* Wayland to X11 */
      meta_xwayland_selection_send_incr_chunk (compositor, selection);
      return TRUE;
    }

  return FALSE;
}

static gboolean
meta_xwayland_selection_handle_property_notify (MetaWaylandCompositor *compositor,
                                                XEvent                *xevent)
{
  MetaXWaylandSelection *selection_data = compositor->xwayland_manager.selection_data;
  XPropertyEvent *event = (XPropertyEvent *) xevent;

  return handle_incr_chunk (compositor, &selection_data->clipboard, event);
}

static gboolean
meta_xwayland_selection_handle_selection_request (MetaWaylandCompositor *compositor,
                                                  XEvent                *xevent)
{
  XSelectionRequestEvent *event = (XSelectionRequestEvent *) xevent;
  MetaWaylandDataSource *data_source;
  MetaSelectionBridge *selection;

  selection = atom_to_selection_bridge (compositor, event->selection);

  if (!selection)
    return FALSE;

  /* We must fetch from the currently active source, not the Xwayland one */
  data_source = data_device_get_active_source_for_atom (&compositor->seat->data_device,
                                                        selection->selection_atom);
  if (!data_source)
    return FALSE;

  g_clear_pointer (&selection->wayland_selection,
                   (GDestroyNotify) wayland_selection_data_free);

  if (event->target == gdk_x11_get_xatom_by_name ("TARGETS"))
    {
      meta_xwayland_selection_send_targets (compositor,
                                            data_source,
                                            event->requestor,
                                            event->property);
      reply_selection_request (event, TRUE);
    }
  else if (event->target == gdk_x11_get_xatom_by_name ("TIMESTAMP"))
    {
      meta_xwayland_selection_send_timestamp (compositor,
                                              event->requestor, event->property,
                                              selection->timestamp);
      reply_selection_request (event, TRUE);
    }
  else
    {
      if (data_source &&
          meta_wayland_data_source_has_mime_type (data_source,
                                                  gdk_x11_get_xatom_name (event->target)))
        {
          selection->wayland_selection = wayland_selection_data_new (event,
                                                                     compositor);

          if (selection->wayland_selection)
            wayland_selection_data_read (selection);
        }

      if (!selection->wayland_selection)
        reply_selection_request (event, FALSE);
    }

  return TRUE;
}

static gboolean
meta_xwayland_selection_handle_xfixes_selection_notify (MetaWaylandCompositor *compositor,
                                                        XEvent                *xevent)
{
  XFixesSelectionNotifyEvent *event = (XFixesSelectionNotifyEvent *) xevent;
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  MetaSelectionBridge *selection;

  selection = atom_to_selection_bridge (compositor, event->selection);

  if (!selection)
    return FALSE;

  if (selection->selection_atom == gdk_x11_get_xatom_by_name ("CLIPBOARD"))
    {
      if (event->owner == None)
        {
          if (selection->source && selection->owner != selection->window)
            {
              /* An X client went away, clear the selection */
              meta_wayland_data_device_set_selection (&compositor->seat->data_device, NULL,
                                                      wl_display_next_serial (compositor->wayland_display));
              selection->source = NULL;
            }

          selection->owner = None;
        }
      else
        {
          selection->owner = event->owner;

          if (selection->owner == selection->window)
            {
              /* This our own selection window */
              selection->timestamp = event->timestamp;
              return TRUE;
            }

          g_clear_pointer (&selection->x11_selection,
                           (GDestroyNotify) x11_selection_data_free);

          XConvertSelection (xdisplay,
                             event->selection,
                             gdk_x11_get_xatom_by_name ("TARGETS"),
                             gdk_x11_get_xatom_by_name ("_META_SELECTION"),
                             selection->window,
                             selection->timestamp);
          XFlush (xdisplay);
        }
    }

  return TRUE;
}

gboolean
meta_xwayland_selection_handle_event (XEvent *xevent)
{
  MetaWaylandCompositor *compositor;

  compositor = meta_wayland_compositor_get_default ();

  if (!compositor->xwayland_manager.selection_data)
    return FALSE;

  switch (xevent->type)
    {
    case SelectionNotify:
      return meta_xwayland_selection_handle_selection_notify (compositor, xevent);
    case PropertyNotify:
      return meta_xwayland_selection_handle_property_notify (compositor, xevent);
    case SelectionRequest:
      return meta_xwayland_selection_handle_selection_request (compositor, xevent);
    default:
      {
        MetaDisplay *display = meta_get_display ();

        if (xevent->type - display->xfixes_event_base == XFixesSelectionNotify)
          return meta_xwayland_selection_handle_xfixes_selection_notify (compositor, xevent);

        return FALSE;
      }
    }
}

static void
meta_selection_bridge_ownership_notify (struct wl_listener *listener,
                                        void               *data)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  MetaSelectionBridge *selection =
    wl_container_of (listener, selection, ownership_listener);
  MetaWaylandDataSource *owner = data;

  if (!owner && selection->window == selection->owner)
    {
      XSetSelectionOwner (xdisplay, selection->selection_atom,
                          None, selection->timestamp);
    }
  else if (owner && selection->source != owner)
    {
      XSetSelectionOwner (xdisplay,
                          selection->selection_atom,
                          selection->window,
                          CurrentTime);
    }
}

static void
init_selection_bridge (MetaSelectionBridge *selection,
                       Atom                 selection_atom,
                       struct wl_signal    *signal)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  XSetWindowAttributes attributes;
  guint mask;

  attributes.event_mask = PropertyChangeMask;

  selection->ownership_listener.notify = meta_selection_bridge_ownership_notify;
  wl_signal_add (signal, &selection->ownership_listener);

  selection->selection_atom = selection_atom;
  selection->window =
    XCreateWindow (xdisplay,
                   gdk_x11_window_get_xid (gdk_get_default_root_window ()),
                   -1, -1, 1, 1, /* position */
                   0, /* border width */
                   0, /* depth */
                   InputOnly, /* class */
                   CopyFromParent, /* visual */
                   CWEventMask,
                   &attributes);

  mask = XFixesSetSelectionOwnerNotifyMask |
    XFixesSelectionWindowDestroyNotifyMask |
    XFixesSelectionClientCloseNotifyMask;

  XFixesSelectSelectionInput (xdisplay, selection->window,
                              selection_atom, mask);
}

static void
shutdown_selection_bridge (MetaSelectionBridge *selection)
{
  wl_list_remove (&selection->ownership_listener.link);

  XDestroyWindow (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                  selection->window);
  g_clear_pointer (&selection->wayland_selection,
                   (GDestroyNotify) wayland_selection_data_free);
  g_clear_pointer (&selection->x11_selection,
                   (GDestroyNotify) x11_selection_data_free);
}

void
meta_xwayland_init_selection (void)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaXWaylandManager *manager = &compositor->xwayland_manager;

  g_assert (manager->selection_data == NULL);

  manager->selection_data = g_slice_new0 (MetaXWaylandSelection);

  init_selection_bridge (&manager->selection_data->clipboard,
                         gdk_x11_get_xatom_by_name ("CLIPBOARD"),
                         &compositor->seat->data_device.selection_ownership_signal);
}

void
meta_xwayland_shutdown_selection (void)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaXWaylandManager *manager = &compositor->xwayland_manager;
  MetaXWaylandSelection *selection = manager->selection_data;

  g_assert (selection != NULL);

  if (selection->clipboard.source)
    {
      meta_wayland_data_device_set_selection (&compositor->seat->data_device, NULL,
                                              wl_display_next_serial (compositor->wayland_display));
    }

  shutdown_selection_bridge (&selection->clipboard);

  g_slice_free (MetaXWaylandSelection, selection);
  manager->selection_data = NULL;
}
