/*
 * Cogl-GStreamer.
 *
 * GStreamer integration library for Cogl.
 *
 * cogl-gst-video-sink.h - Gstreamer Video Sink that renders to a
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
 FLOP
 */

#ifndef __COGL_GST_VIDEO_SINK_H__
#define __COGL_GST_VIDEO_SINK_H__
#include <glib-object.h>
#include <gst/base/gstbasesink.h>
#include <cogl/cogl.h>

G_BEGIN_DECLS

#define COGL_GST_TYPE_VIDEO_SINK cogl_gst_video_sink_get_type()

#define COGL_GST_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  COGL_GST_TYPE_VIDEO_SINK, CoglGstVideoSink))

#define COGL_GST_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  COGL_GST_TYPE_VIDEO_SINK, CoglGstVideoSinkClass))

#define COGL_GST_IS_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  COGL_GST_TYPE_VIDEO_SINK))

#define COGL_GST_IS_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  COGL_GST_TYPE_VIDEO_SINK))

#define COGL_GST_VIDEO_SINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  COGL_GST_TYPE_VIDEO_SINK, CoglGstVideoSinkClass))

#define COGL_GST_PARAM_STATIC        \
  (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

#define COGL_GST_PARAM_READABLE      \
  (G_PARAM_READABLE | COGL_GST_PARAM_STATIC)

#define COGL_GST_PARAM_WRITABLE      \
  (G_PARAM_WRITABLE | COGL_GST_PARAM_STATIC)

#define COGL_GST_PARAM_READWRITE     \
  (G_PARAM_READABLE | G_PARAM_WRITABLE | COGL_GST_PARAM_STATIC)

typedef struct _CoglGstVideoSink CoglGstVideoSink;
typedef struct _CoglGstVideoSinkClass CoglGstVideoSinkClass;
typedef struct _CoglGstVideoSinkPrivate CoglGstVideoSinkPrivate;

struct _CoglGstVideoSink
{
  GstBaseSink parent;
  CoglGstVideoSinkPrivate *priv;
};

struct _CoglGstVideoSinkClass
{
  GstBaseSinkClass parent_class;

  void (* new_frame) (CoglGstVideoSink *sink);
  void (* pipeline_ready) (CoglGstVideoSink *sink);

  void *_padding_dummy[8];
};

GType       cogl_gst_video_sink_get_type    (void) G_GNUC_CONST;

CoglGstVideoSink*
cogl_gst_video_sink_new (CoglContext *ctx);

CoglPipeline*
cogl_gst_video_sink_get_pipeline (CoglGstVideoSink *vt);

void
cogl_gst_video_sink_set_context (CoglGstVideoSink *vt,
                                 CoglContext *ctx);

GMainLoop*
cogl_gst_video_sink_get_main_loop (CoglGstVideoSink *loop);

int
cogl_gst_video_sink_get_free_layer (CoglGstVideoSink *sink);

int
cogl_gst_video_sink_attach_frame (CoglGstVideoSink *sink,
                                  CoglPipeline *pln);

G_END_DECLS

#endif
