#include <clutter/cltr.h>

gchar *ImgPath = NULL;

int 
usage(char *progname)
{
  fprintf(stderr, "Usage ... check source for now\n");
  exit(-1);
}

gpointer
photo_grid_populate(gpointer data) 
{
  CltrPhotoGrid    *grid = (CltrPhotoGrid *)data;
  GDir             *dir;
  GError           *error;
  const gchar      *entry = NULL;
  gchar            *fullpath = NULL;
  int               n_pixb = 0, i =0;
  ClutterFont      *font = NULL;
  PixbufPixel       font_col = { 255, 0, 0, 255 };

  font = font_new("Sans Bold 96");

  if ((dir = g_dir_open (ImgPath, 0, &error)) == NULL)
    {
      /* handle this much better */
      fprintf(stderr, "failed to open '%s'\n", ImgPath);
      return NULL;
    }

  while ((entry = g_dir_read_name (dir)) != NULL)
    {
      if (!strcasecmp(&entry[strlen(entry)-4], ".png")
	  || !strcasecmp(&entry[strlen(entry)-4], ".jpg")
	  || !strcasecmp(&entry[strlen(entry)-5], ".jpeg"))
	n_pixb++;
    }

  g_dir_rewind (dir);

  while ((entry = g_dir_read_name (dir)) != NULL)
    {
      Pixbuf *pixb = NULL; 
      fullpath = g_strconcat(ImgPath, "/", entry, NULL);
 
      pixb = pixbuf_new_from_file(fullpath);

      if (pixb)
	{
	  CltrPhotoGridCell *cell;
	  gchar              buf[24];
	  Pixbuf            *tmp_pixb;

	  cell = cltr_photo_grid_cell_new(grid, pixb);

	  /*
	  g_snprintf(&buf[0], 24, "%i", i);
	  font_draw(font, cltr_photo_grid_cell_pixbuf(cell), 
		    buf, 10, 10, &font_col);
	  */
	  g_mutex_lock(cltr_photo_grid_mutex(grid));

	  if (!cltr_photo_grid_get_active_cell(grid))
	    cltr_photo_grid_set_active_cell(grid,
					    cltr_photo_grid_get_first_cell(grid));

	  cltr_photo_grid_append_cell(grid, cell);

	  g_mutex_unlock(cltr_photo_grid_mutex(grid));

	  i++;
	}

      g_free(fullpath);
    }

  g_dir_close (dir);

  g_mutex_lock(cltr_photo_grid_mutex(grid));

  cltr_photo_grid_set_populated(grid, TRUE);

  g_mutex_unlock(cltr_photo_grid_mutex(grid));

  cltr_widget_queue_paint(CLTR_WIDGET(grid));

  return NULL;
}

int
main(int argc, char **argv)
{
  CltrWidget *win = NULL, *grid = NULL;
  gchar      *img_path = NULL;
  gboolean    want_fullscreen = FALSE;
  gint        i, cols = 3;

  GThread    *loader_thread; 

  cltr_init(&argc, &argv);

  if (argc < 2)
    usage(argv[0]);

  for (i = 1; i < argc; i++) 
    {
      if (!strcmp ("--image-path", argv[i]) || !strcmp ("-i", argv[i])) 
	{
	  if (++i>=argc) usage (argv[0]);
	  ImgPath = argv[i];
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

  win = cltr_window_new(640, 480);

  if (want_fullscreen)
    cltr_window_set_fullscreen(CLTR_WINDOW(win));

  grid = cltr_photo_grid_new(640, 480, cols, cols, ImgPath);

  cltr_window_focus_widget(CLTR_WINDOW(win), grid);

  cltr_widget_add_child(win, grid, 0, 0);

  cltr_widget_show_all(win);

  /* grid->state = CLTR_PHOTO_GRID_STATE_BROWSE; */

  loader_thread = g_thread_create (photo_grid_populate,
				   (gpointer)grid,
				   TRUE,
				   NULL);


  cltr_main_loop();

  return 0;
}
