/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

/**
 * SECTION:clutter-util
 * @short_description: Utility functions
 *
 * Various miscellaneous utilility functions.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>

#include "clutter-main.h"
#include "clutter-interval.h"
#include "clutter-private.h"

#include "deprecated/clutter-util.h"

/**
 * clutter_util_next_p2:
 * @a: Value to get the next power
 *
 * Calculates the nearest power of two, greater than or equal to @a.
 *
 * Return value: The nearest power of two, greater or equal to @a.
 *
 * Deprecated: 1.2
 */
gint
clutter_util_next_p2 (gint a)
{
  int rval = 1;

  while (rval < a)
    rval <<= 1;

  return rval;
}

/*< private >
 * _clutter_gettext:
 * @str: a string to localize
 *
 * Retrieves the localized version of @str, using the Clutter domain
 *
 * Return value: the translated string
 */
const gchar *
_clutter_gettext (const gchar *str)
{
  return g_dgettext (GETTEXT_PACKAGE, str);
}

/* Help macros to scale from OpenGL <-1,1> coordinates system to
 * window coordinates ranging [0,window-size]
 */
#define MTX_GL_SCALE_X(x,w,v1,v2) ((((((x) / (w)) + 1.0f) / 2.0f) * (v1)) + (v2))
#define MTX_GL_SCALE_Y(y,w,v1,v2) ((v1) - (((((y) / (w)) + 1.0f) / 2.0f) * (v1)) + (v2))
#define MTX_GL_SCALE_Z(z,w,v1,v2) (MTX_GL_SCALE_X ((z), (w), (v1), (v2)))

typedef struct _ClutterVertex4
{
  float x;
  float y;
  float z;
  float w;
} ClutterVertex4;

void
_clutter_util_fully_transform_vertices (const CoglMatrix *modelview,
                                        const CoglMatrix *projection,
                                        const float *viewport,
                                        const ClutterVertex *vertices_in,
                                        ClutterVertex *vertices_out,
                                        int n_vertices)
{
  CoglMatrix modelview_projection;
  ClutterVertex4 *vertices_tmp;
  int i;

  vertices_tmp = g_alloca (sizeof (ClutterVertex4) * n_vertices);

  if (n_vertices >= 4)
    {
      /* XXX: we should find a way to cache this per actor */
      cogl_matrix_multiply (&modelview_projection,
                            projection,
                            modelview);
      cogl_matrix_project_points (&modelview_projection,
                                  3,
                                  sizeof (ClutterVertex),
                                  vertices_in,
                                  sizeof (ClutterVertex4),
                                  vertices_tmp,
                                  n_vertices);
    }
  else
    {
      cogl_matrix_transform_points (modelview,
                                    3,
                                    sizeof (ClutterVertex),
                                    vertices_in,
                                    sizeof (ClutterVertex4),
                                    vertices_tmp,
                                    n_vertices);

      cogl_matrix_project_points (projection,
                                  3,
                                  sizeof (ClutterVertex4),
                                  vertices_tmp,
                                  sizeof (ClutterVertex4),
                                  vertices_tmp,
                                  n_vertices);
    }

  for (i = 0; i < n_vertices; i++)
    {
      ClutterVertex4 vertex_tmp = vertices_tmp[i];
      ClutterVertex *vertex_out = &vertices_out[i];
      /* Finally translate from OpenGL coords to window coords */
      vertex_out->x = MTX_GL_SCALE_X (vertex_tmp.x, vertex_tmp.w,
                                      viewport[2], viewport[0]);
      vertex_out->y = MTX_GL_SCALE_Y (vertex_tmp.y, vertex_tmp.w,
                                      viewport[3], viewport[1]);
    }
}

/*< private >
 * _clutter_util_rectangle_union:
 * @src1: first rectangle to union
 * @src2: second rectangle to union
 * @dest: (out): return location for the unioned rectangle
 *
 * Calculates the union of two rectangles.
 *
 * The union of rectangles @src1 and @src2 is the smallest rectangle which
 * includes both @src1 and @src2 within it.
 *
 * It is allowed for @dest to be the same as either @src1 or @src2.
 *
 * This function should really be in Cairo.
 */
