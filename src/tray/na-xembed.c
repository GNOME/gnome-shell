/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2022 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 *
 * Based on GTK+ code by Owen Taylor <otaylor@gtk.org>
 */

#include "config.h"

#include "na-xembed.h"

#include <meta/meta-x11-errors.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct _NaXembedPrivate NaXembedPrivate;

struct _NaXembedPrivate
{
  MetaX11Display *x11_display;
  Window socket_window;
  Window plug_window;

  int root_x;
  int root_y;
  int request_width;
  int request_height;
  int current_width;
  int current_height;
  int resize_count;
  int xembed_version;

  unsigned int event_func_id;
  guint resize_id;

  XVisualInfo *xvisual_info;

  Atom atom__XEMBED;
  Atom atom__XEMBED_INFO;
  Atom atom_WM_NORMAL_HINTS;

  gboolean have_size;
  gboolean need_map;
  gboolean is_mapped;
  gboolean has_alpha;
};

/* XEMBED messages */
typedef enum _XembedMessageType XembedMessageType;

enum _XembedMessageType {
  XEMBED_EMBEDDED_NOTIFY,
  XEMBED_WINDOW_ACTIVATE,
  XEMBED_WINDOW_DEACTIVATE,
  XEMBED_REQUEST_FOCUS,
  XEMBED_FOCUS_IN,
  XEMBED_FOCUS_OUT,
  XEMBED_FOCUS_NEXT,
  XEMBED_FOCUS_PREV,
  XEMBED_GRAB_KEY,
  XEMBED_UNGRAB_KEY,
  XEMBED_MODALITY_ON,
  XEMBED_MODALITY_OFF,
};

/* Flags for _XEMBED_INFO */
#define XEMBED_MAPPED (1 << 0)

#define XEMBED_PROTOCOL_VERSION 1

G_DEFINE_TYPE_WITH_PRIVATE (NaXembed, na_xembed, G_TYPE_OBJECT)

enum {
  PLUG_ADDED,
  PLUG_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

enum {
  PROP_0,
  PROP_X11_DISPLAY,
  N_PROPS
};

static GParamSpec *props[N_PROPS] = { 0, };

static void
xembed_send_message (NaXembed          *xembed,
                     Window             recipient,
                     XembedMessageType  message,
                     glong              detail,
                     glong              data1,
                     glong              data2)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);
  XClientMessageEvent xclient;

  memset (&xclient, 0, sizeof (xclient));
  xclient.window = recipient;
  xclient.type = ClientMessage;
  xclient.message_type = priv->atom__XEMBED;
  xclient.format = 32;
  xclient.data.l[0] = 0; /* Time */
  xclient.data.l[1] = message;
  xclient.data.l[2] = detail;
  xclient.data.l[3] = data1;
  xclient.data.l[4] = data2;

  meta_x11_error_trap_push (priv->x11_display);
  XSendEvent (meta_x11_display_get_xdisplay (priv->x11_display),
              recipient,
              False, NoEventMask, (XEvent*) &xclient);
  meta_x11_error_trap_pop (priv->x11_display);
}

static void
na_xembed_end_embedding (NaXembed *xembed)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  priv->plug_window = None;
  priv->current_width = 0;
  priv->current_height = 0;
  priv->resize_count = 0;
  g_clear_handle_id (&priv->resize_id, g_source_remove);
}

static void
na_xembed_send_configure_event (NaXembed *xembed)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);
  XConfigureEvent xconfigure;

  memset (&xconfigure, 0, sizeof (xconfigure));
  xconfigure.type = ConfigureNotify;

  xconfigure.event = priv->plug_window;
  xconfigure.window = priv->plug_window;

  xconfigure.x = priv->root_x;
  xconfigure.y = priv->root_y;
  xconfigure.width = priv->current_width;
  xconfigure.height = priv->current_height;

  xconfigure.border_width = 0;
  xconfigure.above = None;
  xconfigure.override_redirect = False;

  meta_x11_error_trap_push (priv->x11_display);
  XSendEvent (meta_x11_display_get_xdisplay (priv->x11_display),
              priv->plug_window,
              False, NoEventMask,
              (XEvent*) &xconfigure);
  meta_x11_error_trap_pop (priv->x11_display);
}

