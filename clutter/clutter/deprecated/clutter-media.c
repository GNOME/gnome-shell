/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Matthew Allum  <mallum@openedhand.com>
 *              Emmanuele Bassi <ebassi@linux.intel.com>
 *
 * Copyright (C) 2006 OpenedHand
 * Copyright (C) 2009 Intel Corp.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:clutter-media
 * @short_description: An interface for controlling playback of media data
 *
 * #ClutterMedia is an interface for controlling playback of media sources.
 *
 * Clutter core does not provide an implementation of this interface, but
 * other integration libraries like Clutter-GStreamer implement it to offer
 * a uniform API for applications.
 *
 * #ClutterMedia is available since Clutter 0.2
 *
 * #ClutterMedia is deprecated since Clutter 1.12. Use the Clutter-GStreamer
 * API directly instead.
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-media.h"
#include "clutter-main.h"
#include "clutter-private.h" 	/* for DBG */

enum
{
  EOS_SIGNAL,
  ERROR_SIGNAL, /* can't be called 'ERROR' otherwise it clashes with wingdi.h */

  LAST_SIGNAL
};

static guint media_signals[LAST_SIGNAL] = { 0, };

typedef ClutterMediaIface       ClutterMediaInterface;

G_DEFINE_INTERFACE (ClutterMedia, clutter_media, G_TYPE_OBJECT);

static void
clutter_media_default_init (ClutterMediaInterface *iface)
{
  GParamSpec *pspec = NULL;

  /**
   * ClutterMedia:uri:
   *
   * The location of a media file, expressed as a valid URI.
   *
   * Since: 0.2
   *
   * Deprecated: 1.12
   */
  pspec = g_param_spec_string ("uri",
                               P_("URI"),
                               P_("URI of a media file"),
                               NULL,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_DEPRECATED);
  g_object_interface_install_property (iface, pspec);

  /**
   * ClutterMedia:playing:
   *
   * Whether the #ClutterMedia actor is playing.
   *
   * Since: 0.2
   *
   * Deprecated: 1.12
   */
  pspec = g_param_spec_boolean ("playing",
                                P_("Playing"),
                                P_("Whether the actor is playing"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE |
                                G_PARAM_DEPRECATED);
  g_object_interface_install_property (iface, pspec);

  /**
   * ClutterMedia:progress:
   *
   * The current progress of the playback, as a normalized
   * value between 0.0 and 1.0.
   *
   * Since: 1.0
   *
   * Deprecated: 1.12
   */
  pspec = g_param_spec_double ("progress",
                               P_("Progress"),
                               P_("Current progress of the playback"),
                               0.0, 1.0, 0.0,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_DEPRECATED);
  g_object_interface_install_property (iface, pspec);

  /**
   * ClutterMedia:subtitle-uri:
   *
   * The location of a subtitle file, expressed as a valid URI.
   *
   * Since: 1.2
   *
   * Deprecated: 1.12
   */
  pspec = g_param_spec_string ("subtitle-uri",
                               P_("Subtitle URI"),
                               P_("URI of a subtitle file"),
                               NULL,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_DEPRECATED);
  g_object_interface_install_property (iface, pspec);

  /**
   * ClutterMedia:subtitle-font-name:
   *
   * The font used to display subtitles. The font description has to
   * follow the same grammar as the one recognized by
   * pango_font_description_from_string().
   *
   * Since: 1.2
   *
   * Deprecated: 1.12
   */
  pspec = g_param_spec_string ("subtitle-font-name",
                               P_("Subtitle Font Name"),
                               P_("The font used to display subtitles"),
                               NULL,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_DEPRECATED);
  g_object_interface_install_property (iface, pspec);

  /**
   * ClutterMedia:audio-volume:
   *
   * The volume of the audio, as a normalized value between
   * 0.0 and 1.0.
   *
   * Since: 1.0
   *
   * Deprecated: 1.12
   */
  pspec = g_param_spec_double ("audio-volume",
                               P_("Audio Volume"),
                               P_("The volume of the audio"),
                               0.0, 1.0, 0.5,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_DEPRECATED);
  g_object_interface_install_property (iface, pspec);

  /**
   * ClutterMedia:can-seek:
   *
   * Whether the current stream is seekable.
   *
   * Since: 0.2
   *
   * Deprecated: 1.12
   */
  pspec = g_param_spec_boolean ("can-seek",
                                P_("Can Seek"),
                                P_("Whether the current stream is seekable"),
                                FALSE,
                                CLUTTER_PARAM_READABLE |
                                G_PARAM_DEPRECATED);
  g_object_interface_install_property (iface, pspec);

  /**
   * ClutterMedia:buffer-fill:
   *
   * The fill level of the buffer for the current stream,
   * as a value between 0.0 and 1.0.
   *
   * Since: 1.0
   *
   * Deprecated: 1.12
   */
  pspec = g_param_spec_double ("buffer-fill",
                               P_("Buffer Fill"),
                               P_("The fill level of the buffer"),
                               0.0, 1.0, 0.0,
                               CLUTTER_PARAM_READABLE |
                               G_PARAM_DEPRECATED);
  g_object_interface_install_property (iface, pspec);

  /**
   * ClutterMedia:duration:
   *
   * The duration of the current stream, in seconds
   *
   * Since: 0.2
   *
   * Deprecated: 1.12
   */
  pspec = g_param_spec_double ("duration",
                               P_("Duration"),
                               P_("The duration of the stream, in seconds"),
                               0, G_MAXDOUBLE, 0,
                               CLUTTER_PARAM_READABLE);
  g_object_interface_install_property (iface, pspec);

  /**
   * ClutterMedia::eos:
   * @media: the #ClutterMedia instance that received the signal
   *
   * The ::eos signal is emitted each time the media stream ends.
   *
   * Since: 0.2
   *
   * Deprecated: 1.12
   */
  media_signals[EOS_SIGNAL] =
    g_signal_new (I_("eos"),
                  CLUTTER_TYPE_MEDIA,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterMediaIface, eos),
                  NULL, NULL,
                  _clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * ClutterMedia::error:
   * @media: the #ClutterMedia instance that received the signal
   * @error: the #GError
   *
   * The ::error signal is emitted each time an error occurred.
   *
   * Since: 0.2
   *
   * Deprecated: 1.12
   */
  media_signals[ERROR_SIGNAL] =
    g_signal_new (I_("error"),
                  CLUTTER_TYPE_MEDIA,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterMediaIface, error),
                  NULL, NULL,
                  _clutter_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  G_TYPE_ERROR);
}


