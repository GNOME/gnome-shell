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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-display-private.h"
#include "cogl-renderer-private.h"
#include "cogl-object-private.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-clip-stack.h"
#include "cogl-journal-private.h"
#include "cogl-winsys-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-matrix-private.h"

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER		0x8D40
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER		0x8D41
#endif
#ifndef GL_STENCIL_ATTACHMENT
#define GL_STENCIL_ATTACHMENT	0x8D00
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0	0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_STENCIL_INDEX8
#define GL_STENCIL_INDEX8       0x8D48
#endif
#ifndef GL_DEPTH_STENCIL
#define GL_DEPTH_STENCIL        0x84F9
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT     0x8D00
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16    0x81A5
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE      0x8212
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE    0x8213
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE     0x8214
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE    0x8215
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE    0x8216
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE  0x8217
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER               0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#endif
#ifndef GL_TEXTURE_SAMPLES_IMG
#define GL_TEXTURE_SAMPLES_IMG            0x9136
#endif

typedef enum {
  _TRY_DEPTH_STENCIL = 1L<<0,
  _TRY_DEPTH         = 1L<<1,
  _TRY_STENCIL       = 1L<<2
} TryFBOFlags;

typedef struct _CoglFramebufferStackEntry
{
  CoglFramebuffer *draw_buffer;
  CoglFramebuffer *read_buffer;
} CoglFramebufferStackEntry;

extern GQuark _cogl_object_onscreen_get_type (void);

static void _cogl_offscreen_free (CoglOffscreen *offscreen);

COGL_OBJECT_DEFINE (Offscreen, offscreen);
COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING (offscreen);

/* XXX:
 * The CoglObject macros don't support any form of inheritance, so for
 * now we implement the CoglObject support for the CoglFramebuffer
 * abstract class manually.
 */

GQuark
cogl_framebuffer_error_quark (void)
{
  return g_quark_from_static_string ("cogl-framebuffer-error-quark");
}

gboolean
_cogl_is_framebuffer (void *object)
{
  CoglObject *obj = object;

  if (obj == NULL)
    return FALSE;

  return obj->klass->type == _cogl_object_onscreen_get_type ()
    || obj->klass->type == _cogl_object_offscreen_get_type ();
}

void
_cogl_framebuffer_init (CoglFramebuffer *framebuffer,
                        CoglContext *ctx,
                        CoglFramebufferType type,
                        CoglPixelFormat format,
                        int width,
                        int height)
{
  framebuffer->context = cogl_object_ref (ctx);

  framebuffer->type             = type;
  framebuffer->width            = width;
  framebuffer->height           = height;
  framebuffer->format           = format;
  framebuffer->viewport_x       = 0;
  framebuffer->viewport_y       = 0;
  framebuffer->viewport_width   = width;
  framebuffer->viewport_height  = height;
  framebuffer->dither_enabled   = TRUE;

  framebuffer->modelview_stack  = _cogl_matrix_stack_new ();
  framebuffer->projection_stack = _cogl_matrix_stack_new ();

  framebuffer->dirty_bitmasks   = TRUE;

  framebuffer->color_mask       = COGL_COLOR_MASK_ALL;

  framebuffer->samples_per_pixel = 0;

  /* Initialise the clip stack */
  _cogl_clip_state_init (&framebuffer->clip_state);

  framebuffer->journal = _cogl_journal_new ();

  /* Ensure we know the framebuffer->clear_color* members can't be
   * referenced for our fast-path read-pixel optimization (see
   * _cogl_journal_try_read_pixel()) until some region of the
   * framebuffer is initialized.
   */
  framebuffer->clear_clip_dirty = TRUE;

  /* XXX: We have to maintain a central list of all framebuffers
   * because at times we need to be able to flush all known journals.
   *
   * Examples where we need to flush all journals are:
   * - because journal entries can reference OpenGL texture
   *   coordinates that may not survive texture-atlas reorganization
   *   so we need the ability to flush those entries.
   * - because although we generally advise against modifying
   *   pipelines after construction we have to handle that possibility
   *   and since pipelines may be referenced in journal entries we
   *   need to be able to flush them before allowing the pipelines to
   *   be changed.
   *
   * Note we don't maintain a list of journals and associate
   * framebuffers with journals by e.g. having a journal->framebuffer
   * reference since that would introduce a circular reference.
   *
   * Note: As a future change to try and remove the need to index all
   * journals it might be possible to defer resolving of OpenGL
   * texture coordinates for rectangle primitives until we come to
   * flush a journal. This would mean for instance that a single
   * rectangle entry in a journal could later be expanded into
   * multiple quad primitives to handle sliced textures but would mean
   * we don't have to worry about retaining references to OpenGL
   * texture coordinates that may later become invalid.
   */
  ctx->framebuffers = g_list_prepend (ctx->framebuffers, framebuffer);
}

void
_cogl_framebuffer_free (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;

  _cogl_clip_state_destroy (&framebuffer->clip_state);

  cogl_object_unref (framebuffer->modelview_stack);
  framebuffer->modelview_stack = NULL;

  cogl_object_unref (framebuffer->projection_stack);
  framebuffer->projection_stack = NULL;

  cogl_object_unref (framebuffer->journal);

  ctx->framebuffers = g_list_remove (ctx->framebuffers, framebuffer);
  cogl_object_unref (ctx);
}

const CoglWinsysVtable *
_cogl_framebuffer_get_winsys (CoglFramebuffer *framebuffer)
{
  return framebuffer->context->display->renderer->winsys_vtable;
}

/* This version of cogl_clear can be used internally as an alternative
 * to avoid flushing the journal or the framebuffer state. This is
 * needed when doing operations that may be called whiling flushing
 * the journal */
void
_cogl_framebuffer_clear_without_flush4f (CoglFramebuffer *framebuffer,
                                         unsigned long buffers,
                                         float red,
                                         float green,
                                         float blue,
                                         float alpha)
{
  GLbitfield gl_buffers = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (buffers & COGL_BUFFER_BIT_COLOR)
    {
      GE( ctx, glClearColor (red, green, blue, alpha) );
      gl_buffers |= GL_COLOR_BUFFER_BIT;

      if (ctx->current_gl_color_mask != framebuffer->color_mask)
        {
          CoglColorMask color_mask = framebuffer->color_mask;
          GE( ctx, glColorMask (!!(color_mask & COGL_COLOR_MASK_RED),
                                !!(color_mask & COGL_COLOR_MASK_GREEN),
                                !!(color_mask & COGL_COLOR_MASK_BLUE),
                                !!(color_mask & COGL_COLOR_MASK_ALPHA)));
          ctx->current_gl_color_mask = color_mask;
          /* Make sure the ColorMask is updated when the next primitive is drawn */
          ctx->current_pipeline_changes_since_flush |=
            COGL_PIPELINE_STATE_LOGIC_OPS;
          ctx->current_pipeline_age--;
        }
    }

  if (buffers & COGL_BUFFER_BIT_DEPTH)
    gl_buffers |= GL_DEPTH_BUFFER_BIT;

  if (buffers & COGL_BUFFER_BIT_STENCIL)
    gl_buffers |= GL_STENCIL_BUFFER_BIT;

  if (!gl_buffers)
    {
      static gboolean shown = FALSE;

      if (!shown)
        {
	  g_warning ("You should specify at least one auxiliary buffer "
                     "when calling cogl_clear");
        }

      return;
    }

  GE (ctx, glClear (gl_buffers));
}

