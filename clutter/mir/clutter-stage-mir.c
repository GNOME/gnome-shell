/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2014 Canonical Ltd.
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
 * Authors:
 *  Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-mir.h"
#include "clutter-stage-mir.h"
#include "clutter-backend-mir-priv.h"
#include "clutter-stage-private.h"
#include "clutter-mir.h"
#include <cogl/cogl.h>

#define clutter_stage_mir_get_type _clutter_stage_mir_get_type

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);
static void clutter_stage_mir_set_fullscreen (ClutterStageWindow *stage_window,
                                              gboolean            fullscreen);
static void clutter_stage_mir_set_cursor_visible (ClutterStageWindow *stage_window,
                                                  gboolean            cursor_visible);

G_DEFINE_TYPE_WITH_CODE (ClutterStageMir,
                         clutter_stage_mir,
                         CLUTTER_TYPE_STAGE_COGL,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
on_stage_resized (CoglOnscreen *onscreen,
                  int width,
                  int height,
                  void *user_data)
{
  clutter_actor_set_size (CLUTTER_ACTOR (user_data), width, height);
}

static gboolean
clutter_stage_mir_realize (ClutterStageWindow *stage_window)
{
  ClutterStageMir *stage_mir = CLUTTER_STAGE_MIR (stage_window);
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  MirSurface *mir_surface;

  if (!clutter_stage_window_parent_iface->realize (stage_window))
    return FALSE;

  cogl_framebuffer_allocate (COGL_FRAMEBUFFER (stage_cogl->onscreen), NULL);
  mir_surface = cogl_mir_onscreen_get_surface (stage_cogl->onscreen);

  if (!mir_surface_is_valid (mir_surface))
    {
      g_warning ("Realized Mir surface not valid");
      return FALSE;
    }

  if (!stage_mir->foreign_mir_surface)
    {
      cogl_onscreen_add_resize_callback (stage_cogl->onscreen, on_stage_resized,
                                         stage_cogl->wrapper, NULL);
    }

  if (stage_mir->surface_state == mir_surface_state_fullscreen)
    {
      clutter_stage_mir_set_fullscreen (stage_window, TRUE);
      stage_mir->surface_state = mir_surface_state_unknown;
    }

  if (!stage_mir->cursor_visible)
    {
      clutter_stage_mir_set_cursor_visible (stage_window, FALSE);
    }

  return TRUE;
}

static void
clutter_stage_mir_show (ClutterStageWindow *stage_window,
                        gboolean            do_raise)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  cogl_onscreen_show (stage_cogl->onscreen);
  clutter_actor_map (CLUTTER_ACTOR (stage_cogl->wrapper));
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage_cogl->wrapper));
}

static void
clutter_stage_mir_hide (ClutterStageWindow *stage_window)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  cogl_onscreen_hide (stage_cogl->onscreen);
  clutter_actor_unmap (CLUTTER_ACTOR (stage_cogl->wrapper));
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage_cogl->wrapper));
}

static void
clutter_stage_mir_set_cursor_visible (ClutterStageWindow *stage_window,
                                      gboolean            cursor_visible)
{
  ClutterStageMir *stage_mir = CLUTTER_STAGE_MIR (stage_window);
  ClutterActor *actor = _clutter_stage_window_get_wrapper (stage_window);
  MirSurface *surface = clutter_mir_stage_get_mir_surface ((ClutterStage *) actor);
  MirCursorConfiguration *cursor_conf;

  if (mir_surface_is_valid (surface))
    {
      cursor_conf = mir_cursor_configuration_from_name (cursor_visible ?
                                                        mir_default_cursor_name :
                                                        mir_disabled_cursor_name);
      mir_surface_configure_cursor (surface, cursor_conf);
      mir_cursor_configuration_destroy (cursor_conf);
    }

  stage_mir->cursor_visible = cursor_visible;
}

