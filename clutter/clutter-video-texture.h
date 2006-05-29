#ifndef _HAVE_CLUTTER_VIDEO_TEXTURE_H
#define _HAVE_CLUTTER_VIDEO_TEXTURE_H

#include <glib-object.h>
#include <clutter/clutter-element.h>
#include <clutter/clutter-texture.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_VIDEO_TEXTURE clutter_video_texture_get_type()

#define CLUTTER_VIDEO_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_VIDEO_TEXTURE, ClutterVideoTexture))

#define CLUTTER_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_VIDEO_TEXTURE, ClutterVideoTextureClass))

#define CLUTTER_IS_VIDEO_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_VIDEO_TEXTURE))

#define CLUTTER_IS_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_VIDEO_TEXTURE))

#define CLUTTER_VIDEO_TEXTURE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_VIDEO_TEXTURE, ClutterVideoTextureClass))

typedef struct ClutterVideoTexturePrivate ClutterVideoTexturePrivate ;
typedef struct _ClutterVideoTexture ClutterVideoTexture;
typedef struct _ClutterVideoTextureClass ClutterVideoTextureClass;


#define CLUTTER_VIDEO_TEXTURE_ERROR clutter_video_texture_error_quark()

typedef enum
{
  /* Plugins */
  CLUTTER_VIDEO_TEXTURE_ERROR_AUDIO_PLUGIN,
  CLUTTER_VIDEO_TEXTURE_ERROR_NO_PLUGIN_FOR_FILE,
  CLUTTER_VIDEO_TEXTURE_ERROR_VIDEO_PLUGIN,
  CLUTTER_VIDEO_TEXTURE_ERROR_AUDIO_BUSY,
	
  /* File */
  CLUTTER_VIDEO_TEXTURE_ERROR_BROKEN_FILE,
  CLUTTER_VIDEO_TEXTURE_ERROR_FILE_GENERIC,
  CLUTTER_VIDEO_TEXTURE_ERROR_FILE_PERMISSION,
  CLUTTER_VIDEO_TEXTURE_ERROR_FILE_ENCRYPTED,
  CLUTTER_VIDEO_TEXTURE_ERROR_FILE_NOT_FOUND,
	
  /* Devices */
  CLUTTER_VIDEO_TEXTURE_ERROR_DVD_ENCRYPTED,
  CLUTTER_VIDEO_TEXTURE_ERROR_INVALID_DEVICE,
	
  /* Network */
  CLUTTER_VIDEO_TEXTURE_ERROR_UNKNOWN_HOST,
  CLUTTER_VIDEO_TEXTURE_ERROR_NETWORK_UNREACHABLE,
  CLUTTER_VIDEO_TEXTURE_ERROR_CONNECTION_REFUSED,
	
  /* Generic */
  CLUTTER_VIDEO_TEXTURE_ERROR_UNVALID_LOCATION,
  CLUTTER_VIDEO_TEXTURE_ERROR_GENERIC,
  CLUTTER_VIDEO_TEXTURE_ERROR_CODEC_NOT_HANDLED,
  CLUTTER_VIDEO_TEXTURE_ERROR_AUDIO_ONLY,
  CLUTTER_VIDEO_TEXTURE_ERROR_CANNOT_CAPTURE,
  CLUTTER_VIDEO_TEXTURE_ERROR_READ_ERROR,
  CLUTTER_VIDEO_TEXTURE_ERROR_PLUGIN_LOAD,
  CLUTTER_VIDEO_TEXTURE_ERROR_STILL_IMAGE,
  CLUTTER_VIDEO_TEXTURE_ERROR_EMPTY_FILE
} ClutterVideoTextureError;

GQuark clutter_video_texture_error_quark (void);

typedef enum
{
  CLUTTER_VIDEO_TEXTURE_AUTO,
  CLUTTER_VIDEO_TEXTURE_SQUARE,
  CLUTTER_VIDEO_TEXTURE_FOURBYTHREE,
  CLUTTER_VIDEO_TEXTURE_ANAMORPHIC,
  CLUTTER_VIDEO_TEXTURE_DVB
} ClutterVideoTextureAspectRatio;

struct _ClutterVideoTexture
{
  ClutterTexture              parent;
  ClutterVideoTexturePrivate *priv;
}; 

