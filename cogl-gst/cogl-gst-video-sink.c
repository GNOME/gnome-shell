/*
 * Cogl-GStreamer.
 *
 * GStreamer integration library for Cogl.
 *
 * cogl-gst-video-sink.c - Gstreamer Video Sink that renders to a
 *                         Cogl Pipeline.
 *
 * Authored by Jonathan Matthew  <jonathan@kaolin.wh9.net>,
 *             Chris Lord        <chris@openedhand.com>
 *             Damien Lespiau    <damien.lespiau@intel.com>
 *             Matthew Allum     <mallum@openedhand.com>
 *             Plamena Manolova  <plamena.n.manolova@intel.com>
 *
 * Copyright (C) 2007, 2008 OpenedHand
 * Copyright (C) 2009, 2010, 2013 Intel Corporation
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/gstvalue.h>
#include <gst/video/video.h>
#include <gst/riff/riff-ids.h>
#include <cogl/cogl.h>
#include <string.h>

#include "cogl-gst-video-sink.h"

#define COGL_GST_TEXTURE_FLAGS \
       (COGL_TEXTURE_NO_SLICING | COGL_TEXTURE_NO_ATLAS)
#define COGL_GST_DEFAULT_PRIORITY G_PRIORITY_HIGH_IDLE

#define BASE_SINK_CAPS "{ AYUV," \
                       "YV12," \
                       "I420," \
                       "RGBA," \
                       "BGRA," \
                       "RGB," \
                       "BGR }"

#define SINK_CAPS GST_VIDEO_CAPS_MAKE (BASE_SINK_CAPS)

#define COGL_GST_PARAM_STATIC        \
  (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

#define COGL_GST_PARAM_READABLE      \
  (G_PARAM_READABLE | COGL_GST_PARAM_STATIC)

#define COGL_GST_PARAM_WRITABLE      \
  (G_PARAM_WRITABLE | COGL_GST_PARAM_STATIC)

#define COGL_GST_PARAM_READWRITE     \
  (G_PARAM_READABLE | G_PARAM_WRITABLE | COGL_GST_PARAM_STATIC)

static GstStaticPadTemplate sinktemplate_all =
  GST_STATIC_PAD_TEMPLATE ("sink",
                           GST_PAD_SINK,
                           GST_PAD_ALWAYS,
                           GST_STATIC_CAPS (SINK_CAPS));

G_DEFINE_TYPE (CoglGstVideoSink, cogl_gst_video_sink, GST_TYPE_BASE_SINK);

enum
{
  PROP_0,
  PROP_UPDATE_PRIORITY
};

enum
{
  PIPELINE_READY_SIGNAL,
  NEW_FRAME_SIGNAL,

  LAST_SIGNAL
};

static guint video_sink_signals[LAST_SIGNAL] = { 0, };

typedef enum
{
  COGL_GST_NOFORMAT,
  COGL_GST_RGB32,
  COGL_GST_RGB24,
  COGL_GST_AYUV,
  COGL_GST_YV12,
  COGL_GST_SURFACE,
  COGL_GST_I420
} CoglGstVideoFormat;

typedef enum
{
  COGL_GST_RENDERER_NEEDS_GLSL = (1 << 0)
} CoglGstRendererFlag;

/* We want to cache the snippets instead of recreating a new one every
 * time we initialise a pipeline so that if we end up recreating the
 * same pipeline again then Cogl will be able to use the pipeline
 * cache to avoid linking a redundant identical shader program */
typedef struct
{
  CoglSnippet *vertex_snippet;
  CoglSnippet *fragment_snippet;
  CoglSnippet *default_sample_snippet;
  int start_position;
} SnippetCacheEntry;

typedef struct
{
  GQueue entries;
} SnippetCache;

typedef struct _CoglGstSource
{
  GSource source;
  CoglGstVideoSink *sink;
  GMutex buffer_lock;
  GstBuffer *buffer;
  CoglBool has_new_caps;
} CoglGstSource;

typedef void (CoglGstRendererPaint) (CoglGstVideoSink *);
typedef void (CoglGstRendererPostPaint) (CoglGstVideoSink *);

typedef struct _CoglGstRenderer
{
  const char *name;
  CoglGstVideoFormat format;
  int flags;
  GstStaticCaps caps;
  int n_layers;
  void (*setup_pipeline) (CoglGstVideoSink *sink,
                          CoglPipeline *pipeline);
  CoglBool (*upload) (CoglGstVideoSink *sink,
                      GstBuffer *buffer);
} CoglGstRenderer;

struct _CoglGstVideoSinkPrivate
{
  CoglContext *ctx;
  CoglPipeline *pipeline;
  CoglTexture *frame[3];
  CoglBool frame_dirty;
  CoglGstVideoFormat format;
  CoglBool bgr;
  CoglGstSource *source;
  GSList *renderers;
  GstCaps *caps;
  CoglGstRenderer *renderer;
  GstFlowReturn flow_return;
  int custom_start;
  int free_layer;
  CoglBool default_sample;
  GstVideoInfo info;
};

