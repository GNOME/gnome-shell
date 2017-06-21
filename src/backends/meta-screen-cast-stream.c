/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat Inc.
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
 *
 */

#include "config.h"

#include "backends/meta-screen-cast-stream.h"

#define META_SCREEN_CAST_STREAM_DBUS_PATH "/org/gnome/Mutter/ScreenCast/Stream"

enum
{
  PROP_0,

  PROP_CONNECTION,
};

enum
{
  CLOSED,

  N_SIGNALS
};

static guint signals[N_SIGNALS];

typedef struct _MetaScreenCastStreamPrivate
{
  GDBusConnection *connection;
  char *object_path;

  MetaScreenCastStreamSrc *src;
} MetaScreenCastStreamPrivate;

static void
meta_screen_cast_stream_init_initable_iface (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaScreenCastStream,
                         meta_screen_cast_stream,
                         META_DBUS_TYPE_SCREEN_CAST_STREAM_SKELETON,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                meta_screen_cast_stream_init_initable_iface)
                         G_ADD_PRIVATE (MetaScreenCastStream))

static MetaScreenCastStreamSrc *
meta_screen_cast_stream_create_src (MetaScreenCastStream  *stream,
                                    const char            *stream_id,
                                    GError               **error)
{
  return META_SCREEN_CAST_STREAM_GET_CLASS (stream)->create_src (stream,
                                                                 stream_id,
                                                                 error);
}

gboolean
meta_screen_cast_stream_start (MetaScreenCastStream  *stream,
                               GError               **error)
{
  MetaDBusScreenCastStream *skeleton = META_DBUS_SCREEN_CAST_STREAM (stream);
  MetaScreenCastStreamPrivate *priv =
    meta_screen_cast_stream_get_instance_private (stream);
  g_autofree char *stream_id = NULL;
  MetaScreenCastStreamSrc *src;
  static unsigned int global_stream_id = 0;

  stream_id = g_strdup_printf ("%u", ++global_stream_id);
  src = meta_screen_cast_stream_create_src (stream, stream_id, error);
  if (!src)
    return FALSE;

  priv->src = src;

  meta_dbus_screen_cast_stream_emit_pipewire_stream_added (skeleton, stream_id);

  return TRUE;
}

void
meta_screen_cast_stream_close (MetaScreenCastStream *stream)
{
  MetaScreenCastStreamPrivate *priv =
    meta_screen_cast_stream_get_instance_private (stream);

  g_clear_object (&priv->src);

  g_signal_emit (stream, signals[CLOSED], 0);
}

char *
meta_screen_cast_stream_get_object_path (MetaScreenCastStream *stream)
{
  MetaScreenCastStreamPrivate *priv =
    meta_screen_cast_stream_get_instance_private (stream);

  return priv->object_path;
}

static void
meta_screen_cast_stream_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  MetaScreenCastStream *stream = META_SCREEN_CAST_STREAM (object);
  MetaScreenCastStreamPrivate *priv =
    meta_screen_cast_stream_get_instance_private (stream);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      priv->connection = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_screen_cast_stream_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  MetaScreenCastStream *stream = META_SCREEN_CAST_STREAM (object);
  MetaScreenCastStreamPrivate *priv =
    meta_screen_cast_stream_get_instance_private (stream);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, priv->connection);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_screen_cast_stream_finalize (GObject *object)
{
  MetaScreenCastStream *stream = META_SCREEN_CAST_STREAM (object);
  MetaScreenCastStreamPrivate *priv =
    meta_screen_cast_stream_get_instance_private (stream);

  if (priv->src)
    meta_screen_cast_stream_close (stream);

  g_clear_pointer (&priv->object_path, g_free);

  G_OBJECT_CLASS (meta_screen_cast_stream_parent_class)->finalize (object);
}

static gboolean
meta_screen_cast_stream_initable_init (GInitable     *initable,
                                       GCancellable  *cancellable,
                                       GError       **error)
{
  MetaScreenCastStream *stream = META_SCREEN_CAST_STREAM (initable);
  MetaScreenCastStreamPrivate *priv =
    meta_screen_cast_stream_get_instance_private (stream);
  static unsigned int global_stream_number = 0;

  priv->object_path =
    g_strdup_printf (META_SCREEN_CAST_STREAM_DBUS_PATH "/u%u",
                     ++global_stream_number);
  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (stream),
                                         priv->connection,
                                         priv->object_path,
                                         error))
    return FALSE;

  return TRUE;
}

static void
meta_screen_cast_stream_init_initable_iface (GInitableIface *iface)
{
  iface->init = meta_screen_cast_stream_initable_init;
}

static void
meta_screen_cast_stream_init (MetaScreenCastStream *stream)
{
}

static void
meta_screen_cast_stream_class_init (MetaScreenCastStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_screen_cast_stream_finalize;
  object_class->set_property = meta_screen_cast_stream_set_property;
  object_class->get_property = meta_screen_cast_stream_get_property;

  g_object_class_install_property (object_class,
                                   PROP_CONNECTION,
                                   g_param_spec_object ("connection",
                                                        "connection",
                                                        "GDBus connection",
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  signals[CLOSED] = g_signal_new ("closed",
                                  G_TYPE_FROM_CLASS (klass),
                                  G_SIGNAL_RUN_LAST,
                                  0,
                                  NULL, NULL, NULL,
                                  G_TYPE_NONE, 0);
}