struct _ClutterVideoTextureClass 
{
  ClutterTextureClass parent_class;

  void (*error) (ClutterVideoTexture *cvt, const char *message,
		 gboolean playback_stopped, gboolean fatal);
  void (*eos) (ClutterVideoTexture *cvt);
  void (*got_metadata) (ClutterVideoTexture *cvt);
  void (*got_redirect) (ClutterVideoTexture *cvt, const char *mrl);
  void (*title_change) (ClutterVideoTexture *cvt, const char *title);
  void (*channels_change) (ClutterVideoTexture *cvt);
  void (*tick) (ClutterVideoTexture *cvt, 
		gint64 current_time, 
		gint64 stream_length,
		float current_position, 
		gboolean seekable);
  void (*buffering) (ClutterVideoTexture *cvt, guint progress);
  void (*speed_warning) (ClutterVideoTexture *cvt);
}; 

GType clutter_video_texture_get_type (void);

ClutterElement*
clutter_video_texture_new (void);

gboolean
clutter_video_texture_open (ClutterVideoTexture *video_texture,
			    const gchar         *mrl, 
			    const gchar         *subtitle_uri, 
			    GError             **error);

gboolean
clutter_video_texture_play (ClutterVideoTexture *video_texture, 
			    GError             ** error);

void
clutter_video_texture_pause (ClutterVideoTexture *video_texture);

gboolean
clutter_video_texture_can_direct_seek (ClutterVideoTexture *video_texture);

gboolean
clutter_video_texture_seek_time (ClutterVideoTexture *video_texture, 
				 gint64               time, 
				 GError             **gerror);

gboolean
clutter_video_texture_seek (ClutterVideoTexture *video_texture, 
			    float                position, 
			    GError             **error);

void
clutter_video_texture_stop (ClutterVideoTexture *video_texture);

gboolean
clutter_video_texture_can_set_volume (ClutterVideoTexture *video_texture);

void
clutter_video_texture_set_volume (ClutterVideoTexture *video_texture, 
				  int                  volume);
int
clutter_video_texture_get_volume (ClutterVideoTexture *video_texture);

gint64
clutter_video_texture_get_current_time (ClutterVideoTexture *video_texture);

gint64
clutter_video_texture_get_stream_length (ClutterVideoTexture *video_texture);

gboolean
clutter_video_texture_is_playing (ClutterVideoTexture *video_texture);

gboolean
clutter_video_texture_is_seekable (ClutterVideoTexture * video_texture);

float
clutter_video_texture_get_position (ClutterVideoTexture  *video_texture);

void
clutter_video_texture_set_aspect_ratio (ClutterVideoTexture  *video_texture,
					ClutterVideoTextureAspectRatio ratio);

ClutterVideoTextureAspectRatio
clutter_video_texture_get_aspect_ratio (ClutterVideoTexture  *video_texture);

/* Metadata 
 * FIXME: This should probably go in some kind of genric 'Media' class
 *        You would open the media, get a media object and then pass 
 *        that to the video texture. media object could handle being
 *        just a metadata reader etc...
*/
typedef enum 
{
    CLUTTER_INFO_TITLE,
    CLUTTER_INFO_ARTIST,
    CLUTTER_INFO_YEAR,
    CLUTTER_INFO_ALBUM,
    CLUTTER_INFO_DURATION,
    CLUTTER_INFO_TRACK_NUMBER,
    /* Video */
    CLUTTER_INFO_HAS_VIDEO,
    CLUTTER_INFO_DIMENSION_X,
    CLUTTER_INFO_DIMENSION_Y,
    CLUTTER_INFO_VIDEO_BITRATE,
    CLUTTER_INFO_VIDEO_CODEC,
    CLUTTER_INFO_FPS,
    /* Audio */
    CLUTTER_INFO_HAS_AUDIO,
    CLUTTER_INFO_AUDIO_BITRATE,
    CLUTTER_INFO_AUDIO_CODEC,
} ClutterVideoTextureMetadataType;


void
clutter_video_texture_get_metadata (ClutterVideoTexture *video_texture,
				    ClutterVideoTextureMetadataType type,
				    GValue                         *value);

G_END_DECLS

#endif