void
_cogl_framebuffer_dirty (CoglFramebuffer *framebuffer)
{
  framebuffer->clear_clip_dirty = TRUE;
}

void
cogl_framebuffer_clear4f (CoglFramebuffer *framebuffer,
                          unsigned long buffers,
                          float red,
                          float green,
                          float blue,
                          float alpha)
{
  CoglClipStack *clip_stack = _cogl_framebuffer_get_clip_stack (framebuffer);
  int scissor_x0;
  int scissor_y0;
  int scissor_x1;
  int scissor_y1;

  _cogl_clip_stack_get_bounds (clip_stack,
                               &scissor_x0, &scissor_y0,
                               &scissor_x1, &scissor_y1);

  /* NB: the previous clear could have had an arbitrary clip.
   * NB: everything for the last frame might still be in the journal
   *     but we can't assume anything about how each entry was
   *     clipped.
   * NB: Clutter will scissor its pick renders which would mean all
   *     journal entries have a common ClipStack entry, but without
   *     a layering violation Cogl has to explicitly walk the journal
   *     entries to determine if this is the case.
   * NB: We have a software only read-pixel optimization in the
   *     journal that determines the color at a given framebuffer
   *     coordinate for simple scenes without rendering with the GPU.
   *     When Clutter is hitting this fast-path we can expect to
   *     receive calls to clear the framebuffer with an un-flushed
   *     journal.
   * NB: To fully support software based picking for Clutter we
   *     need to be able to reliably detect when the contents of a
   *     journal can be discarded and when we can skip the call to
   *     glClear because it matches the previous clear request.
   */

  /* Note: we don't check for the stencil buffer being cleared here
   * since there isn't any public cogl api to manipulate the stencil
   * buffer.
   *
   * Note: we check for an exact clip match here because
   * 1) a smaller clip could mean existing journal entries may
   *    need to contribute to regions outside the new clear-clip
   * 2) a larger clip would mean we need to issue a real
   *    glClear and we only care about cases avoiding a
   *    glClear.
   *
   * Note: Comparing without an epsilon is considered
   * appropriate here.
   */
  if (buffers & COGL_BUFFER_BIT_COLOR &&
      buffers & COGL_BUFFER_BIT_DEPTH &&
      !framebuffer->clear_clip_dirty &&
      framebuffer->clear_color_red == red &&
      framebuffer->clear_color_green == green &&
      framebuffer->clear_color_blue == blue &&
      framebuffer->clear_color_alpha == alpha &&
      scissor_x0 == framebuffer->clear_clip_x0 &&
      scissor_y0 == framebuffer->clear_clip_y0 &&
      scissor_x1 == framebuffer->clear_clip_x1 &&
      scissor_y1 == framebuffer->clear_clip_y1)
    {
      /* NB: We only have to consider the clip state of journal
       * entries if the current clear is clipped since otherwise we
       * know every pixel of the framebuffer is affected by the clear
       * and so all journal entries become redundant and can simply be
       * discarded.
       */
      if (clip_stack)
        {
          /*
           * Note: the function for checking the journal entries is
           * quite strict. It avoids detailed checking of all entry
           * clip_stacks by only checking the details of the first
           * entry and then it only verifies that the remaining
           * entries share the same clip_stack ancestry. This means
           * it's possible for some false negatives here but that will
           * just result in us falling back to a real clear.
           */
          if (_cogl_journal_all_entries_within_bounds (framebuffer->journal,
                                                       scissor_x0, scissor_y0,
                                                       scissor_x1, scissor_y1))
            {
              _cogl_journal_discard (framebuffer->journal);
              goto cleared;
            }
        }
      else
        {
          _cogl_journal_discard (framebuffer->journal);
          goto cleared;
        }
    }

  COGL_NOTE (DRAW, "Clear begin");

  _cogl_framebuffer_flush_journal (framebuffer);

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the pipeline state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (framebuffer, framebuffer, 0);

  _cogl_framebuffer_clear_without_flush4f (framebuffer, buffers,
                                           red, green, blue, alpha);

  /* This is a debugging variable used to visually display the quad
   * batches from the journal. It is reset here to increase the
   * chances of getting the same colours for each frame during an
   * animation */
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_RECTANGLES)) &&
      buffers & COGL_BUFFER_BIT_COLOR)
    {
      framebuffer->context->journal_rectangles_color = 1;
    }

  COGL_NOTE (DRAW, "Clear end");

cleared:

  if (buffers & COGL_BUFFER_BIT_COLOR && buffers & COGL_BUFFER_BIT_DEPTH)
    {
      /* For our fast-path for reading back a single pixel of simple
       * scenes where the whole frame is in the journal we need to
       * track the cleared color of the framebuffer in case the point
       * read doesn't intersect any of the journal rectangles. */
      framebuffer->clear_clip_dirty = FALSE;
      framebuffer->clear_color_red = red;
      framebuffer->clear_color_green = green;
      framebuffer->clear_color_blue = blue;
      framebuffer->clear_color_alpha = alpha;

      /* NB: A clear may be scissored so we need to track the extents
       * that the clear is applicable too... */
      if (clip_stack)
        {
          _cogl_clip_stack_get_bounds (clip_stack,
                                       &framebuffer->clear_clip_x0,
                                       &framebuffer->clear_clip_y0,
                                       &framebuffer->clear_clip_x1,
                                       &framebuffer->clear_clip_y1);
        }
      else
        {
          /* FIXME: set degenerate clip */
        }
    }
  else
    _cogl_framebuffer_dirty (framebuffer);
}

/* Note: the 'buffers' and 'color' arguments were switched around on
 * purpose compared to the original cogl_clear API since it was odd
 * that you would be expected to specify a color before even
 * necessarily choosing to clear the color buffer.
 */
void
cogl_framebuffer_clear (CoglFramebuffer *framebuffer,
                        unsigned long buffers,
                        const CoglColor *color)
{
  cogl_framebuffer_clear4f (framebuffer, buffers,
                            cogl_color_get_red_float (color),
                            cogl_color_get_green_float (color),
                            cogl_color_get_blue_float (color),
                            cogl_color_get_alpha_float (color));
}

int
cogl_framebuffer_get_width (CoglFramebuffer *framebuffer)
{
  return framebuffer->width;
}

int
cogl_framebuffer_get_height (CoglFramebuffer *framebuffer)
{
  return framebuffer->height;
}

CoglClipState *
_cogl_framebuffer_get_clip_state (CoglFramebuffer *framebuffer)
{
  return &framebuffer->clip_state;
}

CoglClipStack *
_cogl_framebuffer_get_clip_stack (CoglFramebuffer *framebuffer)
{
  CoglClipState *clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  return _cogl_clip_state_get_stack (clip_state);
}

void
_cogl_framebuffer_set_clip_stack (CoglFramebuffer *framebuffer,
                                  CoglClipStack *stack)
{
  CoglClipState *clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  _cogl_clip_state_set_stack (clip_state, stack);
}

void
cogl_framebuffer_set_viewport (CoglFramebuffer *framebuffer,
                               float x,
                               float y,
                               float width,
                               float height)
{
  _COGL_RETURN_IF_FAIL (width > 0 && height > 0);

  if (framebuffer->viewport_x == x &&
      framebuffer->viewport_y == y &&
      framebuffer->viewport_width == width &&
      framebuffer->viewport_height == height)
    return;

  _cogl_framebuffer_flush_journal (framebuffer);

  framebuffer->viewport_x = x;
  framebuffer->viewport_y = y;
  framebuffer->viewport_width = width;
  framebuffer->viewport_height = height;

  if (framebuffer->context && cogl_get_draw_framebuffer () == framebuffer)
    framebuffer->context->dirty_gl_viewport = TRUE;
}

