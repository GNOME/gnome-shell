/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

/**
 * SECTION:clutter-video-texture
 * @short_description: Actor for playback of video files.
 *
 * #ClutterVideoTexture is a #ClutterTexture that plays video files.
 */

#include "clutter-video-texture.h"
#include "clutter-main.h"
#include "clutter-private.h" 	/* for DBG */
#include "clutter-marshal.h"

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/audio/gstbaseaudiosink.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include <glib.h>

struct ClutterVideoTexturePrivate
{
  GstElement *playbin;
  char       *uri;
  gboolean    can_seek;
  int         buffer_percent;
  int         duration;
  gboolean    force_aspect_ratio;
  guint       tick_timeout_id;
};

enum {
  PROP_0,
  /* ClutterMedia proprs */
  PROP_URI,
  PROP_PLAYING,
  PROP_POSITION,
  PROP_VOLUME,
  PROP_CAN_SEEK,
  PROP_BUFFER_PERCENT,
  PROP_DURATION,

  /* Extra texture props */
  PROP_FORCE_ASPECT_RATIO
};


#define TICK_TIMEOUT 0.5

static void clutter_media_init (ClutterMediaInterface *iface);

static gboolean tick_timeout (ClutterVideoTexture *video_texture);

G_DEFINE_TYPE_EXTENDED (ClutterVideoTexture,                          \
			clutter_video_texture,                        \
			CLUTTER_TYPE_TEXTURE,                         \
			0,                                            \
			G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MEDIA,    \
					       clutter_media_init));

/* Interface implementation */

static void
set_uri (ClutterMedia *media,
	 const char   *uri)
{
  ClutterVideoTexture        *video_texture = CLUTTER_VIDEO_TEXTURE(media);
  ClutterVideoTexturePrivate *priv; 
  GstState                    state, pending;

  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

  if (!priv->playbin)
    return;

  if (priv->uri)
    g_free (priv->uri);

  if (uri) 
    {
      priv->uri = g_strdup (uri);
    
      /**
       * Ensure the tick timeout is installed.
       * 
       * We also have it installed in PAUSED state, because
       * seeks etc may have a delayed effect on the position.
       **/
    if (priv->tick_timeout_id == 0) 
      {
	priv->tick_timeout_id = g_timeout_add (TICK_TIMEOUT * 1000,
					       (GSourceFunc) tick_timeout,
					       video_texture);
      }
    } 
  else 
    {
      priv->uri = NULL;
    
      if (priv->tick_timeout_id > 0) 
	{
	  g_source_remove (priv->tick_timeout_id);
	  priv->tick_timeout_id = 0;
	}
    }
  
  priv->can_seek = FALSE;
  priv->duration = 0;

  gst_element_get_state (priv->playbin, &state, &pending, 0);

  if (pending)
    state = pending;
  
  gst_element_set_state (priv->playbin, GST_STATE_NULL);
  
  g_object_set (priv->playbin,
		"uri", uri,
		NULL);
  
  /**
   * Restore state.
   **/
  if (uri) 
    gst_element_set_state (priv->playbin, state);
  
  /*
   * Emit notififications for all these to make sure UI is not showing
   * any properties of the old URI.
   */
  g_object_notify (G_OBJECT (video_texture), "uri");
  g_object_notify (G_OBJECT (video_texture), "can-seek");
  g_object_notify (G_OBJECT (video_texture), "duration");
  g_object_notify (G_OBJECT (video_texture), "position");
  
}

static const char *
get_uri (ClutterMedia *media)
{
  ClutterVideoTexture        *video_texture = CLUTTER_VIDEO_TEXTURE(media);
  ClutterVideoTexturePrivate *priv; 

  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), NULL);

  priv = video_texture->priv;

  return priv->uri;
}