static void
cogl_gst_source_finalize (GSource *source)
{
  CoglGstSource *gst_source = (CoglGstSource *) source;

  g_mutex_lock (&gst_source->buffer_lock);
  if (gst_source->buffer)
    gst_buffer_unref (gst_source->buffer);
  gst_source->buffer = NULL;
  g_mutex_unlock (&gst_source->buffer_lock);
  g_mutex_clear (&gst_source->buffer_lock);
}

int
cogl_gst_video_sink_get_free_layer (CoglGstVideoSink *sink)
{
  return sink->priv->free_layer;
}

void
cogl_gst_video_sink_attach_frame (CoglGstVideoSink *sink,
                                  CoglPipeline *pln)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  int i;

  for (i = 0; i < G_N_ELEMENTS (priv->frame); i++)
    if (priv->frame[i] != NULL)
      cogl_pipeline_set_layer_texture (pln, i + priv->custom_start,
                                       priv->frame[i]);
}

static CoglBool
cogl_gst_source_prepare (GSource *source,
                         int *timeout)
{
  CoglGstSource *gst_source = (CoglGstSource *) source;

  *timeout = -1;

  return gst_source->buffer != NULL;
}

static CoglBool
cogl_gst_source_check (GSource *source)
{
  CoglGstSource *gst_source = (CoglGstSource *) source;

  return gst_source->buffer != NULL;
}

static void
cogl_gst_video_sink_set_priority (CoglGstVideoSink *sink,
                                  int priority)
{
  if (sink->priv->source)
    g_source_set_priority ((GSource *) sink->priv->source, priority);
}

static void
dirty_default_pipeline (CoglGstVideoSink *sink)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;

  if (priv->pipeline)
    {
      cogl_object_unref (priv->pipeline);
      priv->pipeline = NULL;
    }
}

void
cogl_gst_video_sink_set_first_layer (CoglGstVideoSink *sink,
                                     int first_layer)
{
  g_return_if_fail (COGL_GST_IS_VIDEO_SINK (sink));

  if (first_layer != sink->priv->custom_start)
    {
      sink->priv->custom_start = first_layer;
      dirty_default_pipeline (sink);

      if (sink->priv->renderer)
        sink->priv->free_layer = (sink->priv->custom_start +
                                  sink->priv->renderer->n_layers);
    }
}

void
cogl_gst_video_sink_set_default_sample (CoglGstVideoSink *sink,
                                        CoglBool default_sample)
{
  g_return_if_fail (COGL_GST_IS_VIDEO_SINK (sink));

  if (default_sample != sink->priv->default_sample)
    {
      sink->priv->default_sample = default_sample;
      dirty_default_pipeline (sink);
    }
}

void
cogl_gst_video_sink_setup_pipeline (CoglGstVideoSink *sink,
                                    CoglPipeline *pipeline)
{
  g_return_if_fail (COGL_GST_IS_VIDEO_SINK (sink));

  if (sink->priv->renderer)
    sink->priv->renderer->setup_pipeline (sink, pipeline);
}

static SnippetCacheEntry *
get_cache_entry (CoglGstVideoSink *sink,
                 SnippetCache *cache)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  GList *l;

  for (l = cache->entries.head; l; l = l->next)
    {
      SnippetCacheEntry *entry = l->data;

      if (entry->start_position == priv->custom_start)
        return entry;
    }

  return NULL;
}

static SnippetCacheEntry *
add_cache_entry (CoglGstVideoSink *sink,
                 SnippetCache *cache,
                 const char *decl)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  SnippetCacheEntry *entry = g_slice_new (SnippetCacheEntry);
  char *default_source;

  entry->start_position = priv->custom_start;

  entry->vertex_snippet =
    cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX_GLOBALS,
                      decl,
                      NULL /* post */);
  entry->fragment_snippet =
    cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT_GLOBALS,
                      decl,
                      NULL /* post */);

  default_source =
    g_strdup_printf ("  cogl_layer *= cogl_gst_sample_video%i "
                     "(cogl_tex_coord%i_in.st);\n",
                     priv->custom_start,
                     priv->custom_start);
  entry->default_sample_snippet =
    cogl_snippet_new (COGL_SNIPPET_HOOK_LAYER_FRAGMENT,
                      NULL, /* declarations */
                      default_source);
  g_free (default_source);

  g_queue_push_head (&cache->entries, entry);

  return entry;
}