float
cogl_framebuffer_get_viewport_x (CoglFramebuffer *framebuffer)
{
  return framebuffer->viewport_x;
}

float
cogl_framebuffer_get_viewport_y (CoglFramebuffer *framebuffer)
{
  return framebuffer->viewport_y;
}

float
cogl_framebuffer_get_viewport_width (CoglFramebuffer *framebuffer)
{
  return framebuffer->viewport_width;
}

float
cogl_framebuffer_get_viewport_height (CoglFramebuffer *framebuffer)
{
  return framebuffer->viewport_height;
}

void
cogl_framebuffer_get_viewport4fv (CoglFramebuffer *framebuffer,
                                  float *viewport)
{
  viewport[0] = framebuffer->viewport_x;
  viewport[1] = framebuffer->viewport_y;
  viewport[2] = framebuffer->viewport_width;
  viewport[3] = framebuffer->viewport_height;
}

CoglMatrixStack *
_cogl_framebuffer_get_modelview_stack (CoglFramebuffer *framebuffer)
{
  return framebuffer->modelview_stack;
}

CoglMatrixStack *
_cogl_framebuffer_get_projection_stack (CoglFramebuffer *framebuffer)
{
  return framebuffer->projection_stack;
}

void
_cogl_framebuffer_add_dependency (CoglFramebuffer *framebuffer,
                                  CoglFramebuffer *dependency)
{
  GList *l;

  for (l = framebuffer->deps; l; l = l->next)
    {
      CoglFramebuffer *existing_dep = l->data;
      if (existing_dep == dependency)
        return;
    }

  /* TODO: generalize the primed-array type structure we e.g. use for
   * cogl_object_set_user_data or for pipeline children as a way to
   * avoid quite a lot of mid-scene micro allocations here... */
  framebuffer->deps =
    g_list_prepend (framebuffer->deps, cogl_object_ref (dependency));
}

void
_cogl_framebuffer_remove_all_dependencies (CoglFramebuffer *framebuffer)
{
  GList *l;
  for (l = framebuffer->deps; l; l = l->next)
    cogl_object_unref (l->data);
  g_list_free (framebuffer->deps);
  framebuffer->deps = NULL;
}

void
_cogl_framebuffer_flush_journal (CoglFramebuffer *framebuffer)
{
  _cogl_journal_flush (framebuffer->journal, framebuffer);
}

void
_cogl_framebuffer_flush_dependency_journals (CoglFramebuffer *framebuffer)
{
  GList *l;
  for (l = framebuffer->deps; l; l = l->next)
    _cogl_framebuffer_flush_journal (l->data);
  _cogl_framebuffer_remove_all_dependencies (framebuffer);
}

static inline void
_cogl_framebuffer_init_bits (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;

  cogl_framebuffer_allocate (framebuffer, NULL);

  if (G_LIKELY (!framebuffer->dirty_bitmasks))
    return;

#ifdef HAVE_COGL_GL
  if (ctx->driver == COGL_DRIVER_GL &&
      cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN) &&
      framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN)
    {
      GLenum attachment, pname;

      attachment = GL_COLOR_ATTACHMENT0;

      pname = GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE;
      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &framebuffer->red_bits) );

      pname = GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE;
      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &framebuffer->green_bits)
          );

      pname = GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE;
      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &framebuffer->blue_bits)
          );

      pname = GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE;
      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &framebuffer->alpha_bits)
          );
    }
  else
#endif /* HAVE_COGL_GL */
    {
      GE( ctx, glGetIntegerv (GL_RED_BITS,   &framebuffer->red_bits)   );
      GE( ctx, glGetIntegerv (GL_GREEN_BITS, &framebuffer->green_bits) );
      GE( ctx, glGetIntegerv (GL_BLUE_BITS,  &framebuffer->blue_bits)  );
      GE( ctx, glGetIntegerv (GL_ALPHA_BITS, &framebuffer->alpha_bits) );
    }


  COGL_NOTE (OFFSCREEN,
             "RGBA Bits for framebuffer[%p, %s]: %d, %d, %d, %d",
             framebuffer,
             framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN
               ? "offscreen"
               : "onscreen",
             framebuffer->red_bits,
             framebuffer->blue_bits,
             framebuffer->green_bits,
             framebuffer->alpha_bits);

  framebuffer->dirty_bitmasks = FALSE;
}

CoglHandle
_cogl_offscreen_new_to_texture_full (CoglTexture *texture,
                                     CoglOffscreenFlags create_flags,
                                     unsigned int level)
{
  CoglOffscreen *offscreen;
  CoglFramebuffer *fb;
  int level_width;
  int level_height;
  int i;
  CoglHandle ret;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
    return COGL_INVALID_HANDLE;

  /* Make texture is a valid texture object */
  if (!cogl_is_texture (texture))
    return COGL_INVALID_HANDLE;

  /* The texture must not be sliced */
  if (cogl_texture_is_sliced (texture))
    return COGL_INVALID_HANDLE;

  /* Calculate the size of the texture at this mipmap level to ensure
     that it's a valid level */
  level_width = cogl_texture_get_width (texture);
  level_height = cogl_texture_get_height (texture);

  for (i = 0; i < level; i++)
    {
      /* If neither dimension can be further divided then the level is
         invalid */
      if (level_width == 1 && level_height == 1)
        {
          g_warning ("Invalid texture level passed to "
                     "_cogl_offscreen_new_to_texture_full");
          return COGL_INVALID_HANDLE;
        }

      if (level_width > 1)
        level_width >>= 1;
      if (level_height > 1)
        level_height >>= 1;
    }

  offscreen = g_new0 (CoglOffscreen, 1);
  offscreen->texture = cogl_object_ref (texture);
  offscreen->texture_level = level;
  offscreen->texture_level_width = level_width;
  offscreen->texture_level_height = level_height;
  offscreen->create_flags = create_flags;

  fb = COGL_FRAMEBUFFER (offscreen);

  _cogl_framebuffer_init (fb,
                          ctx,
                          COGL_FRAMEBUFFER_TYPE_OFFSCREEN,
                          cogl_texture_get_format (texture),
                          level_width,
                          level_height);

  ret = _cogl_offscreen_object_new (offscreen);

  _cogl_texture_associate_framebuffer (texture, fb);

  return ret;
}

CoglHandle
cogl_offscreen_new_to_texture (CoglTexture *texture)
{
  return _cogl_offscreen_new_to_texture_full (texture, 0, 0);
}

static void
_cogl_offscreen_free (CoglOffscreen *offscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (offscreen);
  CoglContext *ctx = framebuffer->context;
  GSList *l;

  /* Chain up to parent */
  _cogl_framebuffer_free (framebuffer);

  for (l = offscreen->renderbuffers; l; l = l->next)
    {
      GLuint renderbuffer = GPOINTER_TO_UINT (l->data);
      GE (ctx, glDeleteRenderbuffers (1, &renderbuffer));
    }
  g_slist_free (offscreen->renderbuffers);

  GE (ctx, glDeleteFramebuffers (1, &offscreen->fbo_handle));

  if (offscreen->texture != COGL_INVALID_HANDLE)
    cogl_object_unref (offscreen->texture);

  g_free (offscreen);
}

