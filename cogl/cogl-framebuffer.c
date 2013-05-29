/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2012 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-debug.h"
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
#include "cogl-primitive-private.h"
#include "cogl-offscreen.h"
#include "cogl1-context.h"
#include "cogl-private.h"
#include "cogl-primitives-private.h"
#include "cogl-path-private.h"
#include "cogl-error-private.h"
#include "cogl-texture-gl-private.h"

typedef struct _CoglFramebufferStackEntry
{
  CoglFramebuffer *draw_buffer;
  CoglFramebuffer *read_buffer;
} CoglFramebufferStackEntry;

extern CoglObjectClass _cogl_onscreen_class;

#ifdef COGL_ENABLE_DEBUG
static CoglUserDataKey wire_pipeline_key;
#endif

static void _cogl_offscreen_free (CoglOffscreen *offscreen);

COGL_OBJECT_DEFINE_WITH_CODE (Offscreen, offscreen,
                              _cogl_offscreen_class.virt_unref =
                              _cogl_framebuffer_unref);
COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING (offscreen);

/* XXX:
 * The CoglObject macros don't support any form of inheritance, so for
 * now we implement the CoglObject support for the CoglFramebuffer
 * abstract class manually.
 */

uint32_t
cogl_framebuffer_error_quark (void)
{
  return g_quark_from_static_string ("cogl-framebuffer-error-quark");
}

CoglBool
cogl_is_framebuffer (void *object)
{
  CoglObject *obj = object;

  if (obj == NULL)
    return FALSE;

  return (obj->klass == &_cogl_onscreen_class ||
          obj->klass == &_cogl_offscreen_class);
}

void
_cogl_framebuffer_init (CoglFramebuffer *framebuffer,
                        CoglContext *ctx,
                        CoglFramebufferType type,
                        CoglPixelFormat format,
                        int width,
                        int height)
{
  framebuffer->context = ctx;

  framebuffer->type = type;
  framebuffer->width = width;
  framebuffer->height = height;
  framebuffer->format = format;
  framebuffer->viewport_x = 0;
  framebuffer->viewport_y = 0;
  framebuffer->viewport_width = width;
  framebuffer->viewport_height = height;
  framebuffer->viewport_age = 0;
  framebuffer->viewport_age_for_scissor_workaround = -1;
  framebuffer->dither_enabled = TRUE;

  framebuffer->modelview_stack = cogl_matrix_stack_new (ctx);
  framebuffer->projection_stack = cogl_matrix_stack_new (ctx);

  framebuffer->dirty_bitmasks = TRUE;

  framebuffer->color_mask = COGL_COLOR_MASK_ALL;

  framebuffer->samples_per_pixel = 0;

  /* Initialise the clip stack */
  _cogl_clip_state_init (&framebuffer->clip_state);

  framebuffer->journal = _cogl_journal_new (framebuffer);

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

  _cogl_fence_cancel_fences_for_framebuffer (framebuffer);

  _cogl_clip_state_destroy (&framebuffer->clip_state);

  cogl_object_unref (framebuffer->modelview_stack);
  framebuffer->modelview_stack = NULL;

  cogl_object_unref (framebuffer->projection_stack);
  framebuffer->projection_stack = NULL;

  cogl_object_unref (framebuffer->journal);

  if (ctx->viewport_scissor_workaround_framebuffer == framebuffer)
    ctx->viewport_scissor_workaround_framebuffer = NULL;

  ctx->framebuffers = g_list_remove (ctx->framebuffers, framebuffer);

  if (ctx->current_draw_buffer == framebuffer)
    ctx->current_draw_buffer = NULL;
  if (ctx->current_read_buffer == framebuffer)
    ctx->current_read_buffer = NULL;
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
  CoglContext *ctx = framebuffer->context;

  if (!buffers)
    {
      static CoglBool shown = FALSE;

      if (!shown)
        {
	  g_warning ("You should specify at least one auxiliary buffer "
                     "when calling cogl_framebuffer_clear");
        }

      return;
    }

  ctx->driver_vtable->framebuffer_clear (framebuffer,
                                         buffers,
                                         red, green, blue, alpha);
}

void
_cogl_framebuffer_mark_mid_scene (CoglFramebuffer *framebuffer)
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
  CoglContext *ctx = framebuffer->context;
  CoglClipStack *clip_stack = _cogl_framebuffer_get_clip_stack (framebuffer);
  int scissor_x0;
  int scissor_y0;
  int scissor_x1;
  int scissor_y1;
  CoglBool saved_viewport_scissor_workaround;

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

  /* XXX: ONGOING BUG: Intel viewport scissor
   *
   * The semantics of cogl_framebuffer_clear() are that it should not
   * be affected by the current viewport and so if we are currently
   * applying a workaround for viewport scissoring we need to
   * temporarily disable the workaround before clearing so any
   * special scissoring for the workaround will be removed first.
   *
   * Note: we only need to disable the workaround if the current
   * viewport doesn't match the framebuffer's size since otherwise
   * the workaround wont affect clearing anyway.
   */
  if (ctx->needs_viewport_scissor_workaround &&
      (framebuffer->viewport_x != 0 ||
       framebuffer->viewport_y != 0 ||
       framebuffer->viewport_width != framebuffer->width ||
       framebuffer->viewport_height != framebuffer->height))
    {
      saved_viewport_scissor_workaround = TRUE;
      ctx->needs_viewport_scissor_workaround = FALSE;
      ctx->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_CLIP;
    }
  else
    saved_viewport_scissor_workaround = FALSE;

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the pipeline state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (framebuffer, framebuffer,
                                 COGL_FRAMEBUFFER_STATE_ALL);

  _cogl_framebuffer_clear_without_flush4f (framebuffer, buffers,
                                           red, green, blue, alpha);

  /* XXX: ONGOING BUG: Intel viewport scissor
   *
   * See comment about temporarily disabling this workaround above
   */
  if (saved_viewport_scissor_workaround)
    {
      ctx->needs_viewport_scissor_workaround = TRUE;
      ctx->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_CLIP;
    }

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
    _cogl_framebuffer_mark_mid_scene (framebuffer);
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
  CoglContext *context = framebuffer->context;

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
  framebuffer->viewport_age++;

  if (context->current_draw_buffer == framebuffer)
    {
      context->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_VIEWPORT;

      if (context->needs_viewport_scissor_workaround)
        context->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_CLIP;
    }
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
  _cogl_journal_flush (framebuffer->journal);
}

void
_cogl_framebuffer_flush_dependency_journals (CoglFramebuffer *framebuffer)
{
  GList *l;
  for (l = framebuffer->deps; l; l = l->next)
    _cogl_framebuffer_flush_journal (l->data);
  _cogl_framebuffer_remove_all_dependencies (framebuffer);
}

CoglOffscreen *
_cogl_offscreen_new_to_texture_full (CoglTexture *texture,
                                     CoglOffscreenFlags create_flags,
                                     int level)
{
  CoglContext *ctx = texture->context;
  CoglOffscreen *offscreen;
  CoglFramebuffer *fb;
  int level_width;
  int level_height;
  CoglOffscreen *ret;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_texture (texture), NULL);
  _COGL_RETURN_VAL_IF_FAIL (level < _cogl_texture_get_n_levels (texture),
                            NULL);

  _cogl_texture_get_level_size (texture,
                                level,
                                &level_width,
                                &level_height,
                                NULL);

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