void
_clutter_util_rectangle_union (const cairo_rectangle_int_t *src1,
                               const cairo_rectangle_int_t *src2,
                               cairo_rectangle_int_t       *dest)
{
  int dest_x, dest_y;

  dest_x = MIN (src1->x, src2->x);
  dest_y = MIN (src1->y, src2->y);

  dest->width = MAX (src1->x + src1->width, src2->x + src2->width) - dest_x;
  dest->height = MAX (src1->y + src1->height, src2->y + src2->height) - dest_y;
  dest->x = dest_x;
  dest->y = dest_y;
}

typedef struct
{
  GType value_type;
  ClutterProgressFunc func;
} ProgressData;

G_LOCK_DEFINE_STATIC (progress_funcs);
static GHashTable *progress_funcs = NULL;

gboolean
_clutter_has_progress_function (GType gtype)
{
  const char *type_name = g_type_name (gtype);

  if (progress_funcs == NULL)
    return FALSE;

  return g_hash_table_lookup (progress_funcs, type_name) != NULL;
}

gboolean
_clutter_run_progress_function (GType gtype,
                                const GValue *initial,
                                const GValue *final,
                                gdouble progress,
                                GValue *retval)
{
  ProgressData *pdata;
  gboolean res;

  G_LOCK (progress_funcs);

  if (G_UNLIKELY (progress_funcs == NULL))
    {
      res = FALSE;
      goto out;
    }

  pdata = g_hash_table_lookup (progress_funcs, g_type_name (gtype));
  if (G_UNLIKELY (pdata == NULL))
    {
      res = FALSE;
      goto out;
    }

  res = pdata->func (initial, final, progress, retval);

out:
  G_UNLOCK (progress_funcs);

  return res;
}

static void
progress_data_destroy (gpointer data_)
{
  g_slice_free (ProgressData, data_);
}

/**
 * clutter_interval_register_progress_func: (skip)
 * @value_type: a #GType
 * @func: a #ClutterProgressFunc, or %NULL to unset a previously
 *   set progress function
 *
 * Sets the progress function for a given @value_type, like:
 *
 * |[
 *   clutter_interval_register_progress_func (MY_TYPE_FOO,
 *                                            my_foo_progress);
 * ]|
 *
 * Whenever a #ClutterInterval instance using the default
 * #ClutterInterval::compute_value implementation is set as an
 * interval between two #GValue of type @value_type, it will call
 * @func to establish the value depending on the given progress,
 * for instance:
 *
 * |[
 *   static gboolean
 *   my_int_progress (const GValue *a,
 *                    const GValue *b,
 *                    gdouble       progress,
 *                    GValue       *retval)
 *   {
 *     gint ia = g_value_get_int (a);
 *     gint ib = g_value_get_int (b);
 *     gint res = factor * (ib - ia) + ia;
 *
 *     g_value_set_int (retval, res);
 *
 *     return TRUE;
 *   }
 *
 *   clutter_interval_register_progress_func (G_TYPE_INT, my_int_progress);
 * ]|
 *
 * To unset a previously set progress function of a #GType, pass %NULL
 * for @func.
 *
 * Since: 1.0
 */
void
clutter_interval_register_progress_func (GType               value_type,
                                         ClutterProgressFunc func)
{
  ProgressData *progress_func;
  const char *type_name;

  g_return_if_fail (value_type != G_TYPE_INVALID);

  type_name = g_type_name (value_type);

  G_LOCK (progress_funcs);

  if (G_UNLIKELY (progress_funcs == NULL))
    progress_funcs = g_hash_table_new_full (NULL, NULL,
                                            NULL,
                                            progress_data_destroy);

  progress_func =
    g_hash_table_lookup (progress_funcs, type_name);

  if (G_UNLIKELY (progress_func))
    {
      if (func == NULL)
        {
          g_hash_table_remove (progress_funcs, type_name);
          g_slice_free (ProgressData, progress_func);
        }
      else
        progress_func->func = func;
    }
  else
    {
      progress_func = g_slice_new (ProgressData);
      progress_func->value_type = value_type;
      progress_func->func = func;

      g_hash_table_replace (progress_funcs,
                            (gpointer) type_name,
                            progress_func);
    }

  G_UNLOCK (progress_funcs);
}