/**
 * clutter_media_set_uri:
 * @media: a #ClutterMedia
 * @uri: the URI of the media stream
 *
 * Sets the URI of @media to @uri.
 *
 * Since: 0.2
 *
 * Deprecated: 1.12
 */
void
clutter_media_set_uri (ClutterMedia *media,
		       const gchar  *uri)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  g_object_set (G_OBJECT (media), "uri", uri, NULL);
}

/**
 * clutter_media_get_uri:
 * @media: a #ClutterMedia
 *
 * Retrieves the URI from @media.
 *
 * Return value: the URI of the media stream. Use g_free()
 *   to free the returned string
 *
 * Since: 0.2
 *
 * Deprecated: 1.12
 */
gchar *
clutter_media_get_uri (ClutterMedia *media)
{
  gchar *retval = NULL;

  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), NULL);

  g_object_get (G_OBJECT (media), "uri", &retval, NULL);

  return retval;
}

/**
 * clutter_media_set_playing:
 * @media: a #ClutterMedia
 * @playing: %TRUE to start playing
 *
 * Starts or stops playing of @media. 
 
 * The implementation might be asynchronous, so the way to know whether
 * the actual playing state of the @media is to use the #GObject::notify
 * signal on the #ClutterMedia:playing property and then retrieve the
 * current state with clutter_media_get_playing(). ClutterGstVideoTexture
 * in clutter-gst is an example of such an asynchronous implementation.
 *
 * Since: 0.2
 *
 * Deprecated: 1.12
 */
