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
 * SECTION:clutter-media
 * @short_description: An interface for controlling playback of media data.
 *
 * #ClutterMedia is an interface for controlling playback of media sources.
 *
 * It is not implemented inside Clutter, but other integration libraries
 * like Clutter-GStreamer, implement it to offer a uniform API for
 * applications.
 *
 * ClutterMedia is available since Clutter 0.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-media.h"
#include "clutter-main.h"
#include "clutter-enum-types.h"
#include "clutter-private.h" 	/* for DBG */

enum
{
  EOS,
  ERROR,

  LAST_SIGNAL
};

static void clutter_media_base_init (gpointer g_class);

static guint media_signals[LAST_SIGNAL] = { 0, };

GType
clutter_media_get_type (void)
{
  static GType media_type = 0;

  if (!media_type)
    {
      static const GTypeInfo media_info =
      {
	sizeof (ClutterMediaInterface),
	clutter_media_base_init,
	NULL,
      };

      media_type = g_type_register_static (G_TYPE_INTERFACE, "ClutterMedia",
					   &media_info, 0);
    }

  return media_type;
}

static void
clutter_media_base_init (gpointer g_iface)
{
  static gboolean initialized = FALSE;

  if (G_UNLIKELY (!initialized))
    {
      initialized = TRUE;

      /* props */

      g_object_interface_install_property
	(g_iface,
	 g_param_spec_string
	 ("uri",
	  "URI",
	  "The loaded URI.",
	  NULL,
	  G_PARAM_READWRITE |
	  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
	  G_PARAM_STATIC_BLURB));

      g_object_interface_install_property
	(g_iface,
	 g_param_spec_boolean
	 ("playing",
	  "Playing",
	  "TRUE if playing.",
	  FALSE,
	  G_PARAM_READWRITE |
	  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
	  G_PARAM_STATIC_BLURB));

      g_object_interface_install_property
	(g_iface,
	 g_param_spec_int
	 ("position",
	  "Position",
	  "The position in the current stream in seconds.",
	  0, G_MAXINT, 0,
	  G_PARAM_READWRITE |
	  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
	  G_PARAM_STATIC_BLURB));

      g_object_interface_install_property
	(g_iface,
	 g_param_spec_double
	 ("volume",
	  "Volume",
	  "The audio volume.",
	  0, 100, 50,
	  G_PARAM_READWRITE |
	  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
	  G_PARAM_STATIC_BLURB));

      g_object_interface_install_property
	(g_iface,
	 g_param_spec_boolean
	 ("can-seek",
	  "Can seek",
	  "TRUE if the current stream is seekable.",
	  FALSE,
	  G_PARAM_READABLE |
	  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
	  G_PARAM_STATIC_BLURB));

      g_object_interface_install_property
	(g_iface,
	 g_param_spec_int
	 ("buffer-percent",
	  "Buffer percent",
	  "The percentage the current stream buffer is filled.",
	  0, 100, 0,
	  G_PARAM_READABLE |
	  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
	  G_PARAM_STATIC_BLURB));

      g_object_interface_install_property
	(g_iface,
	 g_param_spec_int
	 ("duration",
	  "Duration",
	  "The duration of the current stream in seconds.",
	  0, G_MAXINT, 0,
	  G_PARAM_READABLE |
	  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
	  G_PARAM_STATIC_BLURB));

      /**
       * ClutterMedia::eos:
       * @media: the #ClutterMedia instance that received the signal
       *
       * The ::eos signal is emitted each time the media stream ends.
       *
       * Since: 0.2
       */
      media_signals[EOS] =
        g_signal_new ("eos",
                      CLUTTER_TYPE_MEDIA,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ClutterMediaInterface, eos),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
      /**
       * ClutterMedia::error:
       * @media: the #ClutterMedia instance that received the signal
       * @error: the #GError
       *
       * The ::error signal is emitted each time an error occurred.
       *
       * Since: 0.2
       */
      media_signals[ERROR] =
        g_signal_new ("error",
                      CLUTTER_TYPE_MEDIA,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ClutterMediaInterface, error),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1,
                      G_TYPE_POINTER);
    }
}

