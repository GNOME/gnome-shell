/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

struct _MetaSurfaceContentWayland {
  MetaShapedTexture *texture;
  MetaWaylandSurface *surface;
};

MetaSurfaceContentWayland *
meta_surface_content_wayland_new (MetaWaylandSurface *surface)
{
}
