/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-texture-frame.h: Expandible texture actor
 *
 * Copyright 2007, 2008 OpenedHand Ltd
 * Copyright 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_TEXTURE_FRAME_H__
#define __ST_TEXTURE_FRAME_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define ST_TYPE_TEXTURE_FRAME                 (st_texture_frame_get_type ())
#define ST_TEXTURE_FRAME(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ST_TYPE_TEXTURE_FRAME, StTextureFrame))
#define ST_TEXTURE_FRAME_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_TEXTURE_FRAME, StTextureFrameClass))
#define ST_IS_TEXTURE_FRAME(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ST_TYPE_TEXTURE_FRAME))
#define ST_IS_TEXTURE_FRAME_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_TEXTURE_FRAME))
#define ST_TEXTURE_FRAME_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_TEXTURE_FRAME, StTextureFrameClass))

typedef struct _StTextureFrame                StTextureFrame;
typedef struct _StTextureFramePrivate         StTextureFramePrivate;
typedef struct _StTextureFrameClass           StTextureFrameClass;

/**
 * StTextureFrame:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _StTextureFrame
{
  /*< private >*/
  ClutterActor parent_instance;
  
  StTextureFramePrivate    *priv;
};

struct _StTextureFrameClass 
{
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_clutter_box_1) (void);
  void (*_clutter_box_2) (void);
  void (*_clutter_box_3) (void);
  void (*_clutter_box_4) (void);
}; 

GType st_texture_frame_get_type (void) G_GNUC_CONST;

ClutterActor *  st_texture_frame_new                (ClutterTexture *texture,
                                                     gfloat          top,
                                                     gfloat          right,
                                                     gfloat          bottom,
                                                     gfloat          left);
void            st_texture_frame_set_parent_texture (StTextureFrame *frame,
                                                     ClutterTexture *texture);
ClutterTexture *st_texture_frame_get_parent_texture (StTextureFrame *frame);
void            st_texture_frame_set_frame          (StTextureFrame *frame,
                                                     gfloat          top,
                                                     gfloat          right,
                                                     gfloat          bottom,
                                                     gfloat          left);
void            st_texture_frame_get_frame          (StTextureFrame *frame,
                                                     gfloat         *top,
                                                     gfloat         *right,
                                                     gfloat         *bottom,
                                                     gfloat         *left);

G_END_DECLS

#endif /* __ST_TEXTURE_FRAME_H__ */
