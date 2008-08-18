/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file effects.h "Special effects" other than compositor effects.
 * 
 * Before we had a serious compositor, we supported swooping
 * rectangles for minimising and so on.  These are still supported
 * today, even when the compositor is enabled.  The file contains two
 * parts:
 *
 *  1) A set of functions, each of which implements a special effect.
 *     (Only the minimize function does anything interesting; we should
 *      probably get rid of the rest.)
 *
 *  2) A set of functions for moving a highlighted wireframe box around
 *     the screen, optionally with height and width shown in the middle.
 *     This is used for moving and resizing when reduced_resources is set.
 *
 * There was formerly a system which allowed callers to drop in their
 * own handlers for various things; it was never used (people who want
 * their own handlers can just modify this file, after all) and it added
 * a good deal of extra complexity, so it has been removed.  If you want it,
 * it can be found in svn r3769.
 */

/* 
 * Copyright (C) 2001 Anders Carlsson, Havoc Pennington
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

#ifndef META_EFFECTS_H
#define META_EFFECTS_H

#include "util.h"
#include "screen-private.h"

typedef enum
{
  META_EFFECT_MINIMIZE,
  META_EFFECT_UNMINIMIZE,
  META_EFFECT_FOCUS,
  META_EFFECT_CLOSE,
  META_NUM_EFFECTS
} MetaEffectType;

/**
 * A callback which will be called when the effect has finished.
 */
typedef void (* MetaEffectFinished) (gpointer    data);

/**
 * Performs the minimize effect.
 *
 * \param window       The window we're moving
 * \param window_rect  Its current state
 * \param target       Where it should end up
 * \param finished     Callback for when it's finished
 * \param data         Data for callback
 */
void        meta_effect_run_minimize     (MetaWindow         *window,
                                          MetaRectangle	     *window_rect,
                                          MetaRectangle	     *target,
                                          MetaEffectFinished  finished,
                                          gpointer            data);

/**
 * Performs the unminimize effect.  There is no such effect.
 * FIXME: delete this.
 *
 * \param window       The window we're moving
 * \param icon_rect    Its current state
 * \param window_rect  Where it should end up
 * \param finished     Callback for when it's finished
 * \param data         Data for callback
 */
void        meta_effect_run_unminimize (MetaWindow         *window,
                                          MetaRectangle      *window_rect,
                                          MetaRectangle      *icon_rect,
                                          MetaEffectFinished  finished,
                                          gpointer            data);

/**
 * Performs the close effect.  There is no such effect.
 * FIXME: delete this.
 *
 * \param window       The window we're moving
 * \param finished     Callback for when it's finished
 * \param data         Data for callback
 */
void        meta_effect_run_close        (MetaWindow         *window,
                                          MetaEffectFinished  finished,
                                          gpointer            data);

/**
 * Performs the focus effect.  There is no such effect.
 * FIXME: delete this.
 *
 * \param window       The window we're moving
 * \param finished     Callback for when it's finished
 * \param data         Data for callback
 */
void        meta_effect_run_focus        (MetaWindow         *window,
                                          MetaEffectFinished  finished,
                                          gpointer            data);

/**
 * Grabs the server and paints a wireframe rectangle on the screen.
 * Since this involves starting a grab, please be considerate of other
 * users and don't keep the grab for long.  You may move the wireframe
 * around using meta_effects_update_wireframe() and remove it, and undo
 * the grab, using meta_effects_end_wireframe().
 *
 * \param screen  The screen to draw the rectangle on.
 * \param rect    The size of the rectangle to draw.
 * \param width   The width to display in the middle (or 0 not to)
 * \param height  The width to display in the middle (or 0 not to)
 */
void meta_effects_begin_wireframe  (MetaScreen          *screen,
                                    const MetaRectangle *rect,
                                    int                  width,
                                    int                  height);

/**
 * Moves a wireframe rectangle around after its creation by
 * meta_effects_begin_wireframe().  (Perhaps we ought to remember the old
 * positions and not require people to pass them in?)
 *
 * \param old_rect  Where the rectangle is now
 * \param old_width The width that was displayed on it (or 0 if there wasn't)
 * \param old_height The height that was displayed on it (or 0 if there wasn't)
 * \param new_rect  Where the rectangle is going
 * \param new_width The width that will be displayed on it (or 0 not to)
 * \param new_height The height that will be displayed on it (or 0 not to)
 */
void meta_effects_update_wireframe (MetaScreen          *screen,
                                    const MetaRectangle *old_rect,
                                    int                  old_width,
                                    int                  old_height,
                                    const MetaRectangle *new_rect,
                                    int                  new_width,
                                    int                  new_height);

/**
 * Removes a wireframe rectangle from the screen and ends the grab started by
 * meta_effects_begin_wireframe().
 *
 * \param old_rect  Where the rectangle is now
 * \param old_width The width that was displayed on it (or 0 if there wasn't)
 * \param old_height The height that was displayed on it (or 0 if there wasn't)
 */
void meta_effects_end_wireframe    (MetaScreen          *screen,
                                    const MetaRectangle *old_rect,
                                    int                  width,
                                    int                  height);

#endif /* META_EFFECTS_H */