CoglOffscreen *
cogl_offscreen_new_to_texture (CoglTexture *texture)
{
  return _cogl_offscreen_new_to_texture_full (texture, 0, 0);
}

static void
_cogl_offscreen_free (CoglOffscreen *offscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (offscreen);
  CoglContext *ctx = framebuffer->context;

  ctx->driver_vtable->offscreen_free (offscreen);

  /* Chain up to parent */
  _cogl_framebuffer_free (framebuffer);

  if (offscreen->texture != NULL)
    cogl_object_unref (offscreen->texture);

  if (offscreen->depth_texture != NULL)
    cogl_object_unref (offscreen->depth_texture);

  g_free (offscreen);
}

CoglBool
cogl_framebuffer_allocate (CoglFramebuffer *framebuffer,
                           CoglError **error)
{
  CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
  const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);

  if (framebuffer->allocated)
    return TRUE;

  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      if (framebuffer->config.depth_texture_enabled)
        {
          _cogl_set_error (error, COGL_FRAMEBUFFER_ERROR,
                           COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                           "Can't allocate onscreen framebuffer with a "
                           "texture based depth buffer");
          return FALSE;
        }

      if (!winsys->onscreen_init (onscreen, error))
        return FALSE;
    }
  else
    {
      CoglContext *ctx = framebuffer->context;
      CoglOffscreen *offscreen = COGL_OFFSCREEN (framebuffer);

      if (!cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
        {
          _cogl_set_error (error, COGL_SYSTEM_ERROR,
                           COGL_SYSTEM_ERROR_UNSUPPORTED,
                           "Offscreen framebuffers not supported by system");
          return FALSE;
        }

      if (cogl_texture_is_sliced (offscreen->texture))
        {
          _cogl_set_error (error, COGL_SYSTEM_ERROR,
                           COGL_SYSTEM_ERROR_UNSUPPORTED,
                           "Can't create offscreen framebuffer from "
                           "sliced texture");
          return FALSE;
        }

      if (!cogl_texture_allocate (offscreen->texture, error))
        return FALSE;

      if (!ctx->driver_vtable->offscreen_allocate (offscreen, error))
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

  entry = create_stack_entry (NULL, NULL);

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
  /* XXX: To support the deprecated cogl_set_draw_buffer API we keep
   * track of the last onscreen framebuffer that was set so that it
   * can be restored if the COGL_WINDOW_BUFFER enum is used. A
   * reference isn't taken to the framebuffer because otherwise we
   * would have a circular reference between the context and the
   * framebuffer. Instead the pointer is set to NULL in
   * _cogl_onscreen_free as a kind of a cheap weak reference */
  if (new_draw_buffer &&
      new_draw_buffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    new_draw_buffer->context->window_buffer = new_draw_buffer;
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

  _COGL_RETURN_IF_FAIL (cogl_is_framebuffer (draw_buffer));
  _COGL_RETURN_IF_FAIL (cogl_is_framebuffer (read_buffer));

  current_draw_buffer = cogl_get_draw_framebuffer ();
  current_read_buffer = _cogl_get_read_framebuffer ();

  if (current_draw_buffer != draw_buffer ||
      current_read_buffer != read_buffer)
    _cogl_set_framebuffers_real (draw_buffer, read_buffer);
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

  _COGL_RETURN_IF_FAIL (cogl_is_framebuffer (draw_buffer));
  _COGL_RETURN_IF_FAIL (cogl_is_framebuffer (read_buffer));

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
    notify_buffers_changed (to_pop->draw_buffer,
                            to_restore->draw_buffer,
                            to_pop->read_buffer,
                            to_restore->read_buffer);

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

static unsigned long
_cogl_framebuffer_compare_viewport_state (CoglFramebuffer *a,
                                          CoglFramebuffer *b)
{
  if (a->viewport_x != b->viewport_x ||
      a->viewport_y != b->viewport_y ||
      a->viewport_width != b->viewport_width ||
      a->viewport_height != b->viewport_height ||
      /* NB: we render upside down to offscreen framebuffers and that
       * can affect how we setup the GL viewport... */
      a->type != b->type)
    {
      unsigned long differences = COGL_FRAMEBUFFER_STATE_VIEWPORT;
      CoglContext *context = a->context;

      /* XXX: ONGOING BUG: Intel viewport scissor
       *
       * Intel gen6 drivers don't currently correctly handle offset
       * viewports, since primitives aren't clipped within the bounds of
       * the viewport.  To workaround this we push our own clip for the
       * viewport that will use scissoring to ensure we clip as expected.
       *
       * This workaround implies that a change in viewport state is
       * effectively also a change in the clipping state.
       *
       * TODO: file a bug upstream!
       */
      if (G_UNLIKELY (context->needs_viewport_scissor_workaround))
          differences |= COGL_FRAMEBUFFER_STATE_CLIP;

      return differences;
    }
  else
    return 0;
}

static unsigned long
_cogl_framebuffer_compare_clip_state (CoglFramebuffer *a,
                                      CoglFramebuffer *b)
{
  if (((a->clip_state.stacks == NULL || b->clip_state.stacks == NULL) &&
       a->clip_state.stacks != b->clip_state.stacks)
      ||
      a->clip_state.stacks->data != b->clip_state.stacks->data)
    return COGL_FRAMEBUFFER_STATE_CLIP;
  else
    return 0;
}

static unsigned long
_cogl_framebuffer_compare_dither_state (CoglFramebuffer *a,
                                        CoglFramebuffer *b)
{
  return a->dither_enabled != b->dither_enabled ?
    COGL_FRAMEBUFFER_STATE_DITHER : 0;
}

static unsigned long
_cogl_framebuffer_compare_modelview_state (CoglFramebuffer *a,
                                           CoglFramebuffer *b)
{
  /* We always want to flush the modelview state. All this does is set
     the current modelview stack on the context to the framebuffer's
     stack. */
  return COGL_FRAMEBUFFER_STATE_MODELVIEW;
}

static unsigned long
_cogl_framebuffer_compare_projection_state (CoglFramebuffer *a,
                                            CoglFramebuffer *b)
{
  /* We always want to flush the projection state. All this does is
     set the current projection stack on the context to the
     framebuffer's stack. */
  return COGL_FRAMEBUFFER_STATE_PROJECTION;
}

static unsigned long
_cogl_framebuffer_compare_color_mask_state (CoglFramebuffer *a,
                                            CoglFramebuffer *b)
{
  if (cogl_framebuffer_get_color_mask (a) !=
      cogl_framebuffer_get_color_mask (b))
    return COGL_FRAMEBUFFER_STATE_COLOR_MASK;
  else
    return 0;
}

static unsigned long
_cogl_framebuffer_compare_front_face_winding_state (CoglFramebuffer *a,
                                                    CoglFramebuffer *b)
{
  if (a->type != b->type)
    return COGL_FRAMEBUFFER_STATE_FRONT_FACE_WINDING;
  else
    return 0;
}

unsigned long
_cogl_framebuffer_compare (CoglFramebuffer *a,
                           CoglFramebuffer *b,
                           unsigned long state)
{
  unsigned long differences = 0;
  int bit;

  if (state & COGL_FRAMEBUFFER_STATE_BIND)
    {
      differences |= COGL_FRAMEBUFFER_STATE_BIND;
      state &= ~COGL_FRAMEBUFFER_STATE_BIND;
    }

  COGL_FLAGS_FOREACH_START (&state, 1, bit)
    {
      /* XXX: We considered having an array of callbacks for each state index
       * that we'd call here but decided that this way the compiler is more
       * likely going to be able to in-line the comparison functions and use
       * the index to jump straight to the required code. */
      switch (bit)
        {
        case COGL_FRAMEBUFFER_STATE_INDEX_VIEWPORT:
          differences |=
            _cogl_framebuffer_compare_viewport_state (a, b);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_CLIP:
          differences |= _cogl_framebuffer_compare_clip_state (a, b);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_DITHER:
          differences |= _cogl_framebuffer_compare_dither_state (a, b);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_MODELVIEW:
          differences |=
            _cogl_framebuffer_compare_modelview_state (a, b);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_PROJECTION:
          differences |=
            _cogl_framebuffer_compare_projection_state (a, b);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_COLOR_MASK:
          differences |=
            _cogl_framebuffer_compare_color_mask_state (a, b);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_FRONT_FACE_WINDING:
          differences |=
            _cogl_framebuffer_compare_front_face_winding_state (a, b);
          break;
        default:
          g_warn_if_reached ();
        }
    }
  COGL_FLAGS_FOREACH_END;

  return differences;
}

void
_cogl_framebuffer_flush_state (CoglFramebuffer *draw_buffer,
                               CoglFramebuffer *read_buffer,
                               CoglFramebufferState state)
{
  CoglContext *ctx = draw_buffer->context;

  ctx->driver_vtable->framebuffer_flush_state (draw_buffer,
                                               read_buffer,
                                               state);
}

int
cogl_framebuffer_get_red_bits (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferBits bits;

  ctx->driver_vtable->framebuffer_query_bits (framebuffer, &bits);

  return bits.red;
}

int
cogl_framebuffer_get_green_bits (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferBits bits;

  ctx->driver_vtable->framebuffer_query_bits (framebuffer, &bits);

  return bits.green;
}

int
cogl_framebuffer_get_blue_bits (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferBits bits;

  ctx->driver_vtable->framebuffer_query_bits (framebuffer, &bits);

  return bits.blue;
}

int
cogl_framebuffer_get_alpha_bits (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferBits bits;

  ctx->driver_vtable->framebuffer_query_bits (framebuffer, &bits);

  return bits.alpha;
}

int
cogl_framebuffer_get_depth_bits (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferBits bits;

  ctx->driver_vtable->framebuffer_query_bits (framebuffer, &bits);

  return bits.depth;
}

int
_cogl_framebuffer_get_stencil_bits (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferBits bits;

  ctx->driver_vtable->framebuffer_query_bits (framebuffer, &bits);

  return bits.stencil;
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
  /* XXX: Currently color mask changes don't go through the journal */
  _cogl_framebuffer_flush_journal (framebuffer);

  framebuffer->color_mask = color_mask;

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_COLOR_MASK;
}

CoglBool
cogl_framebuffer_get_dither_enabled (CoglFramebuffer *framebuffer)
{
  return framebuffer->dither_enabled;
}

void
cogl_framebuffer_set_dither_enabled (CoglFramebuffer *framebuffer,
                                     CoglBool dither_enabled)
{
  if (framebuffer->dither_enabled == dither_enabled)
    return;

  cogl_flush (); /* Currently dithering changes aren't tracked in the journal */
  framebuffer->dither_enabled = dither_enabled;

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_DITHER;
}

CoglPixelFormat
cogl_framebuffer_get_color_format (CoglFramebuffer *framebuffer)
{
  return framebuffer->format;
}

void
cogl_framebuffer_set_depth_texture_enabled (CoglFramebuffer *framebuffer,
                                            CoglBool enabled)
{
  _COGL_RETURN_IF_FAIL (!framebuffer->allocated);

  framebuffer->config.depth_texture_enabled = enabled;
}

CoglBool
cogl_framebuffer_get_depth_texture_enabled (CoglFramebuffer *framebuffer)
{
  return framebuffer->config.depth_texture_enabled;
}

CoglTexture *
cogl_framebuffer_get_depth_texture (CoglFramebuffer *framebuffer)
{
  /* lazily allocate the framebuffer... */
  if (!cogl_framebuffer_allocate (framebuffer, NULL))
    return NULL;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_offscreen (framebuffer), NULL);
  return COGL_OFFSCREEN(framebuffer)->depth_texture;
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

static CoglBool
_cogl_framebuffer_try_fast_read_pixel (CoglFramebuffer *framebuffer,
                                       int x,
                                       int y,
                                       CoglReadPixelsFlags source,
                                       CoglBitmap *bitmap)
{
  CoglBool found_intersection;
  CoglPixelFormat format;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_FAST_READ_PIXEL)))
    return FALSE;

  if (source != COGL_READ_PIXELS_COLOR_BUFFER)
    return FALSE;

  format = cogl_bitmap_get_format (bitmap);

  if (format != COGL_PIXEL_FORMAT_RGBA_8888_PRE &&
      format != COGL_PIXEL_FORMAT_RGBA_8888)
    return FALSE;

  if (!_cogl_journal_try_read_pixel (framebuffer->journal,
                                     x, y, bitmap,
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
      uint8_t *pixel;
      CoglError *ignore_error = NULL;

      /* we currently only care about cases where the premultiplied or
       * unpremultipled colors are equivalent... */
      if (framebuffer->clear_color_alpha != 1.0)
        return FALSE;

      pixel = _cogl_bitmap_map (bitmap,
                                COGL_BUFFER_ACCESS_WRITE,
                                COGL_BUFFER_MAP_HINT_DISCARD,
                                &ignore_error);
      if (pixel == NULL)
        {
          cogl_error_free (ignore_error);
          return FALSE;
        }

      pixel[0] = framebuffer->clear_color_red * 255.0;
      pixel[1] = framebuffer->clear_color_green * 255.0;
      pixel[2] = framebuffer->clear_color_blue * 255.0;
      pixel[3] = framebuffer->clear_color_alpha * 255.0;

      _cogl_bitmap_unmap (bitmap);

      return TRUE;
    }

  return FALSE;
}

CoglBool
_cogl_framebuffer_read_pixels_into_bitmap (CoglFramebuffer *framebuffer,
                                           int x,
                                           int y,
                                           CoglReadPixelsFlags source,
                                           CoglBitmap *bitmap,
                                           CoglError **error)
{
  CoglContext *ctx;
  int width;
  int height;

  _COGL_RETURN_VAL_IF_FAIL (source & COGL_READ_PIXELS_COLOR_BUFFER, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_framebuffer (framebuffer), FALSE);

  if (!cogl_framebuffer_allocate (framebuffer, error))
    return FALSE;

  width = cogl_bitmap_get_width (bitmap);
  height = cogl_bitmap_get_height (bitmap);

  if (width == 1 && height == 1 && !framebuffer->clear_clip_dirty)
    {
      /* If everything drawn so far for this frame is still in the
       * Journal then if all of the rectangles only have a flat
       * opaque color we have a fast-path for reading a single pixel
       * that avoids the relatively high cost of flushing primitives
       * to be drawn on the GPU (considering how simple the geometry
       * is in this case) and then blocking on the long GPU pipelines
       * for the result.
       */
      if (_cogl_framebuffer_try_fast_read_pixel (framebuffer,
                                                 x, y, source, bitmap))
        return TRUE;
    }

  ctx = cogl_framebuffer_get_context (framebuffer);

  /* make sure any batched primitives get emitted to the driver
   * before issuing our read pixels...
   */
  _cogl_framebuffer_flush_journal (framebuffer);

  return ctx->driver_vtable->framebuffer_read_pixels_into_bitmap (framebuffer,
                                                                  x, y,
                                                                  source,
                                                                  bitmap,
                                                                  error);
}

CoglBool
cogl_framebuffer_read_pixels_into_bitmap (CoglFramebuffer *framebuffer,
                                          int x,
                                          int y,
                                          CoglReadPixelsFlags source,
                                          CoglBitmap *bitmap)
{
  CoglError *ignore_error = NULL;
  CoglBool status =
    _cogl_framebuffer_read_pixels_into_bitmap (framebuffer,
                                               x, y, source, bitmap,
                                               &ignore_error);
  if (!status)
    cogl_error_free (ignore_error);
  return status;
}

CoglBool
cogl_framebuffer_read_pixels (CoglFramebuffer *framebuffer,
                              int x,
                              int y,
                              int width,
                              int height,
                              CoglPixelFormat format,
                              uint8_t *pixels)
{
  int bpp = _cogl_pixel_format_get_bytes_per_pixel (format);
  CoglBitmap *bitmap;
  CoglBool ret;

  bitmap = cogl_bitmap_new_for_data (framebuffer->context,
                                     width, height,
                                     format,
                                     bpp * width, /* rowstride */
                                     pixels);

  /* Note: we don't try and catch errors here since we created the
   * bitmap storage up-front and can assume we wont hit an
   * out-of-memory error which should be the only exception
   * this api throws.
   */
  ret = _cogl_framebuffer_read_pixels_into_bitmap (framebuffer,
                                                   x, y,
                                                   COGL_READ_PIXELS_COLOR_BUFFER,
                                                   bitmap,
                                                   NULL);
  cogl_object_unref (bitmap);

  return ret;
}

void
_cogl_blit_framebuffer (CoglFramebuffer *src,
                        CoglFramebuffer *dest,
                        int src_x,
                        int src_y,
                        int dst_x,
                        int dst_y,
                        int width,
                        int height)
{
  CoglContext *ctx = src->context;

  _COGL_RETURN_IF_FAIL (ctx->private_feature_flags &
                        COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT);

  /* We can only support blitting between offscreen buffers because
     otherwise we would need to mirror the image and GLES2.0 doesn't
     support this */
  _COGL_RETURN_IF_FAIL (cogl_is_offscreen (src));
  _COGL_RETURN_IF_FAIL (cogl_is_offscreen (dest));
  /* The buffers must be the same format */
  _COGL_RETURN_IF_FAIL (src->format == dest->format);

  /* Make sure the current framebuffers are bound. We explicitly avoid
     flushing the clip state so we can bind our own empty state */
  _cogl_framebuffer_flush_state (dest,
                                 src,
                                 COGL_FRAMEBUFFER_STATE_ALL &
                                 ~COGL_FRAMEBUFFER_STATE_CLIP);

  /* Flush any empty clip stack because glBlitFramebuffer is affected
     by the scissor and we want to hide this feature for the Cogl API
     because it's not obvious to an app how the clip state will affect
     the scissor */
  _cogl_clip_stack_flush (NULL, dest);

  /* XXX: Because we are manually flushing clip state here we need to
   * make sure that the clip state gets updated the next time we flush
   * framebuffer state by marking the current framebuffer's clip state
   * as changed */
  ctx->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_CLIP;

  ctx->glBlitFramebuffer (src_x, src_y,
                          src_x + width, src_y + height,
                          dst_x, dst_y,
                          dst_x + width, dst_y + height,
                          GL_COLOR_BUFFER_BIT,
                          GL_NEAREST);
}

void
cogl_framebuffer_discard_buffers (CoglFramebuffer *framebuffer,
                                  unsigned long buffers)
{
  CoglContext *ctx = framebuffer->context;

  _COGL_RETURN_IF_FAIL (buffers & COGL_BUFFER_BIT_COLOR);

  ctx->driver_vtable->framebuffer_discard_buffers (framebuffer, buffers);
}

void
cogl_framebuffer_finish (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;

  _cogl_framebuffer_flush_journal (framebuffer);

  ctx->driver_vtable->framebuffer_finish (framebuffer);
}

void
cogl_framebuffer_push_matrix (CoglFramebuffer *framebuffer)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  cogl_matrix_stack_push (modelview_stack);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cogl_framebuffer_pop_matrix (CoglFramebuffer *framebuffer)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  cogl_matrix_stack_pop (modelview_stack);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cogl_framebuffer_identity_matrix (CoglFramebuffer *framebuffer)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  cogl_matrix_stack_load_identity (modelview_stack);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cogl_framebuffer_scale (CoglFramebuffer *framebuffer,
                        float x,
                        float y,
                        float z)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  cogl_matrix_stack_scale (modelview_stack, x, y, z);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cogl_framebuffer_translate (CoglFramebuffer *framebuffer,
                            float x,
                            float y,
                            float z)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  cogl_matrix_stack_translate (modelview_stack, x, y, z);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;
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
  cogl_matrix_stack_rotate (modelview_stack, angle, x, y, z);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cogl_framebuffer_rotate_quaternion (CoglFramebuffer *framebuffer,
                                    const CoglQuaternion *quaternion)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  cogl_matrix_stack_rotate_quaternion (modelview_stack, quaternion);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cogl_framebuffer_rotate_euler (CoglFramebuffer *framebuffer,
                               const CoglEuler *euler)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  cogl_matrix_stack_rotate_euler (modelview_stack, euler);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cogl_framebuffer_transform (CoglFramebuffer *framebuffer,
                            const CoglMatrix *matrix)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  cogl_matrix_stack_multiply (modelview_stack, matrix);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;
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

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_PROJECTION;
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

  cogl_matrix_stack_load_identity (projection_stack);

  cogl_matrix_stack_frustum (projection_stack,
                             left,
                             right,
                             bottom,
                             top,
                             z_near,
                             z_far);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_PROJECTION;
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
  cogl_matrix_stack_set (projection_stack, &ortho);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_PROJECTION;
}

