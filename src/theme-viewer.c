/* Metacity theme viewer and test app main() */

/* 
 * Copyright (C) 2002 Havoc Pennington
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include "util.h"
#include "theme.h"
#include <gtk/gtk.h>
#include <time.h>

static void run_position_expression_tests (void);
static void run_position_expression_timings (void);  

static int client_width = 200;
static int client_height = 200;

static void
set_widget_to_frame_size (MetaFrameStyle *style,
                          GtkWidget      *widget)
{
  int top_height, bottom_height, left_width, right_width;

  meta_frame_layout_get_borders (style->layout,
                                 widget,
                                 15, /* FIXME */
                                 0,  /* FIXME */
                                 &top_height,
                                 &bottom_height,
                                 &left_width,
                                 &right_width);

  gtk_widget_set_size_request (widget,
                               client_width + left_width + right_width,
                               client_height + top_height + bottom_height);
}

static gboolean
expose_handler (GtkWidget *widget,
                GdkEventExpose *event,
                gpointer data)
{
  MetaFrameStyle *style;
  MetaButtonState button_states[META_BUTTON_TYPE_LAST] =
  {
    META_BUTTON_STATE_NORMAL,
    META_BUTTON_STATE_NORMAL,
    META_BUTTON_STATE_NORMAL,
    META_BUTTON_STATE_NORMAL
  };
  int top_height, bottom_height, left_width, right_width;

  style = meta_frame_style_get_test ();
  
  meta_frame_layout_get_borders (style->layout,
                                 widget,
                                 15, /* FIXME */
                                 0,  /* FIXME */
                                 &top_height,
                                 &bottom_height,
                                 &left_width,
                                 &right_width);

  meta_frame_style_draw (style,
                         widget,
                         widget->window,
                         0, 0,
                         &event->area,
                         0, /* flags */
                         client_width, client_height,
                         NULL, /* FIXME */
                         15,   /* FIXME */
                         button_states);

  /* Draw the "client" */

  gdk_draw_rectangle (widget->window,
                      widget->style->white_gc, 
                      TRUE,
                      left_width, top_height,
                      client_width, client_height);
  
  return TRUE;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *layout;
  GtkWidget *sw;
  GtkWidget *da;
  GdkColor desktop_color;
  
  bindtextdomain (GETTEXT_PACKAGE, METACITY_LOCALEDIR);
  
  run_position_expression_tests ();
#if 0
  run_position_expression_timings ();
#endif
  
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 270, 270);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (window), sw);

  layout = gtk_layout_new (NULL, NULL);

  gtk_layout_set_size (GTK_LAYOUT (layout), 500, 500);
  
  gtk_container_add (GTK_CONTAINER (sw), layout);

  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

  desktop_color.red = 0x5144;
  desktop_color.green = 0x75D6;
  desktop_color.blue = 0xA699;
  
  gtk_widget_modify_bg (layout, GTK_STATE_NORMAL, &desktop_color);

  da = gtk_drawing_area_new ();
  set_widget_to_frame_size (meta_frame_style_get_test (),
                            da);

  g_signal_connect (G_OBJECT (da), "expose_event",
                    G_CALLBACK (expose_handler), NULL);

  gtk_layout_put (GTK_LAYOUT (layout),
                  da,
                  5, 5);
  
  gtk_widget_show_all (window);
  
  gtk_main ();
  
  return 0;
}

typedef struct
{
  GdkRectangle rect;
  const char *expr;
  int expected_x;
  int expected_y;
  MetaPositionExprError expected_error;
} PositionExpressionTest;

#define NO_ERROR -1

