#include <clutter/cltr.h>

typedef struct ItemEntry ItemEntry;
typedef struct VideoCtrls VideoCtrls;

typedef struct DemoApp
{
  CltrAnimator *anim;
  CltrWidget   *list;
  CltrWidget   *video;
  CltrWidget   *win;
  VideoCtrls   *video_ctrls;

  GList        *items;

} DemoApp;

struct ItemEntry
{
  gchar        *nice_name;
  gchar        *path;
  gchar        *uri;
  gint64        stoptime;
  CltrListCell *cell;
};

enum {
  VIDEO_PLAY_BTN = 0,
  VIDEO_STOP_BTN,
  VIDEO_REWND_BTN,
  VIDEO_FFWD_BTN,
  N_VIDEO_BTNS
};

struct VideoCtrls
{
  ClutterFont *font;


  CltrWidget *container;
  CltrWidget *buttons[N_VIDEO_BTNS];
};

static void
zoom_out_complete (CltrAnimator *anim, void *userdata);

int 
usage(char *progname)
{
  fprintf(stderr, "Usage ... check source for now\n");
  exit(-1);
}

/* video control buttons */

void
init_video_ctrl(DemoApp *app)
{
  VideoCtrls *v;
  int         width, height, x =0, y = 0;
  PixbufPixel col = { 0xff, 0xff, 0xff, 0xff };

  v = app->video_ctrls = g_malloc0(sizeof(VideoCtrls));

  v->font = font_new ("Sans Bold 20");

  font_get_pixel_size (v->font, "1234567890", &width, &height); 

  height += 6;

  v->container = cltr_overlay_new(width, height * N_VIDEO_BTNS);

  v->buttons[VIDEO_PLAY_BTN] = cltr_button_new(width, height);

  cltr_button_set_label(v->buttons[VIDEO_PLAY_BTN],
			"PlAY", v->font, &col);

  cltr_widget_add_child(v->container, 
			v->buttons[VIDEO_PLAY_BTN],
 			x, y);
  y += height;

  v->buttons[VIDEO_STOP_BTN] =  cltr_button_new(width, height); 

  cltr_button_set_label(v->buttons[VIDEO_STOP_BTN],
			"STOP", 
			v->font, &col);

  cltr_widget_add_child(v->container, 
			v->buttons[VIDEO_STOP_BTN],
			x, y);
  y += height;


  v->buttons[VIDEO_REWND_BTN] =  cltr_button_new(width, height);

  cltr_button_set_label(v->buttons[VIDEO_REWND_BTN],
			"RWND", 
			v->font, &col);

  cltr_widget_add_child(v->container, 
			v->buttons[VIDEO_REWND_BTN],
			x, y);
  y += height;

  v->buttons[VIDEO_FFWD_BTN] =  cltr_button_new(width, height);

  cltr_button_set_label(v->buttons[VIDEO_FFWD_BTN],
			"FFWD", 
			v->font, &col);

  cltr_widget_add_child(v->container, 
			v->buttons[VIDEO_FFWD_BTN],
			x, y);
  y += height;
  
  cltr_widget_add_child(app->video, v->container, 100, 100);

  /* focus */

  cltr_widget_set_focus_next(v->buttons[VIDEO_PLAY_BTN], 
			     v->buttons[VIDEO_STOP_BTN], 
			     CLTR_SOUTH);

  cltr_widget_set_focus_next(v->buttons[VIDEO_STOP_BTN], 
			     v->buttons[VIDEO_PLAY_BTN], 
			     CLTR_NORTH);

}

void
show_video_ctrl(DemoApp *app)
{
  cltr_widget_show_all(app->video_ctrls->container);
}



/* ********************* */

