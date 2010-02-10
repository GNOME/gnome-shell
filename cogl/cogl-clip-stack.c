/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

void _cogl_add_path_to_stencil_buffer (floatVec2    nodes_min,
                                       floatVec2    nodes_max,
                                       unsigned int path_size,
                                       CoglPathNode *path,
                                       gboolean      merge,
                                       gboolean      need_clear);

typedef struct _CoglClipStack CoglClipStack;

typedef struct _CoglClipStackEntryRect CoglClipStackEntryRect;
typedef struct _CoglClipStackEntryWindowRect CoglClipStackEntryWindowRect;
typedef struct _CoglClipStackEntryPath CoglClipStackEntryPath;

typedef enum
  {
    COGL_CLIP_STACK_RECT,
    COGL_CLIP_STACK_WINDOW_RECT,
    COGL_CLIP_STACK_PATH
  } CoglClipStackEntryType;

struct _CoglClipStack
{
  GList *stack_top;
};

struct _CoglClipStackEntryRect
{
  CoglClipStackEntryType type;

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
  CoglClipStackEntryType type;

  /* The window space rectangle for this clip */
  float                  x0;
  float                  y0;
  float                  x1;
  float                  y1;
};

struct _CoglClipStackEntryPath
{
  CoglClipStackEntryType type;

  /* The matrix that was current when the clip was set */
  CoglMatrix             matrix;

  floatVec2              path_nodes_min;
  floatVec2              path_nodes_max;

  unsigned int           path_size;
  CoglPathNode           path[1];
};

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

  /* If the order of the top and bottom lines is different from the
     order of the left and right lines then the clip rect must have
     been transformed so that the back is visible. We therefore need
     to swap one pair of vertices otherwise all of the planes will be
     the wrong way around */
  if ((vertex_tl[0] < vertex_tr[0] ? 1 : 0)
      != (vertex_bl[1] < vertex_tl[1] ? 1 : 0))
    {
      float temp[4];
      memcpy (temp, vertex_tl, sizeof (temp));
      memcpy (vertex_tl, vertex_tr, sizeof (temp));
      memcpy (vertex_tr, temp, sizeof (temp));
      memcpy (temp, vertex_bl, sizeof (temp));
      memcpy (vertex_bl, vertex_br, sizeof (temp));
      memcpy (vertex_br, temp, sizeof (temp));
    }

  set_clip_plane (GL_CLIP_PLANE0, vertex_tl, vertex_tr);
  set_clip_plane (GL_CLIP_PLANE1, vertex_tr, vertex_br);
  set_clip_plane (GL_CLIP_PLANE2, vertex_br, vertex_bl);
  set_clip_plane (GL_CLIP_PLANE3, vertex_bl, vertex_tl);
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

void
cogl_clip_push_window_rectangle (int x_offset,
	                         int y_offset,
	                         int width,
	                         int height)
{
  CoglHandle framebuffer;
  CoglClipStackState *clip_state;
  CoglClipStack *stack;
  int framebuffer_height;
  CoglClipStackEntryWindowRect *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  stack = clip_state->stacks->data;

  framebuffer_height = _cogl_framebuffer_get_height (framebuffer);

  entry = g_slice_new (CoglClipStackEntryWindowRect);

  /* We store the entry coordinates in OpenGL window coordinate space and so
   * because Cogl defines the window origin to be top left but OpenGL defines
   * it as bottom left we may need to convert the incoming coordinates.
   *
   * NB: Cogl forces all offscreen rendering to be done upside down so in this
   * case no conversion is needed.
   */
  entry->type = COGL_CLIP_STACK_WINDOW_RECT;
  entry->x0 = x_offset;
  entry->x1 = x_offset + width;
  if (cogl_is_offscreen (framebuffer))
    {
      entry->y0 = y_offset;
      entry->y1 = y_offset + height;
    }
  else
    {
      entry->y0 = framebuffer_height - y_offset - height;
      entry->y1 = framebuffer_height - y_offset;
    }

  /* Store it in the stack */
  stack->stack_top = g_list_prepend (stack->stack_top, entry);

  clip_state->stack_dirty = TRUE;
}

