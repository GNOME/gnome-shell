/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2013-2017 Red Hat
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

#include "config.h"

#include "backends/native/meta-crtc-kms.h"

#include "backends/meta-backend-private.h"
#include "backends/native/meta-gpu-kms.h"

#define ALL_TRANSFORMS (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)
#define ALL_TRANSFORMS_MASK ((1 << ALL_TRANSFORMS) - 1)

typedef struct _MetaCrtcKms
{
  unsigned int index;
  uint32_t underscan_prop_id;
  uint32_t underscan_hborder_prop_id;
  uint32_t underscan_vborder_prop_id;
  uint32_t primary_plane_id;
  uint32_t rotation_prop_id;
  uint32_t rotation_map[ALL_TRANSFORMS];
  uint32_t all_hw_transforms;
} MetaCrtcKms;

gboolean
meta_crtc_kms_is_transform_handled (MetaCrtc             *crtc,
                                    MetaMonitorTransform  transform)
{
  MetaCrtcKms *crtc_kms = crtc->driver_private;

  if ((1 << transform) & crtc_kms->all_hw_transforms)
    return TRUE;
  else
    return FALSE;
}

void
meta_crtc_kms_apply_transform (MetaCrtc *crtc)
{
  MetaCrtcKms *crtc_kms = crtc->driver_private;
  MetaGpu *gpu = meta_crtc_get_gpu (crtc);
  MetaGpuKms *gpu_kms = META_GPU_KMS (gpu);
  int kms_fd;
  MetaMonitorTransform hw_transform;

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  if (crtc_kms->all_hw_transforms & (1 << crtc->transform))
    hw_transform = crtc->transform;
  else
    hw_transform = META_MONITOR_TRANSFORM_NORMAL;

  if (!meta_crtc_kms_is_transform_handled (crtc, META_MONITOR_TRANSFORM_NORMAL))
    return;

  if (drmModeObjectSetProperty (kms_fd,
                                crtc_kms->primary_plane_id,
                                DRM_MODE_OBJECT_PLANE,
                                crtc_kms->rotation_prop_id,
                                crtc_kms->rotation_map[hw_transform]) != 0)
    {
      g_warning ("Failed to apply DRM plane transform %d: %m", hw_transform);

      /*
       * Blacklist this HW transform, we want to fallback to our
       * fallbacks in this case.
       */
      crtc_kms->all_hw_transforms &= ~(1 << hw_transform);
    }
}

void
meta_crtc_kms_set_underscan (MetaCrtc *crtc,
                             gboolean  is_underscanning)
{
  MetaCrtcKms *crtc_kms = crtc->driver_private;
  MetaGpu *gpu = meta_crtc_get_gpu (crtc);
  MetaGpuKms *gpu_kms = META_GPU_KMS (gpu);
  int kms_fd;

  if (!crtc_kms->underscan_prop_id)
    return;

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  if (is_underscanning)
    {
      drmModeObjectSetProperty (kms_fd, crtc->crtc_id,
                                DRM_MODE_OBJECT_CRTC,
                                crtc_kms->underscan_prop_id, (uint64_t) 1);

      if (crtc_kms->underscan_hborder_prop_id)
        {
          uint64_t value;

          value = crtc->current_mode->width * 0.05;
          drmModeObjectSetProperty (kms_fd, crtc->crtc_id,
                                    DRM_MODE_OBJECT_CRTC,
                                    crtc_kms->underscan_hborder_prop_id, value);
        }
      if (crtc_kms->underscan_vborder_prop_id)
        {
          uint64_t value;

          value = crtc->current_mode->height * 0.05;
          drmModeObjectSetProperty (kms_fd, crtc->crtc_id,
                                    DRM_MODE_OBJECT_CRTC,
                                    crtc_kms->underscan_vborder_prop_id, value);
        }

    }
  else
    {
      drmModeObjectSetProperty (kms_fd, crtc->crtc_id,
                                DRM_MODE_OBJECT_CRTC,
                                crtc_kms->underscan_prop_id, (uint64_t) 0);
    }
}

static int
find_property_index (MetaGpu                    *gpu,
                     drmModeObjectPropertiesPtr  props,
                     const char                 *prop_name,
                     drmModePropertyPtr         *out_prop)
{
  MetaGpuKms *gpu_kms = META_GPU_KMS (gpu);
  int kms_fd;
  unsigned int i;

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  for (i = 0; i < props->count_props; i++)
    {
      drmModePropertyPtr prop;

      prop = drmModeGetProperty (kms_fd, props->props[i]);
      if (!prop)
        continue;

      if (strcmp (prop->name, prop_name) == 0)
        {
          *out_prop = prop;
          return i;
        }

      drmModeFreeProperty (prop);
    }

  return -1;
}

static void
parse_transforms (MetaCrtc          *crtc,
                  drmModePropertyPtr prop)
{
  MetaCrtcKms *crtc_kms = crtc->driver_private;
  int i;

  for (i = 0; i < prop->count_enums; i++)
    {
      int transform = -1;

      if (strcmp (prop->enums[i].name, "rotate-0") == 0)
        transform = META_MONITOR_TRANSFORM_NORMAL;
      else if (strcmp (prop->enums[i].name, "rotate-90") == 0)
        transform = META_MONITOR_TRANSFORM_90;
      else if (strcmp (prop->enums[i].name, "rotate-180") == 0)
        transform = META_MONITOR_TRANSFORM_180;
      else if (strcmp (prop->enums[i].name, "rotate-270") == 0)
        transform = META_MONITOR_TRANSFORM_270;

      if (transform != -1)
        {
          crtc_kms->all_hw_transforms |= 1 << transform;
          crtc_kms->rotation_map[transform] = 1 << prop->enums[i].value;
        }
    }
}