gboolean
populate(DemoApp *app, char *path)
{
  GDir             *dir;
  GError           *error;
  const gchar      *entry = NULL;
  int               n_pixb = 0, i =0;
  CltrList         *list = CLTR_LIST(app->list);
  Pixbuf           *default_thumb_pixb = NULL;

  default_thumb_pixb = pixbuf_new_from_file("clutter-logo-800x600.png");

  if (!default_thumb_pixb)
    g_error( "failed to open clutter-logo-800x600.png\n");

  if ((dir = g_dir_open (path, 0, &error)) == NULL)
    {
      /* handle this much better */
      g_error( "failed to open '%s'\n", path);
      return FALSE;
    }

  g_printf("One sec.");

  while ((entry = g_dir_read_name (dir)) != NULL)
    {
      Pixbuf       *pixb = NULL;
      gint          i = 0;
      ItemEntry    *new_item;
      char         *img_path;

      /* Eeek! */
      if (!(g_str_has_suffix (entry, ".mpg") ||
	    g_str_has_suffix (entry, ".MPG") ||
	    g_str_has_suffix (entry, ".mpg4") ||
	    g_str_has_suffix (entry, ".MPG4") ||
	    g_str_has_suffix (entry, ".avi") ||
	    g_str_has_suffix (entry, ".mov") ||
	    g_str_has_suffix (entry, ".MOV") ||
	    g_str_has_suffix (entry, ".ogg") ||
	    g_str_has_suffix (entry, ".OGG") ||
	    g_str_has_suffix (entry, ".AVI")))
	{
	  continue;
	}

      new_item = g_malloc0(sizeof(ItemEntry));

      new_item->nice_name = g_strdup(entry);

      i = strlen(new_item->nice_name) - 1;
      while (i-- && new_item->nice_name[i] != '.') ;
      if (i > 0) 
	new_item->nice_name[i] = '\0';

      img_path = g_strconcat(path, "/", new_item->nice_name, ".png", NULL);

      pixb = pixbuf_new_from_file(img_path);

      if (!pixb) 
	pixb = default_thumb_pixb;

      new_item->cell = cltr_list_cell_new(list, pixb, new_item->nice_name);

      cltr_list_append_cell(list, new_item->cell);

      new_item->uri = g_strconcat("file://", path, "/", entry, NULL);
      new_item->path = g_strdup(path);

      app->items = g_list_append(app->items, new_item);

      g_free(img_path);

      g_printf(".");
    }

  g_dir_close (dir);

  g_printf("\n");

  return TRUE;
}

ItemEntry*
cell_to_item(DemoApp *app, CltrListCell *cell)
{
  GList *item = NULL;

  item = g_list_first(app->items);

  while (item)
    {
      ItemEntry *entry = item->data;

      if (entry->cell == cell)
	return entry;

      item = g_list_next(item);
    }

  return NULL;
}



void
handle_xevent(CltrWidget *win, XEvent *xev, void *cookie)
{
  KeySym          kc;
  DemoApp        *app = (DemoApp*)cookie;

  if (xev->type == KeyPress)
    {
      XKeyEvent *xkeyev = &xev->xkey;

      kc = XKeycodeToKeysym(xkeyev->display, xkeyev->keycode, 0);

      switch (kc)
	{
	case XK_Return:
	  {
	    ItemEntry    *item;
	    char          filename[1024];
	    Pixbuf       *spixb, *dpixb;
	    int           dstx, dsty, dstw, dsth;
	    PixbufPixel   col = { 0, 0, 0, 0xff };
	    int           x1, y1, x2, y2;

	    cltr_video_pause (CLTR_VIDEO(app->video));

	    item = cell_to_item(app, cltr_list_get_active_cell(app->list));

	    item->stoptime = cltr_video_get_time (app->video);

	    snprintf(filename, 1024, "%s/%s.png", item->path, item->nice_name);

	    spixb = cltr_video_get_pixbuf (app->video);

	    /* fixup pixbuf so scaled like video 
             *
	    */

	    /* XXX wrongly assume width > height */

	    dstw = spixb->width;

	    dsth = (spixb->width * cltr_widget_height(win)) 
	                  / cltr_widget_width(win) ;

	    printf("dsth %i, spixb h %i\n", dsth, spixb->height);

	    dsty = (dsth - spixb->height)/2; dstx = 0;

	    dpixb = pixbuf_new(dstw, dsth);
	    pixbuf_fill_rect(dpixb, 0, 0, -1, -1, &col);
	    pixbuf_copy(spixb, dpixb, 0, 0, 
			spixb->width, spixb->height, dstx, dsty);

	    cltr_list_cell_set_pixbuf(cltr_list_get_active_cell(app->list),
				      dpixb);

	    pixbuf_write_png(dpixb, filename);


	    /* reset the viewing pixbuf */

	    pixbuf_unref(dpixb);

	    cltr_list_get_active_cell_video_box_co_ords(CLTR_LIST(app->list), 
							&x1, &y1, &x2, &y2);

	    cltr_video_stop (CLTR_VIDEO(app->video));

	    /* zoom out, XXX old anim needs freeing */

	    app->anim = cltr_animator_zoom_new(app->list,
					       x1, y1, x2, y2,
					       0,0,800,600);

	    printf("got return, seek time %li, %i, %i \n", 
		   cltr_video_get_time (CLTR_VIDEO(app->video)),
		   x1, y1);

	    cltr_widget_show(app->list);
	    
	    cltr_animator_run(app->anim, zoom_out_complete, app);
	  }
	  break;
	}
    }

}

