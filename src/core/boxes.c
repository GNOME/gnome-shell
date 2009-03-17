/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Simple box operations */

/* 
 * Copyright (C) 2005, 2006 Elijah Newren
 * [meta_rectangle_intersect() is copyright the GTK+ Team according to Havoc,
 * see gdkrectangle.c.  As far as Havoc knows, he probably wrote
 * meta_rectangle_equal(), and I'm guessing it's (C) Red Hat.  So...]
 * Copyright (C) 1995-2000  GTK+ Team
 * Copyright (C) 2002 Red Hat, Inc.
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

#include "boxes.h"
#include "util.h"
#include <X11/Xutil.h>  /* Just for the definition of the various gravities */

char*
meta_rectangle_to_string (const MetaRectangle *rect,
                          char                *output)
{
  /* 25 chars: 2 commas, space, plus, trailing \0 + 5 for each digit.
   * Should be more than enough space.  Note that of this space, the
   * trailing \0 will be overwritten for all but the last rectangle.
   */
  g_snprintf (output, RECT_LENGTH, "%d,%d +%d,%d", 
              rect->x, rect->y, rect->width, rect->height);

  return output;
}

char*
meta_rectangle_region_to_string (GList      *region,
                                 const char *separator_string,
                                 char       *output)
{
  /* 27 chars: 2 commas, 2 square brackets, space, plus, trailing \0 + 5
   * for each digit.  Should be more than enough space.  Note that of this
   * space, the trailing \0 will be overwritten for all but the last
   * rectangle.
   */
  char rect_string[RECT_LENGTH];

  GList *tmp = region;
  char *cur = output;

  if (region == NULL)
    g_snprintf (output, 10, "(EMPTY)");

  while (tmp)
    {
      MetaRectangle *rect = tmp->data;
      g_snprintf (rect_string, RECT_LENGTH, "[%d,%d +%d,%d]", 
                  rect->x, rect->y, rect->width, rect->height);
      cur = g_stpcpy (cur, rect_string);
      tmp = tmp->next;
      if (tmp)
        cur = g_stpcpy (cur, separator_string);
    }

  return output;
}

char*
meta_rectangle_edge_to_string (const MetaEdge *edge,
                               char           *output)
{
  /* 25 chars: 2 commas, space, plus, trailing \0 + 5 for each digit.
   * Should be more than enough space.  Note that of this space, the
   * trailing \0 will be overwritten for all but the last rectangle.
   *
   * Plus 2 for parenthesis, 4 for 2 more numbers, 2 more commas, and
   * 2 more spaces, for a total of 10 more.
   */
  g_snprintf (output, EDGE_LENGTH, "[%d,%d +%d,%d], %2d, %2d", 
              edge->rect.x, edge->rect.y, edge->rect.width, edge->rect.height,
              edge->side_type, edge->edge_type);

  return output;
}

char*
meta_rectangle_edge_list_to_string (GList      *edge_list,
                                    const char *separator_string,
                                    char       *output)
{
  /* 27 chars: 2 commas, 2 square brackets, space, plus, trailing \0 + 5 for
   * each digit.  Should be more than enough space.  Note that of this
   * space, the trailing \0 will be overwritten for all but the last
   * rectangle.
   *
   * Plus 2 for parenthesis, 4 for 2 more numbers, 2 more commas, and
   * 2 more spaces, for a total of 10 more.
   */
  char rect_string[EDGE_LENGTH];

  char *cur = output;
  GList *tmp = edge_list;

  if (edge_list == NULL)
    g_snprintf (output, 10, "(EMPTY)");

  while (tmp)
    {
      MetaEdge      *edge = tmp->data;
      MetaRectangle *rect = &edge->rect;
      g_snprintf (rect_string, EDGE_LENGTH, "([%d,%d +%d,%d], %2d, %2d)", 
                  rect->x, rect->y, rect->width, rect->height,
                  edge->side_type, edge->edge_type);
      cur = g_stpcpy (cur, rect_string);
      tmp = tmp->next;
      if (tmp)
        cur = g_stpcpy (cur, separator_string);
    }

  return output;
}

MetaRectangle
meta_rect (int x, int y, int width, int height)
{
  MetaRectangle temporary;
  temporary.x = x;
  temporary.y = y;
  temporary.width  = width;
  temporary.height = height;

  return temporary;
}

int
meta_rectangle_area (const MetaRectangle *rect)
{
  g_return_val_if_fail (rect != NULL, 0);
  return rect->width * rect->height;
}

gboolean
meta_rectangle_intersect (const MetaRectangle *src1,
			  const MetaRectangle *src2,
			  MetaRectangle *dest)
{
  int dest_x, dest_y;
  int dest_w, dest_h;
  int return_val;

  g_return_val_if_fail (src1 != NULL, FALSE);
  g_return_val_if_fail (src2 != NULL, FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);

  return_val = FALSE;

  dest_x = MAX (src1->x, src2->x);
  dest_y = MAX (src1->y, src2->y);
  dest_w = MIN (src1->x + src1->width, src2->x + src2->width) - dest_x;
  dest_h = MIN (src1->y + src1->height, src2->y + src2->height) - dest_y;
  
  if (dest_w > 0 && dest_h > 0)
    {
      dest->x = dest_x;
      dest->y = dest_y;
      dest->width = dest_w;
      dest->height = dest_h;
      return_val = TRUE;
    }
  else
    {
      dest->width = 0;
      dest->height = 0;
    }

  return return_val;
}

gboolean
meta_rectangle_equal (const MetaRectangle *src1,
                      const MetaRectangle *src2)
{
  return ((src1->x == src2->x) &&
          (src1->y == src2->y) &&
          (src1->width == src2->width) &&
          (src1->height == src2->height));
}

void
meta_rectangle_union (const MetaRectangle *rect1,
                      const MetaRectangle *rect2,
                      MetaRectangle       *dest)
{
  int dest_x, dest_y;
  int dest_w, dest_h;

  dest_x = rect1->x;
  dest_y = rect1->y;
  dest_w = rect1->width;
  dest_h = rect1->height;

  if (rect2->x < dest_x)
    {
      dest_w += dest_x - rect2->x;
      dest_x = rect2->x;
    }
  if (rect2->y < dest_y)
    {
      dest_h += dest_y - rect2->y;
      dest_y = rect2->y;
    }
  if (rect2->x + rect2->width > dest_x + dest_w)
    dest_w = rect2->x + rect2->width - dest_x;
  if (rect2->y + rect2->height > dest_y + dest_h)
    dest_h = rect2->y + rect2->height - dest_y;

  dest->x = dest_x;
  dest->y = dest_y;
  dest->width = dest_w;
  dest->height = dest_h;
}

gboolean
meta_rectangle_overlap (const MetaRectangle *rect1,
                        const MetaRectangle *rect2)
{
  g_return_val_if_fail (rect1 != NULL, FALSE);
  g_return_val_if_fail (rect2 != NULL, FALSE);

  return !((rect1->x + rect1->width  <= rect2->x) ||
           (rect2->x + rect2->width  <= rect1->x) ||
           (rect1->y + rect1->height <= rect2->y) ||
           (rect2->y + rect2->height <= rect1->y));
}

gboolean
meta_rectangle_vert_overlap (const MetaRectangle *rect1,
                             const MetaRectangle *rect2)
{
  return (rect1->y < rect2->y + rect2->height &&
          rect2->y < rect1->y + rect1->height);
}

gboolean
meta_rectangle_horiz_overlap (const MetaRectangle *rect1,
                              const MetaRectangle *rect2)
{
  return (rect1->x < rect2->x + rect2->width &&
          rect2->x < rect1->x + rect1->width);
}

