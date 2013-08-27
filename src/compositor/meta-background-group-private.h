/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef META_BACKGROUND_GROUP_PRIVATE_H
#define META_BACKGROUND_GROUP_PRIVATE_H

#include <meta/screen.h>
#include <meta/meta-background-group.h>

void meta_background_group_set_clip_region  (MetaBackgroundGroup *self,
                                             cairo_region_t      *visible_region);
#endif /* META_BACKGROUND_GROUP_PRIVATE_H */
