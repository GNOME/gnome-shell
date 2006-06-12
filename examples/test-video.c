#include <clutter/clutter.h>

ClutterElement *rect; /* um... */


void input_cb (ClutterStage *stage, 
	       ClutterEvent *event,
	       gpointer      user_data)
{
  ClutterVideoTexture *vtex = CLUTTER_VIDEO_TEXTURE(user_data);
  static gint paused = 0;

  if (event->type == CLUTTER_KEY_RELEASE)
    {
      if (paused)
	{
	  clutter_media_set_playing (CLUTTER_MEDIA(vtex), TRUE);
	  paused = 0;
	}
      else
	{
	  clutter_media_set_playing (CLUTTER_MEDIA(vtex), FALSE);
	  paused = 1;
	}
    }
}

void
size_change (ClutterTexture *texture, 
	     gint            width,
	     gint            height,
	     gpointer        user_data)
{
  ClutterElement  *stage;
  ClutterGeometry  stage_geom;
  gint             vid_width, vid_height, new_y, new_height;

  stage = clutter_stage_get_default ();

  clutter_element_get_geometry (stage, &stage_geom);

  clutter_texture_get_base_size (texture, &vid_width, &vid_height);

  printf("*** vid : %ix%i stage %ix%i ***\n", 
	 vid_width, vid_height, stage_geom.width, stage_geom.height);


  new_height = ( vid_height * stage_geom.width ) / vid_width;
  new_y      = (stage_geom.height - new_height) / 2;

  clutter_element_set_position (CLUTTER_ELEMENT (texture), 0, new_y);

  clutter_element_set_size (CLUTTER_ELEMENT (texture),
			    stage_geom.width,
			    new_height);

  // clutter_element_set_opacity (CLUTTER_ELEMENT (texture), 50);

  printf("*** Pos set to +%i+%i , %ix%i ***\n", 
	 0, new_y, stage_geom.width, new_height);
}

void 
tick (GObject      *object,
      GParamSpec   *pspec,
      ClutterLabel *label)
{
  ClutterVideoTexture *vtex;
  gint                 w, h, position, duration;
  gchar                buf[256];

  vtex = CLUTTER_VIDEO_TEXTURE(object);

  position = clutter_media_get_position (CLUTTER_MEDIA(vtex));
  duration = clutter_media_get_duration (CLUTTER_MEDIA(vtex));

  g_snprintf(buf, 256, "%i / %i", position, duration);

  clutter_label_set_text (label, buf);

  clutter_texture_get_base_size (CLUTTER_TEXTURE(label), &w, &h);
  clutter_element_set_size(rect, w+10, h+10);
}

int
main (int argc, char *argv[])
{
  ClutterElement        *label, *vtexture, *ctexture;
  ClutterElement        *stage;
  ClutterColor           rect_color =  { 0xde, 0xde, 0xdf, 0xaa };
  ClutterColor           stage_color = { 0x00, 0x00, 0x00, 0x00 };
  GError                *err = NULL;

  if (argc < 2)
    g_error("%s <video file>", argv[0]);

  clutter_init (&argc, &argv);

  vtexture = clutter_video_texture_new ();

  clutter_media_set_filename(CLUTTER_MEDIA(vtexture), argv[1]);

  stage = clutter_stage_get_default ();

  /* Broken..
  g_object_set(vtexture, "repeat-x", TRUE, NULL);
  g_object_set(vtexture, "repeat-y", TRUE, NULL);
  */

  if (vtexture == NULL || err != NULL)
    {
      g_error("failed to create vtexture, err: %s", err->message);
    }

  label = clutter_label_new_with_text ("Sans Bold 24", "Loading...");

  clutter_element_set_position(label, 10, 10);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_element_set_size(rect, 0, 0);
  clutter_element_set_position(rect, 5, 5);

  ctexture = clutter_clone_texture_new (CLUTTER_TEXTURE(vtexture));
  clutter_element_set_opacity (CLUTTER_ELEMENT (ctexture), 100);

  clutter_element_set_size (ctexture, 640, 50);
  clutter_element_set_position (ctexture, 0, 430);

  clutter_group_add_many (CLUTTER_GROUP (stage), 
			  vtexture, rect, label, ctexture, NULL);

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color); 

  g_signal_connect (stage, "input-event",
		    G_CALLBACK (input_cb), 
		    vtexture);
 
  clutter_group_show_all (CLUTTER_GROUP (stage));

  clutter_media_set_playing (CLUTTER_MEDIA(vtexture), TRUE);

  g_signal_connect (CLUTTER_MEDIA(vtexture),
		    "notify::position",
		    G_CALLBACK (tick),
		    label);

  g_object_set (G_OBJECT(vtexture), "sync-size", FALSE, NULL);

  g_signal_connect (CLUTTER_TEXTURE(vtexture), 
		    "size-change",
		    G_CALLBACK (size_change), NULL);

  clutter_main();

  g_object_unref (stage);

  return 0;
}