static gboolean
try_creating_fbo (CoglOffscreen *offscreen,
                  TryFBOFlags flags)
{
  CoglFramebuffer *fb = COGL_FRAMEBUFFER (offscreen);
  CoglContext *ctx = fb->context;
  GLuint gl_depth_stencil_handle;
  GLuint gl_depth_handle;
  GLuint gl_stencil_handle;
  GLuint tex_gl_handle;
  GLenum tex_gl_target;
  GLuint fbo_gl_handle;
  GLenum status;
  int n_samples;
  int height;
  int width;

  if (!cogl_texture_get_gl_texture (offscreen->texture,
                                    &tex_gl_handle, &tex_gl_target))
    return FALSE;

  if (tex_gl_target != GL_TEXTURE_2D
#ifdef HAVE_COGL_GL
      && tex_gl_target != GL_TEXTURE_RECTANGLE_ARB
#endif
      )
    return FALSE;

  if (fb->config.samples_per_pixel)
    {
      if (!ctx->glFramebufferTexture2DMultisampleIMG)
        return FALSE;
      n_samples = fb->config.samples_per_pixel;
    }
  else
    n_samples = 0;

  width = offscreen->texture_level_width;
  height = offscreen->texture_level_height;

  /* We are about to generate and bind a new fbo, so we pretend to
   * change framebuffer state so that the old framebuffer will be
   * rebound again before drawing. */
  ctx->dirty_bound_framebuffer = 1;

  /* Generate framebuffer */
  ctx->glGenFramebuffers (1, &fbo_gl_handle);
  GE (ctx, glBindFramebuffer (GL_FRAMEBUFFER, fbo_gl_handle));
  offscreen->fbo_handle = fbo_gl_handle;

  if (n_samples)
    {
      GE (ctx, glFramebufferTexture2DMultisampleIMG (GL_FRAMEBUFFER,
                                                     GL_COLOR_ATTACHMENT0,
                                                     tex_gl_target, tex_gl_handle,
                                                     n_samples,
                                                     offscreen->texture_level));
    }
  else
    GE (ctx, glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     tex_gl_target, tex_gl_handle,
                                     offscreen->texture_level));

  if (flags & _TRY_DEPTH_STENCIL)
    {
      /* Create a renderbuffer for depth and stenciling */
      GE (ctx, glGenRenderbuffers (1, &gl_depth_stencil_handle));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, gl_depth_stencil_handle));
      if (n_samples)
        GE (ctx, glRenderbufferStorageMultisampleIMG (GL_RENDERBUFFER,
                                                      n_samples,
                                                      GL_DEPTH_STENCIL,
                                                      width, height));
      else
        GE (ctx, glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_STENCIL,
                                        width, height));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, 0));
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_STENCIL_ATTACHMENT,
                                          GL_RENDERBUFFER,
                                          gl_depth_stencil_handle));
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_DEPTH_ATTACHMENT,
                                          GL_RENDERBUFFER,
                                          gl_depth_stencil_handle));
      offscreen->renderbuffers =
        g_slist_prepend (offscreen->renderbuffers,
                         GUINT_TO_POINTER (gl_depth_stencil_handle));
    }

  if (flags & _TRY_DEPTH)
    {
      GE (ctx, glGenRenderbuffers (1, &gl_depth_handle));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, gl_depth_handle));
      /* For now we just ask for GL_DEPTH_COMPONENT16 since this is all that's
       * available under GLES */
      if (n_samples)
        GE (ctx, glRenderbufferStorageMultisampleIMG (GL_RENDERBUFFER,
                                                      n_samples,
                                                      GL_DEPTH_COMPONENT16,
                                                      width, height));
      else
        GE (ctx, glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                        width, height));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, 0));
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_DEPTH_ATTACHMENT,
                                          GL_RENDERBUFFER, gl_depth_handle));
      offscreen->renderbuffers =
        g_slist_prepend (offscreen->renderbuffers,
                         GUINT_TO_POINTER (gl_depth_handle));
    }

  if (flags & _TRY_STENCIL)
    {
      GE (ctx, glGenRenderbuffers (1, &gl_stencil_handle));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, gl_stencil_handle));
      if (n_samples)
        GE (ctx, glRenderbufferStorageMultisampleIMG (GL_RENDERBUFFER,
                                                      n_samples,
                                                      GL_STENCIL_INDEX8,
                                                      width, height));
      else
        GE (ctx, glRenderbufferStorage (GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                                        width, height));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, 0));
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_STENCIL_ATTACHMENT,
                                          GL_RENDERBUFFER, gl_stencil_handle));
      offscreen->renderbuffers =
        g_slist_prepend (offscreen->renderbuffers,
                         GUINT_TO_POINTER (gl_stencil_handle));
    }

  /* Make sure it's complete */
  status = ctx->glCheckFramebufferStatus (GL_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE)
    {
      GSList *l;

      GE (ctx, glDeleteFramebuffers (1, &fbo_gl_handle));

      for (l = offscreen->renderbuffers; l; l = l->next)
        {
          GLuint renderbuffer = GPOINTER_TO_UINT (l->data);
          GE (ctx, glDeleteRenderbuffers (1, &renderbuffer));
        }

      g_slist_free (offscreen->renderbuffers);
      offscreen->renderbuffers = NULL;

      return FALSE;
    }

  /* Update the real number of samples_per_pixel now that we have a
   * complete framebuffer */
  if (n_samples)
    {
      GLenum attachment = GL_COLOR_ATTACHMENT0;
      GLenum pname = GL_TEXTURE_SAMPLES_IMG;
      int texture_samples;

      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &texture_samples) );
      fb->samples_per_pixel = texture_samples;
    }

  return TRUE;
}

static gboolean
_cogl_offscreen_allocate (CoglOffscreen *offscreen,
                          GError **error)
{
  CoglFramebuffer *fb = COGL_FRAMEBUFFER (offscreen);
  CoglContext *ctx = fb->context;
  static TryFBOFlags flags;
  static gboolean have_working_flags = FALSE;
  gboolean fbo_created;

  /* XXX: The framebuffer_object spec isn't clear in defining whether attaching
   * a texture as a renderbuffer with mipmap filtering enabled while the
   * mipmaps have not been uploaded should result in an incomplete framebuffer
   * object. (different drivers make different decisions)
   *
   * To avoid an error with drivers that do consider this a problem we
   * explicitly set non mipmapped filters here. These will later be reset when
   * the texture is actually used for rendering according to the filters set on
   * the corresponding CoglPipeline.
   */
  _cogl_texture_set_filters (offscreen->texture, GL_NEAREST, GL_NEAREST);

  if ((offscreen->create_flags & COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL))
    fbo_created = try_creating_fbo (offscreen, 0);
  else
    {
      if ((have_working_flags &&
           try_creating_fbo (offscreen, flags)) ||
#ifdef HAVE_COGL_GL
          (ctx->driver == COGL_DRIVER_GL &&
           try_creating_fbo (offscreen, flags = _TRY_DEPTH_STENCIL)) ||
#endif
          try_creating_fbo (offscreen, flags = _TRY_DEPTH | _TRY_STENCIL) ||
          try_creating_fbo (offscreen, flags = _TRY_STENCIL) ||
          try_creating_fbo (offscreen, flags = _TRY_DEPTH) ||
          try_creating_fbo (offscreen, flags = 0))
        {
          /* Record that the last set of flags succeeded so that we can
             try that set first next time */
          have_working_flags = TRUE;
          fbo_created = TRUE;
        }
      else
        fbo_created = FALSE;
    }

  if (!fbo_created)
    {
      g_set_error (error, COGL_FRAMEBUFFER_ERROR,
                   COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                   "Failed to create an OpenGL framebuffer object");
      return FALSE;
    }

  return TRUE;
}

