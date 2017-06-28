/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015-2017 Red Hat Inc.
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

#include "backends/meta-screen-cast-stream-src.h"

#include <errno.h>
#include <pipewire/pipewire.h>
#include <spa/format-builder.h>
#include <spa/format-utils.h>
#include <spa/props.h>
#include <spa/type-map.h>
#include <spa/video/format-utils.h>
#include <stdint.h>
#include <sys/mman.h>

#include "backends/meta-screen-cast-stream.h"
#include "clutter/clutter-mutter.h"
#include "core/meta-fraction.h"
#include "meta/boxes.h"

#define PRIVATE_OWNER_FROM_FIELD(TypeName, field_ptr, field_name) \
  (TypeName *)((guint8 *)(field_ptr) - G_PRIVATE_OFFSET (TypeName, field_name))

enum
{
  PROP_0,

  PROP_STREAM,
};

enum
{
  READY,
  CLOSED,

  N_SIGNALS
};

static guint signals[N_SIGNALS];

typedef struct _MetaSpaType
{
  uint32_t format;
  uint32_t props;
  struct spa_type_meta meta;
  struct spa_type_data data;
  struct spa_type_media_type media_type;
  struct spa_type_media_subtype media_subtype;
  struct spa_type_format_video format_video;
  struct spa_type_video_format video_format;
} MetaSpaType;

typedef struct _MetaPipeWireSource
{
  GSource base;

  struct pw_loop *pipewire_loop;
} MetaPipeWireSource;

typedef struct _MetaScreenCastStreamSrcPrivate
{
  MetaScreenCastStream *stream;

  struct pw_core *pipewire_core;
  struct pw_remote *pipewire_remote;
  struct pw_type *pipewire_type;
  MetaPipeWireSource *pipewire_source;
  struct spa_hook pipewire_remote_listener;

  gboolean is_enabled;

  struct pw_stream *pipewire_stream;
  struct spa_hook pipewire_stream_listener;

  MetaSpaType spa_type;
  uint8_t params_buffer[1024];
  struct spa_video_info_raw video_format;

  uint64_t last_frame_timestamp_us;
} MetaScreenCastStreamSrcPrivate;

static void
meta_screen_cast_stream_src_init_initable_iface (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaScreenCastStreamSrc,
                         meta_screen_cast_stream_src,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                meta_screen_cast_stream_src_init_initable_iface)
                         G_ADD_PRIVATE (MetaScreenCastStreamSrc))

#define PROP(f, key, type, ...)                                         \
          SPA_POD_PROP (f, key, 0, type, 1, __VA_ARGS__)
#define PROP_U_MM(f, key, type, ...)                                    \
          SPA_POD_PROP (f, key, (SPA_POD_PROP_FLAG_UNSET |              \
                                 SPA_POD_PROP_RANGE_MIN_MAX),           \
                        type, 3, __VA_ARGS__)

static void
meta_screen_cast_stream_src_get_specs (MetaScreenCastStreamSrc *src,
                                       int                     *width,
                                       int                     *height,
                                       float                   *frame_rate)
{
  MetaScreenCastStreamSrcClass *klass =
    META_SCREEN_CAST_STREAM_SRC_GET_CLASS (src);

  klass->get_specs (src, width, height, frame_rate);
}

static void
meta_screen_cast_stream_src_record_frame (MetaScreenCastStreamSrc *src,
                                          uint8_t                 *data)
{
  MetaScreenCastStreamSrcClass *klass =
    META_SCREEN_CAST_STREAM_SRC_GET_CLASS (src);

  klass->record_frame (src, data);
}

