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
  CltrWidget *win = NULL, *list = NULL;
  CltrFont   *font = NULL;
  PixbufPixel col = { 0xff, 0, 0, 0xff };

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

  list = cltr_list_new(800, 600, 800, 600/5);
  
  cltr_widget_add_child(win, list, 0, 0);

  cltr_window_focus_widget(CLTR_WINDOW(win), list);

  cltr_widget_show_all(win);

  cltr_main_loop();
}