gboolean
cogl_framebuffer_allocate (CoglFramebuffer *framebuffer,
                           GError **error)
{
  CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
  const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);

  if (framebuffer->allocated)
    return TRUE;

  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      if (!winsys->onscreen_init (onscreen, error))
        return FALSE;
    }
  else
    {
      if (!_cogl_offscreen_allocate (COGL_OFFSCREEN (framebuffer), error))
        return FALSE;
    }

  framebuffer->allocated = TRUE;

  return TRUE;
}

static CoglFramebufferStackEntry *
create_stack_entry (CoglFramebuffer *draw_buffer,
                    CoglFramebuffer *read_buffer)
{
  CoglFramebufferStackEntry *entry = g_slice_new (CoglFramebufferStackEntry);

  entry->draw_buffer = draw_buffer;
  entry->read_buffer = read_buffer;

  return entry;
}

GSList *
_cogl_create_framebuffer_stack (void)
{
  CoglFramebufferStackEntry *entry;
  GSList *stack = NULL;

  entry = create_stack_entry (COGL_INVALID_HANDLE, COGL_INVALID_HANDLE);

  return g_slist_prepend (stack, entry);
}

void
_cogl_free_framebuffer_stack (GSList *stack)
{
  GSList *l;

  for (l = stack; l != NULL; l = l->next)
    {
      CoglFramebufferStackEntry *entry = l->data;

      if (entry->draw_buffer)
        cogl_object_unref (entry->draw_buffer);

      if (entry->read_buffer)
        cogl_object_unref (entry->draw_buffer);

      g_slice_free (CoglFramebufferStackEntry, entry);
    }
  g_slist_free (stack);
}

static void
notify_buffers_changed (CoglFramebuffer *old_draw_buffer,
                        CoglFramebuffer *new_draw_buffer,
                        CoglFramebuffer *old_read_buffer,
                        CoglFramebuffer *new_read_buffer)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  ctx->dirty_bound_framebuffer = 1;
  ctx->dirty_gl_viewport = 1;

  /* We've effectively just switched the current modelview and
   * projection matrix stacks and clip state so we need to dirty
   * them to ensure they get flushed for the next batch of geometry
   * we flush */
  if (new_draw_buffer)
    {
      _cogl_matrix_stack_dirty (new_draw_buffer->modelview_stack);
      _cogl_matrix_stack_dirty (new_draw_buffer->projection_stack);
    }

  _cogl_clip_stack_dirty ();

  if (old_draw_buffer && new_draw_buffer)
    {
      /* If the two draw framebuffers have a different color mask then
         we need to ensure the logic ops are reflushed the next time
         something is drawn */
      if (cogl_framebuffer_get_color_mask (old_draw_buffer) !=
          cogl_framebuffer_get_color_mask (new_draw_buffer))
        {
          ctx->current_pipeline_changes_since_flush |=
            COGL_PIPELINE_STATE_LOGIC_OPS;
          ctx->current_pipeline_age--;
        }

      /* If we're switching from onscreen to offscreen and the last
         flush pipeline is using backface culling then we also need to
         reflush the cull face state because the winding order of the
         front face is flipped for offscreen buffers */
      if (old_draw_buffer->type != new_draw_buffer->type &&
          ctx->current_pipeline &&
          cogl_pipeline_get_cull_face_mode (ctx->current_pipeline) !=
          COGL_PIPELINE_CULL_FACE_MODE_NONE)
        {
          ctx->current_pipeline_changes_since_flush |=
            COGL_PIPELINE_STATE_CULL_FACE;
          ctx->current_pipeline_age--;
        }
    }

  /* XXX: To support the deprecated cogl_set_draw_buffer API we keep
   * track of the last onscreen framebuffer that was set so that it
   * can be restored if the COGL_WINDOW_BUFFER enum is used. A
   * reference isn't taken to the framebuffer because otherwise we
   * would have a circular reference between the context and the
   * framebuffer. Instead the pointer is set to NULL in
   * _cogl_onscreen_free as a kind of a cheap weak reference */
  if (new_draw_buffer &&
      new_draw_buffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    ctx->window_buffer = new_draw_buffer;
}

/* Set the current framebuffer without checking if it's already the
 * current framebuffer. This is used by cogl_pop_framebuffer while
 * the top of the stack is currently not up to date. */
static void
_cogl_set_framebuffers_real (CoglFramebuffer *draw_buffer,
                             CoglFramebuffer *read_buffer)
{
  CoglFramebufferStackEntry *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (ctx != NULL);
  _COGL_RETURN_IF_FAIL (draw_buffer && read_buffer ?
                    draw_buffer->context == read_buffer->context : TRUE);

  entry = ctx->framebuffer_stack->data;

  notify_buffers_changed (entry->draw_buffer,
                          draw_buffer,
                          entry->read_buffer,
                          read_buffer);

  if (draw_buffer)
    cogl_object_ref (draw_buffer);
  if (entry->draw_buffer)
    cogl_object_unref (entry->draw_buffer);

  if (read_buffer)
    cogl_object_ref (read_buffer);
  if (entry->read_buffer)
    cogl_object_unref (entry->read_buffer);

  entry->draw_buffer = draw_buffer;
  entry->read_buffer = read_buffer;
}

static void
_cogl_set_framebuffers (CoglFramebuffer *draw_buffer,
                        CoglFramebuffer *read_buffer)
{
  CoglFramebuffer *current_draw_buffer;
  CoglFramebuffer *current_read_buffer;

  _COGL_RETURN_IF_FAIL (_cogl_is_framebuffer (draw_buffer));
  _COGL_RETURN_IF_FAIL (_cogl_is_framebuffer (read_buffer));

  current_draw_buffer = cogl_get_draw_framebuffer ();
  current_read_buffer = _cogl_get_read_framebuffer ();

  if (current_draw_buffer != draw_buffer ||
      current_read_buffer != read_buffer)
    {
      /* XXX: eventually we want to remove this implicit journal flush
       * so we can log into the journal beyond framebuffer changes to
       * support batching scenes that depend on the results of
       * mid-scene renders to textures. Current will be NULL when the
       * framebuffer stack is first created so we need to guard
       * against that here */
      if (current_draw_buffer)
        _cogl_framebuffer_flush_journal (current_draw_buffer);
      if (current_read_buffer)
        _cogl_framebuffer_flush_journal (current_read_buffer);
      _cogl_set_framebuffers_real (draw_buffer, read_buffer);
    }
}

void
cogl_set_framebuffer (CoglFramebuffer *framebuffer)
{
  _cogl_set_framebuffers (framebuffer, framebuffer);
}

/* XXX: deprecated API */
void
cogl_set_draw_buffer (CoglBufferTarget target, CoglHandle handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (target == COGL_WINDOW_BUFFER)
    handle = ctx->window_buffer;

  /* This is deprecated public API. The public API doesn't currently
     really expose the concept of separate draw and read buffers so
     for the time being this actually just sets both buffers */
  cogl_set_framebuffer (handle);
}

