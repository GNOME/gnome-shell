/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
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
 * SECTION:clutter-audio
 * @short_description: Object for playback of audio files.
 *
 * #ClutterAudio is an object that plays audio files.
 */

#include "clutter-audio.h"
#include "clutter-main.h"
#include "clutter-private.h" 	/* for DBG */
#include "clutter-marshal.h"

#include <gst/gst.h>
#include <gst/audio/gstbaseaudiosink.h>

#include <glib.h>

struct _ClutterAudioPrivate
{
  GstElement *playbin;
  char       *uri;
  gboolean    can_seek;
  int         buffer_percent;
  int         duration;
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
  PROP_DURATION
};


#define TICK_TIMEOUT 0.5

static void clutter_media_init (ClutterMediaInterface *iface);

static gboolean tick_timeout (ClutterAudio *audio);

G_DEFINE_TYPE_EXTENDED (ClutterAudio,                          \
			clutter_audio,                        \
			G_TYPE_OBJECT,                         \
			0,                                            \
			G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MEDIA,    \
					       clutter_media_init));

/* Interface implementation */

static void
set_uri (ClutterMedia *media,
	 const char   *uri)
{
  ClutterAudio        *audio = CLUTTER_AUDIO(media);
  ClutterAudioPrivate *priv; 
  GstState                    state, pending;

  g_return_if_fail (CLUTTER_IS_AUDIO (audio));

  priv = audio->priv;

  if (!priv->playbin)
    return;

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
					       audio);
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
  g_object_notify (G_OBJECT (audio), "uri");
  g_object_notify (G_OBJECT (audio), "can-seek");
  g_object_notify (G_OBJECT (audio), "duration");
  g_object_notify (G_OBJECT (audio), "position");
  
}

static const char *
get_uri (ClutterMedia *media)
{
  ClutterAudio        *audio = CLUTTER_AUDIO(media);
  ClutterAudioPrivate *priv; 

  g_return_val_if_fail (CLUTTER_IS_AUDIO (audio), NULL);

  priv = audio->priv;

  return priv->uri;
}

static void
set_playing (ClutterMedia *media,
	     gboolean      playing)
{
  ClutterAudio         *audio = CLUTTER_AUDIO(media);
  ClutterAudioPrivate *priv; 

  g_return_if_fail (CLUTTER_IS_AUDIO (audio));

  priv = audio->priv;

  if (!priv->playbin)
    return;
        
  if (priv->uri) 
    {
      GstState state;
    
      if (playing)
	state = GST_STATE_PLAYING;
      else
	state = GST_STATE_PAUSED;
    
      gst_element_set_state (audio->priv->playbin, state);
    } 
  else 
    {
      if (playing)
	g_warning ("Tried to play, but no URI is loaded.");
    }
  
  g_object_notify (G_OBJECT (audio), "playing");
  g_object_notify (G_OBJECT (audio), "position");
}

static gboolean
get_playing (ClutterMedia *media)
{
  ClutterAudio        *audio = CLUTTER_AUDIO(media);
  ClutterAudioPrivate *priv; 
  GstState                    state, pending;

  g_return_val_if_fail (CLUTTER_IS_AUDIO (audio), FALSE);
	
  priv = audio->priv;

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
  ClutterAudio        *audio = CLUTTER_AUDIO(media);
  ClutterAudioPrivate *priv; 
  GstState                    state, pending;

  g_return_if_fail (CLUTTER_IS_AUDIO (audio));

  priv = audio->priv;

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
  ClutterAudio        *audio = CLUTTER_AUDIO(media);
  ClutterAudioPrivate *priv; 
  GstQuery                   *query;
  gint64                      position;

  g_return_val_if_fail (CLUTTER_IS_AUDIO (audio), -1);

  priv = audio->priv;

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
  ClutterAudio        *audio = CLUTTER_AUDIO(media);
  ClutterAudioPrivate *priv; 

  g_return_if_fail (CLUTTER_IS_AUDIO (audio));
  // g_return_if_fail (volume >= 0.0 && volume <= GST_VOL_MAX);

  priv = audio->priv;
  
  if (!priv->playbin)
    return;
  
  g_object_set (G_OBJECT (audio->priv->playbin),
		"volume", volume,
		NULL);
  
  g_object_notify (G_OBJECT (audio), "volume");
}

