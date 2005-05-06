#ifndef _HAVE_CLTR_VIDEO_H
#define _HAVE_CLTR_VIDEO_H

#include "cltr.h"

#include <gst/play/play.h>
#include <gst/gconf/gconf.h>

typedef struct CltrVideo CltrVideo;

/* Signals - cltrimagesink needs to deliver texture signals :/ */

enum {
  CLTR_VIDEO_ASYNC_TEXTURE,
  CLTR_VIDEO_ASYNC_VIDEO_SIZE,
  CLTR_VIDEO_ASYNC_ERROR,
  CLTR_VIDEO_ASYNC_FOUND_TAG,
  CLTR_VIDEO_ASYNC_NOTIFY_STREAMINFO,
  CLTR_VIDEO_ASYNC_EOS,
  CLTR_VIDEO_ASYNC_BUFFERING,
  CLTR_VIDEO_ASYNC_REDIRECT
};

typedef struct CltrVideoSignal
{
  gint signal_id;
  union
  {
    struct
    {
      gint width;
      gint height;
    } video_size;
    struct
    {
      GstElement *element;
      GError *error;
      char *debug_message;
    } error;
    struct
    {
      GstElement *source;
      GstTagList *tag_list;
    } found_tag;
    struct
    {
      gint percent;
    } buffering;
    struct
    {
      gchar *new_location;
    } redirect;
    struct
    {
      CltrTexture *ref;
    } texture;
  } signal_data;
}
CltrVideoSignal;

#define CLTR_VIDEO(w) ((CltrVideo*)(w))

CltrWidget*
cltr_video_new(int width, int height);

gboolean
cltr_video_set_source(CltrVideo *video, char *location);

gboolean
cltr_video_play ( CltrVideo *video, GError ** Error);

gboolean
cltr_video_seek (CltrVideo *video, float position, GError **gerror);

gboolean
cltr_video_seek_time (CltrVideo *video, gint64 time, GError **gerror);

void
cltr_video_stop ( CltrVideo *video);

void
cltr_video_close ( CltrVideo *video);

void
cltr_video_pause ( CltrVideo *video);

gboolean
cltr_video_can_set_volume ( CltrVideo *video );

void
cltr_video_set_volume ( CltrVideo *video, int volume);

int
cltr_video_get_volume ( CltrVideo *video);


#endif