/* XXX: This is deprecated API */
void
cogl_clip_push_window_rect (float x_offset,
	                    float y_offset,
	                    float width,
	                    float height)
{
  cogl_clip_push_window_rectangle (x_offset, y_offset, width, height);
}

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width) \
    (  ( ((x) + 1.0) * ((vp_width) / 2.0) ) + (vp_origin_x)  )
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height) \
    (  ( ((-(y)) + 1.0) * ((vp_height) / 2.0) ) + (vp_origin_y)  )

/* Transform a homogeneous vertex position from model space to Cogl
 * window coordinates (with 0,0 being top left) */
static void
transform_point (CoglMatrix *matrix_mv,
                 CoglMatrix *matrix_p,
                 float *viewport,
                 float *x,
                 float *y)
{
  float z = 0;
  float w = 1;

  /* Apply the modelview matrix transform */
  cogl_matrix_transform_point (matrix_mv, x, y, &z, &w);

  /* Apply the projection matrix transform */
  cogl_matrix_transform_point (matrix_p, x, y, &z, &w);

  /* Perform perspective division */
  *x /= w;
  *y /= w;

  /* Apply viewport transform */
  *x = VIEWPORT_TRANSFORM_X (*x, viewport[0], viewport[2]);
  *y = VIEWPORT_TRANSFORM_Y (*y, viewport[1], viewport[3]);
}

#undef VIEWPORT_SCALE_X
#undef VIEWPORT_SCALE_Y

/* Try to push a rectangle given in object coordinates as a rectangle in window
 * coordinates instead of object coordinates */
gboolean
try_pushing_rect_as_window_rect (float x_1,
	                         float y_1,
                                 float x_2,
                                 float y_2)
{
  CoglMatrix matrix;
  CoglMatrix matrix_p;
  float v[4];

  cogl_get_modelview_matrix (&matrix);

  /* If the modelview meets these constraints then a transformed rectangle
   * should still be a rectangle when it reaches screen coordinates.
   *
   * FIXME: we are are making certain assumptions about the projection
   * matrix a.t.m and should really be looking at the combined modelview
   * and projection matrix.
   * FIXME: we don't consider rotations that are a multiple of 90 degrees
   * which could be quite common.
   */
  if (matrix.xy != 0 || matrix.xz != 0 ||
      matrix.yx != 0 || matrix.yz != 0 ||
      matrix.zx != 0 || matrix.zy != 0)
    return FALSE;

  cogl_get_projection_matrix (&matrix_p);
  cogl_get_viewport (v);

  transform_point (&matrix, &matrix_p, v, &x_1, &y_1);
  transform_point (&matrix, &matrix_p, v, &x_2, &y_2);

  /* Consider that the modelview matrix may flip the rectangle
   * along the x or y axis... */
#define SWAP(A,B) do { float tmp = B; B = A; A = tmp; } while (0)
  if (x_1 > x_2)
    SWAP (x_1, x_2);
  if (y_1 > y_2)
    SWAP (y_1, y_2);
#undef SWAP

  cogl_clip_push_window_rectangle (x_1, y_1, x_2 - x_1, y_2 - y_1);
  return TRUE;
}

void
cogl_clip_push_rectangle (float x_1,
                          float y_1,
                          float x_2,
                          float y_2)
{
  CoglHandle framebuffer;
  CoglClipStackState *clip_state;
  CoglClipStack *stack;
  CoglClipStackEntryRect *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  /* Try and catch window space rectangles so we can redirect to
   * cogl_clip_push_window_rect which will use scissoring. */
  if (try_pushing_rect_as_window_rect (x_1, y_1, x_2, y_2))
    return;

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  stack = clip_state->stacks->data;

  entry = g_slice_new (CoglClipStackEntryRect);

  /* Make a new entry */
  entry->type = COGL_CLIP_STACK_RECT;
  entry->x0 = x_1;
  entry->y0 = y_1;
  entry->x1 = x_2;
  entry->y1 = y_2;

  cogl_get_modelview_matrix (&entry->matrix);

  /* Store it in the stack */
  stack->stack_top = g_list_prepend (stack->stack_top, entry);

  clip_state->stack_dirty = TRUE;
}