static double
get_volume (ClutterMedia *media)
{
  ClutterAudio        *audio = CLUTTER_AUDIO(media);
  ClutterAudioPrivate *priv; 
  double                      volume;

  g_return_val_if_fail (CLUTTER_IS_AUDIO (audio), 0.0);

  priv = audio->priv;
  
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
  ClutterAudio        *audio = CLUTTER_AUDIO(media);

  g_return_val_if_fail (CLUTTER_IS_AUDIO (audio), FALSE);
  
  return audio->priv->can_seek;
}

static int
get_buffer_percent (ClutterMedia *media)
{
  ClutterAudio         *audio = CLUTTER_AUDIO(media);

  g_return_val_if_fail (CLUTTER_IS_AUDIO (audio), -1);
  
  return audio->priv->buffer_percent;
}

static int
get_duration (ClutterMedia *media)
{
  ClutterAudio         *audio = CLUTTER_AUDIO(media);

  g_return_val_if_fail (CLUTTER_IS_AUDIO (audio), -1);

  return audio->priv->duration;
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
clutter_audio_dispose (GObject *object)
{
  ClutterAudio        *self;
  ClutterAudioPrivate *priv; 

  self = CLUTTER_AUDIO(object); 
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

  G_OBJECT_CLASS (clutter_audio_parent_class)->dispose (object);
}

static void
clutter_audio_finalize (GObject *object)
{
  ClutterAudio        *self;
  ClutterAudioPrivate *priv; 

  self = CLUTTER_AUDIO(object); 
  priv = self->priv;

  if (priv->uri)
    g_free(priv->uri);

  G_OBJECT_CLASS (clutter_audio_parent_class)->finalize (object);
}

static void
clutter_audio_set_property (GObject      *object, 
				    guint         property_id,
				    const GValue *value, 
				    GParamSpec   *pspec)
{
  ClutterAudio *audio;

  audio = CLUTTER_AUDIO(object);

  switch (property_id)
    {
    case PROP_URI:
      clutter_media_set_uri (CLUTTER_MEDIA(audio), 
			     g_value_get_string (value));
      break;
    case PROP_PLAYING:
      clutter_media_set_playing (CLUTTER_MEDIA(audio),
				 g_value_get_boolean (value));
      break;
    case PROP_POSITION:
      clutter_media_set_position (CLUTTER_MEDIA(audio),
				  g_value_get_int (value));
      break;
    case PROP_VOLUME:
      clutter_media_set_volume (CLUTTER_MEDIA(audio),
				g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_audio_get_property (GObject    *object, 
				    guint       property_id,
				    GValue     *value, 
				    GParamSpec *pspec)
{
  ClutterAudio *audio;
  ClutterMedia        *media;

  audio = CLUTTER_AUDIO (object);
  media         = CLUTTER_MEDIA (audio);

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
clutter_audio_class_init (ClutterAudioClass *klass)
{
  GObjectClass        *object_class;

  object_class = (GObjectClass*)klass;

  object_class->dispose      = clutter_audio_dispose;
  object_class->finalize     = clutter_audio_finalize;
  object_class->set_property = clutter_audio_set_property;
  object_class->get_property = clutter_audio_get_property;

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
                      ClutterAudio *audio)
{
  GError *error;

  error = NULL;
  gst_message_parse_error (message, &error, NULL);
        
  g_signal_emit_by_name (CLUTTER_MEDIA(audio), "error", error);

  g_error_free (error);
}

static void
bus_message_eos_cb (GstBus            *bus,
                    GstMessage        *message,
                    ClutterAudio *audio)
{
  g_object_notify (G_OBJECT (audio), "position");

  g_signal_emit_by_name (CLUTTER_MEDIA(audio), "eos");
}

static void
bus_message_tag_cb (GstBus            *bus,
                    GstMessage        *message,
                    ClutterAudio *audio)
{
  GstTagList *tag_list;

  gst_message_parse_tag (message, &tag_list);

  g_signal_emit_by_name (CLUTTER_MEDIA(audio), 
			 "metadata-available", 
			 tag_list);
  
  gst_tag_list_free (tag_list);
}

static void
bus_message_buffering_cb (GstBus            *bus,
                          GstMessage        *message,
                          ClutterAudio *audio)
{
  const GstStructure *str;
  
  str = gst_message_get_structure (message);
  if (!str)
    return;

  if (!gst_structure_get_int (str,
			      "buffer-percent",
			      &audio->priv->buffer_percent))
    return;
        
  g_object_notify (G_OBJECT (audio), "buffer-percent");
}

static void
bus_message_duration_cb (GstBus            *bus,
                         GstMessage        *message,
                         ClutterAudio *audio)
{
  GstFormat format;
  gint64 duration;

  gst_message_parse_duration (message,
			      &format,
			      &duration);

  if (format != GST_FORMAT_TIME)
    return;
  
  audio->priv->duration = duration / GST_SECOND;

  g_object_notify (G_OBJECT (audio), "duration");
}

static void
bus_message_state_change_cb (GstBus            *bus,
                             GstMessage        *message,
                             ClutterAudio *audio)
{
  gpointer src;
  GstState old_state, new_state;

  src = GST_MESSAGE_SRC (message);
        
  if (src != audio->priv->playbin)
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
      
      if (gst_element_query (audio->priv->playbin, query)) {
	gst_query_parse_seeking (query,
				 NULL,
				 &audio->priv->can_seek,
				 NULL,
				 NULL);
      } else {
	/*
	 * Could not query for ability to seek. Determine
	 * using URI.
	 */
	
	if (g_str_has_prefix (audio->priv->uri,
			      "http://")) {
	  audio->priv->can_seek = FALSE;
	} else {
	  audio->priv->can_seek = TRUE;
	}
      }
      
      gst_query_unref (query);
      
      g_object_notify (G_OBJECT (audio), "can-seek");
      
      /**
       * Determine the duration.
       **/
      query = gst_query_new_duration (GST_FORMAT_TIME);
      
      if (gst_element_query (audio->priv->playbin, query)) 
	{
	  gint64 duration;
	  
	  gst_query_parse_duration (query, NULL, &duration);

	  audio->priv->duration = duration / GST_SECOND;
                        
	  g_object_notify (G_OBJECT (audio), "duration");
	}

      gst_query_unref (query);
    }
}

static gboolean
tick_timeout (ClutterAudio *audio)
{
  g_object_notify (G_OBJECT (audio), "position");

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
lay_pipeline (ClutterAudio *audio)
{
  ClutterAudioPrivate *priv;
  GstElement                 *audio_sink = NULL;

  priv = audio->priv;

  priv->playbin = gst_element_factory_make ("playbin", "playbin");

  if (!priv->playbin) 
    {
      g_warning ("Unable to create playbin GST element.");
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

  g_object_set (G_OBJECT (priv->playbin),
		"audio-sink", audio_sink,
		NULL);

  return TRUE;
}

static void
clutter_audio_init (ClutterAudio *audio)
{
  ClutterAudioPrivate *priv;
  GstBus                     *bus;

  priv                 = g_new0 (ClutterAudioPrivate, 1);
  audio->priv  = priv;

  if (!lay_pipeline(audio))
    {
      g_warning("Failed to initiate suitable playback pipeline.");
      return;
    }

  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->playbin));

  gst_bus_add_signal_watch (bus);

  g_signal_connect_object (bus,
			   "message::error",
			   G_CALLBACK (bus_message_error_cb),
			   audio,
			   0);

  g_signal_connect_object (bus,
			   "message::eos",
			   G_CALLBACK (bus_message_eos_cb),
			   audio,
			   0);

  g_signal_connect_object (bus,
			   "message::tag",
			   G_CALLBACK (bus_message_tag_cb),
			   audio,
			   0);

  g_signal_connect_object (bus,
			   "message::buffering",
			   G_CALLBACK (bus_message_buffering_cb),
			   audio,
			   0);

  g_signal_connect_object (bus,
			   "message::duration",
			   G_CALLBACK (bus_message_duration_cb),
			   audio,
			   0);

  g_signal_connect_object (bus,
			   "message::state-changed",
			   G_CALLBACK (bus_message_state_change_cb),
			   audio,
			   0);

  gst_object_unref (GST_OBJECT (bus));

  return;
}

/**
 * clutter_audio_new:
 *
 * Creates #ClutterAudio object.
 *
 * Return value: A newly allocated #ClutterAudio object.
 */
ClutterAudio*
clutter_audio_new (void)
{
  return g_object_new (CLUTTER_TYPE_AUDIO, NULL);
}

