/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>

#include <glib.h>

#include "cogl.h"
#include "cogl-clip-stack.h"
#include "cogl-primitives.h"
#include "cogl-context.h"
#include "cogl-internal.h"
#include "cogl-framebuffer-private.h"
#include "cogl-journal-private.h"
#include "cogl-util.h"
#include "cogl-path-private.h"

typedef struct _CoglClipStack CoglClipStack;
typedef struct _CoglClipStackEntry CoglClipStackEntry;
typedef struct _CoglClipStackEntryRect CoglClipStackEntryRect;
typedef struct _CoglClipStackEntryWindowRect CoglClipStackEntryWindowRect;
typedef struct _CoglClipStackEntryPath CoglClipStackEntryPath;

typedef enum
  {
    COGL_CLIP_STACK_RECT,
    COGL_CLIP_STACK_WINDOW_RECT,
    COGL_CLIP_STACK_PATH
  } CoglClipStackEntryType;

/* A clip stack consists a list of entries. Each entry has a reference
 * count and a link to its parent node. The child takes a reference on
 * the parent and the CoglClipStack holds a reference to the top of
 * the stack. There are no links back from the parent to the
 * children. This allows stacks that have common ancestry to share the
 * entries.
 *
 * For example, the following sequence of operations would generate
 * the tree below:
 *
 * CoglHandle stack_a = _cogl_clip_stack_new ();
 * _cogl_set_clip_stack (stack_a);
 * cogl_clip_stack_push_rectangle (...);
 * cogl_clip_stack_push_rectangle (...);
 * CoglHandle stack_b = _cogl_clip_stack_copy (stack_a);
 * cogl_clip_stack_push_from_path ();
 * cogl_set_clip_stack (stack_b);
 * cogl_clip_stack_push_window_rectangle (...);
 *
 *  stack_a
 *         \ holds a ref to
 *          +-----------+
 *          | path node |
 *          |ref count 1|
 *          +-----------+
 *                       \
 *                        +-----------+  +-----------+
 *       both tops hold   | rect node |  | rect node |
 *       a ref to the     |ref count 2|--|ref count 1|
 *       same rect node   +-----------+  +-----------+
 *                       /
 *          +-----------+
 *          | win. rect |
 *          |ref count 1|
 *          +-----------+
 *         / holds a ref to
 *  stack_b
 *
 */

struct _CoglClipStack
{
  CoglHandleObject _parent;

  CoglClipStackEntry *stack_top;
};

struct _CoglClipStackEntry
{
  CoglClipStackEntryType  type;

  /* This will be null if there is no parent. If it is not null then
     this node must be holding a reference to the parent */
  CoglClipStackEntry     *parent;

  unsigned int            ref_count;
};

struct _CoglClipStackEntryRect
{
  CoglClipStackEntry     _parent_data;

  /* The rectangle for this clip */
  float                  x0;
  float                  y0;
  float                  x1;
  float                  y1;

  /* The matrix that was current when the clip was set */
  CoglMatrix             matrix;
};

struct _CoglClipStackEntryWindowRect
{
  CoglClipStackEntry     _parent_data;

  /* The window space rectangle for this clip. This is stored in
     Cogl's coordinate space (ie, 0,0 is the top left) */
  int                    x0;
  int                    y0;
  int                    x1;
  int                    y1;
};

struct _CoglClipStackEntryPath
{
  CoglClipStackEntry     _parent_data;

  /* The matrix that was current when the clip was set */
  CoglMatrix             matrix;

  CoglHandle             path;
};

static void _cogl_clip_stack_free (CoglClipStack *stack);

COGL_HANDLE_DEFINE (ClipStack, clip_stack);

#define COGL_CLIP_STACK(stack) ((CoglClipStack *) (stack))

static void
project_vertex (const CoglMatrix *modelview_matrix,
		const CoglMatrix *projection_matrix,
		float *vertex)
{
  int i;

  /* Apply the modelview matrix */
  cogl_matrix_transform_point (modelview_matrix,
                               &vertex[0], &vertex[1],
                               &vertex[2], &vertex[3]);
  /* Apply the projection matrix */
  cogl_matrix_transform_point (projection_matrix,
                               &vertex[0], &vertex[1],
                               &vertex[2], &vertex[3]);
  /* Convert from homogenized coordinates */
  for (i = 0; i < 4; i++)
    vertex[i] /= vertex[3];
}

