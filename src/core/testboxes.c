/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter box operation testing program */

/*
 * Copyright (C) 2005 Elijah Newren
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "boxes-private.h"
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xutil.h> /* Just for the definition of the various gravities */
#include <time.h>      /* To initialize random seed */

#define NUM_RANDOM_RUNS 10000

static void
init_random_ness ()
{
  srand(time(NULL));
}

static void
get_random_rect (MetaRectangle *rect)
{
  rect->x = rand () % 1600;
  rect->y = rand () % 1200;
  rect->width  = rand () % 1600 + 1;
  rect->height = rand () % 1200 + 1;
}

static MetaRectangle*
new_meta_rect (int x, int y, int width, int height)
{
  MetaRectangle* temporary;
  temporary = g_new (MetaRectangle, 1);
  temporary->x = x;
  temporary->y = y;
  temporary->width  = width;
  temporary->height = height;

  return temporary;
}

static MetaStrut*
new_meta_strut (int x, int y, int width, int height, int side)
{
  MetaStrut* temporary;
  temporary = g_new (MetaStrut, 1);
  temporary->rect = meta_rect(x, y, width, height);
  temporary->side = side;

  return temporary;
}

static MetaEdge*
new_screen_edge (int x, int y, int width, int height, int side_type)
{
  MetaEdge* temporary;
  temporary = g_new (MetaEdge, 1);
  temporary->rect.x = x;
  temporary->rect.y = y;
  temporary->rect.width  = width;
  temporary->rect.height = height;
  temporary->side_type = side_type;
  temporary->edge_type = META_EDGE_SCREEN;

  return temporary;
}

static MetaEdge*
new_monitor_edge (int x, int y, int width, int height, int side_type)
{
  MetaEdge* temporary;
  temporary = g_new (MetaEdge, 1);
  temporary->rect.x = x;
  temporary->rect.y = y;
  temporary->rect.width  = width;
  temporary->rect.height = height;
  temporary->side_type = side_type;
  temporary->edge_type = META_EDGE_MONITOR;

  return temporary;
}

static void
test_area ()
{
  MetaRectangle temp;
  int i;
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&temp);
      g_assert (meta_rectangle_area (&temp) == temp.width * temp.height);
    }

  temp = meta_rect (0, 0, 5, 7);
  g_assert (meta_rectangle_area (&temp) == 35);

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_intersect ()
{
  MetaRectangle a = {100, 200,  50,  40};
  MetaRectangle b = {  0,  50, 110, 152};
  MetaRectangle c = {  0,   0,  10,  10};
  MetaRectangle d = {100, 100,  50,  50};
  MetaRectangle b_intersect_d = {100, 100, 10, 50};
  MetaRectangle temp;
  MetaRectangle temp2;

  meta_rectangle_intersect (&a, &b, &temp);
  temp2 = meta_rect (100, 200, 10, 2);
  g_assert (meta_rectangle_equal (&temp, &temp2));
  g_assert (meta_rectangle_area (&temp) == 20);

  meta_rectangle_intersect (&a, &c, &temp);
  g_assert (meta_rectangle_area (&temp) == 0);

  meta_rectangle_intersect (&a, &d, &temp);
  g_assert (meta_rectangle_area (&temp) == 0);

  meta_rectangle_intersect (&b, &d, &b);
  g_assert (meta_rectangle_equal (&b, &b_intersect_d));

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_equal ()
{
  MetaRectangle a = {10, 12, 4, 18};
  MetaRectangle b = a;
  MetaRectangle c = {10, 12, 4, 19};
  MetaRectangle d = {10, 12, 7, 18};
  MetaRectangle e = {10, 62, 4, 18};
  MetaRectangle f = {27, 12, 4, 18};

  g_assert ( meta_rectangle_equal (&a, &b));
  g_assert (!meta_rectangle_equal (&a, &c));
  g_assert (!meta_rectangle_equal (&a, &d));
  g_assert (!meta_rectangle_equal (&a, &e));
  g_assert (!meta_rectangle_equal (&a, &f));

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_overlap_funcs ()
{
  MetaRectangle temp1, temp2;
  int i;
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&temp1);
      get_random_rect (&temp2);
      g_assert (meta_rectangle_overlap (&temp1, &temp2) ==
                (meta_rectangle_horiz_overlap (&temp1, &temp2) &&
                 meta_rectangle_vert_overlap (&temp1, &temp2)));
    }

  temp1 = meta_rect ( 0, 0, 10, 10);
  temp2 = meta_rect (20, 0, 10,  5);
  g_assert (!meta_rectangle_overlap (&temp1, &temp2));
  g_assert (!meta_rectangle_horiz_overlap (&temp1, &temp2));
  g_assert ( meta_rectangle_vert_overlap (&temp1, &temp2));

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_basic_fitting ()
{
  MetaRectangle temp1, temp2, temp3;
  int i;
  /* Four cases:
   *   case   temp1 fits temp2    temp1 could fit temp2
   *     1           Y                      Y
   *     2           N                      Y
   *     3           Y                      N
   *     4           N                      N
   * Of the four cases, case 3 is impossible.  An alternate way of looking
   * at this table is that either the middle column must be no, or the last
   * column must be yes.  So we test that.  Also, we can repeat the test
   * reversing temp1 and temp2.
   */
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&temp1);
      get_random_rect (&temp2);
      g_assert (meta_rectangle_contains_rect (&temp1, &temp2) == FALSE ||
                meta_rectangle_could_fit_rect (&temp1, &temp2) == TRUE);
      g_assert (meta_rectangle_contains_rect (&temp2, &temp1) == FALSE ||
                meta_rectangle_could_fit_rect (&temp2, &temp1) == TRUE);
    }

  temp1 = meta_rect ( 0, 0, 10, 10);
  temp2 = meta_rect ( 5, 5,  5,  5);
  temp3 = meta_rect ( 8, 2,  3,  7);
  g_assert ( meta_rectangle_contains_rect (&temp1, &temp2));
  g_assert (!meta_rectangle_contains_rect (&temp2, &temp1));
  g_assert (!meta_rectangle_contains_rect (&temp1, &temp3));
  g_assert ( meta_rectangle_could_fit_rect (&temp1, &temp3));
  g_assert (!meta_rectangle_could_fit_rect (&temp3, &temp2));

  printf ("%s passed.\n", G_STRFUNC);
}