static void
set_playing (ClutterMedia *media,
	     gboolean      playing)
{
  ClutterVideoTexture         *video_texture = CLUTTER_VIDEO_TEXTURE(media);
  ClutterVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

  if (!priv->playbin)
    return;
        
  if (priv->uri) 
    {
      GstState state;
    
      if (playing)
	state = GST_STATE_PLAYING;
      else
	state = GST_STATE_PAUSED;
    
      gst_element_set_state (video_texture->priv->playbin, state);
    } 
  else 
    {
      if (playing)
	g_warning ("Tried to play, but no URI is loaded.");
    }
  
  g_object_notify (G_OBJECT (video_texture), "playing");
  g_object_notify (G_OBJECT (video_texture), "position");
}

static gboolean
get_playing (ClutterMedia *media)
{
  ClutterVideoTexture        *video_texture = CLUTTER_VIDEO_TEXTURE(media);
  ClutterVideoTexturePrivate *priv; 
  GstState                    state, pending;

  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), FALSE);
	
  priv = video_texture->priv;

  if (!priv->playbin)
    return FALSE;
  
  gst_element_get_state (priv->playbin, &state, &pending, 0);
  
  if (pending)
    return (pending == GST_STATE_PLAYING);
  else
    return (state == GST_STATE_PLAYING);
}

static void
set_position (ClutterMedia *media,
	      int           position) /* seconds */
{
  ClutterVideoTexture        *video_texture = CLUTTER_VIDEO_TEXTURE(media);
  ClutterVideoTexturePrivate *priv; 
  GstState                    state, pending;

  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

  if (!priv->playbin)
    return;

  gst_element_get_state (priv->playbin, &state, &pending, 0);

  if (pending)
    state = pending;

  gst_element_set_state (priv->playbin, GST_STATE_PAUSED);

  gst_element_seek (priv->playbin,
		    1.0,
		    GST_FORMAT_TIME,
		    GST_SEEK_FLAG_FLUSH,
		    GST_SEEK_TYPE_SET,
		    position * GST_SECOND,
		    0, 0);

  gst_element_set_state (priv->playbin, state);
}

static int
get_position (ClutterMedia *media)
{
  ClutterVideoTexture        *video_texture = CLUTTER_VIDEO_TEXTURE(media);
  ClutterVideoTexturePrivate *priv; 
  GstQuery                   *query;
  gint64                      position;

  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), -1);

  priv = video_texture->priv;

  if (!priv->playbin)
    return -1;
  
  query = gst_query_new_position (GST_FORMAT_TIME);
  
  if (gst_element_query (priv->playbin, query)) 
    gst_query_parse_position (query, NULL, &position);
  else
    position = 0;
  
  gst_query_unref (query);
  
  return (position / GST_SECOND);
}

static void
set_volume (ClutterMedia *media,
	    double        volume)
{
  ClutterVideoTexture        *video_texture = CLUTTER_VIDEO_TEXTURE(media);
  ClutterVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture));
  // g_return_if_fail (volume >= 0.0 && volume <= GST_VOL_MAX);

  priv = video_texture->priv;
  
  if (!priv->playbin)
    return;
  
  g_object_set (G_OBJECT (video_texture->priv->playbin),
		"volume", volume,
		NULL);
  
  g_object_notify (G_OBJECT (video_texture), "volume");
}

static double
get_volume (ClutterMedia *media)
{
  ClutterVideoTexture        *video_texture = CLUTTER_VIDEO_TEXTURE(media);
  ClutterVideoTexturePrivate *priv; 
  double                      volume;

  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), 0.0);

  priv = video_texture->priv;
  
  if (!priv->playbin)
    return 0.0;

  g_object_get (priv->playbin,
		"volume", &volume,
		NULL);
  
  return volume;
}

static gboolean
can_seek (ClutterMedia *media)
{
  ClutterVideoTexture        *video_texture = CLUTTER_VIDEO_TEXTURE(media);

  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), FALSE);
  
  return video_texture->priv->can_seek;
}