static void
setup_pipeline_from_cache_entry (CoglGstVideoSink *sink,
                                 CoglPipeline *pipeline,
                                 SnippetCacheEntry *cache_entry,
                                 int n_layers)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;

  if (cache_entry)
    {
      int i;

      /* The global sampling function gets added to both the fragment
       * and vertex stages. The hope is that the GLSL compiler will
       * easily remove the dead code if it's not actually used */
      cogl_pipeline_add_snippet (pipeline, cache_entry->vertex_snippet);
      cogl_pipeline_add_snippet (pipeline, cache_entry->fragment_snippet);

      /* Set all of the layers to just directly copy from the previous
       * layer so that it won't redundantly generate code to sample
       * the intermediate textures */
      for (i = 0; i < n_layers; i++)
        cogl_pipeline_set_layer_combine (pipeline,
                                         priv->custom_start + i,
                                         "RGBA=REPLACE(PREVIOUS)",
                                         NULL);

      if (priv->default_sample)
        cogl_pipeline_add_layer_snippet (pipeline,
                                         priv->custom_start + n_layers - 1,
                                         cache_entry->default_sample_snippet);
    }

  priv->frame_dirty = TRUE;
}

CoglPipeline *
cogl_gst_video_sink_get_pipeline (CoglGstVideoSink *vt)
{
  CoglGstVideoSinkPrivate *priv;

  g_return_val_if_fail (COGL_GST_IS_VIDEO_SINK (vt), NULL);

  priv = vt->priv;

  if (priv->pipeline == NULL)
    {
      priv->pipeline = cogl_pipeline_new (priv->ctx);
      cogl_gst_video_sink_setup_pipeline (vt, priv->pipeline);
      cogl_gst_video_sink_attach_frame (vt, priv->pipeline);
      priv->frame_dirty = FALSE;
    }
  else if (priv->frame_dirty)
    {
      CoglPipeline *pipeline = cogl_pipeline_copy (priv->pipeline);
      cogl_object_unref (priv->pipeline);
      priv->pipeline = pipeline;

      cogl_gst_video_sink_attach_frame (vt, pipeline);
      priv->frame_dirty = FALSE;
    }

  return priv->pipeline;
}

static void
clear_frame_textures (CoglGstVideoSink *sink)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  int i;

  for (i = 0; i < G_N_ELEMENTS (priv->frame); i++)
    {
      if (priv->frame[i] == NULL)
        break;
      else
        cogl_object_unref (priv->frame[i]);
    }

  memset (priv->frame, 0, sizeof (priv->frame));

  priv->frame_dirty = TRUE;
}

static void
cogl_gst_rgb_setup_pipeline (CoglGstVideoSink *sink,
                             CoglPipeline *pipeline)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;

  if (cogl_has_feature (priv->ctx, COGL_FEATURE_ID_GLSL))
    {
      static SnippetCache snippet_cache;
      SnippetCacheEntry *entry = get_cache_entry (sink, &snippet_cache);

      if (entry == NULL)
        {
          char *source;

          source =
            g_strdup_printf ("vec4\n"
                             "cogl_gst_sample_video%i (vec2 UV)\n"
                             "{\n"
                             "  return texture2D (cogl_sampler%i, UV);\n"
                             "}\n",
                             priv->custom_start,
                             priv->custom_start);

          setup_pipeline_from_cache_entry (sink, pipeline, entry, 1);
          g_free (source);
        }
    }
  else
    setup_pipeline_from_cache_entry (sink, pipeline, NULL, 1);
}

static CoglBool
cogl_gst_rgb24_upload (CoglGstVideoSink *sink,
                       GstBuffer *buffer)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  CoglPixelFormat format;
  GstVideoFrame frame;

  if (priv->bgr)
    format = COGL_PIXEL_FORMAT_BGR_888;
  else
    format = COGL_PIXEL_FORMAT_RGB_888;

  if (!gst_video_frame_map (&frame, &priv->info, buffer, GST_MAP_READ))
    goto map_fail;

  clear_frame_textures (sink);

  priv->frame[0] = cogl_texture_new_from_data (priv->info.width,
                                               priv->info.height,
                                               COGL_GST_TEXTURE_FLAGS,
                                               format, format,
                                               priv->info.stride[0],
                                               frame.data[0]);

  gst_video_frame_unmap (&frame);

  return TRUE;

map_fail:
  {
    GST_ERROR_OBJECT (sink, "Could not map incoming video frame");
    return FALSE;
  }
}

static CoglGstRenderer rgb24_renderer =
{
  "RGB 24",
  COGL_GST_RGB24,
  0,
  GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ RGB, BGR }")),
  1, /* n_layers */
  cogl_gst_rgb_setup_pipeline,
  cogl_gst_rgb24_upload,
};

static CoglBool
cogl_gst_rgb32_upload (CoglGstVideoSink *sink,
                       GstBuffer *buffer)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  CoglPixelFormat format;
  GstVideoFrame frame;

  if (priv->bgr)
    format = COGL_PIXEL_FORMAT_BGRA_8888;
  else
    format = COGL_PIXEL_FORMAT_RGBA_8888;

  if (!gst_video_frame_map (&frame, &priv->info, buffer, GST_MAP_READ))
    goto map_fail;

  clear_frame_textures (sink);

  priv->frame[0] = cogl_texture_new_from_data (priv->info.width,
                                               priv->info.height,
                                               COGL_GST_TEXTURE_FLAGS,
                                               format, format,
                                               priv->info.stride[0],
                                               frame.data[0]);

  gst_video_frame_unmap (&frame);

  return TRUE;

