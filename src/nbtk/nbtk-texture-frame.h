/*
 * nbtk-texture-frame.h: Expandible texture actor
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

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_TEXTURE_FRAME_H__
#define __NBTK_TEXTURE_FRAME_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define NBTK_TYPE_TEXTURE_FRAME                 (nbtk_texture_frame_get_type ())
#define NBTK_TEXTURE_FRAME(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_TEXTURE_FRAME, NbtkTextureFrame))
#define NBTK_TEXTURE_FRAME_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_TEXTURE_FRAME, NbtkTextureFrameClass))
#define NBTK_IS_TEXTURE_FRAME(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_TEXTURE_FRAME))
#define NBTK_IS_TEXTURE_FRAME_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_TEXTURE_FRAME))
#define NBTK_TEXTURE_FRAME_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_TEXTURE_FRAME, NbtkTextureFrameClass))

typedef struct _NbtkTextureFrame                NbtkTextureFrame;
typedef struct _NbtkTextureFramePrivate         NbtkTextureFramePrivate;
typedef struct _NbtkTextureFrameClass           NbtkTextureFrameClass;

/**
 * NbtkTextureFrame:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _NbtkTextureFrame
{
  /*< private >*/
  ClutterActor parent_instance;

  NbtkTextureFramePrivate    *priv;
};

struct _NbtkTextureFrameClass
{
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_clutter_box_1) (void);
  void (*_clutter_box_2) (void);
  void (*_clutter_box_3) (void);
  void (*_clutter_box_4) (void);
};

GType           nbtk_texture_frame_get_type           (void) G_GNUC_CONST;
ClutterActor *  nbtk_texture_frame_new                (ClutterTexture   *texture,
                                                       gfloat            top,
                                                       gfloat            right,
                                                       gfloat            bottom,
                                                       gfloat            left);
void            nbtk_texture_frame_set_parent_texture (NbtkTextureFrame *frame,
                                                       ClutterTexture   *texture);
ClutterTexture *nbtk_texture_frame_get_parent_texture (NbtkTextureFrame *frame);
void            nbtk_texture_frame_set_frame          (NbtkTextureFrame *frame,
                                                       gfloat            top,
                                                       gfloat            right,
                                                       gfloat            bottom,
                                                       gfloat            left);
void            nbtk_texture_frame_get_frame          (NbtkTextureFrame *frame,
                                                       gfloat           *top,
                                                       gfloat           *right,
                                                       gfloat           *bottom,
                                                       gfloat           *left);

G_END_DECLS

#endif /* __NBTK_TEXTURE_FRAME_H__ */