/* XXX: Deprecated API */
void
cogl_clip_push (float x_offset,
	        float y_offset,
	        float width,
	        float height)
{
  cogl_clip_push_rectangle (x_offset,
                            y_offset,
                            x_offset + width,
                            y_offset + height);
}

void
cogl_clip_push_from_path_preserve (void)
{
  CoglHandle framebuffer;
  CoglClipStackState *clip_state;
  CoglClipStack *stack;
  CoglClipStackEntryPath *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  stack = clip_state->stacks->data;

  entry = g_malloc (sizeof (CoglClipStackEntryPath)
                    + sizeof (CoglPathNode) * (ctx->path_nodes->len - 1));

  entry->type = COGL_CLIP_STACK_PATH;
  entry->path_nodes_min = ctx->path_nodes_min;
  entry->path_nodes_max = ctx->path_nodes_max;
  entry->path_size = ctx->path_nodes->len;
  memcpy (entry->path, ctx->path_nodes->data,
          sizeof (CoglPathNode) * ctx->path_nodes->len);

  cogl_get_modelview_matrix (&entry->matrix);

  /* Store it in the stack */
  stack->stack_top = g_list_prepend (stack->stack_top, entry);

  clip_state->stack_dirty = TRUE;
}

void
cogl_clip_push_from_path (void)
{
  cogl_clip_push_from_path_preserve ();

  cogl_path_new ();
}

static void
_cogl_clip_pop_real (CoglClipStackState *clip_state)
{
  CoglClipStack *stack;
  gpointer entry;
  CoglClipStackEntryType type;

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  stack = clip_state->stacks->data;

  g_return_if_fail (stack->stack_top != NULL);

  entry = stack->stack_top->data;
  type = *(CoglClipStackEntryType *) entry;

  /* Remove the top entry from the stack */
  if (type == COGL_CLIP_STACK_RECT)
    g_slice_free (CoglClipStackEntryRect, entry);
  else if (type == COGL_CLIP_STACK_WINDOW_RECT)
    g_slice_free (CoglClipStackEntryWindowRect, entry);
  else
    g_free (entry);

  stack->stack_top = g_list_delete_link (stack->stack_top,
                                         stack->stack_top);

  clip_state->stack_dirty = TRUE;
}

void
cogl_clip_pop (void)
{
  CoglHandle framebuffer;
  CoglClipStackState *clip_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  _cogl_clip_pop_real (clip_state);
}