map_fail:
  {
    GST_ERROR_OBJECT (sink, "Could not map incoming video frame");
    return FALSE;
  }
}

static CoglGstRenderer rgb32_renderer =
{
  "RGB 32",
  COGL_GST_RGB32,
  0,
  GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ RGBA, BGRA }")),
  1, /* n_layers */
  cogl_gst_rgb_setup_pipeline,
  cogl_gst_rgb32_upload,
};

static CoglBool
cogl_gst_yv12_upload (CoglGstVideoSink *sink,
                      GstBuffer *buffer)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  GstVideoFrame frame;
  CoglPixelFormat format = COGL_PIXEL_FORMAT_A_8;

  if (!gst_video_frame_map (&frame, &priv->info, buffer, GST_MAP_READ))
    goto map_fail;

  clear_frame_textures (sink);

  priv->frame[0] =
    cogl_texture_new_from_data (GST_VIDEO_INFO_COMP_WIDTH (&priv->info, 0),
                                GST_VIDEO_INFO_COMP_HEIGHT (&priv->info, 0),
                                COGL_GST_TEXTURE_FLAGS, format, format,
                                priv->info.stride[0], frame.data[0]);

  priv->frame[1] =
    cogl_texture_new_from_data (GST_VIDEO_INFO_COMP_WIDTH (&priv->info, 1),
                                GST_VIDEO_INFO_COMP_HEIGHT (&priv->info, 1),
                                COGL_GST_TEXTURE_FLAGS, format, format,
                                priv->info.stride[1], frame.data[1]);

  priv->frame[2] =
    cogl_texture_new_from_data (GST_VIDEO_INFO_COMP_WIDTH (&priv->info, 2),
                                GST_VIDEO_INFO_COMP_HEIGHT (&priv->info, 2),
                                COGL_GST_TEXTURE_FLAGS, format, format,
                                priv->info.stride[2], frame.data[2]);

  gst_video_frame_unmap (&frame);

  return TRUE;

map_fail:
  {
    GST_ERROR_OBJECT (sink, "Could not map incoming video frame");
    return FALSE;
  }
}

static void
cogl_gst_yv12_glsl_setup_pipeline (CoglGstVideoSink *sink,
                                   CoglPipeline *pipeline)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  static SnippetCache snippet_cache;
  SnippetCacheEntry *entry;

  entry = get_cache_entry (sink, &snippet_cache);

  if (entry == NULL)
    {
      char *source;

      source =
        g_strdup_printf ("vec4\n"
                         "cogl_gst_sample_video%i (vec2 UV)\n"
                         "{\n"
                         "  float y = 1.1640625 * "
                         "(texture2D (cogl_sampler%i, UV).a - 0.0625);\n"
                         "  float u = texture2D (cogl_sampler%i, UV).a - 0.5;\n"
                         "  float v = texture2D (cogl_sampler%i, UV).a - 0.5;\n"
                         "  vec4 color;\n"
                         "  color.r = y + 1.59765625 * v;\n"
                         "  color.g = y - 0.390625 * u - 0.8125 * v;\n"
                         "  color.b = y + 2.015625 * u;\n"
                         "  color.a = 1.0;\n"
                         "  return color;\n"
                         "}\n",
                         priv->custom_start,
                         priv->custom_start,
                         priv->custom_start + 1,
                         priv->custom_start + 2);

      entry = add_cache_entry (sink, &snippet_cache, source);
      g_free (source);
    }

  setup_pipeline_from_cache_entry (sink, pipeline, entry, 3);
}

static CoglGstRenderer yv12_glsl_renderer =
{
  "YV12 glsl",
  COGL_GST_YV12,
  COGL_GST_RENDERER_NEEDS_GLSL,
  GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("YV12")),
  3, /* n_layers */
  cogl_gst_yv12_glsl_setup_pipeline,
  cogl_gst_yv12_upload,
};

static CoglGstRenderer i420_glsl_renderer =
{
  "I420 glsl",
  COGL_GST_I420,
  COGL_GST_RENDERER_NEEDS_GLSL,
  GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("I420")),
  3, /* n_layers */
  cogl_gst_yv12_glsl_setup_pipeline,
  cogl_gst_yv12_upload,
};

static void
cogl_gst_ayuv_glsl_setup_pipeline (CoglGstVideoSink *sink,
                                   CoglPipeline *pipeline)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  static SnippetCache snippet_cache;
  SnippetCacheEntry *entry;

  entry = get_cache_entry (sink, &snippet_cache);

  if (entry == NULL)
    {
      char *source;

      source
        = g_strdup_printf ("vec4\n"
                           "cogl_gst_sample_video%i (vec2 UV)\n"
                           "{\n"
                           "  vec4 color = texture2D (cogl_sampler%i, UV);\n"
                           "  float y = 1.1640625 * (color.g - 0.0625);\n"
                           "  float u = color.b - 0.5;\n"
                           "  float v = color.a - 0.5;\n"
                           "  color.a = color.r;\n"
                           "  color.r = y + 1.59765625 * v;\n"
                           "  color.g = y - 0.390625 * u - 0.8125 * v;\n"
                           "  color.b = y + 2.015625 * u;\n"
                           "  return color;\n"
                           "}\n", priv->custom_start,
                           priv->custom_start);

      entry = add_cache_entry (sink, &snippet_cache, source);
      g_free (source);
    }

  setup_pipeline_from_cache_entry (sink, pipeline, entry, 3);
}