static void
free_strut_list (GSList *struts)
{
  GSList *tmp = struts;
  while (tmp)
    {
      g_free (tmp->data);
      tmp = tmp->next;
    }
  g_slist_free (struts);
}

static GSList*
get_strut_list (int which)
{
  GSList *ans;
  MetaDirection wc = 0; /* wc == who cares? ;-) */

  ans = NULL;

  g_assert (which >=0 && which <= 6);
  switch (which)
    {
    case 0:
      break;
    case 1:
      ans = g_slist_prepend (ans, new_meta_strut (   0,    0, 1600,   20, wc));
      ans = g_slist_prepend (ans, new_meta_strut ( 400, 1160, 1600,   40, wc));
      break;
    case 2:
      ans = g_slist_prepend (ans, new_meta_strut (   0,    0, 1600,   20, wc));
      ans = g_slist_prepend (ans, new_meta_strut ( 800, 1100,  400,  100, wc));
      ans = g_slist_prepend (ans, new_meta_strut ( 300, 1150,  150,   50, wc));
      break;
    case 3:
      ans = g_slist_prepend (ans, new_meta_strut (   0,    0, 1600,   20, wc));
      ans = g_slist_prepend (ans, new_meta_strut ( 800, 1100,  400,  100, wc));
      ans = g_slist_prepend (ans, new_meta_strut ( 300, 1150,   80,   50, wc));
      ans = g_slist_prepend (ans, new_meta_strut ( 700,  525,  200,  150, wc));
      break;
    case 4:
      ans = g_slist_prepend (ans, new_meta_strut (   0,    0,  800, 1200, wc));
      ans = g_slist_prepend (ans, new_meta_strut ( 800,    0, 1600,   20, wc));
      break;
    case 5:
      ans = g_slist_prepend (ans, new_meta_strut ( 800,    0, 1600,   20, wc));
      ans = g_slist_prepend (ans, new_meta_strut (   0,    0,  800, 1200, wc));
      ans = g_slist_prepend (ans, new_meta_strut ( 800,   10,  800, 1200, wc));
      break;
    case 6:
      ans = g_slist_prepend (ans, new_meta_strut (   0,    0, 1600,   40, wc));
      ans = g_slist_prepend (ans, new_meta_strut (   0,    0, 1600,   20, wc));
      break;
    }

  return ans;
}

static GList*
get_screen_region (int which)
{
  GList *ret;
  GSList *struts;
  MetaRectangle basic_rect;

  basic_rect = meta_rect (0, 0, 1600, 1200);
  ret = NULL;

  struts = get_strut_list (which);
  ret = meta_rectangle_get_minimal_spanning_set_for_region (&basic_rect, struts);
  free_strut_list (struts);

  return ret;
}

static GList*
get_screen_edges (int which)
{
  GList *ret;
  GSList *struts;
  MetaRectangle basic_rect;

  basic_rect = meta_rect (0, 0, 1600, 1200);
  ret = NULL;

  struts = get_strut_list (which);
  ret = meta_rectangle_find_onscreen_edges (&basic_rect, struts);
  free_strut_list (struts);

  return ret;
}

static GList*
get_monitor_edges (int which_monitor_set, int which_strut_set)
{
  GList *ret;
  GSList *struts;
  GList *xins;

  xins = NULL;
  g_assert (which_monitor_set >=0 && which_monitor_set <= 3);
  switch (which_monitor_set)
    {
    case 0:
      xins = g_list_prepend (xins, new_meta_rect (  0,   0, 1600, 1200));
      break;
    case 1:
      xins = g_list_prepend (xins, new_meta_rect (  0,   0,  800, 1200));
      xins = g_list_prepend (xins, new_meta_rect (800,   0,  800, 1200));
      break;
    case 2:
      xins = g_list_prepend (xins, new_meta_rect (  0,   0, 1600,  600));
      xins = g_list_prepend (xins, new_meta_rect (  0, 600, 1600,  600));
      break;
    case 3:
      xins = g_list_prepend (xins, new_meta_rect (  0,   0, 1600,  600));
      xins = g_list_prepend (xins, new_meta_rect (  0, 600,  800,  600));
      xins = g_list_prepend (xins, new_meta_rect (800, 600,  800,  600));
      break;
    }

  ret = NULL;

  struts = get_strut_list (which_strut_set);
  ret = meta_rectangle_find_nonintersected_monitor_edges (xins, struts);

  free_strut_list (struts);
  meta_rectangle_free_list_and_elements (xins);

  return ret;
}