static void
set_clip_plane (GLint plane_num,
		const float *vertex_a,
		const float *vertex_b)
{
#if defined (HAVE_COGL_GLES2) || defined (HAVE_COGL_GLES)
  GLfloat plane[4];
#else
  GLdouble plane[4];
#endif
  GLfloat angle;
  CoglHandle framebuffer = _cogl_get_framebuffer ();
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglMatrix inverse_projection;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_matrix_stack_get_inverse (projection_stack, &inverse_projection);

  /* Calculate the angle between the axes and the line crossing the
     two points */
  angle = atan2f (vertex_b[1] - vertex_a[1],
                  vertex_b[0] - vertex_a[0]) * (180.0/G_PI);

  _cogl_matrix_stack_push (modelview_stack);

  /* Load the inverse of the projection matrix so we can specify the plane
   * in screen coordinates */
  _cogl_matrix_stack_set (modelview_stack, &inverse_projection);

  /* Rotate about point a */
  _cogl_matrix_stack_translate (modelview_stack,
                                vertex_a[0], vertex_a[1], vertex_a[2]);
  /* Rotate the plane by the calculated angle so that it will connect
     the two points */
  _cogl_matrix_stack_rotate (modelview_stack, angle, 0.0f, 0.0f, 1.0f);
  _cogl_matrix_stack_translate (modelview_stack,
                                -vertex_a[0], -vertex_a[1], -vertex_a[2]);

  _cogl_matrix_stack_flush_to_gl (modelview_stack, COGL_MATRIX_MODELVIEW);

  plane[0] = 0;
  plane[1] = -1.0;
  plane[2] = 0;
  plane[3] = vertex_a[1];
#if defined (HAVE_COGL_GLES2) || defined (HAVE_COGL_GLES)
  GE( glClipPlanef (plane_num, plane) );
#else
  GE( glClipPlane (plane_num, plane) );
#endif

  _cogl_matrix_stack_pop (modelview_stack);
}

static void
set_clip_planes (float x_1,
		 float y_1,
		 float x_2,
		 float y_2)
{
  CoglHandle framebuffer = _cogl_get_framebuffer ();
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  CoglMatrix modelview_matrix;
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglMatrix projection_matrix;
  float signed_area;

  float vertex_tl[4] = { x_1, y_1, 0, 1.0 };
  float vertex_tr[4] = { x_2, y_1, 0, 1.0 };
  float vertex_bl[4] = { x_1, y_2, 0, 1.0 };
  float vertex_br[4] = { x_2, y_2, 0, 1.0 };

  _cogl_matrix_stack_get (projection_stack, &projection_matrix);
  _cogl_matrix_stack_get (modelview_stack, &modelview_matrix);

  project_vertex (&modelview_matrix, &projection_matrix, vertex_tl);
  project_vertex (&modelview_matrix, &projection_matrix, vertex_tr);
  project_vertex (&modelview_matrix, &projection_matrix, vertex_bl);
  project_vertex (&modelview_matrix, &projection_matrix, vertex_br);

  /* Calculate the signed area of the polygon formed by the four
     vertices so that we can know its orientation */
  signed_area = (vertex_tl[0] * (vertex_tr[1] - vertex_bl[1])
                 + vertex_tr[0] * (vertex_br[1] - vertex_tl[1])
                 + vertex_br[0] * (vertex_bl[1] - vertex_tr[1])
                 + vertex_bl[0] * (vertex_tl[1] - vertex_br[1]));

  /* Set the clip planes to form lines between all of the vertices
     using the same orientation as we calculated */
  if (signed_area > 0.0f)
    {
      /* counter-clockwise */
      set_clip_plane (GL_CLIP_PLANE0, vertex_tl, vertex_bl);
      set_clip_plane (GL_CLIP_PLANE1, vertex_bl, vertex_br);
      set_clip_plane (GL_CLIP_PLANE2, vertex_br, vertex_tr);
      set_clip_plane (GL_CLIP_PLANE3, vertex_tr, vertex_tl);
    }
  else
    {
      /* clockwise */
      set_clip_plane (GL_CLIP_PLANE0, vertex_tl, vertex_tr);
      set_clip_plane (GL_CLIP_PLANE1, vertex_tr, vertex_br);
      set_clip_plane (GL_CLIP_PLANE2, vertex_br, vertex_bl);
      set_clip_plane (GL_CLIP_PLANE3, vertex_bl, vertex_tl);
    }
}