void
_cogl_framebuffer_push_projection (CoglFramebuffer *framebuffer)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  cogl_matrix_stack_push (projection_stack);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_PROJECTION;
}

void
_cogl_framebuffer_pop_projection (CoglFramebuffer *framebuffer)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  cogl_matrix_stack_pop (projection_stack);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_PROJECTION;
}

void
cogl_framebuffer_get_modelview_matrix (CoglFramebuffer *framebuffer,
                                       CoglMatrix *matrix)
{
  CoglMatrixEntry *modelview_entry =
    _cogl_framebuffer_get_modelview_entry (framebuffer);
  cogl_matrix_entry_get (modelview_entry, matrix);
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_framebuffer_set_modelview_matrix (CoglFramebuffer *framebuffer,
                                       const CoglMatrix *matrix)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  cogl_matrix_stack_set (modelview_stack, matrix);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_MODELVIEW;

  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_framebuffer_get_projection_matrix (CoglFramebuffer *framebuffer,
                                        CoglMatrix *matrix)
{
  CoglMatrixEntry *projection_entry =
    _cogl_framebuffer_get_projection_entry (framebuffer);
  cogl_matrix_entry_get (projection_entry, matrix);
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_framebuffer_set_projection_matrix (CoglFramebuffer *framebuffer,
                                        const CoglMatrix *matrix)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);

  /* XXX: The projection matrix isn't currently tracked in the journal
   * so we need to flush all journaled primitives first... */
  _cogl_framebuffer_flush_journal (framebuffer);

  cogl_matrix_stack_set (projection_stack, matrix);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_PROJECTION;

  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_framebuffer_push_scissor_clip (CoglFramebuffer *framebuffer,
                                    int x,
                                    int y,
                                    int width,
                                    int height)
{
  CoglClipState *clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  clip_state->stacks->data =
    _cogl_clip_stack_push_window_rectangle (clip_state->stacks->data,
                                            x, y, width, height);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_CLIP;
}

void
cogl_framebuffer_push_rectangle_clip (CoglFramebuffer *framebuffer,
                                      float x_1,
                                      float y_1,
                                      float x_2,
                                      float y_2)
{
  CoglClipState *clip_state = _cogl_framebuffer_get_clip_state (framebuffer);
  CoglMatrixEntry *modelview_entry =
    _cogl_framebuffer_get_modelview_entry (framebuffer);
  CoglMatrixEntry *projection_entry =
    _cogl_framebuffer_get_projection_entry (framebuffer);
  /* XXX: It would be nicer if we stored the private viewport as a
   * vec4 so we could avoid this redundant copy. */
  float viewport[] = {
      framebuffer->viewport_x,
      framebuffer->viewport_y,
      framebuffer->viewport_width,
      framebuffer->viewport_height
  };

  clip_state->stacks->data =
    _cogl_clip_stack_push_rectangle (clip_state->stacks->data,
                                     x_1, y_1, x_2, y_2,
                                     modelview_entry,
                                     projection_entry,
                                     viewport);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_CLIP;
}

void
cogl_framebuffer_push_path_clip (CoglFramebuffer *framebuffer,
                                 CoglPath *path)
{
  CoglClipState *clip_state = _cogl_framebuffer_get_clip_state (framebuffer);
  CoglMatrixEntry *modelview_entry =
    _cogl_framebuffer_get_modelview_entry (framebuffer);
  CoglMatrixEntry *projection_entry =
    _cogl_framebuffer_get_projection_entry (framebuffer);
  /* XXX: It would be nicer if we stored the private viewport as a
   * vec4 so we could avoid this redundant copy. */
  float viewport[] = {
      framebuffer->viewport_x,
      framebuffer->viewport_y,
      framebuffer->viewport_width,
      framebuffer->viewport_height
  };

  clip_state->stacks->data =
    _cogl_clip_stack_push_from_path (clip_state->stacks->data,
                                     path,
                                     modelview_entry,
                                     projection_entry,
                                     viewport);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_CLIP;
}

void
cogl_framebuffer_push_primitive_clip (CoglFramebuffer *framebuffer,
                                      CoglPrimitive *primitive,
                                      float bounds_x1,
                                      float bounds_y1,
                                      float bounds_x2,
                                      float bounds_y2)
{
  CoglClipState *clip_state = _cogl_framebuffer_get_clip_state (framebuffer);
  CoglMatrixEntry *modelview_entry =
    _cogl_framebuffer_get_modelview_entry (framebuffer);
  CoglMatrixEntry *projection_entry =
    _cogl_framebuffer_get_projection_entry (framebuffer);
  /* XXX: It would be nicer if we stored the private viewport as a
   * vec4 so we could avoid this redundant copy. */
  float viewport[] = {
      framebuffer->viewport_x,
      framebuffer->viewport_y,
      framebuffer->viewport_width,
      framebuffer->viewport_height
  };

  clip_state->stacks->data =
    _cogl_clip_stack_push_primitive (clip_state->stacks->data,
                                     primitive,
                                     bounds_x1, bounds_y1,
                                     bounds_x2, bounds_y2,
                                     modelview_entry,
                                     projection_entry,
                                     viewport);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_CLIP;
}

void
cogl_framebuffer_pop_clip (CoglFramebuffer *framebuffer)
{
  CoglClipState *clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  clip_state->stacks->data = _cogl_clip_stack_pop (clip_state->stacks->data);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_CLIP;
}

void
_cogl_framebuffer_save_clip_stack (CoglFramebuffer *framebuffer)
{
  CoglClipState *clip_state = _cogl_framebuffer_get_clip_state (framebuffer);
  _cogl_clip_state_save_clip_stack (clip_state);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_CLIP;
}

void
_cogl_framebuffer_restore_clip_stack (CoglFramebuffer *framebuffer)
{
  CoglClipState *clip_state = _cogl_framebuffer_get_clip_state (framebuffer);
  _cogl_clip_state_restore_clip_stack (clip_state);

  if (framebuffer->context->current_draw_buffer == framebuffer)
    framebuffer->context->current_draw_buffer_changes |=
      COGL_FRAMEBUFFER_STATE_CLIP;
}

void
_cogl_framebuffer_unref (CoglFramebuffer *framebuffer)
{
  /* The journal holds a reference to the framebuffer whenever it is
     non-empty. Therefore if the journal is non-empty and we will have
     exactly one reference then we know the journal is the only thing
     keeping the framebuffer alive. In that case we want to flush the
     journal and let the framebuffer die. It is fine at this point if
     flushing the journal causes something else to take a reference to
     it and it comes back to life */
  if (framebuffer->journal->entries->len > 0)
    {
      unsigned int ref_count = ((CoglObject *) framebuffer)->ref_count;

      /* There should be at least two references - the one we are
         about to drop and the one held by the journal */
      if (ref_count < 2)
        g_warning ("Inconsistent ref count on a framebuffer with journal "
                   "entries.");

      if (ref_count == 2)
        _cogl_framebuffer_flush_journal (framebuffer);
    }

  /* Chain-up */
  _cogl_object_default_unref (framebuffer);
}

#ifdef COGL_ENABLE_DEBUG
static int
get_index (void *indices,
           CoglIndicesType type,
           int _index)
{
  if (!indices)
    return _index;

  switch (type)
    {
    case COGL_INDICES_TYPE_UNSIGNED_BYTE:
      return ((uint8_t *)indices)[_index];
    case COGL_INDICES_TYPE_UNSIGNED_SHORT:
      return ((uint16_t *)indices)[_index];
    case COGL_INDICES_TYPE_UNSIGNED_INT:
      return ((uint32_t *)indices)[_index];
    }

  g_return_val_if_reached (0);
}

static void
add_line (uint32_t *line_indices,
          int base,
          void *user_indices,
          CoglIndicesType user_indices_type,
          int index0,
          int index1,
          int *pos)
{
  index0 = get_index (user_indices, user_indices_type, index0);
  index1 = get_index (user_indices, user_indices_type, index1);

  line_indices[(*pos)++] = base + index0;
  line_indices[(*pos)++] = base + index1;
}

static int
get_line_count (CoglVerticesMode mode, int n_vertices)
{
  if (mode == COGL_VERTICES_MODE_TRIANGLES &&
      (n_vertices % 3) == 0)
    {
      return n_vertices;
    }
  else if (mode == COGL_VERTICES_MODE_TRIANGLE_FAN &&
           n_vertices >= 3)
    {
      return 2 * n_vertices - 3;
    }
  else if (mode == COGL_VERTICES_MODE_TRIANGLE_STRIP &&
           n_vertices >= 3)
    {
      return 2 * n_vertices - 3;
    }
    /* In the journal we are a bit sneaky and actually use GL_QUADS
     * which isn't actually a valid CoglVerticesMode! */
#ifdef HAVE_COGL_GL
  else if (mode == GL_QUADS && (n_vertices % 4) == 0)
    {
      return n_vertices;
    }
#endif

  g_return_val_if_reached (0);
}

static CoglIndices *
get_wire_line_indices (CoglContext *ctx,
                       CoglVerticesMode mode,
                       int first_vertex,
                       int n_vertices_in,
                       CoglIndices *user_indices,
                       int *n_indices)
{
  int n_lines;
  uint32_t *line_indices;
  CoglIndexBuffer *index_buffer;
  void *indices;
  CoglIndicesType indices_type;
  int base = first_vertex;
  int pos;
  int i;
  CoglIndices *ret;

  if (user_indices)
    {
      index_buffer = cogl_indices_get_buffer (user_indices);
      indices = _cogl_buffer_map (COGL_BUFFER (index_buffer),
                                  COGL_BUFFER_ACCESS_READ, 0,
                                  NULL);
      indices_type = cogl_indices_get_type (user_indices);
    }
  else
    {
      index_buffer = NULL;
      indices = NULL;
      indices_type = COGL_INDICES_TYPE_UNSIGNED_BYTE;
    }

  n_lines = get_line_count (mode, n_vertices_in);

  /* Note: we are using COGL_INDICES_TYPE_UNSIGNED_INT so 4 bytes per index. */
  line_indices = g_malloc (4 * n_lines * 2);

  pos = 0;

  if (mode == COGL_VERTICES_MODE_TRIANGLES &&
      (n_vertices_in % 3) == 0)
    {
      for (i = 0; i < n_vertices_in; i += 3)
        {
          add_line (line_indices, base, indices, indices_type, i,   i+1, &pos);
          add_line (line_indices, base, indices, indices_type, i+1, i+2, &pos);
          add_line (line_indices, base, indices, indices_type, i+2, i,   &pos);
        }
    }
  else if (mode == COGL_VERTICES_MODE_TRIANGLE_FAN &&
           n_vertices_in >= 3)
    {
      add_line (line_indices, base, indices, indices_type, 0, 1, &pos);
      add_line (line_indices, base, indices, indices_type, 1, 2, &pos);
      add_line (line_indices, base, indices, indices_type, 0, 2, &pos);

      for (i = 3; i < n_vertices_in; i++)
        {
          add_line (line_indices, base, indices, indices_type, i - 1, i, &pos);
          add_line (line_indices, base, indices, indices_type, 0,     i, &pos);
        }
    }
  else if (mode == COGL_VERTICES_MODE_TRIANGLE_STRIP &&
           n_vertices_in >= 3)
    {
      add_line (line_indices, base, indices, indices_type, 0, 1, &pos);
      add_line (line_indices, base, indices, indices_type, 1, 2, &pos);
      add_line (line_indices, base, indices, indices_type, 0, 2, &pos);

      for (i = 3; i < n_vertices_in; i++)
        {
          add_line (line_indices, base, indices, indices_type, i - 1, i, &pos);
          add_line (line_indices, base, indices, indices_type, i - 2, i, &pos);
        }
    }
    /* In the journal we are a bit sneaky and actually use GL_QUADS
     * which isn't actually a valid CoglVerticesMode! */
#ifdef HAVE_COGL_GL
  else if (mode == GL_QUADS && (n_vertices_in % 4) == 0)
    {
      for (i = 0; i < n_vertices_in; i += 4)
        {
          add_line (line_indices,
                    base, indices, indices_type, i,     i + 1, &pos);
          add_line (line_indices,
                    base, indices, indices_type, i + 1, i + 2, &pos);
          add_line (line_indices,
                    base, indices, indices_type, i + 2, i + 3, &pos);
          add_line (line_indices,
                    base, indices, indices_type, i + 3, i,     &pos);
        }
    }
#endif

  if (user_indices)
    cogl_buffer_unmap (COGL_BUFFER (index_buffer));

  *n_indices = n_lines * 2;

  ret = cogl_indices_new (ctx,
                          COGL_INDICES_TYPE_UNSIGNED_INT,
                          line_indices,
                          *n_indices);

  g_free (line_indices);

  return ret;
}

static CoglBool
remove_layer_cb (CoglPipeline *pipeline,
                 int layer_index,
                 void *user_data)
{
  cogl_pipeline_remove_layer (pipeline, layer_index);
  return TRUE;
}

static void
pipeline_destroyed_cb (CoglPipeline *weak_pipeline, void *user_data)
{
  CoglPipeline *original_pipeline = user_data;

  /* XXX: I think we probably need to provide a custom unref function for
   * CoglPipeline because it's possible that we will reach this callback
   * because original_pipeline is being freed which means cogl_object_unref
   * will have already freed any associated user data.
   *
   * Setting more user data here will *probably* succeed but that may allocate
   * a new user-data array which could be leaked.
   *
   * Potentially we could have a _cogl_object_free_user_data function so
   * that a custom unref function could be written that can destroy weak
   * pipeline children before removing user data.
   */
  cogl_object_set_user_data (COGL_OBJECT (original_pipeline),
                             &wire_pipeline_key, NULL, NULL);

  cogl_object_unref (weak_pipeline);
}

static void
draw_wireframe (CoglContext *ctx,
                CoglFramebuffer *framebuffer,
                CoglPipeline *pipeline,
                CoglVerticesMode mode,
                int first_vertex,
                int n_vertices,
                CoglAttribute **attributes,
                int n_attributes,
                CoglIndices *indices,
                CoglDrawFlags flags)
{
  CoglIndices *wire_indices;
  CoglPipeline *wire_pipeline;
  int n_indices;

  wire_indices = get_wire_line_indices (ctx,
                                        mode,
                                        first_vertex,
                                        n_vertices,
                                        indices,
                                        &n_indices);

  wire_pipeline = cogl_object_get_user_data (COGL_OBJECT (pipeline),
                                             &wire_pipeline_key);

  if (!wire_pipeline)
    {
      wire_pipeline =
        _cogl_pipeline_weak_copy (pipeline, pipeline_destroyed_cb, NULL);

      cogl_object_set_user_data (COGL_OBJECT (pipeline),
                                 &wire_pipeline_key, wire_pipeline,
                                 NULL);

      /* If we have glsl then the pipeline may have an associated
       * vertex program and since we'd like to see the results of the
       * vertex program in the wireframe we just add a final clobber
       * of the wire color leaving the rest of the state untouched. */
      if (cogl_has_feature (framebuffer->context, COGL_FEATURE_ID_GLSL))
        {
          static CoglSnippet *snippet = NULL;

          /* The snippet is cached so that it will reuse the program
           * from the pipeline cache if possible */
          if (snippet == NULL)
            {
              snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                          NULL,
                                          NULL);
              cogl_snippet_set_replace (snippet,
                                        "cogl_color_out = "
                                        "vec4 (0.0, 1.0, 0.0, 1.0);\n");
            }

          cogl_pipeline_add_snippet (wire_pipeline, snippet);
        }
      else
        {
          cogl_pipeline_foreach_layer (wire_pipeline, remove_layer_cb, NULL);
          cogl_pipeline_set_color4f (wire_pipeline, 0, 1, 0, 1);
        }
    }

  /* temporarily disable the wireframe to avoid recursion! */
  flags |= COGL_DRAW_SKIP_DEBUG_WIREFRAME;
  _cogl_framebuffer_draw_indexed_attributes (
                                           framebuffer,
                                           wire_pipeline,
                                           COGL_VERTICES_MODE_LINES,
                                           0,
                                           n_indices,
                                           wire_indices,
                                           attributes,
                                           n_attributes,
                                           flags);
  COGL_DEBUG_SET_FLAG (COGL_DEBUG_WIREFRAME);

  cogl_object_unref (wire_indices);
}
#endif