#if 0
static void
test_merge_regions ()
{
  /* logarithmically distributed random number of struts (range?)
   * logarithmically distributed random size of struts (up to screen size???)
   * uniformly distributed location of center of struts (within screen)
   * merge all regions that are possible
   * print stats on problem setup
   *   number of (non-completely-occluded?) struts
   *   percentage of screen covered
   *   length of resulting non-minimal spanning set
   *   length of resulting minimal spanning set
   * print stats on merged regions:
   *   number boxes merged
   *   number of those merges that were of the form A contains B
   *   number of those merges that were of the form A partially contains B
   *   number of those merges that were of the form A is adjacent to B
   */

  GList* region;
  GList* compare;
  int num_contains, num_merged, num_part_contains, num_adjacent;

  num_contains = num_merged = num_part_contains = num_adjacent = 0;
  compare = region = get_screen_region (2);
  g_assert (region);

  printf ("Merging stats:\n");
  printf ("  Length of initial list: %d\n", g_list_length (region));
#ifdef PRINT_DEBUG
  char rect1[RECT_LENGTH], rect2[RECT_LENGTH];
  char region_list[(RECT_LENGTH + 2) * g_list_length (region)];
  meta_rectangle_region_to_string (region, ", ", region_list);
  printf ("  Initial rectangles: %s\n", region_list);
#endif

  while (compare && compare->next)
    {
      MetaRectangle *a = compare->data;
      GList *other = compare->next;

      g_assert (a->width > 0 && a->height > 0);

      while (other)
        {
          MetaRectangle *b = other->data;
          GList *delete_me = NULL;

          g_assert (b->width > 0 && b->height > 0);

#ifdef PRINT_DEBUG
          printf ("    -- Comparing %s to %s --\n",
                  meta_rectangle_to_string (a, rect1),
                  meta_rectangle_to_string (b, rect2));
#endif

          /* If a contains b, just remove b */
          if (meta_rectangle_contains_rect (a, b))
            {
              delete_me = other;
              num_contains++;
              num_merged++;
            }
          /* If b contains a, just remove a */
          else if (meta_rectangle_contains_rect (a, b))
            {
              delete_me = compare;
              num_contains++;
              num_merged++;
            }
          /* If a and b might be mergeable horizontally */
          else if (a->y == b->y && a->height == b->height)
            {
              /* If a and b overlap */
              if (meta_rectangle_overlap (a, b))
                {
                  int new_x = MIN (a->x, b->x);
                  a->width = MAX (a->x + a->width, b->x + b->width) - new_x;
                  a->x = new_x;
                  delete_me = other;
                  num_part_contains++;
                  num_merged++;
                }
              /* If a and b are adjacent */
              else if (a->x + a->width == b->x || a->x == b->x + b->width)
                {
                  int new_x = MIN (a->x, b->x);
                  a->width = MAX (a->x + a->width, b->x + b->width) - new_x;
                  a->x = new_x;
                  delete_me = other;
                  num_adjacent++;
                  num_merged++;
                }
            }
          /* If a and b might be mergeable vertically */
          else if (a->x == b->x && a->width == b->width)
            {
              /* If a and b overlap */
              if (meta_rectangle_overlap (a, b))
                {
                  int new_y = MIN (a->y, b->y);
                  a->height = MAX (a->y + a->height, b->y + b->height) - new_y;
                  a->y = new_y;
                  delete_me = other;
                  num_part_contains++;
                  num_merged++;
                }
              /* If a and b are adjacent */
              else if (a->y + a->height == b->y || a->y == b->y + b->height)
                {
                  int new_y = MIN (a->y, b->y);
                  a->height = MAX (a->y + a->height, b->y + b->height) - new_y;
                  a->y = new_y;
                  delete_me = other;
                  num_adjacent++;
                  num_merged++;
                }
            }

          other = other->next;

          /* Delete any rectangle in the list that is no longer wanted */
          if (delete_me != NULL)
            {
#ifdef PRINT_DEBUG
              MetaRectangle *bla = delete_me->data;
              printf ("    Deleting rect %s\n",
                      meta_rectangle_to_string (bla, rect1));
#endif

              /* Deleting the rect we're compare others to is a little tricker */
              if (compare == delete_me)
                {
                  compare = compare->next;
                  other = compare->next;
                  a = compare->data;
                }

              /* Okay, we can free it now */
              g_free (delete_me->data);
              region = g_list_delete_link (region, delete_me);
            }

#ifdef PRINT_DEBUG
          char region_list[(RECT_LENGTH + 2) * g_list_length (region)];
          meta_rectangle_region_to_string (region, ", ", region_list);
          printf ("      After comparison, new list is: %s\n", region_list);
#endif
        }

      compare = compare->next;
    }

  printf ("  Num rectangles contained in others          : %d\n",
          num_contains);
  printf ("  Num rectangles partially contained in others: %d\n",
          num_part_contains);
  printf ("  Num rectangles adjacent to others           : %d\n",
          num_adjacent);
  printf ("  Num rectangles merged with others           : %d\n",
          num_merged);
#ifdef PRINT_DEBUG
  char region_list2[(RECT_LENGTH + 2) * g_list_length (region)];
  meta_rectangle_region_to_string (region, ", ", region_list2);
  printf ("  Final rectangles: %s\n", region_list2);
#endif

  meta_rectangle_free_spanning_set (region);
  region = NULL;

  printf ("%s passed.\n", G_STRFUNC);
}
#endif

