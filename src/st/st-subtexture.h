/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-subtexture.h: Class to wrap a texture and "subframe" it.
 *
 * Based on
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

#ifndef __ST_SUBTEXTURE_H__
#define __ST_SUBTEXTURE_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define ST_TYPE_SUBTEXTURE                 (st_subtexture_get_type ())
#define ST_SUBTEXTURE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ST_TYPE_SUBTEXTURE, StSubtexture))
#define ST_SUBTEXTURE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_SUBTEXTURE, StSubtextureClass))
#define ST_IS_SUBTEXTURE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ST_TYPE_SUBTEXTURE))
#define ST_IS_SUBTEXTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_SUBTEXTURE))
#define ST_SUBTEXTURE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_SUBTEXTURE, StSubtextureClass))

typedef struct _StSubtexture                StSubtexture;
typedef struct _StSubtexturePrivate         StSubtexturePrivate;
typedef struct _StSubtextureClass           StSubtextureClass;

/**
 * StSubtexture:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _StSubtexture
{
  /*< private >*/
  ClutterActor parent_instance;
  
  StSubtexturePrivate    *priv;
};

struct _StSubtextureClass 
{
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_st_box_1) (void);
  void (*_st_box_2) (void);
  void (*_st_box_3) (void);
  void (*_st_box_4) (void);
}; 

GType st_subtexture_get_type (void) G_GNUC_CONST;

ClutterActor *  st_subtexture_new                (ClutterTexture *texture,
                                                  gint            top,
                                                  gint            left,
                                                  gint            width,
                                                  gint            height);
void            st_subtexture_set_parent_texture (StSubtexture   *frame,
                                                  ClutterTexture *texture);
ClutterTexture *st_subtexture_get_parent_texture (StSubtexture   *frame);
void            st_subtexture_set_frame          (StSubtexture   *frame,
                                                  gint            top,
                                                  gint            left,
                                                  gint            width,
                                                  gint            height);
void            st_subtexture_get_frame          (StSubtexture   *frame,
                                                  gint           *top,
                                                  gint           *left,
                                                  gint           *width,
                                                  gint           *height);

G_END_DECLS

#endif /* __ST_SUBTEXTURE_H__ */
