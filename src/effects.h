/* Metacity animation effects */

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
#include "screen.h"

#define META_MINIMIZE_ANIMATION_LENGTH 0.35
#define META_SHADE_ANIMATION_LENGTH 0.2

typedef enum
{
  META_BOX_ANIM_SCALE,
  META_BOX_ANIM_SLIDE_UP

} MetaBoxAnimType;

void meta_effects_draw_box_animation (MetaScreen     *screen,
                                      MetaRectangle  *initial_rect,
                                      MetaRectangle  *destination_rect,
                                      double          seconds_duration,
                                      MetaBoxAnimType anim_type);

void meta_effects_begin_wireframe  (MetaScreen          *screen,
                                    const MetaRectangle *rect);
void meta_effects_update_wireframe (MetaScreen          *screen,
                                    const MetaRectangle *old_rect,
                                    const MetaRectangle *new_rect);
void meta_effects_end_wireframe    (MetaScreen          *screen,
                                    const MetaRectangle *old_rect);

#endif /* META_EFFECTS_H */