static CoglBool
cogl_gst_ayuv_upload (CoglGstVideoSink *sink,
                      GstBuffer *buffer)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  CoglPixelFormat format = COGL_PIXEL_FORMAT_RGBA_8888;
  GstVideoFrame frame;

  if (!gst_video_frame_map (&frame, &priv->info, buffer, GST_MAP_READ))
    goto map_fail;

  clear_frame_textures (sink);

  priv->frame[0] = cogl_texture_new_from_data (priv->info.width,
                                               priv->info.height,
                                               COGL_GST_TEXTURE_FLAGS,
                                               format, format,
                                               priv->info.stride[0],
                                               frame.data[0]);

  gst_video_frame_unmap (&frame);

  return TRUE;

map_fail:
  {
    GST_ERROR_OBJECT (sink, "Could not map incoming video frame");
    return FALSE;
  }
}

static CoglGstRenderer ayuv_glsl_renderer =
{
  "AYUV glsl",
  COGL_GST_AYUV,
  COGL_GST_RENDERER_NEEDS_GLSL,
  GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("AYUV")),
  3, /* n_layers */
  cogl_gst_ayuv_glsl_setup_pipeline,
  cogl_gst_ayuv_upload,
};

static GSList*
cogl_gst_build_renderers_list (CoglContext *ctx)
{
  GSList *list = NULL;
  CoglBool has_glsl;
  int i;
  static CoglGstRenderer *const renderers[] =
  {
    &rgb24_renderer,
    &rgb32_renderer,
    &yv12_glsl_renderer,
    &i420_glsl_renderer,
    &ayuv_glsl_renderer,
    NULL
  };

  has_glsl = cogl_has_feature (ctx, COGL_FEATURE_ID_GLSL);

  for (i = 0; renderers[i]; i++)
    if (has_glsl || !(renderers[i]->flags & COGL_GST_RENDERER_NEEDS_GLSL))
      list = g_slist_prepend (list, renderers[i]);

  return list;
}

static void
append_cap (gpointer data,
            gpointer user_data)
{
  CoglGstRenderer *renderer = (CoglGstRenderer *) data;
  GstCaps *caps = (GstCaps *) user_data;
  GstCaps *writable_caps;
  writable_caps =
    gst_caps_make_writable (gst_static_caps_get (&renderer->caps));
  gst_caps_append (caps, writable_caps);
}

static GstCaps *
cogl_gst_build_caps (GSList *renderers)
{
  GstCaps *caps;

  caps = gst_caps_new_empty ();

  g_slist_foreach (renderers, append_cap, caps);

  return caps;
}

void
cogl_gst_video_sink_set_context (CoglGstVideoSink *vt,
                                 CoglContext *ctx)
{
  CoglGstVideoSinkPrivate *priv = vt->priv;

  if (ctx)
    ctx = cogl_object_ref (ctx);

  if (priv->ctx)
    {
      cogl_object_unref (priv->ctx);
      g_slist_free (priv->renderers);
      priv->renderers = NULL;
      if (priv->caps)
        {
          gst_caps_unref (priv->caps);
          priv->caps = NULL;
        }
    }

  if (ctx)
    {
      priv->ctx = ctx;
      priv->renderers = cogl_gst_build_renderers_list (priv->ctx);
      priv->caps = cogl_gst_build_caps (priv->renderers);
    }
}

static CoglGstRenderer *
cogl_gst_find_renderer_by_format (CoglGstVideoSink *sink,
                                  CoglGstVideoFormat format)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  CoglGstRenderer *renderer = NULL;
  GSList *element;

  for (element = priv->renderers; element; element = g_slist_next (element))
    {
      CoglGstRenderer *candidate = (CoglGstRenderer *) element->data;
      if (candidate->format == format)
        {
          renderer = candidate;
          break;
        }
    }

  return renderer;
}

static GstCaps *
cogl_gst_video_sink_get_caps (GstBaseSink *bsink,
                              GstCaps *filter)
{
  CoglGstVideoSink *sink;
  sink = COGL_GST_VIDEO_SINK (bsink);
  return gst_caps_ref (sink->priv->caps);
}