static void
na_xembed_synchronize_size (NaXembed *xembed)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);
  Display *xdisplay = meta_x11_display_get_xdisplay (priv->x11_display);
  int x, y, width, height;

  x = priv->root_x;
  y = priv->root_y;
  width = priv->request_width;
  height = priv->request_height;

  XMoveResizeWindow (xdisplay,
                     priv->socket_window,
                     x, y,
                     width, height);

  if (priv->plug_window)
    {
      meta_x11_error_trap_push (priv->x11_display);

      if (width != priv->current_width ||
          height != priv->current_height)
        {
          XMoveResizeWindow (xdisplay,
                             priv->plug_window,
                             0, 0,
                             width, height);
          if (priv->resize_count)
            priv->resize_count--;

          priv->current_width = width;
          priv->current_height = height;
        }

      if (priv->need_map)
        {
          XMapWindow (xdisplay, priv->plug_window);
          priv->need_map = FALSE;
        }

      while (priv->resize_count)
        {
          na_xembed_send_configure_event (xembed);
          priv->resize_count--;
        }

      meta_x11_error_trap_pop (priv->x11_display);
    }
}

static gboolean
synchronize_size_cb (gpointer user_data)
{
  NaXembed *xembed = user_data;
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  na_xembed_synchronize_size (xembed);
  priv->resize_id = 0;

  return G_SOURCE_REMOVE;
}

static void
na_xembed_resize (NaXembed *xembed)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);
  XSizeHints hints;
  long supplied;

  g_clear_handle_id (&priv->resize_id, g_source_remove);

  meta_x11_error_trap_push (priv->x11_display);

  priv->request_width = 1;
  priv->request_height = 1;

  if (XGetWMNormalHints (meta_x11_display_get_xdisplay (priv->x11_display),
                         priv->plug_window,
                         &hints, &supplied))
    {
      if (hints.flags & PMinSize)
        {
          priv->request_width = MAX (hints.min_width, 1);
          priv->request_height = MAX (hints.min_height, 1);
        }
      else if (hints.flags & PBaseSize)
        {
          priv->request_width = MAX (hints.base_width, 1);
          priv->request_height = MAX (hints.base_height, 1);
        }
    }

  priv->have_size = TRUE;
  meta_x11_error_trap_pop (priv->x11_display);

  priv->resize_id = g_idle_add (synchronize_size_cb, xembed);
}

static gboolean
na_xembed_get_info (NaXembed      *xembed,
                    Window         xwindow,
                    unsigned long *version,
                    unsigned long *flags)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);
  MetaX11Display *x11_display = priv->x11_display;
  Display *xdisplay = meta_x11_display_get_xdisplay (x11_display);
  Atom type;
  int format;
  unsigned long nitems, bytes_after;
  unsigned char *data;
  unsigned long *data_long;
  int status;

  meta_x11_error_trap_push (x11_display);
  status = XGetWindowProperty (xdisplay,
                               xwindow,
                               priv->atom__XEMBED_INFO,
                               0, 2, False,
                               priv->atom__XEMBED_INFO,
                               &type, &format,
                               &nitems, &bytes_after, &data);
  meta_x11_error_trap_pop (x11_display);

  if (status != Success)
    return FALSE;
  if (type == None)
    return FALSE;

  if (type != priv->atom__XEMBED_INFO)
    {
      g_warning ("_XEMBED_INFO property has wrong type");
      XFree (data);
      return FALSE;
    }

  if (nitems < 2)
    {
      g_warning ("_XEMBED_INFO too short");
      XFree (data);
      return FALSE;
    }

  data_long = (unsigned long *)data;
  if (version)
    *version = data_long[0];
  if (flags)
    *flags = data_long[1] & XEMBED_MAPPED;

  XFree (data);
  return TRUE;
}

