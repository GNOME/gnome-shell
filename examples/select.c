#include <clutter/cltr.h>

int 
usage(char *progname)
{
  fprintf(stderr, "Usage ... check source for now\n");
  exit(-1);
}

gboolean
populate(CltrList *list, char *path)
{
  GDir             *dir;
  GError           *error;
  const gchar      *entry = NULL;
  int               n_pixb = 0, i =0;
  
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
      Pixbuf       *pixb = default_thumb_pixb;
      CltrListCell *cell;
      gchar        *nice_name = NULL;
      gint          i = 0;

      if (!(g_str_has_suffix (entry, ".mpg") ||
	    g_str_has_suffix (entry, ".MPG") ||
	    g_str_has_suffix (entry, ".mpg4") ||
	    g_str_has_suffix (entry, ".MPG4") ||
	    g_str_has_suffix (entry, ".avi") ||
	    g_str_has_suffix (entry, ".AVI")))
	{
	  continue;
	}

      nice_name = g_strdup(entry);

      i = strlen(nice_name) - 1;
      while (i-- && nice_name[i] != '.') ;
      if (i > 0) 
	nice_name[i] = '\0';

      cell = cltr_list_cell_new(list, pixb, nice_name);

      cltr_list_append_cell(list, cell);

      g_free(nice_name);

      g_printf(".");
    }

  g_dir_close (dir);

  g_printf("\n");

  return TRUE;
}

void
cell_activated (CltrList     *list, 
		CltrListCell *cell,
		void         *userdata)
{
  int           x1, y1, x2, y2;
  CltrAnimator *anim = NULL;

  cltr_list_get_active_cell_co_ords(CLTR_LIST(list), &x1, &y1, &x2, &y2);

  anim = cltr_animator_fullzoom_new(CLTR_LIST(list), x1, y1, x1+80, y1+60);

  cltr_animator_run(anim, NULL, NULL);
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

  CltrAnimator *anim = NULL;

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

  win = cltr_window_new(800, 600);

  if (want_fullscreen)
    cltr_window_set_fullscreen(CLTR_WINDOW(win));

  list = cltr_list_new(800, 600, 800, 600/5);
  
  if (!populate(list, movie_path))
      exit(-1);

  cltr_widget_add_child(win, list, 0, 0);

  cltr_window_focus_widget(CLTR_WINDOW(win), list);

  cltr_widget_show_all(win);

  cltr_list_on_activate_cell(CLTR_LIST(list), cell_activated, NULL);

  cltr_main_loop();
}