static CoglBool
cogl_gst_video_sink_parse_caps (GstCaps *caps,
                                CoglGstVideoSink *sink,
                                CoglBool save)
{
  CoglGstVideoSinkPrivate *priv = sink->priv;
  GstCaps *intersection;
  GstVideoInfo vinfo;
  CoglGstVideoFormat format;
  CoglBool bgr = FALSE;
  CoglGstRenderer *renderer;

  intersection = gst_caps_intersect (priv->caps, caps);
  if (gst_caps_is_empty (intersection))
    goto no_intersection;

  gst_caps_unref (intersection);

  if (!gst_video_info_from_caps (&vinfo, caps))
    goto unknown_format;

  switch (vinfo.finfo->format)
    {
    case GST_VIDEO_FORMAT_YV12:
      format = COGL_GST_YV12;
      break;
    case GST_VIDEO_FORMAT_I420:
      format = COGL_GST_I420;
      break;
    case GST_VIDEO_FORMAT_AYUV:
      format = COGL_GST_AYUV;
      bgr = FALSE;
      break;
    case GST_VIDEO_FORMAT_RGB:
      format = COGL_GST_RGB24;
      bgr = FALSE;
      break;
    case GST_VIDEO_FORMAT_BGR:
      format = COGL_GST_RGB24;
      bgr = TRUE;
      break;
    case GST_VIDEO_FORMAT_RGBA:
      format = COGL_GST_RGB32;
      bgr = FALSE;
      break;
    case GST_VIDEO_FORMAT_BGRA:
      format = COGL_GST_RGB32;
      bgr = TRUE;
      break;
    default:
      goto unhandled_format;
    }

  renderer = cogl_gst_find_renderer_by_format (sink, format);

  if (G_UNLIKELY (renderer == NULL))
    goto no_suitable_renderer;

  GST_INFO_OBJECT (sink, "found the %s renderer", renderer->name);

  if (save)
    {
      priv->info = vinfo;

      priv->format = format;
      priv->bgr = bgr;

      priv->renderer = renderer;
    }

  return TRUE;


no_intersection:
  {
    GST_WARNING_OBJECT (sink,
        "Incompatible caps, don't intersect with %" GST_PTR_FORMAT, priv->caps);
    return FALSE;
  }

unknown_format:
  {
    GST_WARNING_OBJECT (sink, "Could not figure format of input caps");
    return FALSE;
  }

unhandled_format:
  {
    GST_ERROR_OBJECT (sink, "Provided caps aren't supported by clutter-gst");
    return FALSE;
  }

no_suitable_renderer:
  {
    GST_ERROR_OBJECT (sink, "could not find a suitable renderer");
    return FALSE;
  }
}

static CoglBool
cogl_gst_video_sink_set_caps (GstBaseSink *bsink,
                              GstCaps *caps)
{
  CoglGstVideoSink *sink;
  CoglGstVideoSinkPrivate *priv;

  sink = COGL_GST_VIDEO_SINK (bsink);
  priv = sink->priv;

  if (!cogl_gst_video_sink_parse_caps (caps, sink, FALSE))
    return FALSE;

  g_mutex_lock (&priv->source->buffer_lock);
  priv->source->has_new_caps = TRUE;
  g_mutex_unlock (&priv->source->buffer_lock);

  return TRUE;
}

static CoglBool
cogl_gst_source_dispatch (GSource *source,
                          GSourceFunc callback,
                          void *user_data)
{
  CoglGstSource *gst_source= (CoglGstSource*) source;
  CoglGstVideoSinkPrivate *priv = gst_source->sink->priv;
  GstBuffer *buffer;

  g_mutex_lock (&gst_source->buffer_lock);

  if (G_UNLIKELY (gst_source->has_new_caps))
    {
      GstCaps *caps =
        gst_pad_get_current_caps (GST_BASE_SINK_PAD ((GST_BASE_SINK
                (gst_source->sink))));

      if (!cogl_gst_video_sink_parse_caps (caps, gst_source->sink, TRUE))
        goto negotiation_fail;

      gst_source->has_new_caps = FALSE;
      priv->free_layer = priv->custom_start + priv->renderer->n_layers;

      dirty_default_pipeline (gst_source->sink);

      /* We are now in a state where we could generate the pipeline if
       * the application requests it so we can emit the signal.
       * However we'll actually generate the pipeline lazily only if
       * the application actually asks for it. */
      g_signal_emit (gst_source->sink,
                     video_sink_signals[PIPELINE_READY_SIGNAL],
                     0 /* detail */);
    }

  buffer = gst_source->buffer;
  gst_source->buffer = NULL;

  g_mutex_unlock (&gst_source->buffer_lock);

  if (buffer)
    {
      if (!priv->renderer->upload (gst_source->sink, buffer))
        goto fail_upload;

      g_signal_emit (gst_source->sink,
                     video_sink_signals[NEW_FRAME_SIGNAL], 0,
                     NULL);
      gst_buffer_unref (buffer);
    }
  else
    GST_WARNING_OBJECT (gst_source->sink, "No buffers available for display");

  return TRUE;


negotiation_fail:
  {
    GST_WARNING_OBJECT (gst_source->sink,
        "Failed to handle caps. Stopping GSource");
    priv->flow_return = GST_FLOW_NOT_NEGOTIATED;
    g_mutex_unlock (&gst_source->buffer_lock);

    return FALSE;
  }

fail_upload:
  {
    GST_WARNING_OBJECT (gst_source->sink, "Failed to upload buffer");
    priv->flow_return = GST_FLOW_ERROR;
    gst_buffer_unref (buffer);
    return FALSE;
  }
}