gboolean
meta_rectangle_could_fit_rect (const MetaRectangle *outer_rect,
                               const MetaRectangle *inner_rect)
{
  return (outer_rect->width  >= inner_rect->width &&
          outer_rect->height >= inner_rect->height);
}

gboolean
meta_rectangle_contains_rect  (const MetaRectangle *outer_rect,
                               const MetaRectangle *inner_rect)
{
  return 
    inner_rect->x                      >= outer_rect->x &&
    inner_rect->y                      >= outer_rect->y &&
    inner_rect->x + inner_rect->width  <= outer_rect->x + outer_rect->width &&
    inner_rect->y + inner_rect->height <= outer_rect->y + outer_rect->height;
}

void
meta_rectangle_resize_with_gravity (const MetaRectangle *old_rect,
                                    MetaRectangle       *rect,
                                    int                  gravity,
                                    int                  new_width,
                                    int                  new_height)
{
  /* FIXME: I'm too deep into this to know whether the below comment is
   * still clear or not now that I've moved it out of constraints.c.
   * boxes.h has a good comment, but I'm not sure if the below info is also
   * helpful on top of that (or whether it has superfluous info).
   */
 
  /* These formulas may look overly simplistic at first but you can work
   * everything out with a left_frame_with, right_frame_width,
   * border_width, and old and new client area widths (instead of old total
   * width and new total width) and you come up with the same formulas.
   *
   * Also, note that the reason we can treat NorthWestGravity and
   * StaticGravity the same is because we're not given a location at
   * which to place the window--the window was already placed
   * appropriately before.  So, NorthWestGravity for this function
   * means to just leave the upper left corner of the outer window
   * where it already is, and StaticGravity for this function means to
   * just leave the upper left corner of the inner window where it
   * already is.  But leaving either of those two corners where they
   * already are will ensure that the other corner is fixed as well
   * (since frame size doesn't change)--thus making the two
   * equivalent.
   */

  /* First, the x direction */
  int adjust = 0;
  switch (gravity)
    {
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
      rect->x = old_rect->x;
      break;

    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
      /* FIXME: Needing to adjust new_width kind of sucks, but not doing so
       * would cause drift.
       */
      new_width -= (old_rect->width - new_width) % 2;
      rect->x = old_rect->x + (old_rect->width - new_width)/2;
      break;

    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      rect->x = old_rect->x + (old_rect->width - new_width);
      break;

    case StaticGravity:
    default:
      rect->x = old_rect->x;
      break;
    }
  rect->width = new_width;
  
  /* Next, the y direction */
  adjust = 0;
  switch (gravity)
    {
    case NorthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
      rect->y = old_rect->y;
      break;

    case WestGravity:
    case CenterGravity:
    case EastGravity:
      /* FIXME: Needing to adjust new_height kind of sucks, but not doing so
       * would cause drift.
       */
      new_height -= (old_rect->height - new_height) % 2;
      rect->y = old_rect->y + (old_rect->height - new_height)/2;
      break;

    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
      rect->y = old_rect->y + (old_rect->height - new_height);
      break;

    case StaticGravity:
    default:
      rect->y = old_rect->y;
      break;
    }
  rect->height = new_height;
}

/* Not so simple helper function for get_minimal_spanning_set_for_region() */
static GList*
merge_spanning_rects_in_region (GList *region)
{
  /* NOTE FOR ANY OPTIMIZATION PEOPLE OUT THERE: Please see the
   * documentation of get_minimal_spanning_set_for_region() for performance
   * considerations that also apply to this function.
   */

  GList* compare;
  compare = region;

  if (region == NULL)
    {
      meta_warning ("Region to merge was empty!  Either you have a some "
                    "pathological STRUT list or there's a bug somewhere!\n");
      return NULL;
    }

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

          /* If a contains b, just remove b */
          if (meta_rectangle_contains_rect (a, b))
            {
              delete_me = other;
            }
          /* If b contains a, just remove a */
          else if (meta_rectangle_contains_rect (a, b))
            {
              delete_me = compare;
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
                }
              /* If a and b are adjacent */
              else if (a->x + a->width == b->x || a->x == b->x + b->width)
                {
                  int new_x = MIN (a->x, b->x);
                  a->width = MAX (a->x + a->width, b->x + b->width) - new_x;
                  a->x = new_x;
                  delete_me = other;
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
                }
              /* If a and b are adjacent */
              else if (a->y + a->height == b->y || a->y == b->y + b->height)
                {
                  int new_y = MIN (a->y, b->y);
                  a->height = MAX (a->y + a->height, b->y + b->height) - new_y;
                  a->y = new_y;
                  delete_me = other;
                }
            }

          other = other->next;

          /* Delete any rectangle in the list that is no longer wanted */
          if (delete_me != NULL)
            {
              /* Deleting the rect we compare others to is a little tricker */
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

        }

      compare = compare->next;
    }

  return region;
}

/* Simple helper function for get_minimal_spanning_set_for_region()... */
static gint
compare_rect_areas (gconstpointer a, gconstpointer b)
{
  const MetaRectangle *a_rect = (gconstpointer) a;
  const MetaRectangle *b_rect = (gconstpointer) b;

  int a_area = meta_rectangle_area (a_rect);
  int b_area = meta_rectangle_area (b_rect);

  return b_area - a_area; /* positive ret value denotes b > a, ... */
}

/* This function is trying to find a "minimal spanning set (of rectangles)"
 * for a given region.
 *
 * The region is given by taking basic_rect, then removing the areas
 * covered by all the rectangles in the all_struts list, and then expanding
 * the resulting region by the given number of pixels in each direction.
 *
 * A "minimal spanning set (of rectangles)" is the best name I could come
 * up with for the concept I had in mind.  Basically, for a given region, I
 * want a set of rectangles with the property that a window is contained in
 * the region if and only if it is contained within at least one of the
 * rectangles.
 *
 * The GList* returned will be a list of (allocated) MetaRectangles.
 * The list will need to be freed by calling
 * meta_rectangle_free_spanning_set() on it (or by manually
 * implementing that function...)
 */