CoglFramebuffer *
cogl_get_draw_framebuffer (void)
{
  CoglFramebufferStackEntry *entry;

  _COGL_GET_CONTEXT (ctx, NULL);

  g_assert (ctx->framebuffer_stack);

  entry = ctx->framebuffer_stack->data;

  return entry->draw_buffer;
}

CoglFramebuffer *
_cogl_get_read_framebuffer (void)
{
  CoglFramebufferStackEntry *entry;

  _COGL_GET_CONTEXT (ctx, NULL);

  g_assert (ctx->framebuffer_stack);

  entry = ctx->framebuffer_stack->data;

  return entry->read_buffer;
}

void
_cogl_push_framebuffers (CoglFramebuffer *draw_buffer,
                         CoglFramebuffer *read_buffer)
{
  CoglContext *ctx;
  CoglFramebuffer *old_draw_buffer, *old_read_buffer;

  _COGL_RETURN_IF_FAIL (_cogl_is_framebuffer (draw_buffer));
  _COGL_RETURN_IF_FAIL (_cogl_is_framebuffer (read_buffer));

  ctx = draw_buffer->context;
  _COGL_RETURN_IF_FAIL (ctx != NULL);
  _COGL_RETURN_IF_FAIL (draw_buffer->context == read_buffer->context);

  _COGL_RETURN_IF_FAIL (ctx->framebuffer_stack != NULL);

  /* Copy the top of the stack so that when we call cogl_set_framebuffer
     it will still know what the old framebuffer was */
  old_draw_buffer = cogl_get_draw_framebuffer ();
  if (old_draw_buffer)
    cogl_object_ref (old_draw_buffer);
  old_read_buffer = _cogl_get_read_framebuffer ();
  if (old_read_buffer)
    cogl_object_ref (old_read_buffer);
  ctx->framebuffer_stack =
    g_slist_prepend (ctx->framebuffer_stack,
                     create_stack_entry (old_draw_buffer,
                                         old_read_buffer));

  _cogl_set_framebuffers (draw_buffer, read_buffer);
}

void
cogl_push_framebuffer (CoglFramebuffer *buffer)
{
  _cogl_push_framebuffers (buffer, buffer);
}

/* XXX: deprecated API */
void
cogl_push_draw_buffer (void)
{
  cogl_push_framebuffer (cogl_get_draw_framebuffer ());
}

void
cogl_pop_framebuffer (void)
{
  CoglFramebufferStackEntry *to_pop;
  CoglFramebufferStackEntry *to_restore;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_assert (ctx->framebuffer_stack != NULL);
  g_assert (ctx->framebuffer_stack->next != NULL);

  to_pop = ctx->framebuffer_stack->data;
  to_restore = ctx->framebuffer_stack->next->data;

  if (to_pop->draw_buffer != to_restore->draw_buffer ||
      to_pop->read_buffer != to_restore->read_buffer)
    {
      /* XXX: eventually we want to remove this implicit journal flush
       * so we can log into the journal beyond framebuffer changes to
       * support batching scenes that depend on the results of
       * mid-scene renders to textures. */
      _cogl_framebuffer_flush_journal (to_pop->draw_buffer);
      _cogl_framebuffer_flush_journal (to_pop->read_buffer);

      notify_buffers_changed (to_pop->draw_buffer,
                              to_restore->draw_buffer,
                              to_pop->read_buffer,
                              to_restore->read_buffer);
    }

  cogl_object_unref (to_pop->draw_buffer);
  cogl_object_unref (to_pop->read_buffer);
  g_slice_free (CoglFramebufferStackEntry, to_pop);

  ctx->framebuffer_stack =
    g_slist_delete_link (ctx->framebuffer_stack,
                         ctx->framebuffer_stack);
}

/* XXX: deprecated API */
void
cogl_pop_draw_buffer (void)
{
  cogl_pop_framebuffer ();
}

static void
bind_gl_framebuffer (CoglContext *ctx,
                     GLenum target,
                     CoglFramebuffer *framebuffer)
{
  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN)
    GE (ctx, glBindFramebuffer (target,
                           COGL_OFFSCREEN (framebuffer)->fbo_handle));
  else
    {
      const CoglWinsysVtable *winsys =
        _cogl_framebuffer_get_winsys (framebuffer);
      winsys->onscreen_bind (COGL_ONSCREEN (framebuffer));
      /* glBindFramebuffer is an an extension with OpenGL ES 1.1 */
      if (cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
        GE (ctx, glBindFramebuffer (target, 0));
    }
}

void
_cogl_framebuffer_flush_state (CoglFramebuffer *draw_buffer,
                               CoglFramebuffer *read_buffer,
                               CoglFramebufferFlushFlags flags)
{
  CoglContext *ctx = draw_buffer->context;

  /* Lazily ensure the framebuffer has been allocated */
  cogl_framebuffer_allocate (draw_buffer, NULL);
  cogl_framebuffer_allocate (read_buffer, NULL);

  if (ctx->dirty_bound_framebuffer)
    {
      if (draw_buffer == read_buffer)
        bind_gl_framebuffer (ctx, GL_FRAMEBUFFER, draw_buffer);
      else
        {
          /* NB: Currently we only take advantage of binding separate
           * read/write buffers for offscreen framebuffer blit
           * purposes.  */
          _COGL_RETURN_IF_FAIL (ctx->private_feature_flags &
                                COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT);
          _COGL_RETURN_IF_FAIL (draw_buffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN);
          _COGL_RETURN_IF_FAIL (read_buffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN);

          bind_gl_framebuffer (ctx, GL_DRAW_FRAMEBUFFER, draw_buffer);
          bind_gl_framebuffer (ctx, GL_READ_FRAMEBUFFER, read_buffer);
        }
    }

  ctx->dirty_bound_framebuffer = FALSE;

  if (flags & COGL_FRAMEBUFFER_FLUSH_BIND_ONLY)
    return;

  if (ctx->dirty_gl_viewport)
    {
      float gl_viewport_y;

      g_assert (draw_buffer->viewport_width >=0 &&
                draw_buffer->viewport_height >=0);

      /* Convert the Cogl viewport y offset to an OpenGL viewport y offset
       * NB: OpenGL defines its window and viewport origins to be bottom
       * left, while Cogl defines them to be top left.
       * NB: We render upside down to offscreen framebuffers so we don't
       * need to convert the y offset in this case. */
      if (cogl_is_offscreen (draw_buffer))
        gl_viewport_y = draw_buffer->viewport_y;
      else
        gl_viewport_y = draw_buffer->height -
          (draw_buffer->viewport_y + draw_buffer->viewport_height);

      COGL_NOTE (OPENGL, "Calling glViewport(%f, %f, %f, %f)",
                 draw_buffer->viewport_x,
                 gl_viewport_y,
                 draw_buffer->viewport_width,
                 draw_buffer->viewport_height);

      GE (ctx, glViewport (draw_buffer->viewport_x,
                      gl_viewport_y,
                      draw_buffer->viewport_width,
                      draw_buffer->viewport_height));
      ctx->dirty_gl_viewport = FALSE;
    }

  if (ctx->current_gl_dither_enabled != draw_buffer->dither_enabled)
    {
      if (draw_buffer->dither_enabled)
        GE (ctx, glEnable (GL_DITHER));
      else
        GE (ctx, glDisable (GL_DITHER));
      ctx->current_gl_dither_enabled = draw_buffer->dither_enabled;
    }

  /* XXX: Flushing clip state may trash the modelview and projection
   * matrices so we must do it before flushing the matrices...
   */
  if (!(flags & COGL_FRAMEBUFFER_FLUSH_SKIP_CLIP_STATE))
    _cogl_clip_state_flush (&draw_buffer->clip_state,
                            draw_buffer);

  if (!(flags & COGL_FRAMEBUFFER_FLUSH_SKIP_MODELVIEW))
    _cogl_matrix_stack_flush_to_gl (draw_buffer->modelview_stack,
                                    COGL_MATRIX_MODELVIEW);

  _cogl_matrix_stack_flush_to_gl (draw_buffer->projection_stack,
                                  COGL_MATRIX_PROJECTION);
}