/* This can be called directly by the CoglJournal to draw attributes
 * skipping the implicit journal flush, the framebuffer flush and
 * pipeline validation. */
void
_cogl_framebuffer_draw_attributes (CoglFramebuffer *framebuffer,
                                   CoglPipeline *pipeline,
                                   CoglVerticesMode mode,
                                   int first_vertex,
                                   int n_vertices,
                                   CoglAttribute **attributes,
                                   int n_attributes,
                                   CoglDrawFlags flags)
{
#ifdef COGL_ENABLE_DEBUG
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_WIREFRAME) &&
                  (flags & COGL_DRAW_SKIP_DEBUG_WIREFRAME) == 0) &&
      mode != COGL_VERTICES_MODE_LINES &&
      mode != COGL_VERTICES_MODE_LINE_LOOP &&
      mode != COGL_VERTICES_MODE_LINE_STRIP)
    draw_wireframe (framebuffer->context,
                    framebuffer, pipeline,
                    mode, first_vertex, n_vertices,
                    attributes, n_attributes, NULL,
                    flags);
  else
#endif
    {
      CoglContext *ctx = framebuffer->context;

      ctx->driver_vtable->framebuffer_draw_attributes (framebuffer,
                                                       pipeline,
                                                       mode,
                                                       first_vertex,
                                                       n_vertices,
                                                       attributes,
                                                       n_attributes,
                                                       flags);
    }
}