static void
verify_lists_are_equal (GList *code, GList *answer)
{
  int which = 0;

  while (code && answer)
    {
      MetaRectangle *a = code->data;
      MetaRectangle *b = answer->data;

      if (a->x      != b->x     ||
          a->y      != b->y     ||
          a->width  != b->width ||
          a->height != b->height)
        {
          g_error ("%dth item in code answer answer lists do not match; "
                   "code rect: %d,%d + %d,%d; answer rect: %d,%d + %d,%d\n",
                   which,
                   a->x, a->y, a->width, a->height,
                   b->x, b->y, b->width, b->height);
        }

      code = code->next;
      answer = answer->next;

      which++;
    }

  /* Ought to be at the end of both lists; check if we aren't */
  if (code)
    {
      MetaRectangle *tmp = code->data;
      g_error ("code list longer than answer list by %d items; "
               "first extra item: %d,%d +%d,%d\n",
               g_list_length (code),
               tmp->x, tmp->y, tmp->width, tmp->height);
    }

  if (answer)
    {
      MetaRectangle *tmp = answer->data;
      g_error ("answer list longer than code list by %d items; "
               "first extra item: %d,%d +%d,%d\n",
               g_list_length (answer),
               tmp->x, tmp->y, tmp->width, tmp->height);
    }
}

static void
test_regions_okay ()
{
  GList* region;
  GList* tmp;

  /*************************************************************/
  /* Make sure test region 0 has the right spanning rectangles */
  /*************************************************************/
  region = get_screen_region (0);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_meta_rect (0, 0, 1600, 1200));
  verify_lists_are_equal (region, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (region);

  /*************************************************************/
  /* Make sure test region 1 has the right spanning rectangles */
  /*************************************************************/
  region = get_screen_region (1);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_meta_rect (0, 20,  400, 1180));
  tmp = g_list_prepend (tmp, new_meta_rect (0, 20, 1600, 1140));
  verify_lists_are_equal (region, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (region);

  /*************************************************************/
  /* Make sure test region 2 has the right spanning rectangles */
  /*************************************************************/
  region = get_screen_region (2);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20,  300, 1180));
  tmp = g_list_prepend (tmp, new_meta_rect ( 450,   20,  350, 1180));
  tmp = g_list_prepend (tmp, new_meta_rect (1200,   20,  400, 1180));
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20,  800, 1130));
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20, 1600, 1080));
  verify_lists_are_equal (region, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (region);

  /*************************************************************/
  /* Make sure test region 3 has the right spanning rectangles */
  /*************************************************************/
  region = get_screen_region (3);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_meta_rect ( 380,  675,  420,  525)); /* 220500 */
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20,  300, 1180)); /* 354000 */
  tmp = g_list_prepend (tmp, new_meta_rect ( 380,   20,  320, 1180)); /* 377600 */
  tmp = g_list_prepend (tmp, new_meta_rect (   0,  675,  800,  475)); /* 380000 */
  tmp = g_list_prepend (tmp, new_meta_rect (1200,   20,  400, 1180)); /* 472000 */
  tmp = g_list_prepend (tmp, new_meta_rect (   0,  675, 1600,  425)); /* 680000 */
  tmp = g_list_prepend (tmp, new_meta_rect ( 900,   20,  700, 1080)); /* 756000 */
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20,  700, 1130)); /* 791000 */
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20, 1600,  505)); /* 808000 */
#if 0
  printf ("Got to here...\n");
  char region_list[(RECT_LENGTH+2) * g_list_length (region)];
  char tmp_list[   (RECT_LENGTH+2) * g_list_length (tmp)];
  meta_rectangle_region_to_string (region, ", ", region_list);
  meta_rectangle_region_to_string (region, ", ", tmp_list);
  printf ("%s vs. %s\n", region_list, tmp_list);