void
meta_screen_cast_stream_src_maybe_record_frame (MetaScreenCastStreamSrc *src)
{
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);
  uint32_t buffer_id;
  struct spa_buffer *buffer;
  uint8_t *map = NULL;
  uint8_t *data;
  uint64_t now_us;

  now_us = g_get_monotonic_time ();
  if (priv->last_frame_timestamp_us != 0 &&
      (now_us - priv->last_frame_timestamp_us <
       ((1000000 * priv->video_format.max_framerate.denom) /
        priv->video_format.max_framerate.num)))
    return;

  if (!priv->pipewire_stream)
    return;

  buffer_id = pw_stream_get_empty_buffer (priv->pipewire_stream);
  if (buffer_id == SPA_ID_INVALID)
    return;

  buffer = pw_stream_peek_buffer (priv->pipewire_stream, buffer_id);

  if (buffer->datas[0].type == priv->spa_type.data.MemFd)
    {
      map = mmap (NULL, buffer->datas[0].maxsize + buffer->datas[0].mapoffset,
                  PROT_READ | PROT_WRITE, MAP_SHARED,
                  buffer->datas[0].fd, 0);
      if (map == MAP_FAILED)
        {
          g_warning ("Failed to mmap pipewire stream buffer: %s\n",
                     strerror (errno));
          return;
        }

      data = SPA_MEMBER (map, buffer->datas[0].mapoffset, uint8_t);
    }
  else if (buffer->datas[0].type == priv->spa_type.data.MemPtr)
    {
      data = buffer->datas[0].data;
    }
  else
    {
      return;
    }

  meta_screen_cast_stream_src_record_frame (src, data);
  priv->last_frame_timestamp_us = now_us;

  if (map)
    munmap (map, buffer->datas[0].maxsize + buffer->datas[0].mapoffset);

  pw_stream_send_buffer (priv->pipewire_stream, buffer_id);
}

static gboolean
meta_screen_cast_stream_src_is_enabled (MetaScreenCastStreamSrc *src)
{
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);

  return priv->is_enabled;
}

static void
meta_screen_cast_stream_src_enable (MetaScreenCastStreamSrc *src)
{
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);

  META_SCREEN_CAST_STREAM_SRC_GET_CLASS (src)->enable (src);

  priv->is_enabled = TRUE;
}

static void
meta_screen_cast_stream_src_disable (MetaScreenCastStreamSrc *src)
{
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);

  META_SCREEN_CAST_STREAM_SRC_GET_CLASS (src)->disable (src);

  priv->is_enabled = FALSE;
}

static void
meta_screen_cast_stream_src_notify_closed (MetaScreenCastStreamSrc *src)
{
  g_signal_emit (src, signals[CLOSED], 0);
}

static void
on_stream_state_changed (void                 *data,
                         enum pw_stream_state  old,
                         enum pw_stream_state  state,
                         const char           *error_message)
{
  MetaScreenCastStreamSrc *src = data;
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);
  uint32_t node_id;

  switch (state)
    {
    case PW_STREAM_STATE_ERROR:
      g_warning ("pipewire stream error: %s", error_message);
      meta_screen_cast_stream_src_notify_closed (src);
      break;
    case PW_STREAM_STATE_CONFIGURE:
      node_id = pw_stream_get_node_id (priv->pipewire_stream);
      g_signal_emit (src, signals[READY], 0, (unsigned int) node_id);
      break;
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
    case PW_STREAM_STATE_READY:
    case PW_STREAM_STATE_PAUSED:
      if (meta_screen_cast_stream_src_is_enabled (src))
        meta_screen_cast_stream_src_disable (src);
      break;
    case PW_STREAM_STATE_STREAMING:
      if (!meta_screen_cast_stream_src_is_enabled (src))
        meta_screen_cast_stream_src_enable (src);
      break;
    }
}

