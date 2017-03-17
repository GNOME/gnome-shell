/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat
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
 *
 */

#include "config.h"

#include "backends/x11/cm/meta-renderer-x11-cm.h"

struct _MetaRendererX11Cm
{
  MetaRendererX11 parent;
};

G_DEFINE_TYPE (MetaRendererX11Cm, meta_renderer_x11_cm,
               META_TYPE_RENDERER_X11)

static void
meta_renderer_x11_cm_init (MetaRendererX11Cm *renderer_x11_cm)
{
}

static void
meta_renderer_x11_cm_class_init (MetaRendererX11CmClass *klass)
{
}
