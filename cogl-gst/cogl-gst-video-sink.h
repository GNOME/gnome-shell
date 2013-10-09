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
 */

#ifndef __COGL_GST_VIDEO_SINK_H__
#define __COGL_GST_VIDEO_SINK_H__
#include <glib-object.h>
#include <gst/base/gstbasesink.h>

/* We just need the public Cogl api for cogl-gst but we first need to
 * undef COGL_COMPILATION to avoid getting an error that normally
 * checks cogl.h isn't used internally. */
#ifdef COGL_COMPILATION
#undef COGL_COMPILATION
#endif

#include <cogl/cogl.h>

#include <cogl/cogl.h>

/**
 * SECTION:cogl-gst-video-sink
 * @short_description: A video sink for integrating a GStreamer
 *   pipeline with a Cogl pipeline.
 *
 * #CoglGstVideoSink is a subclass of #GstBaseSink which can be used to
 * create a #CoglPipeline for rendering the frames of the video.
 *
 * To create a basic video player, an application can create a
 * #GstPipeline as normal using gst_pipeline_new() and set the
 * sink on it to one created with cogl_gst_video_sink_new(). The
 * application can then listen for the #CoglGstVideoSink::new-frame
 * signal which will be emitted whenever there are new textures ready
 * for rendering. For simple rendering, the application can just call
 * cogl_gst_video_sink_get_pipeline() in the signal handler and use
 * the returned pipeline to paint the new frame.
 *
 * An application is also free to do more advanced rendering by
 * customizing the pipeline. In that case it should listen for the
 * #CoglGstVideoSink::pipeline-ready signal which will be emitted as
 * soon as the sink has determined enough information about the video
 * to know how it should be rendered. In the handler for this signal,
 * the application can either make modifications to a copy of the
 * pipeline returned by cogl_gst_video_sink_get_pipeline() or it can
 * create its own pipeline from scratch and ask the sink to configure
 * it with cogl_gst_video_sink_setup_pipeline(). If a custom pipeline
 * is created using one of these methods then the application should
 * call cogl_gst_video_sink_attach_frame() on the pipeline before
 * rendering in order to update the textures on the pipeline's layers.
 *
 * If the %COGL_FEATURE_ID_GLSL feature is available then the pipeline
 * used by the sink will have a shader snippet with a function in it
 * called cogl_gst_sample_video0 which takes a single vec2 argument.
 * This can be used by custom snippets set the by the application to
 * sample from the video. The vec2 argument represents the normalised
 * coordinates within the video.
 *
 * Since: 1.16
 */

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

typedef struct _CoglGstVideoSink CoglGstVideoSink;
typedef struct _CoglGstVideoSinkClass CoglGstVideoSinkClass;
typedef struct _CoglGstVideoSinkPrivate CoglGstVideoSinkPrivate;

/**
 * CoglGstVideoSink:
 *
 * The #CoglGstVideoSink structure contains only private data and
 * should be accessed using the provided API.
 *
 * Since: 1.16
 */
struct _CoglGstVideoSink
{
  /*< private >*/
  GstBaseSink parent;
  CoglGstVideoSinkPrivate *priv;
};

/**
 * CoglGstVideoSinkClass:
 * @new_frame: handler for the #CoglGstVideoSink::new-frame signal
 * @pipeline_ready: handler for the #CoglGstVideoSink::pipeline-ready signal
 *
 * Since: 1.16
 */

/**
 * CoglGstVideoSink::new-frame:
 * @sink: the #CoglGstVideoSink
 *
 * The sink will emit this signal whenever there are new textures
 * available for a new frame of the video. After this signal is
 * emitted, an application can call cogl_gst_video_sink_get_pipeline()
 * to get a pipeline suitable for rendering the frame. If the
 * application is using a custom pipeline it can alternatively call
 * cogl_gst_video_sink_attach_frame() to attach the textures.
 *
 * Since: 1.16
 */

/**
 * CoglGstVideoSink::pipeline-ready:
 * @sink: the #CoglGstVideoSink
 *
 * The sink will emit this signal as soon as it has gathered enough
 * information from the video to configure a pipeline. If the
 * application wants to do some customized rendering, it can setup its
 * pipeline after this signal is emitted. The application's pipeline
 * will typically either be a copy of the one returned by
 * cogl_gst_video_sink_get_pipeline() or it can be a completely custom
 * pipeline which is setup using cogl_gst_video_sink_setup_pipeline().
 *
 * Note that it is an error to call either of those functions before
 * this signal is emitted. The #CoglGstVideoSink::new-frame signal
 * will only be emitted after the pipeline is ready so the application
 * could also create its pipeline in the handler for that.
 *
 * Since: 1.16
 */