static void
na_xembed_add_window (NaXembed  *xembed,
                      Window     xid,
                      gboolean   need_reparent)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);
  Display *xdisplay = meta_x11_display_get_xdisplay (priv->x11_display);
  XVisualInfo template = { 0, };
  int n_xvisuals;
  unsigned long version;
  unsigned long flags;

  priv->plug_window = xid;

  meta_x11_error_trap_push (priv->x11_display);

  XSelectInput (xdisplay,
                priv->plug_window,
                StructureNotifyMask | PropertyChangeMask);

  if (meta_x11_error_trap_pop_with_return (priv->x11_display))
    {
      priv->plug_window = None;
      return;
    }

  /* OK, we now will reliably get destroy notification on socket->plug_window */

  meta_x11_error_trap_push (priv->x11_display);

  if (need_reparent)
    {
      XSetWindowAttributes socket_attrs;
      XWindowAttributes plug_attrs;
      int result;

      result = XGetWindowAttributes (xdisplay, priv->plug_window, &plug_attrs);
      if (result == 0)
        {
          meta_x11_error_trap_pop (priv->x11_display);
          priv->plug_window = None;
          return;
        }

      template.visualid = plug_attrs.visual->visualid;
      priv->xvisual_info =
        XGetVisualInfo (meta_x11_display_get_xdisplay (priv->x11_display),
                        VisualIDMask,
                        &template,
                        &n_xvisuals);

      if (!priv->xvisual_info)
        {
          meta_x11_error_trap_pop (priv->x11_display);
          priv->plug_window = None;
          return;
        }

      priv->has_alpha = (priv->xvisual_info->depth >
                         __builtin_popcount (priv->xvisual_info->red_mask |
                                             priv->xvisual_info->green_mask |
                                             priv->xvisual_info->blue_mask));

      socket_attrs.override_redirect = True;

      priv->socket_window =
        XCreateWindow (xdisplay,
                       meta_x11_display_get_xroot (priv->x11_display),
                       -1, -1, 1, 1, 0,
                       priv->xvisual_info->depth,
                       InputOutput,
                       plug_attrs.visual,
                       CWOverrideRedirect,
                       &socket_attrs);

      XUnmapWindow (xdisplay, priv->plug_window); /* Shouldn't actually be necessary for XEMBED, but just in case */
      XReparentWindow (xdisplay,
                       priv->plug_window,
                       priv->socket_window,
                       0, 0);
    }

  priv->have_size = FALSE;

  priv->xembed_version = -1;
  if (na_xembed_get_info (xembed, priv->plug_window, &version, &flags))
    {
      priv->xembed_version = MIN (XEMBED_PROTOCOL_VERSION, version);
      priv->is_mapped = (flags & XEMBED_MAPPED) != 0;
    }
  else
    {
      /* FIXME, we should probably actually check the state before we started */
      priv->is_mapped = TRUE;
    }

  priv->need_map = priv->is_mapped;

  meta_x11_error_trap_pop (priv->x11_display);

  meta_x11_error_trap_push (priv->x11_display);
  XFixesChangeSaveSet (xdisplay, priv->plug_window,
                       SetModeInsert, SaveSetRoot, SaveSetUnmap);
  meta_x11_error_trap_pop (priv->x11_display);

  xembed_send_message (xembed,
		       priv->plug_window,
		       XEMBED_EMBEDDED_NOTIFY, 0,
		       priv->socket_window,
		       priv->xembed_version);

  na_xembed_resize (xembed);

  g_signal_emit (xembed, signals[PLUG_ADDED], 0);

  XMapWindow (xdisplay, priv->socket_window);
}

static void
na_xembed_handle_map_request (NaXembed *xembed)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  if (!priv->is_mapped)
    {
      priv->is_mapped = TRUE;
      priv->need_map = TRUE;
      na_xembed_resize (xembed);
    }
}

static void
na_xembed_handle_unmap_notify (NaXembed *xembed)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  if (priv->is_mapped)
    {
      priv->is_mapped = FALSE;
      na_xembed_resize (xembed);
    }
}