static void
on_stream_format_changed (void              *data,
                          struct spa_format *format)
{
  MetaScreenCastStreamSrc *src = data;
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);
  struct pw_type *pipewire_type = priv->pipewire_type;
  struct spa_type_param_alloc_buffers *param_alloc_buffers;
  struct spa_pod_builder pod_builder = { NULL };
  struct spa_pod_frame object_frame;
  struct spa_pod_frame prop_frame;
  struct spa_param *params[1];
  const int bpp = 4;

  if (!format)
    {
      pw_stream_finish_format (priv->pipewire_stream, SPA_RESULT_OK, NULL, 0);
      return;
    }

  spa_format_video_raw_parse (format,
                              &priv->video_format,
                              &priv->spa_type.format_video);

  spa_pod_builder_init (&pod_builder,
                        priv->params_buffer,
                        sizeof (priv->params_buffer));

  param_alloc_buffers = &pipewire_type->param_alloc_buffers;
  spa_pod_builder_object (&pod_builder, &object_frame, 0,
                          param_alloc_buffers->Buffers,
                          PROP (&prop_frame, param_alloc_buffers->size,
                                SPA_POD_TYPE_INT,
                                (priv->video_format.size.width *
                                 priv->video_format.size.height *
                                 bpp)),
                          PROP (&prop_frame, param_alloc_buffers->stride,
                                SPA_POD_TYPE_INT,
                                priv->video_format.size.width * bpp),
                          PROP_U_MM (&prop_frame, param_alloc_buffers->buffers,
                                     SPA_POD_TYPE_INT,
                                     16, 2, 16),
                          PROP (&prop_frame, param_alloc_buffers->align,
                                SPA_POD_TYPE_INT,
                                16));
  params[0] = SPA_POD_BUILDER_DEREF (&pod_builder, object_frame.ref,
                                     struct spa_param);

  pw_stream_finish_format (priv->pipewire_stream, SPA_RESULT_OK,
                           params, G_N_ELEMENTS (params));
}

static const struct pw_stream_events stream_events = {
  PW_VERSION_STREAM_EVENTS,
  .state_changed = on_stream_state_changed,
  .format_changed = on_stream_format_changed,
};

static struct pw_stream *
create_pipewire_stream (MetaScreenCastStreamSrc  *src,
                        GError                  **error)
{
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);
  struct pw_stream *pipewire_stream;
  const struct spa_format *format;
  uint8_t buffer[1024];
  struct spa_pod_builder pod_builder =
    SPA_POD_BUILDER_INIT (buffer, sizeof (buffer));
  struct spa_pod_frame format_frame;
  struct spa_pod_frame prop_frame;
  MetaSpaType *spa_type = &priv->spa_type;
  int width, height;
  float frame_rate;
  MetaFraction frame_rate_fraction;

  pipewire_stream = pw_stream_new (priv->pipewire_remote,
                                   "meta-screen-cast-src",
                                   NULL);

  meta_screen_cast_stream_src_get_specs (src, &width, &height, &frame_rate);
  frame_rate_fraction = meta_fraction_from_double (frame_rate);

  spa_pod_builder_format (&pod_builder, &format_frame,
                          spa_type->format,
                          spa_type->media_type.video,
                          spa_type->media_subtype.raw,
                          PROP (&prop_frame,
                                spa_type->format_video.format,
                                SPA_POD_TYPE_ID, spa_type->video_format.BGRx),
                          PROP (&prop_frame,
                                spa_type->format_video.size,
                                SPA_POD_TYPE_RECTANGLE,
                                width, height),
                          PROP (&prop_frame,
                                spa_type->format_video.framerate,
                                SPA_POD_TYPE_FRACTION,
                                0, 1),
                          PROP_U_MM (&prop_frame,
                                     spa_type->format_video.max_framerate,
                                     SPA_POD_TYPE_FRACTION,
                                     frame_rate_fraction.num,
                                     frame_rate_fraction.denom,
                                     1, 1,
                                     frame_rate_fraction.num,
                                     frame_rate_fraction.denom));
  format = SPA_POD_BUILDER_DEREF (&pod_builder, format_frame.ref, struct spa_format);

  pw_stream_add_listener (pipewire_stream,
                          &priv->pipewire_stream_listener,
                          &stream_events,
                          src);

  if (!pw_stream_connect (pipewire_stream,
                          PW_DIRECTION_OUTPUT,
                          PW_STREAM_MODE_BUFFER,
                          NULL,
                          PW_STREAM_FLAG_NONE,
                          1, &format))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not connect");
      return NULL;
    }

  return pipewire_stream;
}