#endif
  verify_lists_are_equal (region, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (region);

  /*************************************************************/
  /* Make sure test region 4 has the right spanning rectangles */
  /*************************************************************/
  region = get_screen_region (4);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_meta_rect ( 800,   20,  800, 1180));
  verify_lists_are_equal (region, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (region);

  /*************************************************************/
  /* Make sure test region 5 has the right spanning rectangles */
  /*************************************************************/
  printf ("The next test intentionally causes a warning, "
          "but it can be ignored.\n");
  region = get_screen_region (5);
  verify_lists_are_equal (region, NULL);

  /* FIXME: Still to do:
   *   - Create random struts and check the regions somehow
   */

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_region_fitting ()
{
  GList* region;
  MetaRectangle rect;

  /* See test_basic_fitting() for how/why these automated random tests work */
  int i;
  region = get_screen_region (3);
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&rect);
      g_assert (meta_rectangle_contained_in_region (region, &rect) == FALSE ||
                meta_rectangle_could_fit_in_region (region, &rect) == TRUE);
    }
  meta_rectangle_free_list_and_elements (region);

  /* Do some manual tests too */
  region = get_screen_region (1);

  rect = meta_rect (50, 50, 400, 400);
  g_assert (meta_rectangle_could_fit_in_region (region, &rect));
  g_assert (meta_rectangle_contained_in_region (region, &rect));

  rect = meta_rect (250, 0, 500, 1150);
  g_assert (!meta_rectangle_could_fit_in_region (region, &rect));
  g_assert (!meta_rectangle_contained_in_region (region, &rect));

  rect = meta_rect (250, 0, 400, 400);
  g_assert (meta_rectangle_could_fit_in_region (region, &rect));
  g_assert (!meta_rectangle_contained_in_region (region, &rect));

  meta_rectangle_free_list_and_elements (region);

  region = get_screen_region (2);
  rect = meta_rect (1000, 50, 600, 1100);
  g_assert (meta_rectangle_could_fit_in_region (region, &rect));
  g_assert (!meta_rectangle_contained_in_region (region, &rect));

  meta_rectangle_free_list_and_elements (region);

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_clamping_to_region ()
{
  GList* region;
  MetaRectangle rect;
  MetaRectangle min_size;
  FixedDirections fixed_directions;
  int i;

  min_size.height = min_size.width = 1;
  fixed_directions = 0;

  region = get_screen_region (3);
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      MetaRectangle temp;
      get_random_rect (&rect);
      temp = rect;
      meta_rectangle_clamp_to_fit_into_region (region,
                                               fixed_directions,
                                               &rect,
                                               &min_size);
      g_assert (meta_rectangle_could_fit_in_region (region, &rect) == TRUE);
      g_assert (rect.x == temp.x && rect.y == temp.y);
    }
  meta_rectangle_free_list_and_elements (region);

  /* Do some manual tests too */
  region = get_screen_region (1);

  rect = meta_rect (50, 50, 10000, 10000);
  meta_rectangle_clamp_to_fit_into_region (region,
                                           fixed_directions,
                                           &rect,
                                           &min_size);
  g_assert (rect.width == 1600 && rect.height == 1140);

  rect = meta_rect (275, -50, 410, 10000);
  meta_rectangle_clamp_to_fit_into_region (region,
                                           fixed_directions,
                                           &rect,
                                           &min_size);
  g_assert (rect.width == 400 && rect.height == 1180);

  rect = meta_rect (50, 50, 10000, 10000);
  min_size.height = 1170;
  meta_rectangle_clamp_to_fit_into_region (region,
                                           fixed_directions,
                                           &rect,
                                           &min_size);
  g_assert (rect.width == 400 && rect.height == 1180);

  printf ("The next test intentionally causes a warning, "
          "but it can be ignored.\n");
  rect = meta_rect (50, 50, 10000, 10000);
  min_size.width = 600;  min_size.height = 1170;
  meta_rectangle_clamp_to_fit_into_region (region,
                                           fixed_directions,
                                           &rect,
                                           &min_size);
  g_assert (rect.width == 600 && rect.height == 1170);

  rect = meta_rect (350, 50, 100, 1100);
  min_size.width = 1;  min_size.height = 1;
  fixed_directions = FIXED_DIRECTION_X;
  meta_rectangle_clamp_to_fit_into_region (region,
                                           fixed_directions,
                                           &rect,
                                           &min_size);
  g_assert (rect.width == 100 && rect.height == 1100);

  rect = meta_rect (300, 70, 500, 1100);
  min_size.width = 1;  min_size.height = 1;
  fixed_directions = FIXED_DIRECTION_Y;
  meta_rectangle_clamp_to_fit_into_region (region,
                                           fixed_directions,
                                           &rect,
                                           &min_size);
  g_assert (rect.width == 400 && rect.height == 1100);

  printf ("The next test intentionally causes a warning, "
          "but it can be ignored.\n");
  rect = meta_rect (300, 70, 999999, 999999);
  min_size.width = 100;  min_size.height = 200;
  fixed_directions = FIXED_DIRECTION_Y;
  meta_rectangle_clamp_to_fit_into_region (region,
                                           fixed_directions,
                                           &rect,
                                           &min_size);
  g_assert (rect.width == 100 && rect.height == 999999);

  meta_rectangle_free_list_and_elements (region);

  printf ("%s passed.\n", G_STRFUNC);
}

static gboolean
rect_overlaps_region (const GList         *spanning_rects,
                      const MetaRectangle *rect)
{
  /* FIXME: Should I move this to boxes.[ch]? */
  const GList *temp;
  gboolean     overlaps;

  temp = spanning_rects;
  overlaps = FALSE;
  while (!overlaps && temp != NULL)
    {
      overlaps = overlaps || meta_rectangle_overlap (temp->data, rect);
      temp = temp->next;
    }

  return overlaps;
}

gboolean time_to_print = FALSE;