static void
xembed_filter_func (MetaX11Display *x11_display,
                    XEvent         *xevent,
                    gpointer        user_data)
{
  NaXembed *xembed = user_data;
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);
  Display *xdisplay = meta_x11_display_get_xdisplay (priv->x11_display);

  if (priv->socket_window == None)
    return;
  if (xevent->xany.window != priv->socket_window &&
      xevent->xany.window != priv->plug_window)
    return;

  switch (xevent->type)
    {
    case ClientMessage:
      /* We choose to ignore all _XEMBED ClientEvent messages
       * addressed to the socket.
       */
      break;
    case CreateNotify:
      {
        XCreateWindowEvent *xcwe = &xevent->xcreatewindow;

        if (!priv->plug_window)
          na_xembed_add_window (xembed, xcwe->window, FALSE);

        break;
      }
    case ConfigureRequest:
      {
        XConfigureRequestEvent *xcre = &xevent->xconfigurerequest;

        if (!priv->plug_window)
          na_xembed_add_window (xembed, xcre->window, FALSE);

        if (priv->plug_window)
          {
            if (xcre->value_mask & (CWWidth | CWHeight))
              {
                priv->resize_count++;
                na_xembed_resize (xembed);
              }
            else if (xcre->value_mask & (CWX | CWY))
              {
                na_xembed_send_configure_event (xembed);
              }

            /* Ignore stacking requests. */
          }
        break;
      }
    case DestroyNotify:
      {
        XDestroyWindowEvent *xdwe = &xevent->xdestroywindow;

        /* Note that we get destroy notifies both from SubstructureNotify on
         * our window and StructureNotify on socket->plug_window
         */
        if (priv->plug_window && (xdwe->window == priv->plug_window))
          {
            g_object_ref (xembed);
            g_signal_emit (xembed, signals[PLUG_REMOVED], 0);
            na_xembed_end_embedding (xembed);
            g_object_unref (xembed);
          }
        break;
      }
    case MapRequest:
      if (!priv->plug_window)
        na_xembed_add_window (xembed, xevent->xmaprequest.window, FALSE);

      if (priv->plug_window == xevent->xmaprequest.window)
        na_xembed_handle_map_request (xembed);
      break;
    case PropertyNotify:
      if (priv->plug_window &&
          xevent->xproperty.window == priv->plug_window)
        {
          if (xevent->xproperty.atom == priv->atom_WM_NORMAL_HINTS)
            {
              priv->have_size = FALSE;
              na_xembed_resize (xembed);
            }
          else if (xevent->xproperty.atom == priv->atom__XEMBED_INFO)
            {
              unsigned long flags;

              if (na_xembed_get_info (xembed, priv->plug_window, NULL, &flags))
                {
                  gboolean was_mapped = priv->is_mapped;
                  gboolean is_mapped = (flags & XEMBED_MAPPED) != 0;

                  if (was_mapped != is_mapped)
                    {
                      if (is_mapped)
                        {
                          na_xembed_handle_map_request (xembed);
                        }
                      else
                        {
                          meta_x11_error_trap_push (priv->x11_display);
                          XMapWindow (xdisplay, priv->plug_window);
                          meta_x11_error_trap_pop (priv->x11_display);

                          na_xembed_handle_unmap_notify (xembed);
                        }
                    }
                }
            }
        }
      break;
    case ReparentNotify:
      {
        XReparentEvent *xre = &xevent->xreparent;

        if (priv->plug_window == None &&
            xre->parent == priv->socket_window)
          {
            na_xembed_add_window (xembed, xre->window, FALSE);
          }
        else if (priv->plug_window != None &&
                 xre->window == priv->plug_window &&
                 xre->parent != priv->socket_window)
          {
            g_object_ref (xembed);
            g_signal_emit (xembed, signals[PLUG_REMOVED], 0);
            na_xembed_end_embedding (xembed);
            g_object_unref (xembed);
          }

        break;
      }
    case UnmapNotify:
      if (priv->plug_window != None &&
          xevent->xunmap.window == priv->plug_window)
        na_xembed_handle_unmap_notify (xembed);
      break;
    }
}