void
add_stencil_clip_rectangle (float x_1,
                            float y_1,
                            float x_2,
                            float y_2,
                            gboolean first)
{
  CoglHandle current_source;
  CoglHandle framebuffer = _cogl_get_framebuffer ();

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't log changes to the stencil buffer so need to flush any
   * batched geometry before we start... */
  _cogl_journal_flush ();

  _cogl_framebuffer_flush_state (framebuffer, 0);

  /* temporarily swap in our special stenciling material */
  current_source = cogl_handle_ref (ctx->source_material);
  cogl_set_source (ctx->stencil_material);

  if (first)
    {
      GE( glEnable (GL_STENCIL_TEST) );

      /* Initially disallow everything */
      GE( glClearStencil (0) );
      GE( glClear (GL_STENCIL_BUFFER_BIT) );

      /* Punch out a hole to allow the rectangle */
      GE( glStencilFunc (GL_NEVER, 0x1, 0x1) );
      GE( glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE) );

      cogl_rectangle (x_1, y_1, x_2, y_2);
    }
  else
    {
      CoglMatrixStack *modelview_stack =
        _cogl_framebuffer_get_modelview_stack (framebuffer);
      CoglMatrixStack *projection_stack =
        _cogl_framebuffer_get_projection_stack (framebuffer);

      /* Add one to every pixel of the stencil buffer in the
	 rectangle */
      GE( glStencilFunc (GL_NEVER, 0x1, 0x3) );
      GE( glStencilOp (GL_INCR, GL_INCR, GL_INCR) );
      cogl_rectangle (x_1, y_1, x_2, y_2);

      /* make sure our rectangle hits the stencil buffer before we
       * change the stencil operation */
      _cogl_journal_flush ();

      /* Subtract one from all pixels in the stencil buffer so that
	 only pixels where both the original stencil buffer and the
	 rectangle are set will be valid */
      GE( glStencilOp (GL_DECR, GL_DECR, GL_DECR) );

      _cogl_matrix_stack_push (projection_stack);
      _cogl_matrix_stack_load_identity (projection_stack);

      _cogl_matrix_stack_push (modelview_stack);
      _cogl_matrix_stack_load_identity (modelview_stack);

      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);

      _cogl_matrix_stack_pop (modelview_stack);
      _cogl_matrix_stack_pop (projection_stack);
    }

  /* make sure our rectangles hit the stencil buffer before we restore
   * the stencil function / operation */
  _cogl_journal_flush ();

  /* Restore the stencil mode */
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );

  /* restore the original source material */
  cogl_set_source (current_source);
  cogl_handle_unref (current_source);
}

static void
disable_stencil_buffer (void)
{
  GE( glDisable (GL_STENCIL_TEST) );
}

static void
enable_clip_planes (void)
{
  GE( glEnable (GL_CLIP_PLANE0) );
  GE( glEnable (GL_CLIP_PLANE1) );
  GE( glEnable (GL_CLIP_PLANE2) );
  GE( glEnable (GL_CLIP_PLANE3) );
}

static void
disable_clip_planes (void)
{
  GE( glDisable (GL_CLIP_PLANE3) );
  GE( glDisable (GL_CLIP_PLANE2) );
  GE( glDisable (GL_CLIP_PLANE1) );
  GE( glDisable (GL_CLIP_PLANE0) );
}

static gpointer
_cogl_clip_stack_push_entry (CoglClipStack *clip_stack,
                             size_t size,
                             CoglClipStackEntryType type)
{
  CoglClipStackEntry *entry = g_slice_alloc (size);

  /* The new entry starts with a ref count of 1 because the stack
     holds a reference to it as it is the top entry */
  entry->ref_count = 1;
  entry->type = type;
  entry->parent = clip_stack->stack_top;
  clip_stack->stack_top = entry;

  /* We don't need to take a reference to the parent from the entry
     because the clip_stack would have had to reference the top of
     the stack and we can just steal that */

  return entry;
}

void
_cogl_clip_stack_push_window_rectangle (CoglHandle handle,
                                        int x_offset,
                                        int y_offset,
                                        int width,
                                        int height)
{
  CoglClipStack *stack = COGL_CLIP_STACK (handle);
  CoglClipStackEntryWindowRect *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  entry = _cogl_clip_stack_push_entry (stack,
                                       sizeof (CoglClipStackEntryWindowRect),
                                       COGL_CLIP_STACK_WINDOW_RECT);

  entry->x0 = x_offset;
  entry->x1 = x_offset + width;
  entry->y0 = y_offset;
  entry->y1 = y_offset + height;
}

