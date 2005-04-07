#include "cltr.h"

int
main(int argc, char **argv)
{
  CltrWidget    *win = NULL, *grid = NULL, *test = NULL, *test2 = NULL;
  CltrWidget    *list;

  if (argc < 2)
    {
      g_printerr("usage: '%s' <path to not too heavily populated image dir>\n"
		 , argv[0]);
      exit(-1);
    }

  cltr_init(&argc, &argv);

  win = cltr_window_new(640, 480);

  /*
  grid = cltr_photo_grid_new(640, 480, 4, 4, argv[1]);

  test = cltr_scratch_new(100, 100);
  test2 = cltr_scratch_new(150, 150);

  cltr_widget_add_child(win, grid, 0, 0);


  cltr_widget_add_child(win, test, 320, 240);
  cltr_widget_add_child(win, test2, 400, 300);

  cltr_window_focus_widget(CLTR_WINDOW(win), grid);
  */

  list = cltr_list_new(640,480,640, 160);

  cltr_widget_add_child(win, list, 0, 0);

  cltr_widget_show_all(win);

  cltr_main_loop();

  return 0;
}
