/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
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
 *
 */

/**
 * SECTION:st-types
 * @short_description: type definitions used throughout St
 *
 * Common types for StWidgets.
 */


#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_TYPES_H__
#define __ST_TYPES_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define ST_TYPE_BORDER_IMAGE          (st_border_image_get_type ())
#define ST_TYPE_PADDING               (st_padding_get_type ())

typedef struct _StPadding             StPadding;

GType st_border_image_get_type (void) G_GNUC_CONST;

/**
 * StPadding:
 * @top: padding from the top
 * @right: padding from the right
 * @bottom: padding from the bottom
 * @left: padding from the left
 *
 * The padding from the internal border of the parent container.
 */
struct _StPadding
{
  gfloat top;
  gfloat right;
  gfloat bottom;
  gfloat left;
};

GType st_padding_get_type (void) G_GNUC_CONST;

typedef enum {
  ST_ALIGN_START,
  ST_ALIGN_MIDDLE,
  ST_ALIGN_END
} StAlign;

G_END_DECLS

#endif /* __ST_TYPES_H__ */