static void
zoom_out_complete (CltrAnimator *anim, void *userdata)
{
  DemoApp *app = (DemoApp*)userdata;

  cltr_window_on_xevent(CLTR_WINDOW(app->win), NULL, NULL);



  cltr_widget_hide(app->video);

  cltr_widget_queue_paint(app->win);
}

void
zoom_in_complete (CltrAnimator *anim, void *userdata)
{
  DemoApp      *app = (DemoApp*)userdata;
  ItemEntry    *item;

  /* cltr_animator_reset(anim); */

  item = cell_to_item(app, cltr_list_get_active_cell(app->list));

  cltr_video_set_source(CLTR_VIDEO(app->video), item->uri);

  if (item->stoptime)
    {
      printf("*** seeking to %li\n", item->stoptime);
      cltr_video_seek_time (CLTR_VIDEO(app->video), item->stoptime, NULL);
    }

  cltr_video_play(CLTR_VIDEO(app->video), NULL);

  if (item->stoptime)
    {
      printf("*** seeking to %li\n", item->stoptime);
      cltr_video_seek_time (CLTR_VIDEO(app->video), item->stoptime, NULL);
    }

  cltr_widget_show(app->video);

  cltr_widget_hide(CLTR_WIDGET(app->list));

  show_video_ctrl(app);

  cltr_window_on_xevent(CLTR_WINDOW(app->win), handle_xevent, app);
}

void
cell_activated (CltrList     *list, 
		CltrListCell *cell,
		void         *userdata)
{
  DemoApp      *app = (DemoApp*)userdata;
  int           x1, y1, x2, y2;
  static        have_added_child = 0; /* HACK */

  cltr_list_get_active_cell_video_box_co_ords(CLTR_LIST(list), 
					      &x1, &y1, &x2, &y2);

  if (app->video == NULL) 
    {
      /*
      app->video = cltr_video_new(x2-x1, y2-y1);
      cltr_widget_add_child(app->win, app->video, x1, y1);
      */
      app->video = cltr_video_new(800, 600);
      cltr_widget_add_child(app->win, app->video, 0, 0);

      init_video_ctrl(app);
    }

  app->anim = cltr_animator_zoom_new(CLTR_WIDGET(list),
				     0,0,800,600,
				     x1, y1, x2, y2);
  have_added_child = 1;

  cltr_animator_run(app->anim, zoom_in_complete, app);
}

int
main(int argc, char **argv)
{
  int         i;
  CltrWidget *win = NULL, *list = NULL;
  CltrFont   *font = NULL;
  PixbufPixel col = { 0xff, 0, 0, 0xff };

  gchar      *movie_path = NULL;
  gboolean    want_fullscreen = FALSE;
  gint        cols = 3;

  DemoApp *app;

  cltr_init(&argc, &argv);

  for (i = 1; i < argc; i++) 
    {
      if (!strcmp ("--movie-path", argv[i]) || !strcmp ("-i", argv[i])) 
	{
	  if (++i>=argc) usage (argv[0]);
	  movie_path = argv[i];
	  continue;
	}
      if (!strcmp ("--cols", argv[i]) || !strcmp ("-c", argv[i])) 
	{
	  if (++i>=argc) usage (argv[0]);
	  cols = atoi(argv[i]);
	  continue;
	}
      if (!strcmp ("-fs", argv[i]) || !strcmp ("--fullscreen", argv[i])) 
	{
	  want_fullscreen = TRUE;
	  continue;
	}
      if (!strcmp("--help", argv[i]) || !strcmp("-h", argv[i])) 
	{
	  usage(argv[0]);
	}

      usage(argv[0]);
    }

  if (!movie_path)
    {
      g_error("usage: %s -i <movies path>", argv[0]);
      exit(-1);
    }

  app = g_malloc0(sizeof(DemoApp));

  app->win = cltr_window_new(800, 600);

  if (want_fullscreen)
    cltr_window_set_fullscreen(CLTR_WINDOW(app->win));



  app->list = cltr_list_new(800, 600, 800, 600/5);
  
  if (!populate(app, movie_path))
      exit(-1);

  cltr_widget_add_child(app->win, app->list, 0, 0);

  cltr_window_focus_widget(CLTR_WINDOW(app->win), app->list);

  cltr_widget_show_all(app->win);


  cltr_list_on_activate_cell(CLTR_LIST(app->list), 
			     cell_activated, (gpointer)app);

  cltr_main_loop();
}