void
cogl_framebuffer_draw_attributes (CoglFramebuffer *framebuffer,
                                  CoglPipeline *pipeline,
                                  CoglVerticesMode mode,
                                  int first_vertex,
                                  int n_vertices,
                                  CoglAttribute **attributes,
                                  int n_attributes)
{
  _cogl_framebuffer_draw_attributes (framebuffer,
                                     pipeline,
                                     mode,
                                     first_vertex,
                                     n_vertices,
                                     attributes, n_attributes,
                                     COGL_DRAW_SKIP_LEGACY_STATE);
}

void
cogl_framebuffer_vdraw_attributes (CoglFramebuffer *framebuffer,
                                   CoglPipeline *pipeline,
                                   CoglVerticesMode mode,
                                   int first_vertex,
                                   int n_vertices,
                                   ...)
{
  va_list ap;
  int n_attributes;
  CoglAttribute *attribute;
  CoglAttribute **attributes;
  int i;

  va_start (ap, n_vertices);
  for (n_attributes = 0; va_arg (ap, CoglAttribute *); n_attributes++)
    ;
  va_end (ap);

  attributes = g_alloca (sizeof (CoglAttribute *) * n_attributes);

  va_start (ap, n_vertices);
  for (i = 0; (attribute = va_arg (ap, CoglAttribute *)); i++)
    attributes[i] = attribute;
  va_end (ap);

  _cogl_framebuffer_draw_attributes (framebuffer,
                                     pipeline,
                                     mode, first_vertex, n_vertices,
                                     attributes, n_attributes,
                                     COGL_DRAW_SKIP_LEGACY_STATE);
}

