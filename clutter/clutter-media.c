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

#include "config.h"

#include "clutter-media.h"
#include "clutter-main.h"
#include "clutter-enum-types.h"
#include "clutter-private.h" 	/* for DBG */

static void clutter_media_base_init (gpointer g_class);

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

  if (!initialized)
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

      /* signals */

      g_signal_new ("metadata-available",
		    CLUTTER_TYPE_MEDIA,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (ClutterMediaInterface,
				     metadata_available),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1, G_TYPE_POINTER);
      
      g_signal_new ("eos",
		    CLUTTER_TYPE_MEDIA,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (ClutterMediaInterface,
				     eos),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
      
      g_signal_new ("error",
		    CLUTTER_TYPE_MEDIA,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (ClutterMediaInterface,
				     error),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1, G_TYPE_POINTER);
    }
}

void
clutter_media_set_uri (ClutterMedia *media,
		       const char   *uri)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  CLUTTER_MEDIA_GET_INTERFACE (media)->set_uri (media, uri);
}

const char*
clutter_media_get_uri (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), NULL);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_uri (media);
}

void
clutter_media_set_playing (ClutterMedia *media,
			   gboolean      playing)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  CLUTTER_MEDIA_GET_INTERFACE (media)->set_playing (media, playing);
}

gboolean
clutter_media_get_playing (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), FALSE);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_playing (media);
}

void
clutter_media_set_position (ClutterMedia *media,
			    int           position)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  CLUTTER_MEDIA_GET_INTERFACE (media)->set_position (media, position);
}

int
clutter_media_get_position (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), 0);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_position (media);
}

void
clutter_media_set_volume (ClutterMedia *media,
			  double        volume)
{
  g_return_if_fail (CLUTTER_IS_MEDIA(media));

  CLUTTER_MEDIA_GET_INTERFACE (media)->set_position (media, volume);
}

double
clutter_media_get_volume (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), 0.0);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_volume (media);
}

gboolean
clutter_media_get_can_seek (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), FALSE);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->can_seek (media);
}

int
clutter_media_get_buffer_percent (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), 0);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_buffer_percent (media);
}

int
clutter_media_get_duration (ClutterMedia *media)
{
  g_return_val_if_fail (CLUTTER_IS_MEDIA(media), 0);

  return CLUTTER_MEDIA_GET_INTERFACE (media)->get_duration (media);
}

/* helper funcs */

void
clutter_media_set_filename (ClutterMedia *media, const gchar *filename)
{
  gchar *uri;

  if (filename[0] != '/')
    uri = g_strdup_printf ("file://%s/%s", g_get_current_dir (), filename);
  else 
    uri = g_strdup_printf ("file://%s", filename);

  clutter_media_set_uri (media, uri);

  g_free(uri);
}
