#include <clutter/cltr.h>

int 
usage(char *progname)
{
  fprintf(stderr, "Usage ... check source for now\n");
  exit(-1);
}

int
main(int argc, char **argv)
{
  int         i;
  CltrWidget *win = NULL, *grid = NULL, *test = NULL, *test2 = NULL;

  gchar      *img_path = NULL;
  gboolean    want_fullscreen = FALSE;
  gint        cols = 3;

  cltr_init(&argc, &argv);

  for (i = 1; i < argc; i++) 
    {
      if (!strcmp ("--image-path", argv[i]) || !strcmp ("-i", argv[i])) 
	{
	  if (++i>=argc) usage (argv[0]);
	  img_path = argv[i];
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

  win = cltr_window_new(800, 600);

  if (want_fullscreen)
    cltr_window_set_fullscreen(CLTR_WINDOW(win));


  grid = cltr_photo_grid_new(800, 600, cols, cols, img_path);

  cltr_window_focus_widget(CLTR_WINDOW(win), grid);

  cltr_widget_add_child(win, grid, 0, 0);


  /*
  test = cltr_scratch_new(300, 100);
  test2 = cltr_scratch_new(150, 150);

  cltr_widget_add_child(win, test, 400, 240);
  */



  /*
  cltr_widget_add_child(win, test, 320, 240);
  cltr_widget_add_child(win, test2, 400, 300);

  list = cltr_list_new(640,480,640, 160);

  cltr_widget_add_child(win, list, 0, 0);

  */


  cltr_widget_show_all(win);

  cltr_main_loop();

  return 0;
}