struct _CoglGstVideoSinkClass
{
  /*< private >*/
  GstBaseSinkClass parent_class;

  /*< public >*/
  void (* new_frame) (CoglGstVideoSink *sink);
  void (* pipeline_ready) (CoglGstVideoSink *sink);

  /*< private >*/
  void *_padding_dummy[8];
};

GType
cogl_gst_video_sink_get_type (void) G_GNUC_CONST;

/**
 * cogl_gst_video_sink_new:
 * @ctx: The #CoglContext
 *
 * Creates a new #CoglGstVideoSink which will create resources for use
 * with the given context.
 *
 * Return value: (transfer full): a new #CoglGstVideoSink
 * Since: 1.16
 */
CoglGstVideoSink *
cogl_gst_video_sink_new (CoglContext *ctx);

/**
 * cogl_gst_video_sink_is_ready:
 * @sink: The #CoglGstVideoSink
 *
 * Returns whether the pipeline is ready and so
 * cogl_gst_video_sink_get_pipeline() and
 * cogl_gst_video_sink_setup_pipeline() can be called without causing error.
 *
 * Note: Normally an application will wait until the
 * #CoglGstVideoSink::pipeline-ready signal is emitted instead of
 * polling the ready status with this api, but sometimes when a sink
 * is passed between components that didn't have an opportunity to
 * connect a signal handler this can be useful.
 *
 * Return value: %TRUE if the sink is ready, else %FALSE
 * Since: 1.16
 */
CoglBool
cogl_gst_video_sink_is_ready (CoglGstVideoSink *sink);

/**
 * cogl_gst_video_sink_get_pipeline:
 * @vt: The #CoglGstVideoSink
 *
 * Returns a pipeline suitable for rendering the current frame of the
 * given video sink. The pipeline will already have the textures for
 * the frame attached. For simple rendering, an application will
 * typically call this function immediately before it paints the
 * video. It can then just paint a rectangle using the returned
 * pipeline.
 *
 * An application is free to make a copy of this
 * pipeline and modify it for custom rendering.
 *
 * Note: it is considered an error to call this function before the
 * #CoglGstVideoSink::pipeline-ready signal is emitted.
 *
 * Return value: (transfer none): the pipeline for rendering the
 *   current frame
 * Since: 1.16
 */
CoglPipeline *
cogl_gst_video_sink_get_pipeline (CoglGstVideoSink *vt);

/**
 * cogl_gst_video_sink_set_context:
 * @vt: The #CoglGstVideoSink
 * @ctx: The #CoglContext for the sink to use
 *
 * Sets the #CoglContext that the video sink should use for creating
 * any resources. This function would normally only be used if the
 * sink was constructed via gst_element_factory_make() instead of
 * cogl_gst_video_sink_new().
 *
 * Since: 1.16
 */
void
cogl_gst_video_sink_set_context (CoglGstVideoSink *vt,
                                 CoglContext *ctx);

/**
 * cogl_gst_video_sink_get_free_layer:
 * @sink: The #CoglGstVideoSink
 *
 * This can be used when doing specialised rendering of the video by
 * customizing the pipeline. #CoglGstVideoSink may use up to three
 * private layers on the pipeline in order to attach the textures of
 * the video frame. This function will return the index of the next
 * available unused layer after the sink's internal layers. This can
 * be used by the application to add additional layers, for example to
 * blend in another color in the fragment processing.
 *
 * Return value: the index of the next available layer after the
 *   sink's internal layers.
 * Since: 1.16
 */
int
cogl_gst_video_sink_get_free_layer (CoglGstVideoSink *sink);

/**
 * cogl_gst_video_sink_attach_frame:
 * @sink: The #CoglGstVideoSink
 * @pln: A #CoglPipeline
 *
 * Updates the given pipeline with the textures for the current frame.
 * This can be used if the application wants to customize the
 * rendering using its own pipeline. Typically this would be called in
 * response to the #CoglGstVideoSink::new-frame signal which is
 * emitted whenever the new textures are available. The application
 * would then make a copy of its template pipeline and call this to
 * set the textures.
 *
 * Since: 1.16
 */
void
cogl_gst_video_sink_attach_frame (CoglGstVideoSink *sink,
                                  CoglPipeline *pln);

/**
 * cogl_gst_video_sink_set_first_layer:
 * @sink: The #CoglGstVideoSink
 * @first_layer: The new first layer
 *
 * Sets the index of the first layer that the sink will use for its
 * rendering. This is useful if the application wants to have custom
 * layers that appear before the layers added by the sink. In that
 * case by default the sink's layers will be modulated with the result
 * of the application's layers that come before @first_layer.
 *
 * Note that if this function is called then the name of the function
 * to call in the shader snippets to sample the video will also
 * change. For example, if @first_layer is three then the function
 * will be cogl_gst_sample_video3.
 *
 * Since: 1.16
 */