static GSourceFuncs gst_source_funcs =
{
  cogl_gst_source_prepare,
  cogl_gst_source_check,
  cogl_gst_source_dispatch,
  cogl_gst_source_finalize
};

static CoglGstSource *
cogl_gst_source_new (CoglGstVideoSink *sink)
{
  GSource *source;
  CoglGstSource *gst_source;

  source = g_source_new (&gst_source_funcs, sizeof (CoglGstSource));
  gst_source = (CoglGstSource *) source;

  g_source_set_can_recurse (source, TRUE);
  g_source_set_priority (source, COGL_GST_DEFAULT_PRIORITY);

  gst_source->sink = sink;
  g_mutex_init (&gst_source->buffer_lock);
  gst_source->buffer = NULL;

  return gst_source;
}

static void
cogl_gst_video_sink_init (CoglGstVideoSink *sink)
{
  CoglGstVideoSinkPrivate *priv;

  sink->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (sink,
                                                   COGL_GST_TYPE_VIDEO_SINK,
                                                   CoglGstVideoSinkPrivate);
}

static GstFlowReturn
_cogl_gst_video_sink_render (GstBaseSink *bsink,
                             GstBuffer *buffer)
{
  CoglGstVideoSink *sink = COGL_GST_VIDEO_SINK (bsink);
  CoglGstVideoSinkPrivate *priv = sink->priv;
  CoglGstSource *gst_source = priv->source;

  g_mutex_lock (&gst_source->buffer_lock);

  if (G_UNLIKELY (priv->flow_return != GST_FLOW_OK))
    goto dispatch_flow_ret;

  if (gst_source->buffer)
    gst_buffer_unref (gst_source->buffer);

  gst_source->buffer = gst_buffer_ref (buffer);
  g_mutex_unlock (&gst_source->buffer_lock);

  g_main_context_wakeup (NULL);

  return GST_FLOW_OK;

  dispatch_flow_ret:
  {
    g_mutex_unlock (&gst_source->buffer_lock);
    return priv->flow_return;
  }
}

static void
cogl_gst_video_sink_dispose (GObject *object)
{
  CoglGstVideoSink *self;
  CoglGstVideoSinkPrivate *priv;

  self = COGL_GST_VIDEO_SINK (object);
  priv = self->priv;

  clear_frame_textures (self);

  if (priv->pipeline)
    {
      cogl_object_unref (priv->pipeline);
      priv->pipeline = NULL;
    }

  if (priv->caps)
    {
      gst_caps_unref (priv->caps);
      priv->caps = NULL;
    }

  G_OBJECT_CLASS (cogl_gst_video_sink_parent_class)->dispose (object);
}

static void
cogl_gst_video_sink_finalize (GObject *object)
{
  CoglGstVideoSink *self = COGL_GST_VIDEO_SINK (object);

  cogl_gst_video_sink_set_context (self, NULL);

  G_OBJECT_CLASS (cogl_gst_video_sink_parent_class)->finalize (object);
}

static CoglBool
cogl_gst_video_sink_start (GstBaseSink *base_sink)
{
  CoglGstVideoSink *sink = COGL_GST_VIDEO_SINK (base_sink);
  CoglGstVideoSinkPrivate *priv = sink->priv;

  priv->source = cogl_gst_source_new (sink);
  g_source_attach ((GSource *) priv->source, NULL);
  priv->flow_return = GST_FLOW_OK;
  return TRUE;
}