static void
test_clipping_to_region ()
{
  GList* region;
  MetaRectangle rect, temp;
  FixedDirections fixed_directions = 0;
  int i;

  region = get_screen_region (3);
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&rect);
      if (rect_overlaps_region (region, &rect))
        {
          meta_rectangle_clip_to_region (region, 0, &rect);
          g_assert (meta_rectangle_contained_in_region (region, &rect) == TRUE);
        }
    }
  meta_rectangle_free_list_and_elements (region);

  /* Do some manual tests too */
  region = get_screen_region (2);

  rect = meta_rect (-50, -10, 10000, 10000);
  meta_rectangle_clip_to_region (region,
                                 fixed_directions,
                                 &rect);
  g_assert (meta_rectangle_equal (region->data, &rect));

  rect = meta_rect (300, 1000, 400, 200);
  temp = meta_rect (300, 1000, 400, 150);
  meta_rectangle_clip_to_region (region,
                                 fixed_directions,
                                 &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect (400, 1000, 300, 200);
  temp = meta_rect (450, 1000, 250, 200);
  meta_rectangle_clip_to_region (region,
                                 fixed_directions,
                                 &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect (400, 1000, 300, 200);
  temp = meta_rect (400, 1000, 300, 150);
  meta_rectangle_clip_to_region (region,
                                 FIXED_DIRECTION_X,
                                 &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect (400, 1000, 300, 200);
  temp = meta_rect (400, 1000, 300, 150);
  meta_rectangle_clip_to_region (region,
                                 FIXED_DIRECTION_X,
                                 &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  meta_rectangle_free_list_and_elements (region);

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_shoving_into_region ()
{
  GList* region;
  MetaRectangle rect, temp;
  FixedDirections fixed_directions = 0;
  int i;

  region = get_screen_region (3);
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&rect);
      if (meta_rectangle_could_fit_in_region (region, &rect))
        {
          meta_rectangle_shove_into_region (region, 0, &rect);
          g_assert (meta_rectangle_contained_in_region (region, &rect));
        }
    }
  meta_rectangle_free_list_and_elements (region);

  /* Do some manual tests too */
  region = get_screen_region (2);

  rect = meta_rect (300, 1000, 400, 200);
  temp = meta_rect (300,  950, 400, 200);
  meta_rectangle_shove_into_region (region,
                                    fixed_directions,
                                    &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect (425, 1000, 300, 200);
  temp = meta_rect (450, 1000, 300, 200);
  meta_rectangle_shove_into_region (region,
                                    fixed_directions,
                                    &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect (425, 1000, 300, 200);
  temp = meta_rect (425,  950, 300, 200);
  meta_rectangle_shove_into_region (region,
                                    FIXED_DIRECTION_X,
                                    &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect ( 300, 1000, 400, 200);
  temp = meta_rect (1200, 1000, 400, 200);
  meta_rectangle_shove_into_region (region,
                                    FIXED_DIRECTION_Y,
                                    &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect ( 800, 1150, 400,  50);  /* Completely "offscreen" :) */
  temp = meta_rect ( 800, 1050, 400,  50);
  meta_rectangle_shove_into_region (region,
                                    0,
                                    &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect (-1000,  0, 400, 150);  /* Offscreen in 2 directions */
  temp = meta_rect (    0, 20, 400, 150);
  meta_rectangle_shove_into_region (region,
                                    0,
                                    &rect);
  g_assert (meta_rectangle_equal (&rect, &temp));

  meta_rectangle_free_list_and_elements (region);

  printf ("%s passed.\n", G_STRFUNC);
}

static void
verify_edge_lists_are_equal (GList *code, GList *answer)
{
  int which = 0;

  while (code && answer)
    {
      MetaEdge *a = code->data;
      MetaEdge *b = answer->data;

      if (!meta_rectangle_equal (&a->rect, &b->rect) ||
          a->side_type != b->side_type ||
          a->edge_type != b->edge_type)
        {
          g_error ("%dth item in code answer answer lists do not match; "
                   "code rect: %d,%d + %d,%d; answer rect: %d,%d + %d,%d\n",
                   which,
                   a->rect.x, a->rect.y, a->rect.width, a->rect.height,
                   b->rect.x, b->rect.y, b->rect.width, b->rect.height);
        }

      code = code->next;
      answer = answer->next;

      which++;
    }

  /* Ought to be at the end of both lists; check if we aren't */
  if (code)
    {
      MetaEdge *tmp = code->data;
      g_error ("code list longer than answer list by %d items; "
               "first extra item rect: %d,%d +%d,%d\n",
               g_list_length (code),
               tmp->rect.x, tmp->rect.y, tmp->rect.width, tmp->rect.height);
    }

  if (answer)
    {
      MetaEdge *tmp = answer->data;
      g_error ("answer list longer than code list by %d items; "
               "first extra item rect: %d,%d +%d,%d\n",
               g_list_length (answer),
               tmp->rect.x, tmp->rect.y, tmp->rect.width, tmp->rect.height);
    }
}

static void
test_find_onscreen_edges ()
{
  GList* edges;
  GList* tmp;

  int left   = META_DIRECTION_LEFT;
  int right  = META_DIRECTION_RIGHT;
  int top    = META_DIRECTION_TOP;
  int bottom = META_DIRECTION_BOTTOM;

  /*************************************************/
  /* Make sure test region 0 has the correct edges */
  /*************************************************/
  edges = get_screen_edges (0);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_screen_edge (   0, 1200, 1600, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,    0, 1600, 0, top));
  tmp = g_list_prepend (tmp, new_screen_edge (1600,    0, 0, 1200, right));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,    0, 0, 1200, left));
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************/
  /* Make sure test region 1 has the correct edges */
  /*************************************************/
  edges = get_screen_edges (1);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_screen_edge (   0, 1200,  400, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 400, 1160, 1200, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,   20, 1600, 0, top));
  tmp = g_list_prepend (tmp, new_screen_edge (1600,   20, 0, 1140, right));
  tmp = g_list_prepend (tmp, new_screen_edge ( 400, 1160, 0,   40, right));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,   20, 0, 1180, left));
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************/
  /* Make sure test region 2 has the correct edges */
  /*************************************************/
  edges = get_screen_edges (2);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_screen_edge (1200, 1200,  400, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 450, 1200,  350, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge (   0, 1200,  300, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 300, 1150,  150, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 800, 1100,  400, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,   20, 1600, 0, top));
  tmp = g_list_prepend (tmp, new_screen_edge (1600,   20, 0, 1180, right));
  tmp = g_list_prepend (tmp, new_screen_edge ( 800, 1100, 0,  100, right));
  tmp = g_list_prepend (tmp, new_screen_edge ( 300, 1150, 0,   50, right));
  tmp = g_list_prepend (tmp, new_screen_edge (1200, 1100, 0,  100, left));
  tmp = g_list_prepend (tmp, new_screen_edge ( 450, 1150, 0,   50, left));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,   20, 0, 1180, left));
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************/
  /* Make sure test region 3 has the correct edges */
  /*************************************************/
  edges = get_screen_edges (3);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_screen_edge (1200, 1200,  400, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 380, 1200,  420, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge (   0, 1200,  300, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 300, 1150,   80, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 800, 1100,  400, 0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 700,  525, 200,  0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 700,  675, 200,  0, top));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,   20, 1600, 0, top));
  tmp = g_list_prepend (tmp, new_screen_edge (1600,   20, 0, 1180, right));
  tmp = g_list_prepend (tmp, new_screen_edge ( 800, 1100, 0,  100, right));
  tmp = g_list_prepend (tmp, new_screen_edge ( 700,  525, 0,  150, right));
  tmp = g_list_prepend (tmp, new_screen_edge ( 300, 1150, 0,   50, right));
  tmp = g_list_prepend (tmp, new_screen_edge (1200, 1100, 0,  100, left));
  tmp = g_list_prepend (tmp, new_screen_edge ( 900,  525, 0,  150, left));
  tmp = g_list_prepend (tmp, new_screen_edge ( 380, 1150, 0,   50, left));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,   20, 0, 1180, left));