static void
na_xembed_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  NaXembed *xembed = NA_XEMBED (object);
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  switch (prop_id)
    {
    case PROP_X11_DISPLAY:
      priv->x11_display = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
na_xembed_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  NaXembed *xembed = NA_XEMBED (object);
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  switch (prop_id)
    {
    case PROP_X11_DISPLAY:
      g_value_set_object (value, priv->x11_display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
na_xembed_finalize (GObject *object)
{
  NaXembed *xembed = NA_XEMBED (object);
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  g_clear_pointer (&priv->xvisual_info, XFree);

  if (priv->x11_display && priv->event_func_id)
    meta_x11_display_remove_event_func (priv->x11_display, priv->event_func_id);

  if (priv->plug_window)
    na_xembed_end_embedding (xembed);

  G_OBJECT_CLASS (na_xembed_parent_class)->finalize (object);
}

static void
na_xembed_constructed (GObject *object)
{
  NaXembed *xembed = NA_XEMBED (object);
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);
  Display *xdisplay;

  G_OBJECT_CLASS (na_xembed_parent_class)->constructed (object);

  xdisplay = meta_x11_display_get_xdisplay (priv->x11_display);

  priv->event_func_id =
    meta_x11_display_add_event_func (priv->x11_display,
                                     xembed_filter_func,
                                     object,
                                     NULL);

  priv->atom__XEMBED = XInternAtom (xdisplay, "_XEMBED", False);
  priv->atom__XEMBED_INFO = XInternAtom (xdisplay, "_XEMBED_INFO", False);
  priv->atom_WM_NORMAL_HINTS = XInternAtom (xdisplay, "WM_NORMAL_HINTS", False);
}

static void
na_xembed_class_init (NaXembedClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = na_xembed_set_property;
  object_class->get_property = na_xembed_get_property;
  object_class->finalize = na_xembed_finalize;
  object_class->constructed = na_xembed_constructed;

  signals[PLUG_ADDED] =
    g_signal_new ("plug-added",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (NaXembedClass, plug_added),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[PLUG_REMOVED] =
    g_signal_new ("plug-removed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (NaXembedClass, plug_removed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  props[PROP_X11_DISPLAY] =
    g_param_spec_object ("x11-display",
                         "x11-display",
                         "x11-display",
                         META_TYPE_X11_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
na_xembed_init (NaXembed *xembed)
{
}

void
na_xembed_add_id (NaXembed *xembed,
                  Window    window)
{
  na_xembed_add_window (xembed, window, TRUE);
}

Window
na_xembed_get_plug_window (NaXembed *xembed)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  return priv->plug_window;
}

Window
na_xembed_get_socket_window (NaXembed *xembed)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  return priv->socket_window;
}

void
na_xembed_set_root_position (NaXembed *xembed,
                             int       x,
                             int       y)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  if (priv->root_x == x &&
      priv->root_y == y)
    return;

  priv->root_x = x;
  priv->root_y = y;

  if (priv->resize_id == 0)
    priv->resize_id = g_idle_add (synchronize_size_cb, xembed);
}

MetaX11Display *
na_xembed_get_x11_display (NaXembed *xembed)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  return priv->x11_display;
}

void
na_xembed_get_size (NaXembed *xembed,
                    int      *width,
                    int      *height)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);

  if (width)
    *width = priv->request_width;
  if (height)
    *height = priv->request_height;
}

static void
get_pixel_details (unsigned long  pixel_mask,
                   int           *shift,
                   int           *precision)
{
  unsigned long m = 0;
  int s = 0;
  int p = 0;

  if (pixel_mask != 0)
    {
      m = pixel_mask;
      while (!(m & 0x1))
        {
          s++;
          m >>= 1;
        }

      while (m & 0x1)
        {
          p++;
          m >>= 1;
        }
    }

  if (shift)
    *shift = s;
  if (precision)
    *precision = p;
}

void
na_xembed_set_background_color (NaXembed           *xembed,
                                const ClutterColor *color)
{
  NaXembedPrivate *priv = na_xembed_get_instance_private (xembed);
  XVisualInfo *xvisual_info;
  Display *xdisplay;
  unsigned long pixel;

  if (!priv->socket_window)
    return;
  if (!priv->xvisual_info)
    return;

  xvisual_info = priv->xvisual_info;

  if (priv->has_alpha)
    {
      pixel = 0;
    }
  else
    {
      double red, green, blue;
      int red_shift, red_prec, green_shift, green_prec, blue_shift, blue_prec;
      unsigned int padding;

      /* Shifting by >= width-of-type isn't defined in C */
      if (xvisual_info->depth >= 32)
        padding = 0;
      else
        padding = ((~(uint32_t)0)) << xvisual_info->depth;

      red = (double) color->red / 255.0;
      green = (double) color->green / 255.0;
      blue = (double) color->blue / 255.0;

      get_pixel_details (xvisual_info->red_mask, &red_shift, &red_prec);
      get_pixel_details (xvisual_info->green_mask, &green_shift, &green_prec);
      get_pixel_details (xvisual_info->blue_mask, &blue_shift, &blue_prec);

      pixel = ~(xvisual_info->red_mask |
                xvisual_info->green_mask |
                xvisual_info->blue_mask |
                padding);

      pixel += (((int) (red * ((1 << red_prec) - 1))) << red_shift) +
        (((int) (green * ((1 << green_prec) - 1))) << green_shift) +
        (((int) (blue * ((1 << blue_prec) - 1))) << blue_shift);
    }

  xdisplay = meta_x11_display_get_xdisplay (priv->x11_display);

  XSetWindowBackground (xdisplay, priv->socket_window, pixel);
  XClearWindow (xdisplay, priv->socket_window);
}
