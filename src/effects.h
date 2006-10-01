/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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

typedef struct MetaEffect MetaEffect;
typedef struct MetaEffectPriv MetaEffectPriv;

#define META_MINIMIZE_ANIMATION_LENGTH 0.25
#define META_SHADE_ANIMATION_LENGTH 0.2

typedef enum
{
  META_BOX_ANIM_SCALE,
  META_BOX_ANIM_SLIDE_UP

} MetaBoxAnimType;

typedef enum
{
  META_EFFECT_MINIMIZE,
  META_EFFECT_UNMINIMIZE,
  META_EFFECT_MENU_MAP,
  META_EFFECT_MENU_UNMAP,
  META_EFFECT_DIALOG_MAP,
  META_EFFECT_DIALOG_UNMAP,
  META_EFFECT_TOPLEVEL_MAP,
  META_EFFECT_TOPLEVEL_UNMAP,
  META_EFFECT_WIREFRAME_BEGIN,
  META_EFFECT_WIREFRAME_UPDATE,
  META_EFFECT_WIREFRAME_END,
  META_EFFECT_FOCUS,
  META_EFFECT_CLOSE,
  META_NUM_EFFECTS
} MetaEffectType;

typedef void (* MetaEffectHandler) (MetaEffect *effect,
				    gpointer    data);
typedef void (* MetaEffectFinished) (const MetaEffect *effect,
				     gpointer	      data);

typedef struct
{
  MetaRectangle window_rect;
  MetaRectangle icon_rect;
} MetaMinimizeEffect, MetaUnminimizeEffect;

typedef struct
{
    
} MetaCloseEffect;

typedef struct
{
} MetaFocusEffect;

struct MetaEffect
{
  MetaWindow *window;
  MetaEffectType type;
  gpointer info;		/* effect handler can hang data here */
  
  union
  {
    MetaMinimizeEffect	    minimize;
    MetaUnminimizeEffect    unminimize;
    MetaCloseEffect	    close;
    MetaFocusEffect	    focus;
  } u;
  
  MetaEffectPriv *priv;
};

void        meta_push_effect_handler (MetaEffectHandler   handler,
				      gpointer            data);
void	    meta_pop_effect_handler  (void);

void        meta_effect_run_minimize     (MetaWindow         *window,
					  MetaRectangle	     *window_rect,
					  MetaRectangle	     *target,
					  MetaEffectFinished  finished,
					  gpointer            data);
void        meta_effect_run_unminimize (MetaWindow         *window,
					MetaRectangle      *window_rect,
					MetaRectangle	     *icon_rect,
					MetaEffectFinished  finished,
					gpointer            data);
void        meta_effect_run_close        (MetaWindow         *window,
					  MetaEffectFinished  finished,
					  gpointer            data);
void        meta_effect_run_focus        (MetaWindow         *window,
					  MetaEffectFinished  finished,
					  gpointer            data);
void        meta_effect_end              (MetaEffect         *effect);



/* Stuff that should become static functions */

void meta_effects_draw_box_animation (MetaScreen     *screen,
                                      MetaRectangle  *initial_rect,
                                      MetaRectangle  *destination_rect,
                                      double          seconds_duration,
                                      MetaBoxAnimType anim_type);

void meta_effects_begin_wireframe  (MetaScreen          *screen,
                                    const MetaRectangle *rect,
                                    int                  width,
                                    int                  height);
void meta_effects_update_wireframe (MetaScreen          *screen,
                                    const MetaRectangle *old_rect,
                                    int                  old_width,
                                    int                  old_height,
                                    const MetaRectangle *new_rect,
                                    int                  new_width,
                                    int                  new_height);
void meta_effects_end_wireframe    (MetaScreen          *screen,
                                    const MetaRectangle *old_rect,
                                    int                  width,
                                    int                  height);

#endif /* META_EFFECTS_H */