/**
 * clutter_media_set_uri:
 * @media: #ClutterMedia object
 * @uri: URI
 *
 * Sets the URI of @media to @uri.
 *
 * Since: 0.2
 */
void
clutter_media_set_uri (ClutterMedia *media,
		       const char   *uri)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  CLUTTER_MEDIA_GET_INTERFACE (media)->set_uri (media, uri);
}

/**
 * clutter_media_get_uri:
 * @media: A #ClutterMedia object
 *
 * Retrieves the URI from @media.
 *
 * Return value: The URI as a string.
 *
 * Since: 0.2
 */
const char*
clutter_media_get_uri (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), NULL);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_uri (media);
}

/**
 * clutter_media_set_playing:
 * @media: A #ClutterMedia object
 * @playing: TRUE to start playing, FALSE to stop.
 *
 * Starts or stops @media playing.
 *
 * Since: 0.2
 */
void
clutter_media_set_playing (ClutterMedia *media,
			   gboolean      playing)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  CLUTTER_MEDIA_GET_INTERFACE (media)->set_playing (media, playing);
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
 */
gboolean
clutter_media_get_playing (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), FALSE);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_playing (media);
}

/**
 * clutter_media_set_position:
 * @media: A #ClutterMedia object
 * @position: The desired position.
 *
 * Sets the playback position of @media to @position.
 *
 * Since: 0.2
 */
void
clutter_media_set_position (ClutterMedia *media,
			    int           position)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  CLUTTER_MEDIA_GET_INTERFACE (media)->set_position (media, position);
}

/**
 * clutter_media_get_position:
 * @media: A #ClutterMedia object
 *
 * Retrieves the position of @media.
 *
 * Return value: The playback position.
 *
 * Since: 0.2
 */
int
clutter_media_get_position (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), 0);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_position (media);
}

/**
 * clutter_media_set_volume:
 * @media: A #ClutterMedia object
 * @volume: The volume as a double between 0.0 and 1.0
 *
 * Sets the playback volume of @media to @volume.
 *
 * Since: 0.2
 */
void
clutter_media_set_volume (ClutterMedia *media,
			  double        volume)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  CLUTTER_MEDIA_GET_INTERFACE (media)->set_volume (media, volume);
}

/**
 * clutter_media_get_volume:
 * @media: A #ClutterMedia object
 *
 * Retrieves the playback volume of @media.
 *
 * Return value: The playback volume between 0.0 and 1.0
 *
 * Since: 0.2
 */
double
clutter_media_get_volume (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), 0.0);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_volume (media);
}

/**
 * clutter_media_get_can_seek:
 * @media: A #ClutterMedia object
 *
 * Retrieves whether @media is seekable or not.
 *
 * Return value: %TRUE if @media can seek, %FALSE otherwise.
 *
 * Since: 0.2
 */
gboolean
clutter_media_get_can_seek (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), FALSE);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->can_seek (media);
}

/**
 * clutter_media_get_buffer_percent:
 * @media: A #ClutterMedia object
 *
 * Retrieves the amount of the stream that is buffered.
 *
 * Return value: percentage value
 *
 * Since: 0.2
 */
int
clutter_media_get_buffer_percent (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), 0);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_buffer_percent (media);
}

/**
 * clutter_media_get_duration:
 * @media: A #ClutterMedia object
 *
 * Retrieves the duration of the media stream that @media represents.
 *
 * Return value: The length of the media stream.
 *
 * Since: 0.2
 */
int
clutter_media_get_duration (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), 0);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_duration (media);
}

/* helper funcs */

/**
 * clutter_media_set_filename:
 * @media: A #ClutterMedia object
 * @filename: A filename to media file.
 *
 * Sets the filename of the media source.
 *
 * Since: 0.2
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
      g_signal_emit (media, media_signals[ERROR], 0, uri_error);
      g_error_free (uri_error);
      return;
    }

  clutter_media_set_uri (media, uri);

  g_free (uri);
}
