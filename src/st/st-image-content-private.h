/*
 * st-image-content-private.h: Private StImageContent methods
 *
 * Copyright 2024 Red Hat, Inc.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "st-image-content.h"

G_BEGIN_DECLS

void st_image_content_set_is_symbolic (StImageContent *content,
                                       gboolean        is_symbolic);

gboolean st_image_content_get_is_symbolic (StImageContent *content);

G_END_DECLS
