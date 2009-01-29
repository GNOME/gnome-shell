/* tidy-texture-frame.h: Expandible texture actor
 *
 * Copyright (C) 2007, 2008 OpenedHand Ltd
 * Copyright (C) 2009 Intel Corp.
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

#ifndef _HAVE_TIDY_TEXTURE_FRAME_H
#define _HAVE_TIDY_TEXTURE_FRAME_H

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define TIDY_TYPE_TEXTURE_FRAME                 (tidy_texture_frame_get_type ())
#define TIDY_TEXTURE_FRAME(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_TEXTURE_FRAME, TidyTextureFrame))
#define TIDY_TEXTURE_FRAME_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), TIDY_TYPE_TEXTURE_FRAME, TidyTextureFrameClass))
#define TIDY_IS_TEXTURE_FRAME(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_TEXTURE_FRAME))
#define TIDY_IS_TEXTURE_FRAME_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), TIDY_TYPE_TEXTURE_FRAME))
#define TIDY_TEXTURE_FRAME_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), TIDY_TYPE_TEXTURE_FRAME, TidyTextureFrameClass))

typedef struct _TidyTextureFrame                TidyTextureFrame;
typedef struct _TidyTextureFramePrivate         TidyTextureFramePrivate;
typedef struct _TidyTextureFrameClass           TidyTextureFrameClass;

struct _TidyTextureFrame
{
  /*< private >*/
  ClutterActor parent_instance;
  
  TidyTextureFramePrivate    *priv;
};

struct _TidyTextureFrameClass
{
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_clutter_box_1) (void);
  void (*_clutter_box_2) (void);
  void (*_clutter_box_3) (void);
  void (*_clutter_box_4) (void);
};

GType           tidy_texture_frame_get_type           (void) G_GNUC_CONST;
ClutterActor *  tidy_texture_frame_new                (ClutterTexture   *texture,
                                                       gfloat            top,
                                                       gfloat            right,
                                                       gfloat            bottom,
                                                       gfloat            left);
void            tidy_texture_frame_set_parent_texture (TidyTextureFrame *frame,
                                                       ClutterTexture   *texture);
ClutterTexture *tidy_texture_frame_get_parent_texture (TidyTextureFrame *frame);
void            tidy_texture_frame_set_frame          (TidyTextureFrame *frame,
                                                       gfloat            top,
                                                       gfloat            right,
                                                       gfloat            bottom,
                                                       gfloat            left);
void            tidy_texture_frame_get_frame          (TidyTextureFrame *frame,
                                                       gfloat           *top,
                                                       gfloat           *right,
                                                       gfloat           *bottom,
                                                       gfloat           *left);

G_END_DECLS

#endif /* _HAVE_TIDY_TEXTURE_FRAME_H */