static const PositionExpressionTest position_expression_tests[] = {
  /* Just numbers */
  { { 10, 20, 40, 50 },
    "10", 20, 30, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14.37", 24, 34, NO_ERROR },
  /* Binary expressions with 2 ints */
  { { 10, 20, 40, 50 },
    "14 * 10", 150, 160, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14 + 10", 34, 44, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14 - 10", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "8 / 2", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "8 % 3", 12, 22, NO_ERROR },
  /* Binary expressions with floats and mixed float/ints */
  { { 10, 20, 40, 50 },
    "7.0 / 3.5", 12, 22, NO_ERROR },
  { { 10, 20, 40, 50 },
    "12.1 / 3", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "12 / 2.95", 14, 24, NO_ERROR },
  /* Binary expressions without whitespace after first number */
  { { 10, 20, 40, 50 },
    "14* 10", 150, 160, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14+ 10", 34, 44, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14- 10", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "8/ 2", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "7.0/ 3.5", 12, 22, NO_ERROR },
  { { 10, 20, 40, 50 },
    "12.1/ 3", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "12/ 2.95", 14, 24, NO_ERROR },
  /* Binary expressions without whitespace before second number */
  { { 10, 20, 40, 50 },
    "14 *10", 150, 160, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14 +10", 34, 44, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14 -10", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "8 /2", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "7.0 /3.5", 12, 22, NO_ERROR },
  { { 10, 20, 40, 50 },
    "12.1 /3", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "12 /2.95", 14, 24, NO_ERROR },
  /* Binary expressions without any whitespace */
  { { 10, 20, 40, 50 },
    "14*10", 150, 160, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14+10", 34, 44, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14-10", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "8/2", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "7.0/3.5", 12, 22, NO_ERROR },
  { { 10, 20, 40, 50 },
    "12.1/3", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "12/2.95", 14, 24, NO_ERROR },
  /* Binary expressions with parentheses */
  { { 10, 20, 40, 50 },
    "(14) * (10)", 150, 160, NO_ERROR },
  { { 10, 20, 40, 50 },
    "(14) + (10)", 34, 44, NO_ERROR },
  { { 10, 20, 40, 50 },
    "(14) - (10)", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "(8) / (2)", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "(7.0) / (3.5)", 12, 22, NO_ERROR },
  { { 10, 20, 40, 50 },
    "(12.1) / (3)", 14, 24, NO_ERROR },
  { { 10, 20, 40, 50 },
    "(12) / (2.95)", 14, 24, NO_ERROR },
  /* Lots of extra parentheses */
  { { 10, 20, 40, 50 },
    "(((14)) * ((10)))", 150, 160, NO_ERROR },
  { { 10, 20, 40, 50 },
    "((((14)))) + ((((((((10))))))))", 34, 44, NO_ERROR },
  { { 10, 20, 40, 50 },
    "((((((((((14 - 10))))))))))", 14, 24, NO_ERROR },
  /* Binary expressions with variables */
  { { 10, 20, 40, 50 },
    "2 * width", 90, 100, NO_ERROR },
  { { 10, 20, 40, 50 },
    "2 * height", 110, 120, NO_ERROR },
  { { 10, 20, 40, 50 },
    "width - 10", 40, 50, NO_ERROR },
  { { 10, 20, 40, 50 },
    "height / 2", 35, 45, NO_ERROR },
  /* More than two operands */
  { { 10, 20, 40, 50 },
    "8 / 2 + 5", 19, 29, NO_ERROR },
  { { 10, 20, 40, 50 },
    "8 * 2 + 5", 31, 41, NO_ERROR },
  { { 10, 20, 40, 50 },
    "8 + 2 * 5", 28, 38, NO_ERROR },
  { { 10, 20, 40, 50 },
    "8 + 8 / 2", 22, 32, NO_ERROR },
  { { 10, 20, 40, 50 },
    "14 / (2 + 5)", 12, 22, NO_ERROR },
  { { 10, 20, 40, 50 },
    "8 * (2 + 5)", 66, 76, NO_ERROR },
  { { 10, 20, 40, 50 },
    "(8 + 2) * 5", 60, 70, NO_ERROR },
  { { 10, 20, 40, 50 },
    "(8 + 8) / 2", 18, 28, NO_ERROR },
  /* Errors */
  { { 10, 20, 40, 50 },
    "2 * foo", 0, 0, META_POSITION_EXPR_ERROR_UNKNOWN_VARIABLE },
  { { 10, 20, 40, 50 },
    "2 *", 0, 0, META_POSITION_EXPR_ERROR_FAILED },
  { { 10, 20, 40, 50 },
    "- width", 0, 0, META_POSITION_EXPR_ERROR_FAILED },
  { { 10, 20, 40, 50 },
    "5 % 1.0", 0, 0, META_POSITION_EXPR_ERROR_MOD_ON_FLOAT },
  { { 10, 20, 40, 50 },
    "1.0 % 5", 0, 0, META_POSITION_EXPR_ERROR_MOD_ON_FLOAT },
  { { 10, 20, 40, 50 },
    "! * 2", 0, 0, META_POSITION_EXPR_ERROR_BAD_CHARACTER },
  { { 10, 20, 40, 50 },
    "   ", 0, 0, META_POSITION_EXPR_ERROR_FAILED },
  { { 10, 20, 40, 50 },
    "() () (( ) ()) ((()))", 0, 0, META_POSITION_EXPR_ERROR_FAILED },
  { { 10, 20, 40, 50 },
    "(*) () ((/) ()) ((()))", 0, 0, META_POSITION_EXPR_ERROR_FAILED },
  { { 10, 20, 40, 50 },
    "2 * 5 /", 0, 0, META_POSITION_EXPR_ERROR_FAILED },
  { { 10, 20, 40, 50 },
    "+ 2 * 5", 0, 0, META_POSITION_EXPR_ERROR_FAILED },
  { { 10, 20, 40, 50 },
    "+ 2 * 5", 0, 0, META_POSITION_EXPR_ERROR_FAILED }
};

static void
run_position_expression_tests (void)
{
  int i;
  MetaPositionExprEnv env;
  
  i = 0;
  while (i < G_N_ELEMENTS (position_expression_tests))
    {
      GError *err;
      gboolean retval;
      const PositionExpressionTest *test;
      int x, y;
      
      test = &position_expression_tests[i];

      if (g_getenv ("META_PRINT_TESTS") != NULL)
        g_print ("Test expression: \"%s\" expecting x = %d y = %d",
                 test->expr, test->expected_x, test->expected_y);
      
      err = NULL;      

      env.x = test->rect.x;
      env.y = test->rect.y;
      env.width = test->rect.width;
      env.height = test->rect.height;
      env.object_width = -1;
      env.object_height = -1;
      
      retval = meta_parse_position_expression (test->expr,
                                               &env,
                                               &x, &y,
                                               &err);

      if (retval && err)
        g_error ("position expression test returned TRUE but set error");
      if (!retval && err == NULL)
        g_error ("position expression test returned FALSE but didn't set error");
      if (test->expected_error != NO_ERROR)
        {
          if (err == NULL)
            g_error ("Error was expected but none given");
          if (err->code != test->expected_error)
            g_error ("Error %d was expected but %d given",
                     test->expected_error, err->code);
        }
      else
        {
          if (err)
            g_error ("Error not expected but one was returned: %s",
                     err->message);

          if (x != test->expected_x)
            g_error ("x value was %d, %d was expected", x, test->expected_x);

          if (y != test->expected_y)
            g_error ("y value was %d, %d was expected", y, test->expected_y);
        }

      if (err)
        g_error_free (err);

      ++i;
    }
}

static void
run_position_expression_timings (void)
{
  int i;
  int iters;
  clock_t start;
  clock_t end;
  MetaPositionExprEnv env;
  
#define ITERATIONS 100000

  start = clock ();
  
  iters = 0;
  i = 0;
  while (iters < ITERATIONS)
    {
      const PositionExpressionTest *test;
      int x, y;
      
      test = &position_expression_tests[i];

      env.x = test->rect.x;
      env.y = test->rect.y;
      env.width = test->rect.width;
      env.height = test->rect.height;
      env.object_width = -1;
      env.object_height = -1;
      
      meta_parse_position_expression (test->expr,
                                      &env,
                                      &x, &y, NULL);

      ++iters;
      ++i;
      if (i == G_N_ELEMENTS (position_expression_tests))
        i = 0;
    }

  end = clock ();

  g_print ("%d coordinate expressions parsed in %g seconds (%g seconds average)\n",
           ITERATIONS,
           ((double)end - (double)start) / CLOCKS_PER_SEC,
           ((double)end - (double)start) / CLOCKS_PER_SEC / (double) ITERATIONS);
           
}