static int
get_buffer_percent (ClutterMedia *media)
{
  ClutterVideoTexture         *video_texture = CLUTTER_VIDEO_TEXTURE(media);

  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), -1);
  
  return video_texture->priv->buffer_percent;
}

static int
get_duration (ClutterMedia *media)
{
  ClutterVideoTexture         *video_texture = CLUTTER_VIDEO_TEXTURE(media);

  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), -1);

  return video_texture->priv->duration;
}

static void
clutter_media_init (ClutterMediaInterface *iface)
{
  iface->set_uri            = set_uri;
  iface->get_uri            = get_uri;
  iface->set_playing        = set_playing;
  iface->get_playing        = get_playing;
  iface->set_position       = set_position;
  iface->get_position       = get_position;
  iface->set_volume         = set_volume;
  iface->get_volume         = get_volume;
  iface->can_seek           = can_seek;
  iface->get_buffer_percent = get_buffer_percent;
  iface->get_duration       = get_duration;
}

static void
clutter_video_texture_dispose (GObject *object)
{
  ClutterVideoTexture        *self;
  ClutterVideoTexturePrivate *priv; 

  self = CLUTTER_VIDEO_TEXTURE(object); 
  priv = self->priv;

  /* FIXME: flush an errors off bus ? */
  /* gst_bus_set_flushing (priv->bus, TRUE); */

  if (priv->playbin) 
    {
      gst_element_set_state (priv->playbin, GST_STATE_NULL);
      gst_object_unref (GST_OBJECT (priv->playbin));
      priv->playbin = NULL;
    }

  if (priv->tick_timeout_id > 0) 
    {
      g_source_remove (priv->tick_timeout_id);
      priv->tick_timeout_id = 0;
    }

  G_OBJECT_CLASS (clutter_video_texture_parent_class)->dispose (object);
}

static void
clutter_video_texture_finalize (GObject *object)
{
  ClutterVideoTexture        *self;
  ClutterVideoTexturePrivate *priv; 

  self = CLUTTER_VIDEO_TEXTURE(object); 
  priv = self->priv;

  if (priv->uri)
    g_free(priv->uri);

  G_OBJECT_CLASS (clutter_video_texture_parent_class)->finalize (object);
}