void
_cogl_clip_stack_push_rectangle (CoglHandle handle,
                                 float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2,
                                 const CoglMatrix *modelview_matrix)
{
  CoglClipStack *stack = COGL_CLIP_STACK (handle);
  CoglClipStackEntryRect *entry;

  /* Make a new entry */
  entry = _cogl_clip_stack_push_entry (stack,
                                       sizeof (CoglClipStackEntryRect),
                                       COGL_CLIP_STACK_RECT);

  entry->x0 = x_1;
  entry->y0 = y_1;
  entry->x1 = x_2;
  entry->y1 = y_2;

  entry->matrix = *modelview_matrix;
}

void
_cogl_clip_stack_push_from_path (CoglHandle handle,
                                 CoglHandle path,
                                 const CoglMatrix *modelview_matrix)
{
  CoglClipStack *stack = COGL_CLIP_STACK (handle);
  CoglClipStackEntryPath *entry;

  entry = _cogl_clip_stack_push_entry (stack,
                                       sizeof (CoglClipStackEntryPath),
                                       COGL_CLIP_STACK_PATH);

  entry->path = cogl_path_copy (path);

  entry->matrix = *modelview_matrix;
}

static void
_cogl_clip_stack_entry_unref (CoglClipStackEntry *entry)
{
  /* Unref all of the entries until we hit the root of the list or the
     entry still has a remaining reference */
  while (entry && --entry->ref_count <= 0)
    {
      CoglClipStackEntry *parent = entry->parent;

      switch (entry->type)
        {
        case COGL_CLIP_STACK_RECT:
          g_slice_free1 (sizeof (CoglClipStackEntryRect), entry);
          break;

        case COGL_CLIP_STACK_WINDOW_RECT:
          g_slice_free1 (sizeof (CoglClipStackEntryWindowRect), entry);
          break;

        case COGL_CLIP_STACK_PATH:
          cogl_handle_unref (((CoglClipStackEntryPath *) entry)->path);
          g_slice_free1 (sizeof (CoglClipStackEntryPath), entry);
          break;

        default:
          g_assert_not_reached ();
        }

      entry = parent;
    }
}

void
_cogl_clip_stack_pop (CoglHandle handle)
{
  CoglClipStack *stack = COGL_CLIP_STACK (handle);
  CoglClipStackEntry *entry;

  g_return_if_fail (stack->stack_top != NULL);

  /* To pop we are moving the top of the stack to the old top's parent
     node. The stack always needs to have a reference to the top entry
     so we must take a reference to the new top. The stack would have
     previously had a reference to the old top so we need to decrease
     the ref count on that. We need to ref the new head first in case
     this stack was the only thing referencing the old top. In that
     case the call to _cogl_clip_stack_entry_unref will unref the
     parent. */
  entry = stack->stack_top;
  stack->stack_top = entry->parent;
  if (stack->stack_top)
    stack->stack_top->ref_count++;
  _cogl_clip_stack_entry_unref (entry);
}

