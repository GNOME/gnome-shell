/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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

#ifndef META_OUTPUT_KMS_H
#define META_OUTPUT_KMS_H

#include "backends/meta-output.h"
#include "backends/native/meta-gpu-kms.h"

void meta_output_kms_set_underscan (MetaOutput *output);

void meta_output_kms_set_power_save_mode (MetaOutput *output,
                                          uint64_t    state);

gboolean meta_output_kms_can_clone (MetaOutput *output,
                                    MetaOutput *other_output);

GBytes * meta_output_kms_read_edid (MetaOutput *output);

MetaOutput * meta_create_kms_output (MetaGpuKms        *gpu_kms,
                                     drmModeConnector  *connector,
                                     MetaKmsResources  *resources,
                                     MetaOutput        *old_output,
                                     GError           **error);

#endif /* META_OUTPUT_KMS_H */