static void
cogl_gst_video_sink_set_property (GObject *object,
                                  unsigned int prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  CoglGstVideoSink *sink = COGL_GST_VIDEO_SINK (object);

  switch (prop_id)
    {
    case PROP_UPDATE_PRIORITY:
      cogl_gst_video_sink_set_priority (sink, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cogl_gst_video_sink_get_property (GObject *object,
                                  unsigned int prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  CoglGstVideoSink *sink = COGL_GST_VIDEO_SINK (object);
  CoglGstVideoSinkPrivate *priv = sink->priv;

  switch (prop_id)
    {
    case PROP_UPDATE_PRIORITY:
      g_value_set_int (value, g_source_get_priority ((GSource *) priv->source));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static CoglBool
cogl_gst_video_sink_stop (GstBaseSink *base_sink)
{
  CoglGstVideoSink *sink = COGL_GST_VIDEO_SINK (base_sink);
  CoglGstVideoSinkPrivate *priv = sink->priv;

  if (priv->source)
    {
      GSource *source = (GSource *) priv->source;
      g_source_destroy (source);
      g_source_unref (source);
      priv->source = NULL;
    }

  return TRUE;
}

static void
cogl_gst_video_sink_class_init (CoglGstVideoSinkClass *klass)
{
  GObjectClass *go_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *gb_class = GST_BASE_SINK_CLASS (klass);
  GstElementClass *ge_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *pad_template;
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (CoglGstVideoSinkPrivate));
  go_class->set_property = cogl_gst_video_sink_set_property;
  go_class->get_property = cogl_gst_video_sink_get_property;
  go_class->dispose = cogl_gst_video_sink_dispose;
  go_class->finalize = cogl_gst_video_sink_finalize;

  pad_template = gst_static_pad_template_get (&sinktemplate_all);
  gst_element_class_add_pad_template (ge_class, pad_template);

  gst_element_class_set_metadata (ge_class,
                                  "Cogl video sink", "Sink/Video",
                                  "Sends video data from GStreamer to a "
                                  "Cogl pipeline",
                                  "Jonathan Matthew <jonathan@kaolin.wh9.net>, "
                                  "Matthew Allum <mallum@o-hand.com, "
                                  "Chris Lord <chris@o-hand.com>, "
                                  "Plamena Manolova "
                                  "<plamena.n.manolova@intel.com>");

  gb_class->render = _cogl_gst_video_sink_render;
  gb_class->preroll = _cogl_gst_video_sink_render;
  gb_class->start = cogl_gst_video_sink_start;
  gb_class->stop = cogl_gst_video_sink_stop;
  gb_class->set_caps = cogl_gst_video_sink_set_caps;
  gb_class->get_caps = cogl_gst_video_sink_get_caps;

  pspec = g_param_spec_int ("update-priority",
                            "Update Priority",
                            "Priority of video updates in the thread",
                            -G_MAXINT, G_MAXINT,
                            COGL_GST_DEFAULT_PRIORITY,
                            COGL_GST_PARAM_READWRITE);

  g_object_class_install_property (go_class, PROP_UPDATE_PRIORITY, pspec);

  video_sink_signals[PIPELINE_READY_SIGNAL] =
    g_signal_new ("pipeline-ready",
                  COGL_GST_TYPE_VIDEO_SINK,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (CoglGstVideoSinkClass, pipeline_ready),
                  NULL, /* accumulator */
                  NULL, /* accu_data */
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0 /* n_params */);

  video_sink_signals[NEW_FRAME_SIGNAL] =
    g_signal_new ("new-frame",
                  COGL_GST_TYPE_VIDEO_SINK,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (CoglGstVideoSinkClass, new_frame),
                  NULL, /* accumulator */
                  NULL, /* accu_data */
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0 /* n_params */);
}

CoglGstVideoSink *
cogl_gst_video_sink_new (CoglContext *ctx)
{
  CoglGstVideoSink *sink = g_object_new (COGL_GST_TYPE_VIDEO_SINK, NULL);
  cogl_gst_video_sink_set_context (sink, ctx);
  sink->priv->custom_start = 0;
  sink->priv->default_sample = TRUE;

  return sink;
}

float
cogl_gst_video_sink_get_aspect (CoglGstVideoSink *vt)
{
  GstVideoInfo *info;

  g_return_val_if_fail (COGL_GST_IS_VIDEO_SINK (vt), 0.);

  info = &vt->priv->info;
  return ((float)info->width * (float)info->par_n) /
    ((float)info->height * (float)info->par_d);
}

float
cogl_gst_video_sink_get_width_for_height (CoglGstVideoSink *vt,
                                          float height)
{
  float aspect;

  g_return_val_if_fail (COGL_GST_IS_VIDEO_SINK (vt), 0.);

  aspect = cogl_gst_video_sink_get_aspect (vt);
  return height * aspect;
}

float
cogl_gst_video_sink_get_height_for_width (CoglGstVideoSink *vt,
                                          float width)
{
  float aspect;

  g_return_val_if_fail (COGL_GST_IS_VIDEO_SINK (vt), 0.);

  aspect = cogl_gst_video_sink_get_aspect (vt);
  return width / aspect;
}

void
cogl_gst_video_sink_fit_size (CoglGstVideoSink *vt,
                              const CoglGstRectangle *available,
                              CoglGstRectangle *output)
{
  g_return_if_fail (COGL_GST_IS_VIDEO_SINK (vt));
  g_return_if_fail (available != NULL);
  g_return_if_fail (output != NULL);

  if (available->height == 0.0f)
    {
      output->x = available->x;
      output->y = available->y;
      output->width = output->height = 0;
    }
  else
    {
      float available_aspect = available->width / available->height;
      float video_aspect = cogl_gst_video_sink_get_aspect (vt);

      if (video_aspect > available_aspect)
        {
          output->width = available->width;
          output->height = available->width / video_aspect;
          output->x = available->x;
          output->y = available->y + (available->height - output->height) / 2;
        }
      else
        {
          output->width = available->height * video_aspect;
          output->height = available->height;
          output->x = available->x + (available->width - output->width) / 2;
          output->y = available->y;
        }
    }
}