#if 0
  #define FUDGE 50 /* number of edges */
  char big_buffer1[(EDGE_LENGTH+2)*FUDGE], big_buffer2[(EDGE_LENGTH+2)*FUDGE];
  meta_rectangle_edge_list_to_string (edges, "\n ", big_buffer1);
  meta_rectangle_edge_list_to_string (tmp,   "\n ", big_buffer2);
  printf("Generated edge list:\n %s\nComparison edges list:\n %s\n",
         big_buffer1, big_buffer2);
#endif

  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************/
  /* Make sure test region 4 has the correct edges */
  /*************************************************/
  edges = get_screen_edges (4);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_screen_edge ( 800, 1200, 800,  0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge ( 800,   20, 800,  0, top));
  tmp = g_list_prepend (tmp, new_screen_edge (1600,   20, 0, 1180, right));
  tmp = g_list_prepend (tmp, new_screen_edge ( 800,   20, 0, 1180, left));
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************/
  /* Make sure test region 5 has the correct edges */
  /*************************************************/
  edges = get_screen_edges (5);
  tmp = NULL;
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************/
  /* Make sure test region 6 has the correct edges */
  /*************************************************/
  edges = get_screen_edges (6);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_screen_edge (   0, 1200, 1600,  0, bottom));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,   40, 1600,  0, top));
  tmp = g_list_prepend (tmp, new_screen_edge (1600,   40, 0,  1160, right));
  tmp = g_list_prepend (tmp, new_screen_edge (   0,   40, 0,  1160, left));
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_find_nonintersected_monitor_edges ()
{
  GList* edges;
  GList* tmp;

  int left   = META_DIRECTION_LEFT;
  int right  = META_DIRECTION_RIGHT;
  int top    = META_DIRECTION_TOP;
  int bottom = META_DIRECTION_BOTTOM;

  /*************************************************************************/
  /* Make sure test monitor set 0 for with region 0 has the correct edges */
  /*************************************************************************/
  edges = get_monitor_edges (0, 0);
  tmp = NULL;
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************************************/
  /* Make sure test monitor set 2 for with region 1 has the correct edges */
  /*************************************************************************/
  edges = get_monitor_edges (2, 1);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_monitor_edge (   0,  600, 1600, 0, bottom));
  tmp = g_list_prepend (tmp, new_monitor_edge (   0,  600, 1600, 0, top));
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************************************/
  /* Make sure test monitor set 1 for with region 2 has the correct edges */
  /*************************************************************************/
  edges = get_monitor_edges (1, 2);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_monitor_edge ( 800,   20, 0, 1080, right));
  tmp = g_list_prepend (tmp, new_monitor_edge ( 800,   20, 0, 1180, left));
#if 0
  #define FUDGE 50
  char big_buffer1[(EDGE_LENGTH+2)*FUDGE], big_buffer2[(EDGE_LENGTH+2)*FUDGE];
  meta_rectangle_edge_list_to_string (edges, "\n ", big_buffer1);
  meta_rectangle_edge_list_to_string (tmp,   "\n ", big_buffer2);
  printf("Generated edge list:\n %s\nComparison edges list:\n %s\n",
         big_buffer1, big_buffer2);