static void
on_state_changed (void                 *data,
                  enum pw_remote_state  old,
                  enum pw_remote_state  state,
                  const char           *error_message)
{
  MetaScreenCastStreamSrc *src = data;
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);
  struct pw_stream *pipewire_stream;
  GError *error = NULL;

  switch (state)
    {
    case PW_REMOTE_STATE_ERROR:
      g_warning ("pipewire remote error: %s\n", error_message);
      meta_screen_cast_stream_src_notify_closed (src);
      break;
    case PW_REMOTE_STATE_CONNECTED:
      pipewire_stream = create_pipewire_stream (src, &error);
      if (!pipewire_stream)
        {
          g_warning ("Could not create pipewire stream: %s", error->message);
          g_error_free (error);
          meta_screen_cast_stream_src_notify_closed (src);
        }
      else
        {
          priv->pipewire_stream = pipewire_stream;
        }
      break;
    case PW_REMOTE_STATE_UNCONNECTED:
    case PW_REMOTE_STATE_CONNECTING:
      break;
    }
}

static gboolean
pipewire_loop_source_prepare (GSource *base,
                              int     *timeout)
{
  *timeout = -1;
  return FALSE;
}

static gboolean
pipewire_loop_source_dispatch (GSource     *source,
                               GSourceFunc  callback,
                               gpointer     user_data)
{
  MetaPipeWireSource *pipewire_source = (MetaPipeWireSource *) source;
  int result;

  result = pw_loop_iterate (pipewire_source->pipewire_loop, 0);
  if (result == SPA_RESULT_ERRNO)
    g_warning ("pipewire_loop_iterate failed: %s", strerror (errno));
  else if (result != SPA_RESULT_OK)
    g_warning ("pipewire_loop_iterate failed: %d", result);

  return TRUE;
}

static void
pipewire_loop_source_finalize (GSource *source)
{
  MetaPipeWireSource *pipewire_source = (MetaPipeWireSource *) source;

  pw_loop_leave (pipewire_source->pipewire_loop);
  pw_loop_destroy (pipewire_source->pipewire_loop);
}

static GSourceFuncs pipewire_source_funcs =
{
  pipewire_loop_source_prepare,
  NULL,
  pipewire_loop_source_dispatch,
  pipewire_loop_source_finalize
};

static void
init_spa_type (MetaSpaType         *type,
               struct spa_type_map *map)
{
  type->format = spa_type_map_get_id (map, SPA_TYPE__Format);
  type->props = spa_type_map_get_id (map, SPA_TYPE__Props);
  spa_type_meta_map (map, &type->meta);
  spa_type_data_map (map, &type->data);
  spa_type_media_type_map (map, &type->media_type);
  spa_type_media_subtype_map (map, &type->media_subtype);
  spa_type_format_video_map (map, &type->format_video);
  spa_type_video_format_map (map, &type->video_format);
}

static MetaPipeWireSource *
create_pipewire_source (void)
{
  MetaPipeWireSource *pipewire_source;

  pipewire_source =
    (MetaPipeWireSource *) g_source_new (&pipewire_source_funcs,
                                         sizeof (MetaPipeWireSource));
  pipewire_source->pipewire_loop = pw_loop_new (NULL);
  g_source_add_unix_fd (&pipewire_source->base,
                        pw_loop_get_fd (pipewire_source->pipewire_loop),
                        G_IO_IN | G_IO_ERR);

  pw_loop_enter (pipewire_source->pipewire_loop);
  g_source_attach (&pipewire_source->base, NULL);

  return pipewire_source;
}

static const struct pw_remote_events remote_events = {
  PW_VERSION_REMOTE_EVENTS,
  .state_changed = on_state_changed,
};