void
cogl_gst_video_sink_set_first_layer (CoglGstVideoSink *sink,
                                     int first_layer);

/**
 * cogl_gst_video_sink_set_default_sample:
 * @sink: The #CoglGstVideoSink
 * @default_sample: Whether to add the default sampling
 *
 * By default the pipeline generated by
 * cogl_gst_video_sink_setup_pipeline() and
 * cogl_gst_video_sink_get_pipeline() will have a layer with a shader
 * snippet that automatically samples the video. If the application
 * wants to sample the video in a completely custom way using its own
 * shader snippet it can set @default_sample to %FALSE to avoid this
 * default snippet being added. In that case the application's snippet
 * can call cogl_gst_sample_video0 to sample the texture itself.
 *
 * Since: 1.16
 */
void
cogl_gst_video_sink_set_default_sample (CoglGstVideoSink *sink,
                                        CoglBool default_sample);

/**
 * cogl_gst_video_sink_setup_pipeline:
 * @sink: The #CoglGstVideoSink
 * @pipeline: A #CoglPipeline
 *
 * Configures the given pipeline so that will be able to render the
 * video for the @sink. This should only be used if the application
 * wants to perform some custom rendering using its own pipeline.
 * Typically an application will call this in response to the
 * #CoglGstVideoSink::pipeline-ready signal.
 *
 * Note: it is considered an error to call this function before the
 * #CoglGstVideoSink::pipeline-ready signal is emitted.
 *
 * Since: 1.16
 */
void
cogl_gst_video_sink_setup_pipeline (CoglGstVideoSink *sink,
                                    CoglPipeline *pipeline);

/**
 * cogl_gst_video_sink_get_aspect:
 * @sink: A #CoglGstVideoSink
 *
 * Returns a width-for-height aspect ratio that lets you calculate a
 * suitable width for displaying your video based on a given height by
 * multiplying your chosen height by the returned aspect ratio.
 *
 * This aspect ratio is calculated based on the underlying size of the
 * video buffers and the current pixel-aspect-ratio.
 *
 * Return value: a width-for-height aspect ratio
 *
 * Since: 1.16
 * Stability: unstable
 */
float
cogl_gst_video_sink_get_aspect (CoglGstVideoSink *sink);

/**
 * cogl_gst_video_sink_get_width_for_height:
 * @sink: A #CoglGstVideoSink
 * @height: A specific output @height
 *
 * Calculates a suitable output width for a specific output @height
 * that will maintain the video's aspect ratio.
 *
 * Return value: An output width for the given output @height.
 *
 * Since: 1.16
 * Stability: unstable
 */
float
cogl_gst_video_sink_get_width_for_height (CoglGstVideoSink *sink,
                                          float height);

/**
 * cogl_gst_video_sink_get_height_for_width:
 * @sink: A #CoglGstVideoSink
 * @width: A specific output @width
 *
 * Calculates a suitable output height for a specific output @width
 * that will maintain the video's aspect ratio.
 *
 * Return value: An output height for the given output @width.
 *
 * Since: 1.16
 * Stability: unstable
 */
float
cogl_gst_video_sink_get_height_for_width (CoglGstVideoSink *sink,
                                          float width);

/**
 * CoglGstRectangle:
 * @x: The X coordinate of the top left of the rectangle
 * @y: The Y coordinate of the top left of the rectangle
 * @width: The width of the rectangle
 * @height: The height of the rectangle
 *
 * Describes a rectangle that can be used for video output.
 */
typedef struct _CoglGstRectangle
{
  float x;
  float y;
  float width;
  float height;
} CoglGstRectangle;

/**
 * cogl_gst_video_sink_fit_size:
 * @sink: A #CoglGstVideoSink
 * @available: (in): The space available for video output
 * @output: (inout): The return location for the calculated output position
 *
 * Calculates a suitable @output rectangle that can fit inside the
 * @available space while maintaining the aspect ratio of the current
 * video.
 *
 * Applications would typically use this api for "letterboxing" by
 * using this api to position a video inside a fixed screen space and
 * filling the remaining space with black borders.
 *
 * Since: 1.16
 * Stability: unstable
 */
void
cogl_gst_video_sink_fit_size (CoglGstVideoSink *sink,
                              const CoglGstRectangle *available,
                              CoglGstRectangle *output);

G_END_DECLS

#endif