#endif
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************************************/
  /* Make sure test monitor set 3 for with region 3 has the correct edges */
  /*************************************************************************/
  edges = get_monitor_edges (3, 3);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_monitor_edge ( 900,  600,  700, 0, bottom));
  tmp = g_list_prepend (tmp, new_monitor_edge (   0,  600,  700, 0, bottom));
  tmp = g_list_prepend (tmp, new_monitor_edge ( 900,  600,  700, 0, top));
  tmp = g_list_prepend (tmp, new_monitor_edge (   0,  600,  700, 0, top));
  tmp = g_list_prepend (tmp, new_monitor_edge ( 800,  675, 0,  425, right));
  tmp = g_list_prepend (tmp, new_monitor_edge ( 800,  675, 0,  525, left));
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************************************/
  /* Make sure test monitor set 3 for with region 4 has the correct edges */
  /*************************************************************************/
  edges = get_monitor_edges (3, 4);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_monitor_edge ( 800,  600,  800, 0, bottom));
  tmp = g_list_prepend (tmp, new_monitor_edge ( 800,  600,  800, 0, top));
  tmp = g_list_prepend (tmp, new_monitor_edge ( 800,  600,  0, 600, right));
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  /*************************************************************************/
  /* Make sure test monitor set 3 for with region 5has the correct edges */
  /*************************************************************************/
  edges = get_monitor_edges (3, 5);
  tmp = NULL;
  verify_edge_lists_are_equal (edges, tmp);
  meta_rectangle_free_list_and_elements (tmp);
  meta_rectangle_free_list_and_elements (edges);

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_gravity_resize ()
{
  MetaRectangle oldrect, rect, temp;

  rect.x = -500;  /* Some random amount not equal to oldrect.x to ensure that
                   * the resize is done with respect to oldrect instead of rect
                   */
  oldrect = meta_rect ( 50,  300, 250, 400);
  temp    = meta_rect ( 50,  300,  20,   5);
  meta_rectangle_resize_with_gravity (&oldrect,
                                      &rect,
                                      NorthWestGravity,
                                      20,
                                      5);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect ( 50,  300, 250, 400);
  temp = meta_rect (165,  300,  20,   5);
  meta_rectangle_resize_with_gravity (&rect,
                                      &rect,
                                      NorthGravity,
                                      20,
                                      5);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect ( 50,  300, 250, 400);
  temp = meta_rect (280,  300,  20,   5);
  meta_rectangle_resize_with_gravity (&rect,
                                      &rect,
                                      NorthEastGravity,
                                      20,
                                      5);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect ( 50,  300, 250, 400);
  temp = meta_rect ( 50,  695,  50,   5);
  meta_rectangle_resize_with_gravity (&rect,
                                      &rect,
                                      SouthWestGravity,
                                      50,
                                      5);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect ( 50,  300, 250, 400);
  temp = meta_rect (150,  695,  50,   5);
  meta_rectangle_resize_with_gravity (&rect,
                                      &rect,
                                      SouthGravity,
                                      50,
                                      5);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect ( 50,  300, 250, 400);
  temp = meta_rect (250,  695,  50,   5);
  meta_rectangle_resize_with_gravity (&rect,
                                      &rect,
                                      SouthEastGravity,
                                      50,
                                      5);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect (167,  738, 237, 843);
  temp = meta_rect (167, 1113, 832,  93);
  meta_rectangle_resize_with_gravity (&rect,
                                      &rect,
                                      WestGravity,
                                      832,
                                      93);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect ( 167,  738, 237, 843);
  temp = meta_rect (-131, 1113, 833,  93);
  meta_rectangle_resize_with_gravity (&rect,
                                      &rect,
                                      CenterGravity,
                                      832,
                                      93);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect (300, 1000, 400, 200);
  temp = meta_rect (270,  994, 430, 212);
  meta_rectangle_resize_with_gravity (&rect,
                                      &rect,
                                      EastGravity,
                                      430,
                                      211);
  g_assert (meta_rectangle_equal (&rect, &temp));

  rect = meta_rect (300, 1000, 400, 200);
  temp = meta_rect (300, 1000, 430, 211);
  meta_rectangle_resize_with_gravity (&rect,
                                      &rect,
                                      StaticGravity,
                                      430,
                                      211);
  g_assert (meta_rectangle_equal (&rect, &temp));

  printf ("%s passed.\n", G_STRFUNC);
}

static void
test_find_closest_point_to_line ()
{
  double x1, y1, x2, y2, px, py, rx, ry;
  double answer_x, answer_y;

  x1 =  3.0;  y1 =  49.0;
  x2 =  2.0;  y2 = - 1.0;
  px = -2.6;  py =  19.1;
  answer_x = 2.4; answer_y = 19;
  meta_rectangle_find_linepoint_closest_to_point (x1,  y1,
                                                  x2,  y2,
                                                  px,  py,
                                                  &rx, &ry);
  g_assert (rx == answer_x && ry == answer_y);

  /* Special test for x1 == x2, so that slop of line is infinite */
  x1 =  3.0;  y1 =  49.0;
  x2 =  3.0;  y2 = - 1.0;
  px = -2.6;  py =  19.1;
  answer_x = 3.0; answer_y = 19.1;
  meta_rectangle_find_linepoint_closest_to_point (x1,  y1,
                                                  x2,  y2,
                                                  px,  py,
                                                  &rx, &ry);
  g_assert (rx == answer_x && ry == answer_y);

  /* Special test for y1 == y2, so perp line has slope of infinity */
  x1 =  3.14;  y1 =   7.0;
  x2 =  2.718; y2 =   7.0;
  px = -2.6;   py =  19.1;
  answer_x = -2.6; answer_y = 7;
  meta_rectangle_find_linepoint_closest_to_point (x1,  y1,
                                                  x2,  y2,
                                                  px,  py,
                                                  &rx, &ry);
  g_assert (rx == answer_x && ry == answer_y);

  /* Test when we the point we want to be closest to is actually on the line */
  x1 =  3.0;  y1 =  49.0;
  x2 =  2.0;  y2 = - 1.0;
  px =  2.4;  py =  19.0;
  answer_x = 2.4; answer_y = 19;
  meta_rectangle_find_linepoint_closest_to_point (x1,  y1,
                                                  x2,  y2,
                                                  px,  py,
                                                  &rx, &ry);
  g_assert (rx == answer_x && ry == answer_y);

  printf ("%s passed.\n", G_STRFUNC);
}

int
main()
{
  init_random_ness ();
  test_area ();
  test_intersect ();
  test_equal ();
  test_overlap_funcs ();
  test_basic_fitting ();

  test_regions_okay ();
  test_region_fitting ();

  test_clamping_to_region ();
  test_clipping_to_region ();
  test_shoving_into_region ();

  /* And now the functions dealing with edges more than boxes */
  test_find_onscreen_edges ();
  test_find_nonintersected_monitor_edges ();

  /* And now the misfit functions that don't quite fit in anywhere else... */
  test_gravity_resize ();
  test_find_closest_point_to_line ();

  printf ("All tests passed.\n");
  return 0;
}
