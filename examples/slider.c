#include <clutter/clutter.h>

typedef struct Tile 
{
  ClutterActor *actor;
  gint orig_pos;
} 
Tile;

static Tile                  *Tiles[4][4];
static int                    TileW, TileH, BlankTileX, BlankTileY;
static ClutterEffectTemplate *Template;
static ClutterTimeline       *EffectTimeline;

ClutterActor*
make_tiles (GdkPixbuf *pixbuf)
{
  int x, y , w, h;
  int i = 0, j = 0;
  int pos = 0;
  ClutterActor *group;

  group = clutter_group_new();

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  TileW = w / 4;
  TileH = h / 4;

  for (y = 0; y < h; y += TileH)
    {
      for (x = 0; x < w; x += TileW)
	{
	  GdkPixbuf *subpixbuf;
	  Tile      *tile;
	  
	  subpixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 
				      8, TileW, TileH);
	  
	  gdk_pixbuf_copy_area (pixbuf, x, y, TileW, TileH, 
				subpixbuf, 0, 0);
	  
	  tile = g_slice_new0 (Tile);
	  
	  if (pos != 15)
	    {
	      tile->actor = clutter_texture_new_from_pixbuf (subpixbuf);
	      clutter_group_add (CLUTTER_GROUP (group), tile->actor);
	      clutter_actor_set_position (tile->actor, x, y);
	    } 
	  else 
	    {
	      /* blank tile */
	      tile->actor = NULL;
	      BlankTileX = i;
	      BlankTileY = j;
	    }
	  
	  g_object_unref (subpixbuf);

	  tile->orig_pos = pos;
	  Tiles[j][i] = tile;
	  
	  pos++; i++;
	}
      i=0; j++;
    }

  return group;
}

static void
switch_blank_tile (int i, int j)
{
  Tile            *tmp;
  ClutterKnot      knots[2];

  knots[0].x = i * TileW;
  knots[0].y = j * TileH; 

  knots[1].x = BlankTileX * TileW;
  knots[1].y = BlankTileY * TileH;

  EffectTimeline = clutter_effect_move (Template,
					Tiles[j][i]->actor,
					knots,
					2,
					NULL,
					NULL);

  /* Add a week pointer to returned timeline so we know whilst its
   * playing and thus valid. 
  */
  g_object_add_weak_pointer (G_OBJECT(EffectTimeline), 
			     (gpointer*)&EffectTimeline);

  tmp = Tiles[BlankTileY][BlankTileX];
  Tiles[BlankTileY][BlankTileX] = Tiles[j][i];
  Tiles[j][i] = tmp;
  
  BlankTileY = j;
  BlankTileX = i;
}

static void
key_press_event_cb (ClutterStage    *stage, 
		    ClutterKeyEvent *event, 
		    gpointer         user_data)
{
  Tile *tmp, *tmp2;

  if (clutter_key_event_symbol(event) == CLUTTER_q)
    clutter_main_quit();

  /* Do move if there is a move already happening */
  if (EffectTimeline != NULL)
    return;
  
  switch (clutter_key_event_symbol(event))
    {
    case CLUTTER_Up:
      if (BlankTileY < 3)
	  switch_blank_tile (BlankTileX, BlankTileY+1);
      break;
    case CLUTTER_Down:
      if (BlankTileY > 0)
	  switch_blank_tile (BlankTileX, BlankTileY-1);
      break;
    case CLUTTER_Left:
      if (BlankTileX < 3)
	switch_blank_tile (BlankTileX+1, BlankTileY);
      break;
    case CLUTTER_Right:
      if (BlankTileX > 0)
	switch_blank_tile (BlankTileX-1, BlankTileY);
      break;
    default:
      break;
    }
}

int
main (int argc, char **argv)
{
  GdkPixbuf    *pixbuf;
  ClutterActor *stage, *group;
  ClutterColor  bgcolour;

  /* Initiate clutter */
  clutter_init (&argc, &argv);

  /* Setup the stage */
  stage = clutter_stage_get_default ();
  g_object_set (stage, "fullscreen", TRUE, NULL);  

  clutter_color_parse ("#000000", &bgcolour);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &bgcolour);

  /* Create Tiles */
  pixbuf = gdk_pixbuf_new_from_file ("image.jpg", NULL);
  group = make_tiles (pixbuf);

  /* Add to stage and center */
  clutter_group_add (CLUTTER_GROUP (stage), group);
  clutter_actor_set_position (group, 
   (clutter_actor_get_width (stage) - clutter_actor_get_width (group)) / 2, 
   (clutter_actor_get_height (stage) - clutter_actor_get_height (group)) / 2);

  /* Link up event collection */
  g_signal_connect (stage, 
		    "key-press-event", 
		    G_CALLBACK(key_press_event_cb), 
		    NULL);

  /* Template to use for slider animation */
  Template = clutter_effect_template_new (clutter_timeline_new (15, 60),
					  CLUTTER_ALPHA_RAMP_INC);

  clutter_actor_show_all (stage);

  clutter_main();
}