int
cogl_framebuffer_get_red_bits (CoglFramebuffer *framebuffer)
{
  _cogl_framebuffer_init_bits (framebuffer);

  return framebuffer->red_bits;
}

int
cogl_framebuffer_get_green_bits (CoglFramebuffer *framebuffer)
{
  _cogl_framebuffer_init_bits (framebuffer);

  return framebuffer->green_bits;
}

int
cogl_framebuffer_get_blue_bits (CoglFramebuffer *framebuffer)
{
  _cogl_framebuffer_init_bits (framebuffer);

  return framebuffer->blue_bits;
}

int
cogl_framebuffer_get_alpha_bits (CoglFramebuffer *framebuffer)
{
  _cogl_framebuffer_init_bits (framebuffer);

  return framebuffer->alpha_bits;
}

CoglColorMask
cogl_framebuffer_get_color_mask (CoglFramebuffer *framebuffer)
{
  return framebuffer->color_mask;
}

void
cogl_framebuffer_set_color_mask (CoglFramebuffer *framebuffer,
                                 CoglColorMask color_mask)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_flush (); /* XXX: Currently color mask changes don't go through the journal */
  framebuffer->color_mask = color_mask;

  /* Make sure the ColorMask is updated when the next primitive is drawn */
  ctx->current_pipeline_changes_since_flush |=
    COGL_PIPELINE_STATE_LOGIC_OPS;
  ctx->current_pipeline_age--;
}

gboolean
cogl_framebuffer_get_dither_enabled (CoglFramebuffer *framebuffer)
{
  return framebuffer->dither_enabled;
}

void
cogl_framebuffer_set_dither_enabled (CoglFramebuffer *framebuffer,
                                     gboolean dither_enabled)
{
  if (framebuffer->dither_enabled == dither_enabled)
    return;

  cogl_flush (); /* Currently dithering changes aren't tracked in the journal */
  framebuffer->dither_enabled = dither_enabled;
}

CoglPixelFormat
cogl_framebuffer_get_color_format (CoglFramebuffer *framebuffer)
{
  return framebuffer->format;
}

int
cogl_framebuffer_get_samples_per_pixel (CoglFramebuffer *framebuffer)
{
  if (framebuffer->allocated)
    return framebuffer->samples_per_pixel;
  else
    return framebuffer->config.samples_per_pixel;
}

void
cogl_framebuffer_set_samples_per_pixel (CoglFramebuffer *framebuffer,
                                        int samples_per_pixel)
{
  _COGL_RETURN_IF_FAIL (!framebuffer->allocated);

  framebuffer->config.samples_per_pixel = samples_per_pixel;
}

void
cogl_framebuffer_resolve_samples (CoglFramebuffer *framebuffer)
{
  cogl_framebuffer_resolve_samples_region (framebuffer,
                                           0, 0,
                                           framebuffer->width,
                                           framebuffer->height);

  /* TODO: Make this happen implicitly when the resolve texture next gets used
   * as a source, either via cogl_texture_get_data(), via cogl_read_pixels() or
   * if used as a source for rendering. We would also implicitly resolve if
   * necessary before freeing a CoglFramebuffer.
   *
   * This API should still be kept but it is optional, only necessary
   * if the user wants to explicitly control when the resolve happens e.g.
   * to ensure it's done in advance of it being used as a source.
   *
   * Every texture should have a CoglFramebuffer *needs_resolve member
   * internally. When the texture gets validated before being used as a source
   * we should first check the needs_resolve pointer and if set we'll
   * automatically call cogl_framebuffer_resolve_samples ().
   *
   * Calling cogl_framebuffer_resolve_samples() or
   * cogl_framebuffer_resolve_samples_region() should reset the textures
   * needs_resolve pointer to NULL.
   *
   * Rendering anything to a framebuffer will cause the corresponding
   * texture's ->needs_resolve pointer to be set.
   *
   * XXX: Note: we only need to address this TODO item when adding support for
   * EXT_framebuffer_multisample because currently we only support hardware
   * that resolves implicitly anyway.
   */
}

void
cogl_framebuffer_resolve_samples_region (CoglFramebuffer *framebuffer,
                                         int x,
                                         int y,
                                         int width,
                                         int height)
{
  /* NOP for now since we don't support EXT_framebuffer_multisample yet which
   * requires an explicit resolve. */
}

CoglContext *
cogl_framebuffer_get_context (CoglFramebuffer *framebuffer)
{
  _COGL_RETURN_VAL_IF_FAIL (framebuffer != NULL, NULL);

  return framebuffer->context;
}

gboolean
_cogl_framebuffer_try_fast_read_pixel (CoglFramebuffer *framebuffer,
                                       int x,
                                       int y,
                                       CoglReadPixelsFlags source,
                                       CoglPixelFormat format,
                                       guint8 *pixel)
{
  gboolean found_intersection;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_FAST_READ_PIXEL)))
    return FALSE;

  if (source != COGL_READ_PIXELS_COLOR_BUFFER)
    return FALSE;

  if (format != COGL_PIXEL_FORMAT_RGBA_8888_PRE &&
      format != COGL_PIXEL_FORMAT_RGBA_8888)
    return FALSE;

  if (!_cogl_journal_try_read_pixel (framebuffer->journal,
                                     x, y, format, pixel,
                                     &found_intersection))
    return FALSE;

  /* If we can't determine the color from the primitives in the
   * journal then see if we can use the last recorded clear color
   */

  /* If _cogl_journal_try_read_pixel() failed even though there was an
   * intersection of the given point with a primitive in the journal
   * then we can't fallback to the framebuffer's last clear color...
   * */
  if (found_intersection)
    return TRUE;

  /* If the framebuffer has been rendered too since it was last
   * cleared then we can't return the last known clear color. */
  if (framebuffer->clear_clip_dirty)
    return FALSE;

  if (x >= framebuffer->clear_clip_x0 &&
      x < framebuffer->clear_clip_x1 &&
      y >= framebuffer->clear_clip_y0 &&
      y < framebuffer->clear_clip_y1)
    {

      /* we currently only care about cases where the premultiplied or
       * unpremultipled colors are equivalent... */
      if (framebuffer->clear_color_alpha != 1.0)
        return FALSE;

      pixel[0] = framebuffer->clear_color_red * 255.0;
      pixel[1] = framebuffer->clear_color_green * 255.0;
      pixel[2] = framebuffer->clear_color_blue * 255.0;
      pixel[3] = framebuffer->clear_color_alpha * 255.0;

      return TRUE;
    }

  return FALSE;
}