void
_cogl_flush_clip_state (CoglClipStackState *clip_state)
{
  CoglClipStack *stack;
  int has_clip_planes;
  gboolean using_clip_planes = FALSE;
  gboolean using_stencil_buffer = FALSE;
  GList *node;
  int scissor_x0 = 0;
  int scissor_y0 = 0;
  int scissor_x1 = G_MAXINT;
  int scissor_y1 = G_MAXINT;
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (_cogl_get_framebuffer ());

  if (!clip_state->stack_dirty)
    return;

  /* The current primitive journal does not support tracking changes to the
   * clip stack...  */
  _cogl_journal_flush ();

  /* XXX: the handling of clipping is quite complex. It may involve use of
   * the Cogl Journal or other Cogl APIs which may end up recursively
   * wanting to ensure the clip state is flushed. We need to ensure we
   * don't recurse infinitely...
   */
  clip_state->stack_dirty = FALSE;

  has_clip_planes = cogl_features_available (COGL_FEATURE_FOUR_CLIP_PLANES);

  stack = clip_state->stacks->data;

  clip_state->stencil_used = FALSE;

  disable_clip_planes ();
  disable_stencil_buffer ();
  GE (glDisable (GL_SCISSOR_TEST));

  /* If the stack is empty then there's nothing else to do */
  if (stack->stack_top == NULL)
    return;

  /* Find the bottom of the stack */
  for (node = stack->stack_top; node->next; node = node->next);

  /* Re-add every entry from the bottom of the stack up */
  for (; node; node = node->prev)
    {
      gpointer entry = node->data;
      CoglClipStackEntryType type = *(CoglClipStackEntryType *) entry;

      if (type == COGL_CLIP_STACK_PATH)
        {
          CoglClipStackEntryPath *path = (CoglClipStackEntryPath *) entry;

          _cogl_matrix_stack_push (modelview_stack);
          _cogl_matrix_stack_set (modelview_stack, &path->matrix);

          _cogl_add_path_to_stencil_buffer (path->path_nodes_min,
                                            path->path_nodes_max,
                                            path->path_size,
                                            path->path,
                                            using_stencil_buffer,
                                            TRUE);

          _cogl_matrix_stack_pop (modelview_stack);

          using_stencil_buffer = TRUE;

          /* We can't use clip planes any more */
          has_clip_planes = FALSE;
        }
      else if (type == COGL_CLIP_STACK_RECT)
        {
          CoglClipStackEntryRect *rect = (CoglClipStackEntryRect *) entry;

          _cogl_matrix_stack_push (modelview_stack);
          _cogl_matrix_stack_set (modelview_stack, &rect->matrix);

          /* If this is the first entry and we support clip planes then use
             that instead */
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
          CoglClipStackEntryWindowRect *window_rect = entry;
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

  if (scissor_x0 >= scissor_x1 || scissor_y0 >= scissor_y1)
    scissor_x0 = scissor_y0 = scissor_x1 = scissor_y1 = 0;

  if (!(scissor_x0 == 0 && scissor_y0 == 0 &&
        scissor_x1 == G_MAXINT && scissor_y1 == G_MAXINT))
    {
      GE (glEnable (GL_SCISSOR_TEST));
      GE (glScissor (scissor_x0, scissor_y0,
                     scissor_x1 - scissor_x0,
                     scissor_y1 - scissor_y0));
    }

  clip_state->stencil_used = using_stencil_buffer;
}

/* XXX: This should never have been made public API! */
void
cogl_clip_ensure (void)
{
  CoglClipStackState *clip_state;

  clip_state = _cogl_framebuffer_get_clip_state (_cogl_get_framebuffer ());
  _cogl_flush_clip_state (clip_state);
}

static void
_cogl_clip_stack_save_real (CoglClipStackState *clip_state)
{
  CoglClipStack *stack;

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  stack = g_slice_new (CoglClipStack);
  stack->stack_top = NULL;

  clip_state->stacks = g_slist_prepend (clip_state->stacks, stack);
  clip_state->stack_dirty = TRUE;
}

void
cogl_clip_stack_save (void)
{
  CoglHandle framebuffer;
  CoglClipStackState *clip_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  _cogl_clip_stack_save_real (clip_state);
}

static void
_cogl_clip_stack_restore_real (CoglClipStackState *clip_state)
{
  CoglClipStack *stack;

  g_return_if_fail (clip_state->stacks != NULL);

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  stack = clip_state->stacks->data;

  /* Empty the current stack */
  while (stack->stack_top)
    _cogl_clip_pop_real (clip_state);

  /* Revert to an old stack */
  g_slice_free (CoglClipStack, stack);
  clip_state->stacks = g_slist_delete_link (clip_state->stacks,
                                            clip_state->stacks);

  clip_state->stack_dirty = TRUE;
}

void
cogl_clip_stack_restore (void)
{
  CoglHandle framebuffer;
  CoglClipStackState *clip_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  _cogl_clip_stack_restore_real (clip_state);
}

void
_cogl_clip_stack_state_init (CoglClipStackState *clip_state)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  clip_state->stacks = NULL;
  clip_state->stack_dirty = TRUE;

  /* Add an intial stack */
  _cogl_clip_stack_save_real (clip_state);
}

void
_cogl_clip_stack_state_destroy (CoglClipStackState *clip_state)
{
  /* Destroy all of the stacks */
  while (clip_state->stacks)
    _cogl_clip_stack_restore_real (clip_state);
}

void
_cogl_clip_stack_state_dirty (CoglClipStackState *clip_state)
{
  clip_state->stack_dirty = TRUE;
}