void
clutter_media_set_playing (ClutterMedia *media,
			   gboolean      playing)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  g_object_set (G_OBJECT (media), "playing", playing, NULL);
}

/**
 * clutter_media_get_playing:
 * @media: A #ClutterMedia object
 *
 * Retrieves the playing status of @media.
 *
 * Return value: %TRUE if playing, %FALSE if stopped.
 *
 * Since: 0.2
 *
 * Deprecated: 1.12
 */
gboolean
clutter_media_get_playing (ClutterMedia *media)
{
  gboolean is_playing = FALSE;

  g_return_val_if_fail (CLUTTER_IS_MEDIA (media), FALSE);

  g_object_get (G_OBJECT (media), "playing", &is_playing, NULL);

  return is_playing;
}

/**
 * clutter_media_set_progress:
 * @media: a #ClutterMedia
 * @progress: the progress of the playback, between 0.0 and 1.0
 *
 * Sets the playback progress of @media. The @progress is
 * a normalized value between 0.0 (begin) and 1.0 (end).
 *
 * Since: 1.0
 *
 * Deprecated: 1.12
 */
void
clutter_media_set_progress (ClutterMedia *media,
			    gdouble       progress)
{
  g_return_if_fail (CLUTTER_IS_MEDIA (media));

  g_object_set (G_OBJECT (media), "progress", progress, NULL);
}

/**
 * clutter_media_get_progress:
 * @media: a #ClutterMedia
 *
 * Retrieves the playback progress of @media.
 *
 * Return value: the playback progress, between 0.0 and 1.0
 *
 * Since: 1.0
 *
 * Deprecated: 1.12
 */
gdouble
clutter_media_get_progress (ClutterMedia *media)
{
  gdouble retval = 0.0;

  g_return_val_if_fail (CLUTTER_IS_MEDIA (media), 0);

  g_object_get (G_OBJECT (media), "progress", &retval, NULL);

  return retval;
}

/**
 * clutter_media_set_subtitle_uri:
 * @media: a #ClutterMedia
 * @uri: the URI of a subtitle file
 *
 * Sets the location of a subtitle file to display while playing @media.
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
void
clutter_media_set_subtitle_uri (ClutterMedia *media,
                                 const char   *uri)
{
  g_return_if_fail (CLUTTER_IS_MEDIA (media));

  g_object_set (G_OBJECT (media), "subtitle-uri", uri, NULL);
}

/**
 * clutter_media_get_subtitle_uri:
 * @media: a #ClutterMedia
 *
 * Retrieves the URI of the subtitle file in use.
 *
 * Return value: the URI of the subtitle file. Use g_free()
 *   to free the returned string
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
gchar *
clutter_media_get_subtitle_uri (ClutterMedia *media)
{
  gchar *retval = NULL;

  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), NULL);

  g_object_get (G_OBJECT (media), "subtitle-uri", &retval, NULL);

  return retval;
}

/**
 * clutter_media_set_subtitle_font_name:
 * @media: a #ClutterMedia
 * @font_name: a font name, or %NULL to set the default font name
 *
 * Sets the font used by the subtitle renderer. The @font_name string must be
 * either %NULL, which means that the default font name of the underlying
 * implementation will be used; or must follow the grammar recognized by
 * pango_font_description_from_string() like:
 *
 * |[
 *   clutter_media_set_subtitle_font_name (media, "Sans 24pt");
 * ]|
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
void
clutter_media_set_subtitle_font_name (ClutterMedia *media,
                                      const char   *font_name)
{
  g_return_if_fail (CLUTTER_IS_MEDIA (media));

  g_object_set (G_OBJECT (media), "subtitle-font-name", font_name, NULL);
}

/**
 * clutter_media_get_subtitle_font_name:
 * @media: a #ClutterMedia
 *
 * Retrieves the font name currently used.
 *
 * Return value: a string containing the font name. Use g_free()
 *   to free the returned string
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
gchar *
clutter_media_get_subtitle_font_name (ClutterMedia *media)
{
  gchar *retval = NULL;

  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), NULL);

  g_object_get (G_OBJECT (media), "subtitle-font-name", &retval, NULL);

  return retval;
}

/**
 * clutter_media_set_audio_volume:
 * @media: a #ClutterMedia
 * @volume: the volume as a double between 0.0 and 1.0
 *
 * Sets the playback volume of @media to @volume.
 *
 * Since: 1.0
 *
 * Deprecated: 1.12
 */