void
_cogl_blit_framebuffer (unsigned int src_x,
                        unsigned int src_y,
                        unsigned int dst_x,
                        unsigned int dst_y,
                        unsigned int width,
                        unsigned int height)
{
  CoglFramebuffer *draw_buffer;
  CoglFramebuffer *read_buffer;
  CoglContext *ctx;

  /* FIXME: this function should take explit src and dst framebuffer
   * arguments. */
  draw_buffer = cogl_get_draw_framebuffer ();
  read_buffer = _cogl_get_read_framebuffer ();
  ctx = draw_buffer->context;

  _COGL_RETURN_IF_FAIL (ctx->private_feature_flags &
                    COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT);

  /* We can only support blitting between offscreen buffers because
     otherwise we would need to mirror the image and GLES2.0 doesn't
     support this */
  _COGL_RETURN_IF_FAIL (cogl_is_offscreen (draw_buffer));
  _COGL_RETURN_IF_FAIL (cogl_is_offscreen (read_buffer));
  /* The buffers must be the same format */
  _COGL_RETURN_IF_FAIL (draw_buffer->format == read_buffer->format);

  /* Make sure the current framebuffers are bound. We explicitly avoid
     flushing the clip state so we can bind our own empty state */
  _cogl_framebuffer_flush_state (cogl_get_draw_framebuffer (),
                                 _cogl_get_read_framebuffer (),
                                 COGL_FRAMEBUFFER_FLUSH_SKIP_CLIP_STATE);

  /* Flush any empty clip stack because glBlitFramebuffer is affected
     by the scissor and we want to hide this feature for the Cogl API
     because it's not obvious to an app how the clip state will affect
     the scissor */
  _cogl_clip_stack_flush (NULL, draw_buffer);

  ctx->glBlitFramebuffer (src_x, src_y,
                     src_x + width, src_y + height,
                     dst_x, dst_y,
                     dst_x + width, dst_y + height,
                     GL_COLOR_BUFFER_BIT,
                     GL_NEAREST);
}

static void
_cogl_framebuffer_discard_buffers_real (CoglFramebuffer *framebuffer,
                                        unsigned long buffers)
{
#ifdef GL_EXT_discard_framebuffer
  CoglContext *ctx = framebuffer->context;

  if (ctx->glDiscardFramebuffer)
    {
      GLenum attachments[3];
      int i = 0;

      if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
        {
          if (buffers & COGL_BUFFER_BIT_COLOR)
            attachments[i++] = GL_COLOR_EXT;
          if (buffers & COGL_BUFFER_BIT_DEPTH)
            attachments[i++] = GL_DEPTH_EXT;
          if (buffers & COGL_BUFFER_BIT_STENCIL)
            attachments[i++] = GL_STENCIL_EXT;
        }
      else
        {
          if (buffers & COGL_BUFFER_BIT_COLOR)
            attachments[i++] = GL_COLOR_ATTACHMENT0;
          if (buffers & COGL_BUFFER_BIT_DEPTH)
            attachments[i++] = GL_DEPTH_ATTACHMENT;
          if (buffers & COGL_BUFFER_BIT_STENCIL)
            attachments[i++] = GL_STENCIL_ATTACHMENT;
        }

      GE (ctx, glDiscardFramebuffer (GL_FRAMEBUFFER, i, attachments));
    }
#endif /* GL_EXT_discard_framebuffer */
}

void
cogl_framebuffer_discard_buffers (CoglFramebuffer *framebuffer,
                                  unsigned long buffers)
{
  _COGL_RETURN_IF_FAIL (buffers & COGL_BUFFER_BIT_COLOR);

  _cogl_framebuffer_discard_buffers_real (framebuffer, buffers);
}

void
cogl_framebuffer_finish (CoglFramebuffer *framebuffer)
{
  _cogl_framebuffer_flush_journal (framebuffer);
  GE (framebuffer->context, glFinish ());
}

void
cogl_framebuffer_push_matrix (CoglFramebuffer *framebuffer)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_push (modelview_stack);
}

void
cogl_framebuffer_pop_matrix (CoglFramebuffer *framebuffer)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_pop (modelview_stack);
}

void
cogl_framebuffer_scale (CoglFramebuffer *framebuffer,
                        float x,
                        float y,
                        float z)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_scale (modelview_stack, x, y, z);
}

void
cogl_framebuffer_translate (CoglFramebuffer *framebuffer,
                            float x,
                            float y,
                            float z)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_translate (modelview_stack, x, y, z);
}

void
cogl_framebuffer_rotate (CoglFramebuffer *framebuffer,
                         float angle,
                         float x,
                         float y,
                         float z)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_rotate (modelview_stack, angle, x, y, z);
}

void
cogl_framebuffer_transform (CoglFramebuffer *framebuffer,
                            const CoglMatrix *matrix)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_multiply (modelview_stack, matrix);
}

void
cogl_framebuffer_perspective (CoglFramebuffer *framebuffer,
                              float fov_y,
                              float aspect,
                              float z_near,
                              float z_far)
{
  float ymax = z_near * tanf (fov_y * G_PI / 360.0);

  cogl_framebuffer_frustum (framebuffer,
                            -ymax * aspect,  /* left */
                            ymax * aspect,   /* right */
                            -ymax,           /* bottom */
                            ymax,            /* top */
                            z_near,
                            z_far);
}

void
cogl_framebuffer_frustum (CoglFramebuffer *framebuffer,
                          float left,
                          float right,
                          float bottom,
                          float top,
                          float z_near,
                          float z_far)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);

  /* XXX: The projection matrix isn't currently tracked in the journal
   * so we need to flush all journaled primitives first... */
  _cogl_framebuffer_flush_journal (framebuffer);

  _cogl_matrix_stack_load_identity (projection_stack);

  _cogl_matrix_stack_frustum (projection_stack,
                              left,
                              right,
                              bottom,
                              top,
                              z_near,
                              z_far);
}

void
cogl_framebuffer_orthographic (CoglFramebuffer *framebuffer,
                               float x_1,
                               float y_1,
                               float x_2,
                               float y_2,
                               float near,
                               float far)
{
  CoglMatrix ortho;
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);

  /* XXX: The projection matrix isn't currently tracked in the journal
   * so we need to flush all journaled primitives first... */
  _cogl_framebuffer_flush_journal (framebuffer);

  cogl_matrix_init_identity (&ortho);
  cogl_matrix_orthographic (&ortho, x_1, y_1, x_2, y_2, near, far);
  _cogl_matrix_stack_set (projection_stack, &ortho);
}

void
cogl_framebuffer_get_modelview_matrix (CoglFramebuffer *framebuffer,
                                       CoglMatrix *matrix)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_get (modelview_stack, matrix);
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_framebuffer_set_modelview_matrix (CoglFramebuffer *framebuffer,
                                       CoglMatrix *matrix)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_set (modelview_stack, matrix);
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_framebuffer_get_projection_matrix (CoglFramebuffer *framebuffer,
                                        CoglMatrix *matrix)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  _cogl_matrix_stack_get (projection_stack, matrix);
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_framebuffer_set_projection_matrix (CoglFramebuffer *framebuffer,
                                        CoglMatrix *matrix)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);

  /* XXX: The projection matrix isn't currently tracked in the journal
   * so we need to flush all journaled primitives first... */
  _cogl_framebuffer_flush_journal (framebuffer);

  _cogl_matrix_stack_set (projection_stack, matrix);

  _COGL_MATRIX_DEBUG_PRINT (matrix);
}