static void
clutter_video_texture_set_property (GObject      *object, 
				    guint         property_id,
				    const GValue *value, 
				    GParamSpec   *pspec)
{
  ClutterVideoTexture *video_texture;

  video_texture = CLUTTER_VIDEO_TEXTURE(object);

  switch (property_id)
    {
    case PROP_URI:
      clutter_media_set_uri (CLUTTER_MEDIA(video_texture), 
			     g_value_get_string (value));
      break;
    case PROP_PLAYING:
      clutter_media_set_playing (CLUTTER_MEDIA(video_texture),
				 g_value_get_boolean (value));
      break;
    case PROP_POSITION:
      clutter_media_set_position (CLUTTER_MEDIA(video_texture),
				  g_value_get_int (value));
      break;
    case PROP_VOLUME:
      clutter_media_set_volume (CLUTTER_MEDIA(video_texture),
				g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_video_texture_get_property (GObject    *object, 
				    guint       property_id,
				    GValue     *value, 
				    GParamSpec *pspec)
{
  ClutterVideoTexture *video_texture;
  ClutterMedia        *media;

  video_texture = CLUTTER_VIDEO_TEXTURE (object);
  media         = CLUTTER_MEDIA (video_texture);

  switch (property_id)
    {
    case PROP_URI:
      g_value_set_string (value, clutter_media_get_uri (media));
      break;
    case PROP_PLAYING:
      g_value_set_boolean (value, clutter_media_get_playing (media));
      break;
    case PROP_POSITION:
      g_value_set_int (value, clutter_media_get_position (media));
      break;
    case PROP_VOLUME:
      g_value_set_double (value, clutter_media_get_volume (media));
      break;
    case PROP_CAN_SEEK:
      g_value_set_boolean (value, clutter_media_get_can_seek (media));
      break;
    case PROP_BUFFER_PERCENT:
      g_value_set_int (value, clutter_media_get_buffer_percent (media));
      break;
    case PROP_DURATION:
      g_value_set_int (value, clutter_media_get_duration (media));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_video_texture_class_init (ClutterVideoTextureClass *klass)
{
  GObjectClass        *object_class;
  ClutterActorClass *actor_class;

  object_class = (GObjectClass*)klass;
  actor_class = (ClutterActorClass*)klass;

  object_class->dispose      = clutter_video_texture_dispose;
  object_class->finalize     = clutter_video_texture_finalize;
  object_class->set_property = clutter_video_texture_set_property;
  object_class->get_property = clutter_video_texture_get_property;

  /* Interface props */

  g_object_class_override_property (object_class, PROP_URI, "uri");
  g_object_class_override_property (object_class, PROP_PLAYING, "playing");
  g_object_class_override_property (object_class, PROP_POSITION, "position");
  g_object_class_override_property (object_class, PROP_VOLUME, "volume");
  g_object_class_override_property (object_class, PROP_CAN_SEEK, "can-seek");
  g_object_class_override_property (object_class, PROP_DURATION, "duration");
  g_object_class_override_property (object_class, PROP_BUFFER_PERCENT, 
				    "buffer-percent" );
}

static void
bus_message_error_cb (GstBus            *bus,
                      GstMessage        *message,
                      ClutterVideoTexture *video_texture)
{
  GError *error;

  error = NULL;
  gst_message_parse_error (message, &error, NULL);
        
  g_signal_emit_by_name (CLUTTER_MEDIA(video_texture), "error", error);

  g_error_free (error);
}

static void
bus_message_eos_cb (GstBus            *bus,
                    GstMessage        *message,
                    ClutterVideoTexture *video_texture)
{
  g_object_notify (G_OBJECT (video_texture), "position");

  g_signal_emit_by_name (CLUTTER_MEDIA(video_texture), "eos");
}

static void
bus_message_tag_cb (GstBus            *bus,
                    GstMessage        *message,
                    ClutterVideoTexture *video_texture)
{
  GstTagList *tag_list;

  gst_message_parse_tag (message, &tag_list);

  g_signal_emit_by_name (CLUTTER_MEDIA(video_texture), 
			 "metadata-available", 
			 tag_list);
  
  gst_tag_list_free (tag_list);
}

static void
bus_message_buffering_cb (GstBus            *bus,
                          GstMessage        *message,
                          ClutterVideoTexture *video_texture)
{
  const GstStructure *str;
  
  str = gst_message_get_structure (message);
  if (!str)
    return;

  if (!gst_structure_get_int (str,
			      "buffer-percent",
			      &video_texture->priv->buffer_percent))
    return;
        
  g_object_notify (G_OBJECT (video_texture), "buffer-percent");
}

static void
bus_message_duration_cb (GstBus            *bus,
                         GstMessage        *message,
                         ClutterVideoTexture *video_texture)
{
  GstFormat format;
  gint64 duration;

  gst_message_parse_duration (message,
			      &format,
			      &duration);

  if (format != GST_FORMAT_TIME)
    return;
  
  video_texture->priv->duration = duration / GST_SECOND;

  g_object_notify (G_OBJECT (video_texture), "duration");
}

static void
bus_message_actor_cb (GstBus            *bus,
                        GstMessage        *message,
                        ClutterVideoTexture *video_texture)
{
  const GstStructure *str;

  str = gst_message_get_structure (message);
  if (!str)
    return;
}

static void
bus_message_state_change_cb (GstBus            *bus,
                             GstMessage        *message,
                             ClutterVideoTexture *video_texture)
{
  gpointer src;
  GstState old_state, new_state;

  src = GST_MESSAGE_SRC (message);
        
  if (src != video_texture->priv->playbin)
    return;

  gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

  if (old_state == GST_STATE_READY && 
      new_state == GST_STATE_PAUSED) 
    {
      GstQuery *query;
      
      /**
       * Determine whether we can seek.
       **/
      query = gst_query_new_seeking (GST_FORMAT_TIME);
      
      if (gst_element_query (video_texture->priv->playbin, query)) {
	gst_query_parse_seeking (query,
				 NULL,
				 &video_texture->priv->can_seek,
				 NULL,
				 NULL);
      } else {
	/*
	 * Could not query for ability to seek. Determine
	 * using URI.
	 */
	
	if (g_str_has_prefix (video_texture->priv->uri,
			      "http://")) {
	  video_texture->priv->can_seek = FALSE;
	} else {
	  video_texture->priv->can_seek = TRUE;
	}
      }
      
      gst_query_unref (query);
      
      g_object_notify (G_OBJECT (video_texture), "can-seek");
      
      /**
       * Determine the duration.
       **/
      query = gst_query_new_duration (GST_FORMAT_TIME);
      
      if (gst_element_query (video_texture->priv->playbin, query)) 
	{
	  gint64 duration;
	  
	  gst_query_parse_duration (query, NULL, &duration);

	  video_texture->priv->duration = duration / GST_SECOND;
                        
	  g_object_notify (G_OBJECT (video_texture), "duration");
	}

      gst_query_unref (query);
    }
}

static gboolean
tick_timeout (ClutterVideoTexture *video_texture)
{
  g_object_notify (G_OBJECT (video_texture), "position");

  return TRUE;
}

static void
fakesink_handoff_cb (GstElement *fakesrc, 
		     GstBuffer  *buffer,
		     GstPad     *pad, 
		     gpointer    user_data)
{
  GstStructure  *structure;
  int            width, height;
  GdkPixbuf     *pixb;

  structure = gst_caps_get_structure(GST_CAPS(buffer->caps), 0);
  gst_structure_get_int(structure, "width", &width);
  gst_structure_get_int(structure, "height", &height);

  /* FIXME: We really dont want to do this every time as gobject creation
   *        really need a clutter_texture_set_from_data call ?
  */
  pixb = gdk_pixbuf_new_from_data (GST_BUFFER_DATA (buffer),
				   GDK_COLORSPACE_RGB, 
				   FALSE, 
				   8, 
				   width, 
				   height,
				   (3 * width + 3) &~ 3,
				   NULL,
				   NULL);
  
  if (pixb)
    {
      clutter_texture_set_pixbuf (CLUTTER_TEXTURE(user_data), pixb);
      g_object_unref(G_OBJECT(pixb));
    }
}

static gboolean
lay_pipeline (ClutterVideoTexture *video_texture)
{
  ClutterVideoTexturePrivate *priv;
  GstElement                 *audio_sink = NULL;
  GstElement                  *video_sink, *video_bin, *video_capsfilter;
  GstCaps                    *video_filtercaps;
  GstPad                     *video_ghost_pad;

  priv = video_texture->priv;

  priv->playbin = gst_element_factory_make ("playbin", "playbin");

  if (!priv->playbin) 
    {
      g_warning ("Unable to create playbin GST actor.");
      return FALSE;
    }

  audio_sink = gst_element_factory_make ("gconfaudiosink", "audio-sink");
  if (!audio_sink) 
    {
      audio_sink = gst_element_factory_make ("autoaudiosink", "audio-sink");
      if (!audio_sink) 
	{
	  audio_sink = gst_element_factory_make ("alsasink", "audio-sink");
	  g_warning ("Could not create a GST audio_sink. "
		     "Audio unavailable.");

	  if (!audio_sink) 	/* Need to bother ? */
	    audio_sink = gst_element_factory_make ("fakesink", "audio-sink");
	}
    }

  video_sink = gst_element_factory_make ("fakesink", "fakesink");

  if (video_sink == NULL) 
    {
      g_warning ("Could not create actor 'fakesink' for video playback");
      priv->playbin = NULL;
      return FALSE;
    }

  video_bin = gst_bin_new  ("video-bin");

  video_capsfilter = gst_element_factory_make ("capsfilter", 
					       "video-capsfilter");

  video_filtercaps 
    = gst_caps_new_simple("video/x-raw-rgb",
			  "bpp", G_TYPE_INT, 24,
			  "depth", G_TYPE_INT, 24,
			  "endianness", G_TYPE_INT, G_BIG_ENDIAN, 
			  /* >> 8 for 24bpp */ 
			  "red_mask", G_TYPE_INT, 0xff0000,
			  "green_mask", G_TYPE_INT, 0xff00,
			  "blue_mask", G_TYPE_INT,  0xff,
			  "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
			  "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
			  "framerate", GST_TYPE_FRACTION_RANGE, 
			  0, 1, G_MAXINT, 1, 
			  NULL);

  g_object_set(G_OBJECT(video_capsfilter), 
	       "caps", video_filtercaps, 
	       NULL);

  gst_bin_add(GST_BIN(video_bin), video_capsfilter);
  gst_bin_add(GST_BIN(video_bin), video_sink);

  gst_element_link (video_capsfilter, video_sink);

  video_ghost_pad = gst_ghost_pad_new ("sink", 
				       gst_element_get_pad (video_capsfilter, 
							    "sink"));
  gst_element_add_pad (video_bin, video_ghost_pad);

  g_object_set (G_OBJECT(video_sink), 
		"signal-handoffs", TRUE, 
		"sync", TRUE,
		NULL);

  g_signal_connect(G_OBJECT (video_sink), 
		   "handoff",
		   G_CALLBACK(fakesink_handoff_cb), 
		   video_texture);

  g_object_set (G_OBJECT (priv->playbin),
		"video-sink", video_bin,
		"audio-sink", audio_sink,
		NULL);

  return TRUE;
}

static void
clutter_video_texture_init (ClutterVideoTexture *video_texture)
{
  ClutterVideoTexturePrivate *priv;
  GstBus                     *bus;

  priv                 = g_new0 (ClutterVideoTexturePrivate, 1);
  video_texture->priv  = priv;

  if (!lay_pipeline(video_texture))
    {
      g_warning("Failed to initiate suitable playback pipeline.");
      return;
    }

  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->playbin));

  gst_bus_add_signal_watch (bus);

  g_signal_connect_object (bus,
			   "message::error",
			   G_CALLBACK (bus_message_error_cb),
			   video_texture,
			   0);

  g_signal_connect_object (bus,
			   "message::eos",
			   G_CALLBACK (bus_message_eos_cb),
			   video_texture,
			   0);

  g_signal_connect_object (bus,
			   "message::tag",
			   G_CALLBACK (bus_message_tag_cb),
			   video_texture,
			   0);

  g_signal_connect_object (bus,
			   "message::buffering",
			   G_CALLBACK (bus_message_buffering_cb),
			   video_texture,
			   0);

  g_signal_connect_object (bus,
			   "message::duration",
			   G_CALLBACK (bus_message_duration_cb),
			   video_texture,
			   0);

  g_signal_connect_object (bus,
			   "message::actor",
			   G_CALLBACK (bus_message_actor_cb),
			   video_texture,
			   0);

  g_signal_connect_object (bus,
			   "message::state-changed",
			   G_CALLBACK (bus_message_state_change_cb),
			   video_texture,
			   0);

  gst_object_unref (GST_OBJECT (bus));

  return;
}

/**
 * clutter_video_texture_new:
 *
 * Creates a video texture.
 *
 * Return value: A #ClutterActor implementing a displaying a video texture.
 */
ClutterActor*
clutter_video_texture_new (void)
{
  ClutterVideoTexture        *video_texture;

  video_texture = g_object_new (CLUTTER_TYPE_VIDEO_TEXTURE, 
				"tiled", FALSE, 
				"pixel-format", GL_RGB,
				NULL);

  return CLUTTER_ACTOR(video_texture);
}