void
clutter_media_set_audio_volume (ClutterMedia *media,
			        gdouble       volume)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  g_object_set (G_OBJECT (media), "audio-volume", volume, NULL);
}

/**
 * clutter_media_get_audio_volume:
 * @media: a #ClutterMedia
 *
 * Retrieves the playback volume of @media.
 *
 * Return value: The playback volume between 0.0 and 1.0
 *
 * Since: 1.0
 *
 * Deprecated: 1.12
 */
gdouble
clutter_media_get_audio_volume (ClutterMedia *media)
{
  gdouble retval = 0.0;

  g_return_val_if_fail (CLUTTER_IS_MEDIA (media), 0.0);

  g_object_get (G_OBJECT (media), "audio-volume", &retval, NULL);

  return retval;
}

/**
 * clutter_media_get_can_seek:
 * @media: a #ClutterMedia
 *
 * Retrieves whether @media is seekable or not.
 *
 * Return value: %TRUE if @media can seek, %FALSE otherwise.
 *
 * Since: 0.2
 *
 * Deprecated: 1.12
 */
gboolean
clutter_media_get_can_seek (ClutterMedia *media)
{
  gboolean retval = FALSE;

  g_return_val_if_fail (CLUTTER_IS_MEDIA (media), FALSE);

  g_object_get (G_OBJECT (media), "can-seek", &retval, NULL);

  return retval;
}

/**
 * clutter_media_get_buffer_fill:
 * @media: a #ClutterMedia
 *
 * Retrieves the amount of the stream that is buffered.
 *
 * Return value: the fill level, between 0.0 and 1.0
 *
 * Since: 1.0
 *
 * Deprecated: 1.12
 */
gdouble
clutter_media_get_buffer_fill (ClutterMedia *media)
{
  gdouble retval = 0.0;

  g_return_val_if_fail (CLUTTER_IS_MEDIA (media), 0);

  g_object_get (G_OBJECT (media), "buffer-fill", &retval, NULL);

  return retval;
}

/**
 * clutter_media_get_duration:
 * @media: a #ClutterMedia
 *
 * Retrieves the duration of the media stream that @media represents.
 *
 * Return value: the duration of the media stream, in seconds
 *
 * Since: 0.2
 *
 * Deprecated: 1.12
 */
gdouble
clutter_media_get_duration (ClutterMedia *media)
{
  gdouble retval = 0;

  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), 0);

  g_object_get (G_OBJECT (media), "duration", &retval, NULL);

  return retval;
}

/* helper funcs */

/**
 * clutter_media_set_filename:
 * @media: a #ClutterMedia
 * @filename: A filename
 *
 * Sets the source of @media using a file path.
 *
 * Since: 0.2
 *
 * Deprecated: 1.12
 */
void
clutter_media_set_filename (ClutterMedia *media,
                            const gchar  *filename)
{
  gchar *uri;
  GError *uri_error = NULL;

  if (!g_path_is_absolute (filename))
    {
      gchar *abs_path;

      abs_path = g_build_filename (g_get_current_dir (), filename, NULL);
      uri = g_filename_to_uri (abs_path, NULL, &uri_error);
      g_free (abs_path);
    }
  else
    uri = g_filename_to_uri (filename, NULL, &uri_error);

  if (uri_error)
    {
      g_signal_emit (media, media_signals[ERROR_SIGNAL], 0, uri_error);
      g_error_free (uri_error);
      return;
    }

  clutter_media_set_uri (media, uri);

  g_free (uri);
}