void
_cogl_framebuffer_draw_indexed_attributes (CoglFramebuffer *framebuffer,
                                           CoglPipeline *pipeline,
                                           CoglVerticesMode mode,
                                           int first_vertex,
                                           int n_vertices,
                                           CoglIndices *indices,
                                           CoglAttribute **attributes,
                                           int n_attributes,
                                           CoglDrawFlags flags)
{
#ifdef COGL_ENABLE_DEBUG
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_WIREFRAME) &&
                  (flags & COGL_DRAW_SKIP_DEBUG_WIREFRAME) == 0) &&
      mode != COGL_VERTICES_MODE_LINES &&
      mode != COGL_VERTICES_MODE_LINE_LOOP &&
      mode != COGL_VERTICES_MODE_LINE_STRIP)
    draw_wireframe (framebuffer->context,
                    framebuffer, pipeline,
                    mode, first_vertex, n_vertices,
                    attributes, n_attributes, indices,
                    flags);
  else
#endif
    {
      CoglContext *ctx = framebuffer->context;

      ctx->driver_vtable->framebuffer_draw_indexed_attributes (framebuffer,
                                                               pipeline,
                                                               mode,
                                                               first_vertex,
                                                               n_vertices,
                                                               indices,
                                                               attributes,
                                                               n_attributes,
                                                               flags);
    }
}

