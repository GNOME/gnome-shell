/*
 * Copyright (C) 2012 Intel Corporation
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

#ifndef META_STAGE_PRIVATE_H
#define META_STAGE_PRIVATE_H

#include <meta/meta-stage.h>

#include "meta-cursor.h"
#include <meta/boxes.h>

G_BEGIN_DECLS

typedef struct _MetaOverlay    MetaOverlay;

struct _MetaStage
{
  ClutterStage parent;
};

ClutterActor     *meta_stage_new                     (void);

MetaOverlay      *meta_stage_create_cursor_overlay   (MetaStage   *stage);
void              meta_stage_remove_cursor_overlay   (MetaStage   *stage,
						      MetaOverlay *overlay);

void              meta_stage_update_cursor_overlay   (MetaStage   *stage,
                                                      MetaOverlay *overlay,
                                                      CoglTexture *texture,
                                                      ClutterRect *rect);

void meta_stage_set_active (MetaStage *stage,
                            gboolean   is_active);

void meta_stage_update_view_layout (MetaStage *stage);

G_END_DECLS

#endif /* META_STAGE_PRIVATE_H */
