/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef META_BACKGROUND_PRIVATE_H
#define META_BACKGROUND_PRIVATE_H

#include <config.h>

#include "meta-background-private.h"

CoglTexture *meta_background_get_texture (MetaBackground         *self,
                                          int                     monitor_index,
                                          cairo_rectangle_int_t  *texture_area,
                                          CoglPipelineWrapMode   *wrap_mode);

#endif /* META_BACKGROUND_PRIVATE_H */
