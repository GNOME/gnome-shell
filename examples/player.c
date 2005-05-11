/* GStreamer
 * Copyright (C) 2003 Julien Moutte <julien@moutte.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <clutter/cltr.h>

int Paused = 0;

void
handle_xevent(CltrWidget *win, XEvent *xev, void *cookie)
{
  KeySym          kc;
  CltrVideo      *video = CLTR_VIDEO(cookie);

  if (xev->type == KeyPress)
    {
      XKeyEvent *xkeyev = &xev->xkey;

      kc = XKeycodeToKeysym(xkeyev->display, xkeyev->keycode, 0);

      switch (kc)
	{
	case XK_Return:
	  if (Paused)
	    cltr_video_play (video, NULL);
	  else
	    cltr_video_pause (video);
	  Paused ^= 1;

	  break;
	}
    }

}

int
main (int argc, char *argv[])
{
  CltrWidget *win, *video, *label;
  CltrFont   *font;
  PixbufPixel col = { 0xff, 0xff, 0xff, 0x66 };

  pixel_set_vals(&col, 0x00, 0x00, 0x00, 0x99);

  cltr_init (&argc, &argv);

  if (argc != 2) {
    g_print ("usage: %s <video filename>\n", argv[0]);
    exit (-1);
  }

  font = font_new("Sans bold 96");

  win = cltr_window_new(800, 600);

  video = cltr_video_new(800, 600);

  cltr_video_set_source(CLTR_VIDEO(video), argv[1]);

  cltr_widget_add_child(win, video, 0, 0);  

  label = cltr_label_new("hello world", font, &col);

  cltr_widget_add_child(win, label, 100, 300);  

  cltr_window_on_xevent(CLTR_WINDOW(win), handle_xevent, video);

  cltr_widget_show_all(win);

  cltr_video_play(CLTR_VIDEO(video), NULL);

  cltr_main_loop();

  exit (0);
}