void
cogl_framebuffer_draw_indexed_attributes (CoglFramebuffer *framebuffer,
                                          CoglPipeline *pipeline,
                                          CoglVerticesMode mode,
                                          int first_vertex,
                                          int n_vertices,
                                          CoglIndices *indices,
                                          CoglAttribute **attributes,
                                          int n_attributes)
{
  _cogl_framebuffer_draw_indexed_attributes (framebuffer,
                                             pipeline,
                                             mode, first_vertex,
                                             n_vertices, indices,
                                             attributes, n_attributes,
                                             COGL_DRAW_SKIP_LEGACY_STATE);
}

void
cogl_framebuffer_vdraw_indexed_attributes (CoglFramebuffer *framebuffer,
                                           CoglPipeline *pipeline,
                                           CoglVerticesMode mode,
                                           int first_vertex,
                                           int n_vertices,
                                           CoglIndices *indices,
                                           ...)
{
  va_list ap;
  int n_attributes;
  CoglAttribute **attributes;
  int i;
  CoglAttribute *attribute;

  va_start (ap, indices);
  for (n_attributes = 0; va_arg (ap, CoglAttribute *); n_attributes++)
    ;
  va_end (ap);

  attributes = g_alloca (sizeof (CoglAttribute *) * n_attributes);

  va_start (ap, indices);
  for (i = 0; (attribute = va_arg (ap, CoglAttribute *)); i++)
    attributes[i] = attribute;
  va_end (ap);

  _cogl_framebuffer_draw_indexed_attributes (framebuffer,
                                             pipeline,
                                             mode,
                                             first_vertex,
                                             n_vertices,
                                             indices,
                                             attributes,
                                             n_attributes,
                                             COGL_DRAW_SKIP_LEGACY_STATE);
}

