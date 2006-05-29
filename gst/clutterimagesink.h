/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_CLUTTERIMAGESINK_H__
#define __GST_CLUTTERIMAGESINK_H__

#include <gst/video/gstvideosink.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <clutter/clutter.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <string.h>
#include <math.h>

G_BEGIN_DECLS

#define GST_TYPE_CLUTTERIMAGESINK \
  (gst_clutterimagesink_get_type())
#define GST_CLUTTERIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CLUTTERIMAGESINK, GstClutterImageSink))
#define GST_CLUTTERIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CLUTTERIMAGESINK, GstClutterImageSink))
#define GST_IS_CLUTTERIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CLUTTERIMAGESINK))
#define GST_IS_CLUTTERIMAGESINK_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CLUTTERIMAGESINK))

typedef struct _GstClutterContext GstClutterContext;
typedef struct _GstXWindow GstXWindow;

typedef struct _GstClutterImageBuffer GstClutterImageBuffer;
typedef struct _GstClutterImageBufferClass GstClutterImageBufferClass;

typedef struct _GstClutterImageSink GstClutterImageSink;
typedef struct _GstClutterImageSinkClass GstClutterImageSinkClass;

struct _GstClutterContext 
{
  Display *disp;
  Screen  *screen;
  gint     screen_num;
  gint width, height;
  gint widthmm, heightmm;
  GValue *par;                  /* calculated pixel aspect ratio */
  GstCaps *caps;
};

struct _GstClutterImageBuffer 
{
  GstBuffer buffer;

  /* Reference to the clutterimagesink we belong to */
  GstClutterImageSink *clutterimagesink;

  GdkPixbuf *clutterimage; 	/* FIXME: Rename */

  gint width, height;
  size_t size;
};

struct _GstClutterImageSink 
{
  /* Our element stuff */
  GstVideoSink videosink;

  char *display_name;

  GstClutterContext     *context;
  GstClutterImageBuffer *clutterimage;
  GstClutterImageBuffer *cur_image;
  
  GThread               *event_thread;
  gboolean running;

  /* Framerate numerator and denominator */
  gint                   fps_n;
  gint                   fps_d;

  /* object-set pixel aspect ratio */
  GValue                 *par;

  GMutex                 *x_lock; /* FIXME: rename */
  GMutex                 *flow_lock;
  
  GMutex                 *pool_lock;
  GSList                 *buffer_pool;

  ClutterVideoTexture    *video_texture;

  gboolean                synchronous;
  gboolean                keep_aspect;
};

struct _GstClutterImageSinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_clutterimagesink_get_type(void);

G_END_DECLS

#endif /* __GST_CLUTTERIMAGESINK_H__ */