static void
clutter_stage_mir_set_fullscreen (ClutterStageWindow *stage_window,
                                  gboolean            fullscreen)
{
  ClutterStageMir *stage_mir = CLUTTER_STAGE_MIR (stage_window);
  ClutterActor *actor = _clutter_stage_window_get_wrapper (stage_window);
  MirSurface *surface = clutter_mir_stage_get_mir_surface ((ClutterStage *) actor);

  if (!mir_surface_is_valid (surface))
    {
      stage_mir->surface_state = fullscreen ?
                                 mir_surface_state_fullscreen :
                                 mir_surface_state_unknown;
    }
  else
    {
      if (fullscreen)
        {
          stage_mir->surface_state = mir_surface_get_state (surface);

          if (stage_mir->surface_state != mir_surface_state_fullscreen)
            mir_wait_for (mir_surface_set_state (surface,
                                                 mir_surface_state_fullscreen));
        }
      else if (mir_surface_get_state (surface) == mir_surface_state_fullscreen)
        {
          mir_wait_for (mir_surface_set_state (surface, stage_mir->surface_state));
        }
    }
}

static void
clutter_stage_mir_resize (ClutterStageWindow *stage_window,
                          gint                width,
                          gint                height)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  if (stage_cogl->onscreen)
    {
      cogl_mir_onscreen_resize (stage_cogl->onscreen, width, height);
      clutter_actor_queue_redraw (CLUTTER_ACTOR (stage_cogl->wrapper));
    }
}

static gboolean
clutter_stage_mir_can_clip_redraws (ClutterStageWindow *stage_window)
{
  return TRUE;
}

static void
clutter_stage_mir_init (ClutterStageMir *stage_mir)
{
  stage_mir->cursor_visible = TRUE;
  stage_mir->surface_state = mir_surface_state_unknown;
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_window_parent_iface = g_type_interface_peek_parent (iface);

  iface->realize = clutter_stage_mir_realize;
  iface->show = clutter_stage_mir_show;
  iface->hide = clutter_stage_mir_hide;
  iface->set_fullscreen = clutter_stage_mir_set_fullscreen;
  iface->set_cursor_visible = clutter_stage_mir_set_cursor_visible;
  iface->resize = clutter_stage_mir_resize;
  iface->can_clip_redraws = clutter_stage_mir_can_clip_redraws;
}

static void
clutter_stage_mir_class_init (ClutterStageMirClass *klass)
{
}

/**
 * clutter_mir_stage_get_mir_surface: (skip)
 * @stage: a #ClutterStage
 *
 * Access the underlying data structure representing the surface that is
 * backing the #ClutterStage
 *
 * Note: this function can only be called when running on the Mir
 * platform. Calling this function at any other time will return %NULL.
 *
 * Returns: (transfer none): the Mir surface associated with @stage
 *
 * Since: 1.22
 */
MirSurface *
clutter_mir_stage_get_mir_surface (ClutterStage *stage)
{
  ClutterStageWindow *stage_window = _clutter_stage_get_window (stage);
  ClutterStageCogl *stage_cogl;

  if (!CLUTTER_IS_STAGE_COGL (stage_window))
    return NULL;

  stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  if (!cogl_is_onscreen (stage_cogl->onscreen))
    return NULL;

  return cogl_mir_onscreen_get_surface (stage_cogl->onscreen);
}

/**
 * clutter_mir_stage_set_mir_surface:
 * @stage: a #ClutterStage
 * @surface: A Mir surface to associate with the @stage.
 *
 * Allows you to explicitly provide an existing Mir surface to associate
 * with @stage, preventing Cogl from allocating a surface and shell surface for
 * the stage automatically.
 *
 * This function must be called before @stage is shown.
 *
 * Note: this function can only be called when running on the Mir
 * platform. Calling this function at any other time has no effect.
 *
 * Since: 1.22
 */
void
clutter_mir_stage_set_mir_surface (ClutterStage *stage,
                                   MirSurface *surface)
{
  ClutterStageWindow *stage_window = _clutter_stage_get_window (stage);
  ClutterStageCogl *stage_cogl;

  if (!CLUTTER_IS_STAGE_MIR (stage_window))
    return;

  g_return_if_fail (mir_surface_is_valid (surface));

  stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  if (stage_cogl->onscreen == NULL)
    {
      ClutterBackend *backend = clutter_get_default_backend ();

      /* Use the same default dimensions as clutter_stage_cogl_realize() */
      stage_cogl->onscreen = cogl_onscreen_new (backend->cogl_context,
                                                800, 600);

      cogl_mir_onscreen_set_foreign_surface (stage_cogl->onscreen, surface);
      CLUTTER_STAGE_MIR (stage_window)->foreign_mir_surface = TRUE;
    }
  else
    g_warning (G_STRLOC ": cannot set foreign surface for stage");
}
