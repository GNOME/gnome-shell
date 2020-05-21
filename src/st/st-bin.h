/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-bin.h: Basic container actor
 *
 * Copyright 2009, 2008 Intel Corporation.
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

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_BIN_H__
#define __ST_BIN_H__

#include <st/st-types.h>
#include <st/st-widget.h>

G_BEGIN_DECLS

#define ST_TYPE_BIN                   (st_bin_get_type ())
G_DECLARE_DERIVABLE_TYPE (StBin, st_bin, ST, BIN, StWidget)

/**
 * StBinClass:
 *
 * The #StBinClass struct contains only private data
 */
struct _StBinClass
{
  /*< private >*/
  StWidgetClass parent_class;
};

StWidget   *  st_bin_new           (void);
void          st_bin_set_child     (StBin        *bin,
                                    ClutterActor *child);
ClutterActor *st_bin_get_child     (StBin        *bin);

G_END_DECLS

#endif /* __ST_BIN_H__ */
