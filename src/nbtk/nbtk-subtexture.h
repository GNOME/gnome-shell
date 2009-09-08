/*
 * nbtk-subtexture.h: Class to wrap a texture and "subframe" it.
 *
 * Based on
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


#ifndef __NBTK_SUBTEXTURE_H__
#define __NBTK_SUBTEXTURE_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define NBTK_TYPE_SUBTEXTURE                 (nbtk_subtexture_get_type ())
#define NBTK_SUBTEXTURE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_SUBTEXTURE, NbtkSubtexture))
#define NBTK_SUBTEXTURE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_SUBTEXTURE, NbtkSubtextureClass))
#define NBTK_IS_SUBTEXTURE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_SUBTEXTURE))
#define NBTK_IS_SUBTEXTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_SUBTEXTURE))
#define NBTK_SUBTEXTURE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_SUBTEXTURE, NbtkSubtextureClass))

typedef struct _NbtkSubtexture                NbtkSubtexture;
typedef struct _NbtkSubtexturePrivate         NbtkSubtexturePrivate;
typedef struct _NbtkSubtextureClass           NbtkSubtextureClass;

/**
 * NbtkSubtexture:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _NbtkSubtexture
{
  /*< private >*/
  ClutterActor parent_instance;

  NbtkSubtexturePrivate    *priv;
};

struct _NbtkSubtextureClass
{
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_nbtk_box_1) (void);
  void (*_nbtk_box_2) (void);
  void (*_nbtk_box_3) (void);
  void (*_nbtk_box_4) (void);
};

GType           nbtk_subtexture_get_type           (void) G_GNUC_CONST;
ClutterActor *  nbtk_subtexture_new                (ClutterTexture   *texture,
                                                       gint            top,
                                                       gint            left,
                                                       gint            width,
                                                       gint            height);
void            nbtk_subtexture_set_parent_texture (NbtkSubtexture *frame,
                                                       ClutterTexture   *texture);
ClutterTexture *nbtk_subtexture_get_parent_texture (NbtkSubtexture *frame);
void            nbtk_subtexture_set_frame          (NbtkSubtexture *frame,
                                                       gint            top,
                                                       gint            left,
                                                       gint            width,
                                                       gint            height);
void            nbtk_subtexture_get_frame          (NbtkSubtexture *frame,
                                                       gint           *top,
                                                       gint           *left,
                                                       gint           *width,
                                                       gint           *height);

G_END_DECLS

#endif /* __NBTK_SUBTEXTURE_H__ */
