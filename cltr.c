#include "cltr.h"

int
main(int argc, char **argv)
{
  CltrWidget    *win = NULL, *grid = NULL;

  if (argc < 2)
    {
      g_printerr("usage: '%s' <path to not too heavily populated image dir>\n"
		 , argv[0]);
      exit(-1);
    }

  cltr_init(&argc, &argv);

  win = cltr_window_new(640, 480);

  grid = cltr_photo_grid_new(640, 480, 3, 3, argv[1]);

  cltr_widget_add_child(win, grid, 0, 0);

  cltr_widget_show_all(win);

  cltr_main_loop();

  return 0;
}
