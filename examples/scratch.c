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

  font = font_new("Sans 20");

  test = cltr_button_new_with_label("ButtonBoooo\ndsfdsfdsf sss\nsjhsjhsjhs", font, &col);

  test2 = cltr_button_new_with_label("Button", font, &col);

  cltr_widget_add_child(win, test, 300, 100);

  cltr_widget_add_child(win, test2, 100, 100);

  cltr_window_focus_widget(CLTR_WINDOW(win), test);

  cltr_widget_set_focus_next(test, test2, CLTR_EAST);
  cltr_widget_set_focus_next(test, test2, CLTR_WEST);
  cltr_widget_set_focus_next(test2, test, CLTR_EAST);
  cltr_widget_set_focus_next(test2, test, CLTR_WEST);


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