GList*
meta_rectangle_get_minimal_spanning_set_for_region (
  const MetaRectangle *basic_rect,
  const GSList  *all_struts)
{
  /* NOTE FOR OPTIMIZERS: This function *might* be somewhat slow,
   * especially due to the call to merge_spanning_rects_in_region() (which
   * is O(n^2) where n is the size of the list generated in this function).
   * This is made more onerous due to the fact that it involves a fair
   * number of memory allocation and deallocation calls.  However, n is 1
   * for default installations of Gnome (because partial struts aren't used
   * by default and only partial struts increase the size of the spanning
   * set generated).  With one partial strut, n will be 2 or 3.  With 2
   * partial struts, n will probably be 4 or 5.  So, n probably isn't large
   * enough to make this worth bothering.  Further, it is only called from
   * workspace.c:ensure_work_areas_validated (at least as of the time of
   * writing this comment), which in turn should only be called if the
   * strut list changes or the screen or xinerama size changes.  If it ever
   * does show up on profiles (most likely because people start using
   * ridiculously huge numbers of partial struts), possible optimizations
   * include:
   *
   * (1) rewrite merge_spanning_rects_in_region() to be O(n) or O(nlogn).
   *     I'm not totally sure it's possible, but with a couple copies of
   *     the list and sorting them appropriately, I believe it might be.
   * (2) only call merge_spanning_rects_in_region() with a subset of the
   *     full list of rectangles.  I believe from some of my preliminary
   *     debugging and thinking about it that it is possible to figure out
   *     apriori groups of rectangles which are only merge candidates with
   *     each other.  (See testboxes.c:get_screen_region() when which==2
   *     and track the steps of this function carefully to see what gave
   *     me the hint that this might work)
   * (3) figure out how to avoid merge_spanning_rects_in_region().  I think
   *     it might be possible to modify this function to make that
   *     possible, and I spent just a little while thinking about it, but n
   *     wasn't large enough to convince me to care yet.
   * (4) Some of the stuff Rob mentioned at http://mail.gnome.org/archives\
   *     /metacity-devel-list/2005-November/msg00028.html.  (Sorry for the
   *     URL splitting.)
   */

  GList         *ret;
  GList         *tmp_list;
  const GSList  *strut_iter;
  MetaRectangle *temp_rect;

  /* The algorithm is basically as follows:
   *   Initialize rectangle_set to basic_rect
   *   Foreach strut:
   *     Foreach rectangle in rectangle_set:
   *       - Split the rectangle into new rectangles that don't overlap the
   *         strut (but which are as big as possible otherwise)
   *       - Remove the old (pre-split) rectangle from the rectangle_set,
   *         and replace it with the new rectangles generated from the
   *         splitting
   */

  temp_rect = g_new (MetaRectangle, 1);
  *temp_rect = *basic_rect;
  ret = g_list_prepend (NULL, temp_rect);

  strut_iter = all_struts;
  for (strut_iter = all_struts; strut_iter; strut_iter = strut_iter->next)
    {
      GList *rect_iter; 
      MetaRectangle *strut_rect = &((MetaStrut*)strut_iter->data)->rect;

      tmp_list = ret;
      ret = NULL;
      rect_iter = tmp_list;
      while (rect_iter)
        {
          MetaRectangle *rect = (MetaRectangle*) rect_iter->data;
          if (!meta_rectangle_overlap (rect, strut_rect))
            ret = g_list_prepend (ret, rect);
          else
            {
              /* If there is area in rect left of strut */
              if (BOX_LEFT (*rect) < BOX_LEFT (*strut_rect))
                {
                  temp_rect = g_new (MetaRectangle, 1);
                  *temp_rect = *rect;
                  temp_rect->width = BOX_LEFT (*strut_rect) - BOX_LEFT (*rect);
                  ret = g_list_prepend (ret, temp_rect);
                }
              /* If there is area in rect right of strut */
              if (BOX_RIGHT (*rect) > BOX_RIGHT (*strut_rect))
                {
                  int new_x;
                  temp_rect = g_new (MetaRectangle, 1);
                  *temp_rect = *rect;
                  new_x = BOX_RIGHT (*strut_rect);
                  temp_rect->width = BOX_RIGHT(*rect) - new_x;
                  temp_rect->x = new_x;
                  ret = g_list_prepend (ret, temp_rect);
                }
              /* If there is area in rect above strut */
              if (BOX_TOP (*rect) < BOX_TOP (*strut_rect))
                {
                  temp_rect = g_new (MetaRectangle, 1);
                  *temp_rect = *rect;
                  temp_rect->height = BOX_TOP (*strut_rect) - BOX_TOP (*rect);
                  ret = g_list_prepend (ret, temp_rect);
                }
              /* If there is area in rect below strut */
              if (BOX_BOTTOM (*rect) > BOX_BOTTOM (*strut_rect))
                {
                  int new_y;
                  temp_rect = g_new (MetaRectangle, 1);
                  *temp_rect = *rect;
                  new_y = BOX_BOTTOM (*strut_rect);
                  temp_rect->height = BOX_BOTTOM (*rect) - new_y;
                  temp_rect->y = new_y;
                  ret = g_list_prepend (ret, temp_rect);
                }
              g_free (rect);
            }
          rect_iter = rect_iter->next;
        }
      g_list_free (tmp_list);
    }

  /* Sort by maximal area, just because I feel like it... */
  ret = g_list_sort (ret, compare_rect_areas);

  /* Merge rectangles if possible so that the list really is minimal */
  ret = merge_spanning_rects_in_region (ret);

  return ret;
}

GList*
meta_rectangle_expand_region (GList     *region,
                              const int  left_expand,
                              const int  right_expand,
                              const int  top_expand,
                              const int  bottom_expand)
{
  return meta_rectangle_expand_region_conditionally (region,
                                                     left_expand,
                                                     right_expand,
                                                     top_expand,
                                                     bottom_expand,
                                                     0,
                                                     0);
}

GList*
meta_rectangle_expand_region_conditionally (GList     *region,
                                            const int  left_expand,
                                            const int  right_expand,
                                            const int  top_expand,
                                            const int  bottom_expand,
                                            const int  min_x,
                                            const int  min_y)
{
  GList *tmp_list = region;
  while (tmp_list)
    {
      MetaRectangle *rect = (MetaRectangle*) tmp_list->data;
      if (rect->width >= min_x)
        {
          rect->x      -= left_expand;
          rect->width  += (left_expand + right_expand);
        }
      if (rect->height >= min_y)
        {
          rect->y      -= top_expand;
          rect->height += (top_expand + bottom_expand);
        }
      tmp_list = tmp_list->next;
    }

  return region;
}

void
meta_rectangle_expand_to_avoiding_struts (MetaRectangle       *rect,
                                          const MetaRectangle *expand_to,
                                          const MetaDirection  direction,
                                          const GSList        *all_struts)
{
  const GSList *strut_iter;

  /* If someone wants this function to handle more fine-grained
   * direction expanding in the future (e.g. only left, or fully
   * horizontal plus upward), feel free.  But I'm hard-coding for both
   * horizontal directions (exclusive-)or both vertical directions.
   */
  g_assert ((direction == META_DIRECTION_HORIZONTAL) ^
            (direction == META_DIRECTION_VERTICAL  ));
 
  if (direction == META_DIRECTION_HORIZONTAL)
    {
      rect->x      = expand_to->x;
      rect->width  = expand_to->width;
    }
  else
    {
      rect->y      = expand_to->y;
      rect->height = expand_to->height;
    }

 
  /* Run over all struts */
  for (strut_iter = all_struts; strut_iter; strut_iter = strut_iter->next)
    {
      MetaStrut *strut = (MetaStrut*) strut_iter->data;
 
      /* Skip struts that don't overlap */
      if (!meta_rectangle_overlap (&strut->rect, rect))
        continue;

      if (direction == META_DIRECTION_HORIZONTAL)
        {
          if (strut->side == META_SIDE_LEFT)
            {
              int offset = BOX_RIGHT(strut->rect) - BOX_LEFT(*rect);
              rect->x     += offset;
              rect->width -= offset;
            }
          else if (strut->side == META_SIDE_RIGHT)
            {
              int offset = BOX_RIGHT (*rect) - BOX_LEFT(strut->rect);
              rect->width -= offset;
            }
          /* else ignore the strut */
        }
      else /* direction == META_DIRECTION_VERTICAL */
        {
          if (strut->side == META_SIDE_TOP)
            {
              int offset = BOX_BOTTOM(strut->rect) - BOX_TOP(*rect);
              rect->y      += offset;
              rect->height -= offset;
            }
          else if (strut->side == META_SIDE_BOTTOM)
            {
              int offset = BOX_BOTTOM(*rect) - BOX_TOP(strut->rect);
              rect->height -= offset;
            }
          /* else ignore the strut */
        }
    } /* end loop over struts */
} /* end meta_rectangle_expand_to_avoiding_struts */

void
meta_rectangle_free_list_and_elements (GList *filled_list)
{
  g_list_foreach (filled_list, 
                  (void (*)(gpointer,gpointer))&g_free, /* ew, for ugly */
                  NULL);
  g_list_free (filled_list);
}