void
_cogl_clip_stack_flush (CoglHandle handle,
                        gboolean *stencil_used_p)
{
  CoglClipStack *stack = COGL_CLIP_STACK (handle);
  int has_clip_planes;
  gboolean using_clip_planes = FALSE;
  gboolean using_stencil_buffer = FALSE;
  int scissor_x0 = 0;
  int scissor_y0 = 0;
  int scissor_x1 = G_MAXINT;
  int scissor_y1 = G_MAXINT;
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (_cogl_get_framebuffer ());
  CoglClipStackEntry *entry;

  has_clip_planes = cogl_features_available (COGL_FEATURE_FOUR_CLIP_PLANES);

  disable_clip_planes ();
  disable_stencil_buffer ();
  GE (glDisable (GL_SCISSOR_TEST));

  /* If the stack is empty then there's nothing else to do */
  if (stack->stack_top == NULL)
    return;

  /* Add all of the entries. This will end up adding them in the
     reverse order that they were specified but as all of the clips
     are intersecting it should work out the same regardless of the
     order */
  for (entry = stack->stack_top; entry; entry = entry->parent)
    {
      if (entry->type == COGL_CLIP_STACK_PATH)
        {
          CoglClipStackEntryPath *path_entry = (CoglClipStackEntryPath *) entry;

          _cogl_matrix_stack_push (modelview_stack);
          _cogl_matrix_stack_set (modelview_stack, &path_entry->matrix);

          _cogl_add_path_to_stencil_buffer (path_entry->path,
                                            using_stencil_buffer,
                                            TRUE);

          _cogl_matrix_stack_pop (modelview_stack);

          using_stencil_buffer = TRUE;
        }
      else if (entry->type == COGL_CLIP_STACK_RECT)
        {
          CoglClipStackEntryRect *rect = (CoglClipStackEntryRect *) entry;

          _cogl_matrix_stack_push (modelview_stack);
          _cogl_matrix_stack_set (modelview_stack, &rect->matrix);

          /* If we support clip planes and we haven't already used
             them then use that instead */
          if (has_clip_planes)
            {
              set_clip_planes (rect->x0,
                               rect->y0,
                               rect->x1,
                               rect->y1);
              using_clip_planes = TRUE;
              /* We can't use clip planes a second time */
              has_clip_planes = FALSE;
            }
          else
            {
              add_stencil_clip_rectangle (rect->x0,
                                          rect->y0,
                                          rect->x1,
                                          rect->y1,
                                          !using_stencil_buffer);
              using_stencil_buffer = TRUE;
            }

          _cogl_matrix_stack_pop (modelview_stack);
        }
      else
        {
          /* Get the intersection of all window space rectangles in the clip
           * stack */
          CoglClipStackEntryWindowRect *window_rect =
            (CoglClipStackEntryWindowRect *) entry;
          scissor_x0 = MAX (scissor_x0, window_rect->x0);
          scissor_y0 = MAX (scissor_y0, window_rect->y0);
          scissor_x1 = MIN (scissor_x1, window_rect->x1);
          scissor_y1 = MIN (scissor_y1, window_rect->y1);
        }
    }

  /* Enabling clip planes is delayed to now so that they won't affect
     setting up the stencil buffer */
  if (using_clip_planes)
    enable_clip_planes ();

  if (!(scissor_x0 == 0 && scissor_y0 == 0 &&
        scissor_x1 == G_MAXINT && scissor_y1 == G_MAXINT))
    {
      int scissor_y_start;

      if (scissor_x0 >= scissor_x1 || scissor_y0 >= scissor_y1)
        scissor_x0 = scissor_y0 = scissor_x1 = scissor_y1 = scissor_y_start = 0;
      else
        {
          CoglHandle framebuffer = _cogl_get_framebuffer ();

          /* We store the entry coordinates in Cogl coordinate space
           * but OpenGL requires the window origin to be the bottom
           * left so we may need to convert the incoming coordinates.
           *
           * NB: Cogl forces all offscreen rendering to be done upside
           * down so in this case no conversion is needed.
           */

          if (cogl_is_offscreen (framebuffer))
            scissor_y_start = scissor_y0;
          else
            {
              int framebuffer_height =
                _cogl_framebuffer_get_height (framebuffer);

              scissor_y_start = framebuffer_height - scissor_y1;
            }
        }

      GE (glEnable (GL_SCISSOR_TEST));
      GE (glScissor (scissor_x0, scissor_y_start,
                     scissor_x1 - scissor_x0,
                     scissor_y1 - scissor_y0));
    }

  *stencil_used_p = using_stencil_buffer;
}

CoglHandle
_cogl_clip_stack_new (void)
{
  CoglClipStack *stack;

  stack = g_slice_new (CoglClipStack);
  stack->stack_top = NULL;

  return _cogl_clip_stack_handle_new (stack);
}

void
_cogl_clip_stack_free (CoglClipStack *stack)
{
  /* We only need to unref the top node and this
     should end up freeing all of the parents if need be */
  if (stack->stack_top)
    _cogl_clip_stack_entry_unref (stack->stack_top);

  g_slice_free (CoglClipStack, stack);
}

CoglHandle
_cogl_clip_stack_copy (CoglHandle handle)
{
  CoglHandle new_handle;
  CoglClipStack *new_stack, *old_stack;

  if (!cogl_is_clip_stack (handle))
    return COGL_INVALID_HANDLE;

  old_stack = COGL_CLIP_STACK (handle);

  new_handle = _cogl_clip_stack_new ();
  new_stack = COGL_CLIP_STACK (new_handle);

  /* We can copy the stack by just referencing the other stack's
     data. There's no need to implement copy-on-write because the
     entries of the stack can't be modified. If the other stack pops
     some entries off they will still be kept alive because this stack
     holds a reference. */
  new_stack->stack_top = old_stack->stack_top;
  if (new_stack->stack_top)
    new_stack->stack_top->ref_count++;

  return new_handle;
}