static gboolean
is_primary_plane (MetaGpu                   *gpu,
                  drmModeObjectPropertiesPtr props)
{
  drmModePropertyPtr prop;
  int idx;

  idx = find_property_index (gpu, props, "type", &prop);
  if (idx < 0)
    return FALSE;

  drmModeFreeProperty (prop);
  return props->prop_values[idx] == DRM_PLANE_TYPE_PRIMARY;
}

static void
init_crtc_rotations (MetaCrtc *crtc,
                     MetaGpu  *gpu)
{
  MetaCrtcKms *crtc_kms = crtc->driver_private;
  MetaGpuKms *gpu_kms = META_GPU_KMS (gpu);
  int kms_fd;
  drmModeObjectPropertiesPtr props;
  drmModePlaneRes *planes;
  drmModePlane *drm_plane;
  unsigned int i;

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  planes = drmModeGetPlaneResources (kms_fd);
  if (planes == NULL)
    return;

  for (i = 0; i < planes->count_planes; i++)
    {
      drmModePropertyPtr prop;

      drm_plane = drmModeGetPlane (kms_fd, planes->planes[i]);

      if (!drm_plane)
        continue;

      if ((drm_plane->possible_crtcs & (1 << crtc_kms->index)))
        {
          props = drmModeObjectGetProperties (kms_fd,
                                              drm_plane->plane_id,
                                              DRM_MODE_OBJECT_PLANE);

          if (props && is_primary_plane (gpu, props))
            {
              int rotation_idx;

              crtc_kms->primary_plane_id = drm_plane->plane_id;
              rotation_idx = find_property_index (gpu, props,
                                                  "rotation", &prop);
              if (rotation_idx >= 0)
                {
                  crtc_kms->rotation_prop_id = props->props[rotation_idx];
                  parse_transforms (crtc, prop);
                  drmModeFreeProperty (prop);
                }
            }

          if (props)
            drmModeFreeObjectProperties (props);
        }

      drmModeFreePlane (drm_plane);
    }

  crtc->all_transforms |= crtc_kms->all_hw_transforms;

  drmModeFreePlaneResources (planes);
}

static void
find_crtc_properties (MetaCrtc   *crtc,
                      MetaGpuKms *gpu_kms)
{
  MetaCrtcKms *crtc_kms = crtc->driver_private;
  int kms_fd;
  drmModeObjectPropertiesPtr props;
  unsigned int i;

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);
  props = drmModeObjectGetProperties (kms_fd, crtc->crtc_id,
                                      DRM_MODE_OBJECT_CRTC);
  if (!props)
    return;

  for (i = 0; i < props->count_props; i++)
    {
      drmModePropertyPtr prop = drmModeGetProperty (kms_fd, props->props[i]);
      if (!prop)
        continue;

      if ((prop->flags & DRM_MODE_PROP_ENUM) &&
          strcmp (prop->name, "underscan") == 0)
        crtc_kms->underscan_prop_id = prop->prop_id;
      else if ((prop->flags & DRM_MODE_PROP_RANGE) &&
               strcmp (prop->name, "underscan hborder") == 0)
        crtc_kms->underscan_hborder_prop_id = prop->prop_id;
      else if ((prop->flags & DRM_MODE_PROP_RANGE) &&
               strcmp (prop->name, "underscan vborder") == 0)
        crtc_kms->underscan_vborder_prop_id = prop->prop_id;

      drmModeFreeProperty (prop);
    }

  drmModeFreeObjectProperties (props);
}

static void
meta_crtc_destroy_notify (MetaCrtc *crtc)
{
  g_free (crtc->driver_private);
}

MetaCrtc *
meta_create_kms_crtc (MetaGpuKms   *gpu_kms,
                      drmModeCrtc  *drm_crtc,
                      unsigned int  crtc_index)
{
  MetaGpu *gpu = META_GPU (gpu_kms);
  MetaCrtc *crtc;
  MetaCrtcKms *crtc_kms;

  crtc = g_object_new (META_TYPE_CRTC, NULL);

  crtc->gpu = gpu;
  crtc->crtc_id = drm_crtc->crtc_id;
  crtc->rect.x = drm_crtc->x;
  crtc->rect.y = drm_crtc->y;
  crtc->rect.width = drm_crtc->width;
  crtc->rect.height = drm_crtc->height;
  crtc->is_dirty = FALSE;
  crtc->transform = META_MONITOR_TRANSFORM_NORMAL;
  crtc->all_transforms = meta_is_stage_views_enabled () ?
    ALL_TRANSFORMS_MASK : META_MONITOR_TRANSFORM_NORMAL;

  if (drm_crtc->mode_valid)
    {
      GList *l;

      for (l = meta_gpu_get_modes (gpu); l; l = l->next)
        {
          MetaCrtcMode *mode = l->data;

          if (meta_drm_mode_equal (&drm_crtc->mode, mode->driver_private))
            {
              crtc->current_mode = mode;
              break;
            }
        }
    }

  crtc_kms = g_new0 (MetaCrtcKms, 1);
  crtc_kms->index = crtc_index;

  crtc->driver_private = crtc_kms;
  crtc->driver_notify = (GDestroyNotify) meta_crtc_destroy_notify;

  find_crtc_properties (crtc, gpu_kms);
  init_crtc_rotations (crtc, gpu);

  return crtc;
}