gboolean
meta_rectangle_could_fit_in_region (const GList         *spanning_rects,
                                    const MetaRectangle *rect)
{
  const GList *temp;
  gboolean     could_fit;

  temp = spanning_rects;
  could_fit = FALSE;
  while (!could_fit && temp != NULL)
    {
      could_fit = could_fit || meta_rectangle_could_fit_rect (temp->data, rect);
      temp = temp->next;
    }

  return could_fit;
}

gboolean
meta_rectangle_contained_in_region (const GList         *spanning_rects,
                                    const MetaRectangle *rect)
{
  const GList *temp;
  gboolean     contained;

  temp = spanning_rects;
  contained = FALSE;
  while (!contained && temp != NULL)
    {
      contained = contained || meta_rectangle_contains_rect (temp->data, rect);
      temp = temp->next;
    }

  return contained;
}

gboolean
meta_rectangle_overlaps_with_region (const GList         *spanning_rects,
                                     const MetaRectangle *rect)
{
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


void
meta_rectangle_clamp_to_fit_into_region (const GList         *spanning_rects,
                                         FixedDirections      fixed_directions,
                                         MetaRectangle       *rect,
                                         const MetaRectangle *min_size)
{
  const GList *temp;
  const MetaRectangle *best_rect = NULL;
  int                  best_overlap = 0;

  /* First, find best rectangle from spanning_rects to which we can clamp
   * rect to fit into.
   */
  for (temp = spanning_rects; temp; temp = temp->next)
    {
      MetaRectangle *compare_rect = temp->data;
      int            maximal_overlap_amount_for_compare;
      
      /* If x is fixed and the entire width of rect doesn't fit in compare,
       * skip this rectangle.
       */
      if ((fixed_directions & FIXED_DIRECTION_X) &&
          (compare_rect->x > rect->x || 
           compare_rect->x + compare_rect->width < rect->x + rect->width))
        continue;
        
      /* If y is fixed and the entire height of rect doesn't fit in compare,
       * skip this rectangle.
       */
      if ((fixed_directions & FIXED_DIRECTION_Y) &&
          (compare_rect->y > rect->y || 
           compare_rect->y + compare_rect->height < rect->y + rect->height))
        continue;

      /* If compare can't hold the min_size window, skip this rectangle. */
      if (compare_rect->width  < min_size->width ||
          compare_rect->height < min_size->height)
        continue;

      /* Determine maximal overlap amount */
      maximal_overlap_amount_for_compare =
        MIN (rect->width,  compare_rect->width) *
        MIN (rect->height, compare_rect->height);

      /* See if this is the best rect so far */
      if (maximal_overlap_amount_for_compare > best_overlap)
        {
          best_rect    = compare_rect;
          best_overlap = maximal_overlap_amount_for_compare;
        }
    }

  /* Clamp rect appropriately */
  if (best_rect == NULL)
    {
      meta_warning ("No rect whose size to clamp to found!\n");

      /* If it doesn't fit, at least make it no bigger than it has to be */
      if (!(fixed_directions & FIXED_DIRECTION_X))
        rect->width  = min_size->width;
      if (!(fixed_directions & FIXED_DIRECTION_Y))
        rect->height = min_size->height;
    }
  else
    {
      rect->width  = MIN (rect->width,  best_rect->width);
      rect->height = MIN (rect->height, best_rect->height);
    }
}

void
meta_rectangle_clip_to_region (const GList         *spanning_rects,
                               FixedDirections      fixed_directions,
                               MetaRectangle       *rect)
{
  const GList *temp;
  const MetaRectangle *best_rect = NULL;
  int                  best_overlap = 0;

  /* First, find best rectangle from spanning_rects to which we will clip
   * rect into.
   */
  for (temp = spanning_rects; temp; temp = temp->next)
    {
      MetaRectangle *compare_rect = temp->data;
      MetaRectangle  overlap;
      int            maximal_overlap_amount_for_compare;
     
      /* If x is fixed and the entire width of rect doesn't fit in compare,
       * skip the rectangle.
       */
      if ((fixed_directions & FIXED_DIRECTION_X) &&
          (compare_rect->x > rect->x || 
           compare_rect->x + compare_rect->width < rect->x + rect->width))
        continue;
        
      /* If y is fixed and the entire height of rect doesn't fit in compare,
       * skip the rectangle.
       */
      if ((fixed_directions & FIXED_DIRECTION_Y) &&
          (compare_rect->y > rect->y || 
           compare_rect->y + compare_rect->height < rect->y + rect->height))
        continue;

      /* Determine maximal overlap amount */
      meta_rectangle_intersect (rect, compare_rect, &overlap);
      maximal_overlap_amount_for_compare = meta_rectangle_area (&overlap);

      /* See if this is the best rect so far */
      if (maximal_overlap_amount_for_compare > best_overlap)
        {
          best_rect    = compare_rect;
          best_overlap = maximal_overlap_amount_for_compare;
        }
    }

  /* Clip rect appropriately */
  if (best_rect == NULL)
    meta_warning ("No rect to clip to found!\n");
  else
    {
      /* Extra precaution with checking fixed direction shouldn't be needed
       * due to logic above, but it shouldn't hurt either.
       */
      if (!(fixed_directions & FIXED_DIRECTION_X))
        {
          /* Find the new left and right */
          int new_x = MAX (rect->x, best_rect->x);
          rect->width = MIN ((rect->x + rect->width)           - new_x,
                             (best_rect->x + best_rect->width) - new_x);
          rect->x = new_x;
        }

      /* Extra precaution with checking fixed direction shouldn't be needed
       * due to logic above, but it shouldn't hurt either.
       */
      if (!(fixed_directions & FIXED_DIRECTION_Y))
        {
          /* Clip the top, if needed */
          int new_y = MAX (rect->y, best_rect->y);
          rect->height = MIN ((rect->y + rect->height)           - new_y,
                              (best_rect->y + best_rect->height) - new_y);
          rect->y = new_y;
        }
    }
}

void
meta_rectangle_shove_into_region (const GList         *spanning_rects,
                                  FixedDirections      fixed_directions,
                                  MetaRectangle       *rect)
{
  const GList *temp;
  const MetaRectangle *best_rect = NULL;
  int                  best_overlap = 0;
  int                  shortest_distance = G_MAXINT;

  /* First, find best rectangle from spanning_rects to which we will shove
   * rect into.
   */
  
  for (temp = spanning_rects; temp; temp = temp->next)
    {
      MetaRectangle *compare_rect = temp->data;
      int            maximal_overlap_amount_for_compare;
      int            dist_to_compare;
      
      /* If x is fixed and the entire width of rect doesn't fit in compare,
       * skip this rectangle.
       */
      if ((fixed_directions & FIXED_DIRECTION_X) &&
          (compare_rect->x > rect->x || 
           compare_rect->x + compare_rect->width < rect->x + rect->width))
        continue;
        
      /* If y is fixed and the entire height of rect doesn't fit in compare,
       * skip this rectangle.
       */
      if ((fixed_directions & FIXED_DIRECTION_Y) &&
          (compare_rect->y > rect->y || 
           compare_rect->y + compare_rect->height < rect->y + rect->height))
        continue;

      /* Determine maximal overlap amount between rect & compare_rect */
      maximal_overlap_amount_for_compare =
        MIN (rect->width,  compare_rect->width) *
        MIN (rect->height, compare_rect->height);

      /* Determine distance necessary to put rect into compare_rect */
      dist_to_compare = 0;
      if (compare_rect->x > rect->x)
        dist_to_compare += compare_rect->x - rect->x;
      if (compare_rect->x + compare_rect->width < rect->x + rect->width)
        dist_to_compare += (rect->x + rect->width) -
                           (compare_rect->x + compare_rect->width);
      if (compare_rect->y > rect->y)
        dist_to_compare += compare_rect->y - rect->y;
      if (compare_rect->y + compare_rect->height < rect->y + rect->height)
        dist_to_compare += (rect->y + rect->height) -
                           (compare_rect->y + compare_rect->height);

      /* See if this is the best rect so far */
      if ((maximal_overlap_amount_for_compare > best_overlap) ||
          (maximal_overlap_amount_for_compare == best_overlap &&
           dist_to_compare                    <  shortest_distance))
        {
          best_rect         = compare_rect;
          best_overlap      = maximal_overlap_amount_for_compare;
          shortest_distance = dist_to_compare;
        }
    }

  /* Shove rect appropriately */
  if (best_rect == NULL)
    meta_warning ("No rect to shove into found!\n");
  else
    {
      /* Extra precaution with checking fixed direction shouldn't be needed
       * due to logic above, but it shouldn't hurt either.
       */
      if (!(fixed_directions & FIXED_DIRECTION_X))
        {
          /* Shove to the right, if needed */
          if (best_rect->x > rect->x)
            rect->x = best_rect->x;

          /* Shove to the left, if needed */
          if (best_rect->x + best_rect->width < rect->x + rect->width)
            rect->x = (best_rect->x + best_rect->width) - rect->width;
        }

      /* Extra precaution with checking fixed direction shouldn't be needed
       * due to logic above, but it shouldn't hurt either.
       */
      if (!(fixed_directions & FIXED_DIRECTION_Y))
        {
          /* Shove down, if needed */
          if (best_rect->y > rect->y)
            rect->y = best_rect->y;

          /* Shove up, if needed */
          if (best_rect->y + best_rect->height < rect->y + rect->height)
            rect->y = (best_rect->y + best_rect->height) - rect->height;
        }
    }
}

void
meta_rectangle_find_linepoint_closest_to_point (double x1,
                                                double y1,
                                                double x2,
                                                double y2,
                                                double px,
                                                double py,
                                                double *valx,
                                                double *valy)
{
  /* I'll use the shorthand rx, ry for the return values, valx & valy.
   * Now, we need (rx,ry) to be on the line between (x1,y1) and (x2,y2).
   * For that to happen, we first need the slope of the line from (x1,y1)
   * to (rx,ry) must match the slope of (x1,y1) to (x2,y2), i.e.:
   *   (ry-y1)   (y2-y1)
   *   ------- = -------
   *   (rx-x1)   (x2-x1)
   * If x1==x2, though, this gives divide by zero errors, so we want to
   * rewrite the equation by multiplying both sides by (rx-x1)*(x2-x1):
   *   (ry-y1)(x2-x1) = (y2-y1)(rx-x1)
   * This is a valid requirement even when x1==x2 (when x1==x2, this latter
   * equation will basically just mean that rx must be equal to both x1 and
   * x2)
   *
   * The other requirement that we have is that the line from (rx,ry) to
   * (px,py) must be perpendicular to the line from (x1,y1) to (x2,y2).  So
   * we just need to get a vector in the direction of each line, take the
   * dot product of the two, and ensure that the result is 0:
   *   (rx-px)*(x2-x1) + (ry-py)*(y2-y1) = 0.
   *
   * This gives us two equations and two unknowns:
   *
   *   (ry-y1)(x2-x1) = (y2-y1)(rx-x1)
   *   (rx-px)*(x2-x1) + (ry-py)*(y2-y1) = 0.
   *
   * This particular pair of equations is always solvable so long as
   * (x1,y1) and (x2,y2) are not the same point (and note that anyone who
   * calls this function that way is braindead because it means that they
   * really didn't specify a line after all).  However, the caller should
   * be careful to avoid making (x1,y1) and (x2,y2) too close (e.g. like
   * 10^{-8} apart in each coordinate), otherwise roundoff error could
   * cause issues.  Solving these equations by hand (or using Maple(TM) or
   * Mathematica(TM) or whatever) results in slightly messy expressions,
   * but that's all the below few lines do.
   */

  double diffx, diffy, den;
  diffx = x2 - x1;
  diffy = y2 - y1;
  den = diffx * diffx + diffy * diffy;

  *valx = (py * diffx * diffy + px * diffx * diffx +
           y2 * x1 * diffy - y1 * x2 * diffy) / den;
  *valy = (px * diffx * diffy + py * diffy * diffy +
           x2 * y1 * diffx - x1 * y2 * diffx) / den;
}

/***************************************************************************/
/*                                                                         */
/* Switching gears to code for edges instead of just rectangles            */
/*                                                                         */
/***************************************************************************/

gboolean
meta_rectangle_edge_aligns (const MetaRectangle *rect, const MetaEdge *edge)
{
  /* The reason for the usage of <= below instead of < is because we are
   * interested in in-the-way-or-adject'ness.  So, a left (i.e. vertical
   * edge) occupying y positions 0-9 (which has a y of 0 and a height of
   * 10) and a rectangle with top at y=10 would be considered to "align" by
   * this function.
   */
  switch (edge->side_type)
    {
    case META_SIDE_LEFT:
    case META_SIDE_RIGHT:
      return BOX_TOP (*rect)      <= BOX_BOTTOM (edge->rect) &&
             BOX_TOP (edge->rect) <= BOX_BOTTOM (*rect);
    case META_SIDE_TOP:
    case META_SIDE_BOTTOM:
      return BOX_LEFT (*rect)      <= BOX_RIGHT (edge->rect) &&
             BOX_LEFT (edge->rect) <= BOX_RIGHT (*rect);
    default:
      g_assert_not_reached ();
    }
}

static GList*
get_rect_minus_overlap (const GList   *rect_in_list, 
                        MetaRectangle *overlap)
{
  MetaRectangle *temp;
  MetaRectangle *rect = rect_in_list->data;
  GList *ret = NULL;

  if (BOX_LEFT (*rect) < BOX_LEFT (*overlap))
    {
      temp = g_new (MetaRectangle, 1);
      *temp = *rect;
      temp->width = BOX_LEFT (*overlap) - BOX_LEFT (*rect);
      ret = g_list_prepend (ret, temp);
    }
  if (BOX_RIGHT (*rect) > BOX_RIGHT (*overlap))
    {
      temp = g_new (MetaRectangle, 1);
      *temp = *rect;
      temp->x = BOX_RIGHT (*overlap);
      temp->width = BOX_RIGHT (*rect) - BOX_RIGHT (*overlap);
      ret = g_list_prepend (ret, temp);
    }
  if (BOX_TOP (*rect) < BOX_TOP (*overlap))
    {
      temp = g_new (MetaRectangle, 1);
      temp->x      = overlap->x;
      temp->width  = overlap->width;
      temp->y      = BOX_TOP (*rect);
      temp->height = BOX_TOP (*overlap) - BOX_TOP (*rect);
      ret = g_list_prepend (ret, temp);
    }
  if (BOX_BOTTOM (*rect) > BOX_BOTTOM (*overlap))
    {
      temp = g_new (MetaRectangle, 1);
      temp->x      = overlap->x;
      temp->width  = overlap->width;
      temp->y      = BOX_BOTTOM (*overlap);
      temp->height = BOX_BOTTOM (*rect) - BOX_BOTTOM (*overlap);
      ret = g_list_prepend (ret, temp);
    }

  return ret;
}

static GList*
replace_rect_with_list (GList *old_element, 
                        GList *new_list)
{
  GList *ret;
  g_assert (old_element != NULL);

  if (!new_list)
    {
      /* If there is no new list, just remove the old_element */
      ret = g_list_remove_link (old_element, old_element);
    }
  else
    {
      /* Fix up the prev and next pointers everywhere */
      ret = new_list;
      if (old_element->prev)
        {
          old_element->prev->next = new_list;
          new_list->prev = old_element->prev;
        }
      if (old_element->next)
        {
          GList *tmp = g_list_last (new_list);
          old_element->next->prev = tmp;
          tmp->next = old_element->next;
        }
    }

  /* Free the old_element and return the appropriate "next" point */
  g_free (old_element->data);
  g_list_free_1 (old_element);
  return ret;
}

/* Make a copy of the strut list, make sure that copy only contains parts
 * of the old_struts that intersect with the region rect, and then do some
 * magic to make all the new struts disjoint (okay, we we break up struts
 * that aren't disjoint in a way that the overlapping part is only included
 * once, so it's not really magic...).
 */
static GList*
get_disjoint_strut_rect_list_in_region (const GSList        *old_struts,
                                        const MetaRectangle *region)
{
  GList *strut_rects;
  GList *tmp;

  /* First, copy the list */
  strut_rects = NULL;
  while (old_struts)
    {
      MetaRectangle *cur = &((MetaStrut*)old_struts->data)->rect;
      MetaRectangle *copy = g_new (MetaRectangle, 1);
      *copy = *cur;
      if (meta_rectangle_intersect (copy, region, copy))
        strut_rects = g_list_prepend (strut_rects, copy);
      else
        g_free (copy);

      old_struts = old_struts->next;
    }

  /* Now, loop over the list and check for intersections, fixing things up
   * where they do intersect.
   */
  tmp = strut_rects;
  while (tmp)
    {
      GList *compare;

      MetaRectangle *cur = tmp->data;

      compare = tmp->next;
      while (compare)
        {
          MetaRectangle *comp = compare->data;
          MetaRectangle overlap;

          if (meta_rectangle_intersect (cur, comp, &overlap))
            {
              /* Get a list of rectangles for each strut that don't overlap
               * the intersection region.
               */
              GList *cur_leftover  = get_rect_minus_overlap (tmp,  &overlap);
              GList *comp_leftover = get_rect_minus_overlap (compare, &overlap);

              /* Add the intersection region to cur_leftover */
              MetaRectangle *overlap_allocated = g_new (MetaRectangle, 1);
              *overlap_allocated = overlap;
              cur_leftover = g_list_prepend (cur_leftover, overlap_allocated);

              /* Fix up tmp, compare, and cur -- maybe struts too */
              if (strut_rects == tmp)
                {
                  strut_rects = replace_rect_with_list (tmp, cur_leftover);
                  tmp = strut_rects;
                }
              else
                tmp   = replace_rect_with_list (tmp,     cur_leftover);
              compare = replace_rect_with_list (compare, comp_leftover);

              if (compare == NULL)
                break;

              cur = tmp->data;
            }

          compare = compare->next;
        }

      tmp = tmp->next;
    }

  return strut_rects;
}

gint
meta_rectangle_edge_cmp_ignore_type (gconstpointer a, gconstpointer b)
{
  const MetaEdge *a_edge_rect = (gconstpointer) a;
  const MetaEdge *b_edge_rect = (gconstpointer) b;
  int a_compare, b_compare;

  /* Edges must be both vertical or both horizontal, or it doesn't make
   * sense to compare them.
   */
  g_assert ((a_edge_rect->rect.width  == 0 && b_edge_rect->rect.width == 0) ||
            (a_edge_rect->rect.height == 0 && b_edge_rect->rect.height == 0));

  a_compare = b_compare = 0;  /* gcc-3.4.2 sucks at figuring initialized'ness */

  if (a_edge_rect->side_type == META_SIDE_LEFT ||
      a_edge_rect->side_type == META_SIDE_RIGHT)
    {
      a_compare = a_edge_rect->rect.x;
      b_compare = b_edge_rect->rect.x;
      if (a_compare == b_compare)
        {
          a_compare = a_edge_rect->rect.y;
          b_compare = b_edge_rect->rect.y;
        }
    }
  else if (a_edge_rect->side_type == META_SIDE_TOP ||
           a_edge_rect->side_type == META_SIDE_BOTTOM)
    {
      a_compare = a_edge_rect->rect.y;
      b_compare = b_edge_rect->rect.y;
      if (a_compare == b_compare)
        {
          a_compare = a_edge_rect->rect.x;
          b_compare = b_edge_rect->rect.x;
        }
    }
  else
    g_assert ("Some idiot wanted to sort sides of different types.\n");

  return a_compare - b_compare; /* positive value denotes a > b ... */
}

/* To make things easily testable, provide a nice way of sorting edges */
gint
meta_rectangle_edge_cmp (gconstpointer a, gconstpointer b)
{
  const MetaEdge *a_edge_rect = (gconstpointer) a;
  const MetaEdge *b_edge_rect = (gconstpointer) b;

  int a_compare, b_compare;

  a_compare = a_edge_rect->side_type;
  b_compare = b_edge_rect->side_type;

  if (a_compare == b_compare)
    return meta_rectangle_edge_cmp_ignore_type (a, b);

  return a_compare - b_compare; /* positive value denotes a > b ... */
}

/* Determine whether two given edges overlap */
static gboolean
edges_overlap (const MetaEdge *edge1,
               const MetaEdge *edge2)
{
  if (edge1->rect.width == 0 && edge2->rect.width == 0)
    {
      return meta_rectangle_vert_overlap (&edge1->rect, &edge2->rect) &&
             edge1->rect.x == edge2->rect.x;
    }
  else if (edge1->rect.height == 0 && edge2->rect.height == 0)
    {
      return meta_rectangle_horiz_overlap (&edge1->rect, &edge2->rect) &&
             edge1->rect.y == edge2->rect.y;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
rectangle_and_edge_intersection (const MetaRectangle *rect,
                                 const MetaEdge      *edge,
                                 MetaEdge            *overlap,
                                 int                 *handle_type)
{
  const MetaRectangle *rect2  = &edge->rect;
  MetaRectangle *result = &overlap->rect;
  gboolean intersect = TRUE;

  /* We don't know how to set these, so set them to invalid values */
  overlap->edge_type = -1;
  overlap->side_type = -1;

  /* Figure out what the intersection is */  
  result->x = MAX (rect->x, rect2->x);
  result->y = MAX (rect->y, rect2->y);
  result->width  = MIN (BOX_RIGHT (*rect),  BOX_RIGHT (*rect2))  - result->x;
  result->height = MIN (BOX_BOTTOM (*rect), BOX_BOTTOM (*rect2)) - result->y;

  /* Find out if the intersection is empty; have to do it this way since
   * edges have a thickness of 0
   */
  if ((result->width <  0 || result->height <  0) ||
      (result->width == 0 && result->height == 0))
    {
      result->width = 0;
      result->height = 0;
      intersect = FALSE;
    }
  else
    {
      /* Need to figure out the handle_type, a somewhat weird quantity:
       *   0 - overlap is in middle of rect
       *  -1 - overlap is at the side of rect, and is on the opposite side
       *       of rect than the edge->side_type side
       *   1 - overlap is at the side of rect, and the side of rect it is
       *       on is the edge->side_type side
       */
      switch (edge->side_type)
        {
        case META_SIDE_LEFT:
          if (result->x == rect->x)
            *handle_type = 1;
          else if (result->x == BOX_RIGHT (*rect))
            *handle_type = -1;
          else
            *handle_type = 0;
          break;
        case META_SIDE_RIGHT:
          if (result->x == rect->x)
            *handle_type = -1;
          else if (result->x == BOX_RIGHT (*rect))
            *handle_type = 1;
          else
            *handle_type = 0;
          break;
        case META_SIDE_TOP:
          if (result->y == rect->y)
            *handle_type = 1;
          else if (result->y == BOX_BOTTOM (*rect))
            *handle_type = -1;
          else
            *handle_type = 0;
          break;
        case META_SIDE_BOTTOM:
          if (result->y == rect->y)
            *handle_type = -1;
          else if (result->y == BOX_BOTTOM (*rect))
            *handle_type = 1;
          else
            *handle_type = 0;
          break;
        default:
          g_assert_not_reached ();
        }
    }
  return intersect;
}

/* Add all edges of the given rect to cur_edges and return the result.  If
 * rect_is_internal is false, the side types are switched (LEFT<->RIGHT and
 * TOP<->BOTTOM).
 */
static GList*
add_edges (GList               *cur_edges, 
           const MetaRectangle *rect,
           gboolean             rect_is_internal)
{
  MetaEdge *temp_edge;
  int i;

  for (i=0; i<4; i++)
    {
      temp_edge = g_new (MetaEdge, 1);
      temp_edge->rect = *rect;
      switch (i)
        {
        case 0:
          temp_edge->side_type = 
            rect_is_internal ? META_SIDE_LEFT : META_SIDE_RIGHT;
          temp_edge->rect.width = 0;
          break;
        case 1:
          temp_edge->side_type = 
            rect_is_internal ? META_SIDE_RIGHT : META_SIDE_LEFT;
          temp_edge->rect.x     += temp_edge->rect.width;
          temp_edge->rect.width  = 0;
          break;
        case 2:
          temp_edge->side_type = 
            rect_is_internal ? META_SIDE_TOP : META_SIDE_BOTTOM;
          temp_edge->rect.height = 0;
          break;
        case 3:
          temp_edge->side_type = 
            rect_is_internal ? META_SIDE_BOTTOM : META_SIDE_TOP;
          temp_edge->rect.y      += temp_edge->rect.height;
          temp_edge->rect.height  = 0;
          break;
        }
      temp_edge->edge_type = META_EDGE_SCREEN;
      cur_edges = g_list_prepend (cur_edges, temp_edge);
    }

  return cur_edges;
}

/* Remove any part of old_edge that intersects remove and add any resulting
 * edges to cur_list.  Return cur_list when finished.
 */
static GList*
split_edge (GList *cur_list, 
            const MetaEdge *old_edge, 
            const MetaEdge *remove)
{
  MetaEdge *temp_edge;
  switch (old_edge->side_type)
    {
    case META_SIDE_LEFT:
    case META_SIDE_RIGHT:
      g_assert (meta_rectangle_vert_overlap (&old_edge->rect, &remove->rect));
      if (BOX_TOP (old_edge->rect)  < BOX_TOP (remove->rect))
        {
          temp_edge = g_new (MetaEdge, 1);
          *temp_edge = *old_edge;
          temp_edge->rect.height = BOX_TOP (remove->rect)
                                 - BOX_TOP (old_edge->rect);
          cur_list = g_list_prepend (cur_list, temp_edge);
        }
      if (BOX_BOTTOM (old_edge->rect) > BOX_BOTTOM (remove->rect))
        {
          temp_edge = g_new (MetaEdge, 1);
          *temp_edge = *old_edge;
          temp_edge->rect.y      = BOX_BOTTOM (remove->rect);
          temp_edge->rect.height = BOX_BOTTOM (old_edge->rect)
                                 - BOX_BOTTOM (remove->rect);
          cur_list = g_list_prepend (cur_list, temp_edge);
        }
      break;
    case META_SIDE_TOP:
    case META_SIDE_BOTTOM:
      g_assert (meta_rectangle_horiz_overlap (&old_edge->rect, &remove->rect));
      if (BOX_LEFT (old_edge->rect)  < BOX_LEFT (remove->rect))
        {
          temp_edge = g_new (MetaEdge, 1);
          *temp_edge = *old_edge;
          temp_edge->rect.width = BOX_LEFT (remove->rect)
                                - BOX_LEFT (old_edge->rect);
          cur_list = g_list_prepend (cur_list, temp_edge);
        }
      if (BOX_RIGHT (old_edge->rect) > BOX_RIGHT (remove->rect))
        {
          temp_edge = g_new (MetaEdge, 1);
          *temp_edge = *old_edge;
          temp_edge->rect.x     = BOX_RIGHT (remove->rect);
          temp_edge->rect.width = BOX_RIGHT (old_edge->rect)
                                - BOX_RIGHT (remove->rect);
          cur_list = g_list_prepend (cur_list, temp_edge);
        }
      break;
    default:
      g_assert_not_reached ();
    }

  return cur_list;
}

/* Split up edge and remove preliminary edges from strut_edges depending on
 * if and how rect and edge intersect.
 */
static void
fix_up_edges (MetaRectangle *rect,         MetaEdge *edge, 
              GList         **strut_edges, GList    **edge_splits,
              gboolean      *edge_needs_removal)
{
  MetaEdge overlap;
  int      handle_type;

  if (!rectangle_and_edge_intersection (rect, edge, &overlap, &handle_type))
    return;

  if (handle_type == 0 || handle_type == 1)
    {
      /* Put the result of removing overlap from edge into edge_splits */
      *edge_splits = split_edge (*edge_splits, edge, &overlap);
      *edge_needs_removal = TRUE;
    }

  if (handle_type == -1 || handle_type == 1)
    {
      /* Remove the overlap from strut_edges */
      /* First, loop over the edges of the strut */
      GList *tmp = *strut_edges;
      while (tmp)
        {
          MetaEdge *cur = tmp->data;
          /* If this is the edge that overlaps, then we need to split it */
          if (edges_overlap (cur, &overlap))
            {
              GList *delete_me = tmp;

              /* Split this edge into some new ones */
              *strut_edges = split_edge (*strut_edges, cur, &overlap);

              /* Delete the old one */
              tmp = tmp->next;
              g_free (cur);
              *strut_edges = g_list_delete_link (*strut_edges, delete_me);
            }
          else
            tmp = tmp->next;
        }
    }
}

/* This function removes intersections of edges with the rectangles from the
 * list of edges.
 */
GList*
meta_rectangle_remove_intersections_with_boxes_from_edges (
  GList        *edges,
  const GSList *rectangles)
{
  const GSList *rect_iter;
  const int opposing = 1;

  /* Now remove all intersections of rectangles with the edge list */
  rect_iter = rectangles;
  while (rect_iter)
    {
      MetaRectangle *rect = rect_iter->data;
      GList *edge_iter = edges;
      while (edge_iter)
        {
          MetaEdge *edge = edge_iter->data;
          MetaEdge overlap;
          int      handle;
          gboolean edge_iter_advanced = FALSE;

          /* If this edge overlaps with this rect... */
          if (rectangle_and_edge_intersection (rect, edge, &overlap, &handle))
            {

              /* "Intersections" where the edges touch but are opposite
               * sides (e.g. a left edge against the right edge) should not
               * be split.  Note that the comments in
               * rectangle_and_edge_intersection() say that opposing edges
               * occur when handle is -1, BUT you need to remember that we
               * treat the left side of a window as a right edge because
               * it's what the right side of the window being moved should
               * be-resisted-by/snap-to.  So opposing is really 1.  Anyway,
               * we just keep track of it in the opposing constant set up
               * above and if handle isn't equal to that, then we know the
               * edge should be split.
               */
              if (handle != opposing)
                {
                  /* Keep track of this edge so we can delete it below */
                  GList *delete_me = edge_iter;
                  edge_iter = edge_iter->next;
                  edge_iter_advanced = TRUE;

                  /* Split the edge and add the result to beginning of edges */
                  edges = split_edge (edges, edge, &overlap);

                  /* Now free the edge... */
                  g_free (edge);
                  edges = g_list_delete_link (edges, delete_me);
                }
            }

          if (!edge_iter_advanced)
            edge_iter = edge_iter->next;
        }

      rect_iter = rect_iter->next;
    }

  return edges;
}

/* This function is trying to find all the edges of an onscreen region. */
GList*
meta_rectangle_find_onscreen_edges (const MetaRectangle *basic_rect,
                                    const GSList        *all_struts)
{
  GList        *ret;
  GList        *fixed_strut_rects;
  GList        *edge_iter; 
  const GList  *strut_rect_iter;

  /* The algorithm is basically as follows:
   *   Make sure the struts are disjoint
   *   Initialize the edge_set to the edges of basic_rect
   *   Foreach strut:
   *     Put together a preliminary new edge from the edges of the strut
   *     Foreach edge in edge_set:
   *       - Split the edge if it is partially contained inside the strut
   *       - If the edge matches an edge of the strut (i.e. a strut just
   *         against the edge of the screen or a not-next-to-edge-of-screen
   *         strut adjacent to another), then both the edge from the
   *         edge_set and the preliminary edge for the strut will need to
   *         be split
   *     Add any remaining "preliminary" strut edges to the edge_set
   */

  /* Make sure the struts are disjoint */
  fixed_strut_rects =
    get_disjoint_strut_rect_list_in_region (all_struts, basic_rect);

  /* Start off the list with the edges of basic_rect */
  ret = add_edges (NULL, basic_rect, TRUE);

  strut_rect_iter = fixed_strut_rects;
  while (strut_rect_iter)
    {
      MetaRectangle *strut_rect = (MetaRectangle*) strut_rect_iter->data;

      /* Get the new possible edges we may need to add from the strut */
      GList *new_strut_edges = add_edges (NULL, strut_rect, FALSE);

      edge_iter = ret;
      while (edge_iter)
        {
          MetaEdge *cur_edge = edge_iter->data;
          GList *splits_of_cur_edge = NULL;
          gboolean edge_needs_removal = FALSE;

          fix_up_edges (strut_rect,       cur_edge, 
                        &new_strut_edges, &splits_of_cur_edge,
                        &edge_needs_removal);

          if (edge_needs_removal)
            {
              /* Delete the old edge */
              GList *delete_me = edge_iter;
              edge_iter = edge_iter->next;
              g_free (cur_edge);
              ret = g_list_delete_link (ret, delete_me);

              /* Add the new split parts of the edge */
              ret = g_list_concat (splits_of_cur_edge, ret);
            }
          else
            {
              edge_iter = edge_iter->next;
            }

          /* edge_iter was already advanced above */
        }

      ret = g_list_concat (new_strut_edges, ret);
      strut_rect_iter = strut_rect_iter->next;
    }

  /* Sort the list */
  ret = g_list_sort (ret, meta_rectangle_edge_cmp);

  /* Free the fixed struts list */
  meta_rectangle_free_list_and_elements (fixed_strut_rects);

  return ret;
}

GList*
meta_rectangle_find_nonintersected_xinerama_edges (
                                    const GList         *xinerama_rects,
                                    const GSList        *all_struts)
{
  /* This function cannot easily be merged with
   * meta_rectangle_find_onscreen_edges() because real screen edges
   * and strut edges both are of the type "there ain't anything
   * immediately on the other side"; xinerama edges are different.
   */
  GList *ret;
  const GList  *cur;
  GSList *temp_rects;

  /* Initialize the return list to be empty */
  ret = NULL;

  /* start of ret with all the edges of xineramas that are adjacent to
   * another xinerama.
   */
  cur = xinerama_rects;
  while (cur)
    {
      MetaRectangle *cur_rect = cur->data;
      const GList *compare = xinerama_rects;
      while (compare)
        {
          MetaRectangle *compare_rect = compare->data;

          /* Check if cur might be horizontally adjacent to compare */
          if (meta_rectangle_vert_overlap(cur_rect, compare_rect))
            {
              MetaSide side_type;
              int y      = MAX (cur_rect->y, compare_rect->y);
              int height = MIN (BOX_BOTTOM (*cur_rect) - y,
                                BOX_BOTTOM (*compare_rect) - y);
              int width  = 0;
              int x;

              if (BOX_LEFT (*cur_rect)  == BOX_RIGHT (*compare_rect))
                {
                  /* compare_rect is to the left of cur_rect */
                  x = BOX_LEFT (*cur_rect);
                  side_type = META_SIDE_LEFT;
                }
              else if (BOX_RIGHT (*cur_rect) == BOX_LEFT (*compare_rect))
                {
                  /* compare_rect is to the right of cur_rect */
                  x = BOX_RIGHT (*cur_rect);
                  side_type = META_SIDE_RIGHT;
                }
              else
                /* These rectangles aren't adjacent after all */
                x = INT_MIN;

              /* If the rectangles really are adjacent */
              if (x != INT_MIN)
                {
                  /* We need a left edge for the xinerama on the right, and
                   * a right edge for the xinerama on the left.  Just fill
                   * up the edges and stick 'em on the list.
                   */
                  MetaEdge *new_edge  = g_new (MetaEdge, 1);

                  new_edge->rect = meta_rect (x, y, width, height);
                  new_edge->side_type = side_type;
                  new_edge->edge_type = META_EDGE_XINERAMA;

                  ret = g_list_prepend (ret, new_edge);
                }
            }

          /* Check if cur might be vertically adjacent to compare */
          if (meta_rectangle_horiz_overlap(cur_rect, compare_rect))
            {
              MetaSide side_type;
              int x      = MAX (cur_rect->x, compare_rect->x);
              int width  = MIN (BOX_RIGHT (*cur_rect) - x,
                                BOX_RIGHT (*compare_rect) - x);
              int height = 0;
              int y;

              if (BOX_TOP (*cur_rect)  == BOX_BOTTOM (*compare_rect))
                {
                  /* compare_rect is to the top of cur_rect */
                  y = BOX_TOP (*cur_rect);
                  side_type = META_SIDE_TOP;
                }
              else if (BOX_BOTTOM (*cur_rect) == BOX_TOP (*compare_rect))
                {
                  /* compare_rect is to the bottom of cur_rect */
                  y = BOX_BOTTOM (*cur_rect);
                  side_type = META_SIDE_BOTTOM;
                }
              else
                /* These rectangles aren't adjacent after all */
                y = INT_MIN;

              /* If the rectangles really are adjacent */
              if (y != INT_MIN)
                {
                  /* We need a top edge for the xinerama on the bottom, and
                   * a bottom edge for the xinerama on the top.  Just fill
                   * up the edges and stick 'em on the list.
                   */
                  MetaEdge *new_edge = g_new (MetaEdge, 1);

                  new_edge->rect = meta_rect (x, y, width, height);
                  new_edge->side_type = side_type;
                  new_edge->edge_type = META_EDGE_XINERAMA;

                  ret = g_list_prepend (ret, new_edge);
                }
            }

          compare = compare->next;
        }
      cur = cur->next;
    }

  temp_rects = NULL;
  for (; all_struts; all_struts = all_struts->next)
    temp_rects = g_slist_prepend (temp_rects,
                                  &((MetaStrut*)all_struts->data)->rect);
  ret = meta_rectangle_remove_intersections_with_boxes_from_edges (ret, 
                                                                   temp_rects);
  g_slist_free (temp_rects);

  /* Sort the list */
  ret = g_list_sort (ret, meta_rectangle_edge_cmp);

  return ret;
}