void
_cogl_framebuffer_draw_primitive (CoglFramebuffer *framebuffer,
                                  CoglPipeline *pipeline,
                                  CoglPrimitive *primitive,
                                  CoglDrawFlags flags)
{
  if (primitive->indices)
    _cogl_framebuffer_draw_indexed_attributes (framebuffer,
                                               pipeline,
                                               primitive->mode,
                                               primitive->first_vertex,
                                               primitive->n_vertices,
                                               primitive->indices,
                                               primitive->attributes,
                                               primitive->n_attributes,
                                               flags);
  else
    _cogl_framebuffer_draw_attributes (framebuffer,
                                       pipeline,
                                       primitive->mode,
                                       primitive->first_vertex,
                                       primitive->n_vertices,
                                       primitive->attributes,
                                       primitive->n_attributes,
                                       flags);
}

void
cogl_framebuffer_draw_primitive (CoglFramebuffer *framebuffer,
                                 CoglPipeline *pipeline,
                                 CoglPrimitive *primitive)
{
  _cogl_framebuffer_draw_primitive (framebuffer, pipeline, primitive,
                                    COGL_DRAW_SKIP_LEGACY_STATE);
}

void
cogl_framebuffer_draw_rectangle (CoglFramebuffer *framebuffer,
                                 CoglPipeline *pipeline,
                                 float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2)
{
  const float position[4] = {x_1, y_1, x_2, y_2};
  CoglMultiTexturedRect rect;

  /* XXX: All the _*_rectangle* APIs normalize their input into an array of
   * _CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_framebuffer_draw_multitextured_rectangles.
   */

  rect.position = position;
  rect.tex_coords = NULL;
  rect.tex_coords_len = 0;

  _cogl_framebuffer_draw_multitextured_rectangles (framebuffer,
                                                   pipeline,
                                                   &rect,
                                                   1,
                                                   TRUE);
}

void
cogl_framebuffer_draw_textured_rectangle (CoglFramebuffer *framebuffer,
                                          CoglPipeline *pipeline,
                                          float x_1,
                                          float y_1,
                                          float x_2,
                                          float y_2,
                                          float s_1,
                                          float t_1,
                                          float s_2,
                                          float t_2)
{
  const float position[4] = {x_1, y_1, x_2, y_2};
  const float tex_coords[4] = {s_1, t_1, s_2, t_2};
  CoglMultiTexturedRect rect;

  /* XXX: All the _*_rectangle* APIs normalize their input into an array of
   * CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_framebuffer_draw_multitextured_rectangles.
   */

  rect.position = position;
  rect.tex_coords = tex_coords;
  rect.tex_coords_len = 4;

  _cogl_framebuffer_draw_multitextured_rectangles (framebuffer,
                                                   pipeline,
                                                   &rect,
                                                   1,
                                                   TRUE);
}

void
cogl_framebuffer_draw_multitextured_rectangle (CoglFramebuffer *framebuffer,
                                               CoglPipeline *pipeline,
                                               float x_1,
                                               float y_1,
                                               float x_2,
                                               float y_2,
                                               const float *tex_coords,
                                               int tex_coords_len)
{
  const float position[4] = {x_1, y_1, x_2, y_2};
  CoglMultiTexturedRect rect;

  /* XXX: All the _*_rectangle* APIs normalize their input into an array of
   * CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_framebuffer_draw_multitextured_rectangles.
   */

  rect.position = position;
  rect.tex_coords = tex_coords;
  rect.tex_coords_len = tex_coords_len;

  _cogl_framebuffer_draw_multitextured_rectangles (framebuffer,
                                                   pipeline,
                                                   &rect,
                                                   1,
                                                   TRUE);
}

void
cogl_framebuffer_draw_rectangles (CoglFramebuffer *framebuffer,
                                  CoglPipeline *pipeline,
                                  const float *coordinates,
                                  unsigned int n_rectangles)
{
  CoglMultiTexturedRect *rects;
  int i;

  /* XXX: All the _*_rectangle* APIs normalize their input into an array of
   * CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_framebuffer_draw_multitextured_rectangles.
   */

  rects = g_alloca (n_rectangles * sizeof (CoglMultiTexturedRect));

  for (i = 0; i < n_rectangles; i++)
    {
      rects[i].position = &coordinates[i * 4];
      rects[i].tex_coords = NULL;
      rects[i].tex_coords_len = 0;
    }

  _cogl_framebuffer_draw_multitextured_rectangles (framebuffer,
                                                   pipeline,
                                                   rects,
                                                   n_rectangles,
                                                   TRUE);
}

void
cogl_framebuffer_draw_textured_rectangles (CoglFramebuffer *framebuffer,
                                           CoglPipeline *pipeline,
                                           const float *coordinates,
                                           unsigned int n_rectangles)
{
  CoglMultiTexturedRect *rects;
  int i;

  /* XXX: All the _*_rectangle* APIs normalize their input into an array of
   * _CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_framebuffer_draw_multitextured_rectangles.
   */

  rects = g_alloca (n_rectangles * sizeof (CoglMultiTexturedRect));

  for (i = 0; i < n_rectangles; i++)
    {
      rects[i].position = &coordinates[i * 8];
      rects[i].tex_coords = &coordinates[i * 8 + 4];
      rects[i].tex_coords_len = 4;
    }

  _cogl_framebuffer_draw_multitextured_rectangles (framebuffer,
                                                   pipeline,
                                                   rects,
                                                   n_rectangles,
                                                   TRUE);
}

void
cogl_framebuffer_fill_path (CoglFramebuffer *framebuffer,
                            CoglPipeline *pipeline,
                            CoglPath *path)
{
  _COGL_RETURN_IF_FAIL (cogl_is_framebuffer (framebuffer));
  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));
  _COGL_RETURN_IF_FAIL (cogl_is_path (path));

  _cogl_path_fill_nodes (path, framebuffer, pipeline, 0 /* flags */);
}

void
cogl_framebuffer_stroke_path (CoglFramebuffer *framebuffer,
                              CoglPipeline *pipeline,
                              CoglPath *path)
{
  _COGL_RETURN_IF_FAIL (cogl_is_framebuffer (framebuffer));
  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));
  _COGL_RETURN_IF_FAIL (cogl_is_path (path));

  _cogl_path_stroke_nodes (path, framebuffer, pipeline);
}