static gboolean
meta_screen_cast_stream_src_initable_init (GInitable     *initable,
                                           GCancellable  *cancellable,
                                           GError       **error)
{
  MetaScreenCastStreamSrc *src = META_SCREEN_CAST_STREAM_SRC (initable);
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);

  priv->pipewire_source = create_pipewire_source ();
  priv->pipewire_core = pw_core_new (priv->pipewire_source->pipewire_loop,
                                     NULL);
  if (!priv->pipewire_core)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create pipewire core");
      return FALSE;
    }

  priv->pipewire_remote = pw_remote_new (priv->pipewire_core, NULL);
  if (!priv->pipewire_remote)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Couldn't creat pipewire remote");
      return FALSE;
    }

  pw_remote_add_listener (priv->pipewire_remote,
                          &priv->pipewire_remote_listener,
                          &remote_events,
                          src);

  priv->pipewire_type = pw_core_get_type (priv->pipewire_core);
  init_spa_type (&priv->spa_type, priv->pipewire_type->map);

  if (pw_remote_connect (priv->pipewire_remote) != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Couldn't connect pipewire remote");
      return FALSE;
    }

  return TRUE;
}

static void
meta_screen_cast_stream_src_init_initable_iface (GInitableIface *iface)
{
  iface->init = meta_screen_cast_stream_src_initable_init;
}

MetaScreenCastStream *
meta_screen_cast_stream_src_get_stream (MetaScreenCastStreamSrc *src)
{
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);

  return priv->stream;
}

static void
meta_screen_cast_stream_src_finalize (GObject *object)
{
  MetaScreenCastStreamSrc *src = META_SCREEN_CAST_STREAM_SRC (object);
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);

  if (meta_screen_cast_stream_src_is_enabled (src))
    meta_screen_cast_stream_src_disable (src);

  g_clear_pointer (&priv->pipewire_stream, (GDestroyNotify) pw_stream_destroy);
  pw_remote_destroy (priv->pipewire_remote);
  pw_core_destroy (priv->pipewire_core);
  g_source_destroy (&priv->pipewire_source->base);

  G_OBJECT_CLASS (meta_screen_cast_stream_src_parent_class)->finalize (object);
}

static void
meta_screen_cast_stream_src_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  MetaScreenCastStreamSrc *src = META_SCREEN_CAST_STREAM_SRC (object);
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);

  switch (prop_id)
    {
    case PROP_STREAM:
      priv->stream = g_value_get_object (value);
      break;;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_screen_cast_stream_src_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  MetaScreenCastStreamSrc *src = META_SCREEN_CAST_STREAM_SRC (object);
  MetaScreenCastStreamSrcPrivate *priv =
    meta_screen_cast_stream_src_get_instance_private (src);

  switch (prop_id)
    {
    case PROP_STREAM:
      g_value_set_object (value, priv->stream);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_screen_cast_stream_src_init (MetaScreenCastStreamSrc *src)
{
}

static void
meta_screen_cast_stream_src_class_init (MetaScreenCastStreamSrcClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_screen_cast_stream_src_finalize;
  object_class->set_property = meta_screen_cast_stream_src_set_property;
  object_class->get_property = meta_screen_cast_stream_src_get_property;

  g_object_class_install_property (object_class,
                                   PROP_STREAM,
                                   g_param_spec_object ("stream",
                                                        "stream",
                                                        "MetaScreenCastStream",
                                                        META_TYPE_SCREEN_CAST_STREAM,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  signals[READY] = g_signal_new ("ready",
                                 G_TYPE_FROM_CLASS (klass),
                                 G_SIGNAL_RUN_LAST,
                                 0,
                                 NULL, NULL, NULL,
                                 G_TYPE_NONE, 1,
                                 G_TYPE_UINT);
  signals[CLOSED] = g_signal_new ("closed",
                                  G_TYPE_FROM_CLASS (klass),
                                  G_SIGNAL_RUN_LAST,
                                  0,
                                  NULL, NULL, NULL,
                                  G_TYPE_NONE, 0);
}
