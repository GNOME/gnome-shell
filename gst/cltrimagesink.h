#ifndef __GST_CLTRIMAGESINK_H__
#define __GST_CLTRIMAGESINK_H__

#include <gst/video/videosink.h>
#include <clutter/cltr.h>

G_BEGIN_DECLS

#define GST_TYPE_CLTRIMAGESINK \
  (gst_cltrimagesink_get_type())
#define GST_CLTRIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CLTRIMAGESINK, GstCltrimageSink))
#define GST_CLTRIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CLTRIMAGESINK, GstCltrimageSink))
#define GST_IS_CLTRIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CLTRIMAGESINK))
#define GST_IS_CLTRIMAGESINK_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CLTRIMAGESINK))

typedef struct _GstCltrimageSink GstCltrimageSink;
typedef struct _GstCltrimageSinkClass GstCltrimageSinkClass;

struct _GstCltrimageSink 
{
  /* Our element stuff */
  GstVideoSink videosink;
  
  CltrTexture *texture;
  
  int pixel_width, pixel_height;

  gdouble framerate;
  GMutex *x_lock;
  
  GstClockTime time;
  
  GMutex *pool_lock;
  GSList *image_pool;

  GstCaps *caps;

  CltrWidget *widget;
};

struct _GstCltrimageSinkClass {
  GstVideoSinkClass parent_class;

  /* signals */
  void (*handoff)     (GstElement *element, GstBuffer *buf, GstPad *pad);
  void (*bufferalloc) (GstElement *element, GstBuffer *buf, GstPad *pad);
};

GType gst_cltrimagesink_get_type(void); /* XXX needed ? */

G_END_DECLS

#endif /* __GST_CLTRIMAGESINK_H__ */
