/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2013 Red Hat Inc.
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
 * Author: Giovanni Campagna <gcampagn@redhat.com>
 */

#include "config.h"

#include "meta-monitor-manager-kms.h"
#include "meta-monitor-config.h"
#include "meta-monitor-config-manager.h"
#include "meta-backend-private.h"
#include "meta-renderer-native.h"

#include <string.h>
#include <stdlib.h>
#include <clutter/clutter.h>

#include <drm.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <meta/main.h>
#include <meta/errors.h>

#include <gudev/gudev.h>

#include "meta-default-modes.h"

#define ALL_TRANSFORMS (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)
#define ALL_TRANSFORMS_MASK ((1 << ALL_TRANSFORMS) - 1)
#define SYNC_TOLERANCE 0.01    /* 1 percent */

static float supported_scales_kms[] = {
  1.0,
  2.0
};

typedef struct
{
  drmModeConnector *connector;

  unsigned n_encoders;
  drmModeEncoderPtr *encoders;
  drmModeEncoderPtr  current_encoder;

  /*
   * Bitmasks of encoder position in the resources array (used during clone
   * setup).
   */
  uint32_t encoder_mask;
  uint32_t enc_clone_mask;

  uint32_t dpms_prop_id;
  uint32_t edid_blob_id;
  uint32_t tile_blob_id;

  int suggested_x;
  int suggested_y;
  uint32_t hotplug_mode_update;

  gboolean has_scaling;
} MetaOutputKms;

typedef struct
{
  uint32_t underscan_prop_id;
  uint32_t underscan_hborder_prop_id;
  uint32_t underscan_vborder_prop_id;
  uint32_t primary_plane_id;
  uint32_t rotation_prop_id;
  uint32_t rotation_map[ALL_TRANSFORMS];
  uint32_t all_hw_transforms;
} MetaCrtcKms;

typedef struct
{
  GSource source;

  gpointer fd_tag;
  MetaMonitorManagerKms *manager_kms;
} MetaKmsSource;

struct _MetaMonitorManagerKms
{
  MetaMonitorManager parent_instance;

  int fd;
  MetaKmsSource *source;

  drmModeConnector **connectors;
  unsigned int       n_connectors;

  GUdevClient *udev;
  guint uevent_handler_id;

  GSettings *desktop_settings;

  gboolean page_flips_not_supported;

  int max_buffer_width;
  int max_buffer_height;
};

struct _MetaMonitorManagerKmsClass
{
  MetaMonitorManagerClass parent_class;
};

G_DEFINE_TYPE (MetaMonitorManagerKms, meta_monitor_manager_kms, META_TYPE_MONITOR_MANAGER);

static void
free_resources (MetaMonitorManagerKms *manager_kms)
{
  unsigned i;

  for (i = 0; i < manager_kms->n_connectors; i++)
    drmModeFreeConnector (manager_kms->connectors[i]);

  g_free (manager_kms->connectors);
}

static int
compare_outputs (const void *one,
                 const void *two)
{
  const MetaOutput *o_one = one, *o_two = two;

  return strcmp (o_one->name, o_two->name);
}

static char *
make_output_name (drmModeConnector *connector)
{
  static const char * const connector_type_names[] = {
    "None",
    "VGA",
    "DVI-I",
    "DVI-D",
    "DVI-A",
    "Composite",
    "SVIDEO",
    "LVDS",
    "Component",
    "DIN",
    "DP",
    "HDMI",
    "HDMI-B",
    "TV",
    "eDP",
    "Virtual",
    "DSI",
  };

  if (connector->connector_type < G_N_ELEMENTS (connector_type_names))
    return g_strdup_printf ("%s-%d",
                            connector_type_names[connector->connector_type],
                            connector->connector_type_id);
  else
    return g_strdup_printf ("Unknown%d-%d",
                            connector->connector_type,
                            connector->connector_type_id);
}

static void
meta_output_destroy_notify (MetaOutput *output)
{
  MetaOutputKms *output_kms;
  unsigned i;

  output_kms = output->driver_private;

  for (i = 0; i < output_kms->n_encoders; i++)
    drmModeFreeEncoder (output_kms->encoders[i]);
  g_free (output_kms->encoders);

  g_slice_free (MetaOutputKms, output_kms);
}

static void
meta_monitor_mode_destroy_notify (MetaCrtcMode *mode)
{
  g_slice_free (drmModeModeInfo, mode->driver_private);
}

static void
meta_crtc_destroy_notify (MetaCrtc *crtc)
{
  g_free (crtc->driver_private);
}

static gboolean
drm_mode_equal (gconstpointer one,
                gconstpointer two)
{
  const drmModeModeInfo *m_one = one;
  const drmModeModeInfo *m_two = two;

  return m_one->clock == m_two->clock &&
    m_one->hdisplay == m_two->hdisplay &&
    m_one->hsync_start == m_two->hsync_start &&
    m_one->hsync_end == m_two->hsync_end &&
    m_one->htotal == m_two->htotal &&
    m_one->hskew == m_two->hskew &&
    m_one->vdisplay == m_two->vdisplay &&
    m_one->vsync_start == m_two->vsync_start &&
    m_one->vsync_end == m_two->vsync_end &&
    m_one->vtotal == m_two->vtotal &&
    m_one->vscan == m_two->vscan &&
    m_one->vrefresh == m_two->vrefresh &&
    m_one->flags == m_two->flags &&
    m_one->type == m_two->type &&
    strncmp (m_one->name, m_two->name, DRM_DISPLAY_MODE_LEN) == 0;
}

static guint
drm_mode_hash (gconstpointer ptr)
{
  const drmModeModeInfo *mode = ptr;
  guint hash = 0;

  /* We don't include the name in the hash because it's generally
     derived from the other fields (hdisplay, vdisplay and flags)
  */

  hash ^= mode->clock;
  hash ^= mode->hdisplay ^ mode->hsync_start ^ mode->hsync_end;
  hash ^= mode->vdisplay ^ mode->vsync_start ^ mode->vsync_end;
  hash ^= mode->vrefresh;
  hash ^= mode->flags ^ mode->type;

  return hash;
}

static void
find_connector_properties (MetaMonitorManagerKms *manager_kms,
                           MetaOutputKms         *output_kms)
{
  int i;

  output_kms->hotplug_mode_update = 0;
  output_kms->suggested_x = -1;
  output_kms->suggested_y = -1;
  for (i = 0; i < output_kms->connector->count_props; i++)
    {
      drmModePropertyPtr prop = drmModeGetProperty (manager_kms->fd, output_kms->connector->props[i]);
      if (!prop)
        continue;

      if ((prop->flags & DRM_MODE_PROP_ENUM) && strcmp (prop->name, "DPMS") == 0)
        output_kms->dpms_prop_id = prop->prop_id;
      else if ((prop->flags & DRM_MODE_PROP_BLOB) && strcmp (prop->name, "EDID") == 0)
        output_kms->edid_blob_id = output_kms->connector->prop_values[i];
      else if ((prop->flags & DRM_MODE_PROP_BLOB) &&
               strcmp (prop->name, "TILE") == 0)
        output_kms->tile_blob_id = output_kms->connector->prop_values[i];
      else if ((prop->flags & DRM_MODE_PROP_RANGE) &&
               strcmp (prop->name, "suggested X") == 0)
        output_kms->suggested_x = output_kms->connector->prop_values[i];
      else if ((prop->flags & DRM_MODE_PROP_RANGE) &&
               strcmp (prop->name, "suggested Y") == 0)
        output_kms->suggested_y = output_kms->connector->prop_values[i];
      else if ((prop->flags & DRM_MODE_PROP_RANGE) &&
               strcmp (prop->name, "hotplug_mode_update") == 0)
        output_kms->hotplug_mode_update = output_kms->connector->prop_values[i];
      else if (strcmp (prop->name, "scaling mode") == 0)
        output_kms->has_scaling = TRUE;

      drmModeFreeProperty (prop);
    }
}

static void
find_crtc_properties (MetaMonitorManagerKms *manager_kms,
                      MetaCrtc *meta_crtc)
{
  MetaCrtcKms *crtc_kms;
  drmModeObjectPropertiesPtr props;
  size_t i;

  crtc_kms = meta_crtc->driver_private;

  props = drmModeObjectGetProperties (manager_kms->fd, meta_crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
  if (!props)
    return;

  for (i = 0; i < props->count_props; i++)
    {
      drmModePropertyPtr prop = drmModeGetProperty (manager_kms->fd, props->props[i]);
      if (!prop)
        continue;

      if ((prop->flags & DRM_MODE_PROP_ENUM) && strcmp (prop->name, "underscan") == 0)
        crtc_kms->underscan_prop_id = prop->prop_id;
      else if ((prop->flags & DRM_MODE_PROP_RANGE) && strcmp (prop->name, "underscan hborder") == 0)
        crtc_kms->underscan_hborder_prop_id = prop->prop_id;
      else if ((prop->flags & DRM_MODE_PROP_RANGE) && strcmp (prop->name, "underscan vborder") == 0)
        crtc_kms->underscan_vborder_prop_id = prop->prop_id;

      drmModeFreeProperty (prop);
    }
}

static drmModePropertyBlobPtr
read_edid_blob (MetaMonitorManagerKms *manager_kms,
                uint32_t               edid_blob_id,
                GError               **error)
{
  drmModePropertyBlobPtr edid_blob = NULL;

  edid_blob = drmModeGetPropertyBlob (manager_kms->fd, edid_blob_id);
  if (!edid_blob)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                   "%s", strerror (errno));
      return NULL;
    }

  return edid_blob;
}

static GBytes *
read_output_edid (MetaMonitorManagerKms *manager_kms,
                  MetaOutput            *output,
                  GError               **error)
{
  MetaOutputKms *output_kms = output->driver_private;
  drmModePropertyBlobPtr edid_blob;

  g_assert (output_kms->edid_blob_id != 0);

  edid_blob = read_edid_blob (manager_kms, output_kms->edid_blob_id, error);
  if (!edid_blob)
    return NULL;

  if (edid_blob->length == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "EDID blob was empty");
      drmModeFreePropertyBlob (edid_blob);
      return NULL;
    }

  return g_bytes_new_with_free_func (edid_blob->data, edid_blob->length,
                                     (GDestroyNotify) drmModeFreePropertyBlob,
                                     edid_blob);
}

static gboolean
output_get_tile_info (MetaMonitorManagerKms *manager_kms,
                      MetaOutput            *output)
{
  MetaOutputKms *output_kms = output->driver_private;
  drmModePropertyBlobPtr tile_blob = NULL;
  int ret;

  if (output_kms->tile_blob_id == 0)
    return FALSE;

  tile_blob = drmModeGetPropertyBlob (manager_kms->fd, output_kms->tile_blob_id);
  if (!tile_blob)
    {
      meta_warning ("Failed to read TILE of output %s: %s\n", output->name, strerror(errno));
      return FALSE;
    }

  if (tile_blob->length > 0)
    {
      ret = sscanf ((char *)tile_blob->data, "%d:%d:%d:%d:%d:%d:%d:%d",
                    &output->tile_info.group_id,
                    &output->tile_info.flags,
                    &output->tile_info.max_h_tiles,
                    &output->tile_info.max_v_tiles,
                    &output->tile_info.loc_h_tile,
                    &output->tile_info.loc_v_tile,
                    &output->tile_info.tile_w,
                    &output->tile_info.tile_h);
      drmModeFreePropertyBlob (tile_blob);

      if (ret != 8)
        {
          meta_warning ("Couldn't understand output tile property blob\n");
          return FALSE;
        }
      return TRUE;
    }
  else
    {
      drmModeFreePropertyBlob (tile_blob);
      return FALSE;
    }
}

static MetaCrtcMode *
find_meta_mode (MetaMonitorManager    *manager,
                const drmModeModeInfo *drm_mode)
{
  unsigned k;

  for (k = 0; k < manager->n_modes; k++)
    {
      if (drm_mode_equal (drm_mode, manager->modes[k].driver_private))
        return &manager->modes[k];
    }

  g_assert_not_reached ();
  return NULL;
}

static float
drm_mode_vrefresh (const drmModeModeInfo *mode)
{
  float refresh = 0.0;

  if (mode->htotal > 0 && mode->vtotal > 0)
    {
      /* Calculate refresh rate in milliHz first for extra precision. */
      refresh = (mode->clock * 1000000LL) / mode->htotal;
      refresh += (mode->vtotal / 2);
      refresh /= mode->vtotal;
      if (mode->vscan > 1)
        refresh /= mode->vscan;
      refresh /= 1000.0;
    }
  return refresh;
}

static void
init_mode (MetaCrtcMode          *mode,
           const drmModeModeInfo *drm_mode,
           long                   mode_id)
{
  mode->mode_id = mode_id;
  mode->name = g_strndup (drm_mode->name, DRM_DISPLAY_MODE_LEN);
  mode->width = drm_mode->hdisplay;
  mode->height = drm_mode->vdisplay;
  mode->flags = drm_mode->flags;
  mode->refresh_rate = drm_mode_vrefresh (drm_mode);
  mode->driver_private = g_slice_dup (drmModeModeInfo, drm_mode);
  mode->driver_notify = (GDestroyNotify)meta_monitor_mode_destroy_notify;
}

static int
compare_modes (const void *one,
               const void *two)
{
  MetaCrtcMode *a = *(MetaCrtcMode **) one;
  MetaCrtcMode *b = *(MetaCrtcMode **) two;

  if (a->width != b->width)
    return a->width > b->width ? -1 : 1;
  if (a->height != b->height)
    return a->height > b->height ? -1 : 1;
  if (a->refresh_rate != b->refresh_rate)
    return a->refresh_rate > b->refresh_rate ? -1 : 1;

  return g_strcmp0 (b->name, a->name);
}

static MetaOutput *
find_output_by_id (MetaOutput *outputs,
                   unsigned    n_outputs,
                   glong       id)
{
  unsigned i;

  for (i = 0; i < n_outputs; i++)
    if (outputs[i].winsys_id == id)
      return &outputs[i];

  return NULL;
}

static int
find_property_index (MetaMonitorManager         *manager,
                     drmModeObjectPropertiesPtr  props,
                     const gchar                *prop_name,
                     drmModePropertyPtr         *found)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  unsigned int i;

  for (i = 0; i < props->count_props; i++)
    {
      drmModePropertyPtr prop;

      prop = drmModeGetProperty (manager_kms->fd, props->props[i]);
      if (!prop)
        continue;

      if (strcmp (prop->name, prop_name) == 0)
        {
          *found = prop;
          return i;
        }

      drmModeFreeProperty (prop);
    }

  return -1;
}

static void
parse_transforms (MetaMonitorManager *manager,
                  drmModePropertyPtr  prop,
                  MetaCrtc           *crtc)
{
  MetaCrtcKms *crtc_kms = crtc->driver_private;
  int i;

  for (i = 0; i < prop->count_enums; i++)
    {
      int cur = -1;

      if (strcmp (prop->enums[i].name, "rotate-0") == 0)
        cur = META_MONITOR_TRANSFORM_NORMAL;
      else if (strcmp (prop->enums[i].name, "rotate-90") == 0)
        cur = META_MONITOR_TRANSFORM_90;
      else if (strcmp (prop->enums[i].name, "rotate-180") == 0)
        cur = META_MONITOR_TRANSFORM_180;
      else if (strcmp (prop->enums[i].name, "rotate-270") == 0)
        cur = META_MONITOR_TRANSFORM_270;

      if (cur != -1)
        {
          crtc_kms->all_hw_transforms |= 1 << cur;
          crtc_kms->rotation_map[cur] = 1 << prop->enums[i].value;
        }
    }
}

static gboolean
is_primary_plane (MetaMonitorManager         *manager,
                  drmModeObjectPropertiesPtr  props)
{
  drmModePropertyPtr prop;
  int idx;

  idx = find_property_index (manager, props, "type", &prop);
  if (idx < 0)
    return FALSE;

  drmModeFreeProperty (prop);
  return props->prop_values[idx] == DRM_PLANE_TYPE_PRIMARY;
}

static void
init_crtc_rotations (MetaMonitorManager *manager,
                     MetaCrtc           *crtc,
                     unsigned int        idx)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  drmModeObjectPropertiesPtr props;
  drmModePlaneRes *planes;
  drmModePlane *drm_plane;
  MetaCrtcKms *crtc_kms;
  unsigned int i;

  crtc_kms = crtc->driver_private;

  planes = drmModeGetPlaneResources(manager_kms->fd);
  if (planes == NULL)
    return;

  for (i = 0; i < planes->count_planes; i++)
    {
      drmModePropertyPtr prop;

      drm_plane = drmModeGetPlane (manager_kms->fd, planes->planes[i]);

      if (!drm_plane)
        continue;

      if ((drm_plane->possible_crtcs & (1 << idx)))
        {
          props = drmModeObjectGetProperties (manager_kms->fd,
                                              drm_plane->plane_id,
                                              DRM_MODE_OBJECT_PLANE);

          if (props && is_primary_plane (manager, props))
            {
              int rotation_idx;

              crtc_kms->primary_plane_id = drm_plane->plane_id;
              rotation_idx = find_property_index (manager, props, "rotation", &prop);

              if (rotation_idx >= 0)
                {
                  crtc_kms->rotation_prop_id = props->props[rotation_idx];
                  parse_transforms (manager, prop, crtc);
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
add_common_modes (MetaMonitorManager *manager,
                  MetaOutput         *output)
{
  const drmModeModeInfo *mode;
  GPtrArray *array;
  unsigned i;
  unsigned max_hdisplay = 0;
  unsigned max_vdisplay = 0;
  float max_vrefresh = 0.0;

  for (i = 0; i < output->n_modes; i++)
    {
      mode = output->modes[i]->driver_private;
      max_hdisplay = MAX (max_hdisplay, mode->hdisplay);
      max_vdisplay = MAX (max_vdisplay, mode->vdisplay);
      max_vrefresh = MAX (max_vrefresh, drm_mode_vrefresh (mode));
    }

  max_vrefresh = MAX (max_vrefresh, 60.0);
  max_vrefresh *= (1 + SYNC_TOLERANCE);

  array = g_ptr_array_new ();
  for (i = 0; i < G_N_ELEMENTS (meta_default_drm_mode_infos); i++)
    {
      mode = &meta_default_drm_mode_infos[i];
      if (mode->hdisplay > max_hdisplay ||
          mode->vdisplay > max_vdisplay ||
          drm_mode_vrefresh (mode) > max_vrefresh)
        continue;

      g_ptr_array_add (array, find_meta_mode (manager, mode));
    }

  output->modes = g_renew (MetaCrtcMode *, output->modes,
                           output->n_modes + array->len);
  memcpy (output->modes + output->n_modes, array->pdata,
          array->len * sizeof (MetaCrtcMode *));
  output->n_modes += array->len;

  g_ptr_array_free (array, TRUE);
}

static void
init_crtc (MetaCrtc           *crtc,
           MetaMonitorManager *manager,
           drmModeCrtc        *drm_crtc)
{
  unsigned int i;

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
      for (i = 0; i < manager->n_modes; i++)
        {
          if (drm_mode_equal (&drm_crtc->mode, manager->modes[i].driver_private))
            {
              crtc->current_mode = &manager->modes[i];
              break;
            }
        }
    }

  crtc->driver_private = g_new0 (MetaCrtcKms, 1);
  crtc->driver_notify = (GDestroyNotify) meta_crtc_destroy_notify;
}

static void
init_output (MetaOutput         *output,
             MetaMonitorManager *manager,
             drmModeConnector   *connector,
             MetaOutput         *old_output)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  MetaOutputKms *output_kms;
  GArray *crtcs;
  GBytes *edid;
  unsigned int i;
  unsigned int crtc_mask;

  output_kms = g_slice_new0 (MetaOutputKms);
  output->driver_private = output_kms;
  output->driver_notify = (GDestroyNotify)meta_output_destroy_notify;

  output->winsys_id = connector->connector_id;
  output->name = make_output_name (connector);
  output->width_mm = connector->mmWidth;
  output->height_mm = connector->mmHeight;

  switch (connector->subpixel)
    {
    case DRM_MODE_SUBPIXEL_NONE:
      output->subpixel_order = COGL_SUBPIXEL_ORDER_NONE;
      break;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
      output->subpixel_order = COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB;
      break;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
      output->subpixel_order = COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR;
      break;
    case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
      output->subpixel_order = COGL_SUBPIXEL_ORDER_VERTICAL_RGB;
      break;
    case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
      output->subpixel_order = COGL_SUBPIXEL_ORDER_VERTICAL_BGR;
      break;
    case DRM_MODE_SUBPIXEL_UNKNOWN:
    default:
      output->subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
      break;
    }

  output->preferred_mode = NULL;
  output->n_modes = connector->count_modes;
  output->modes = g_new0 (MetaCrtcMode *, output->n_modes);
  for (i = 0; i < output->n_modes; i++) {
      output->modes[i] = find_meta_mode (manager, &connector->modes[i]);
      if (connector->modes[i].type & DRM_MODE_TYPE_PREFERRED)
        output->preferred_mode = output->modes[i];
  }

  if (!output->preferred_mode)
    output->preferred_mode = output->modes[0];

  output_kms->connector = connector;
  find_connector_properties (manager_kms, output_kms);

  /* FIXME: MSC feature bit? */
  /* Presume that if the output supports scaling, then we have
   * a panel fitter capable of adjusting any mode to suit.
   */
  if (output_kms->has_scaling)
    add_common_modes (manager, output);

  qsort (output->modes, output->n_modes, sizeof (MetaCrtcMode *), compare_modes);

  output_kms->n_encoders = connector->count_encoders;
  output_kms->encoders = g_new0 (drmModeEncoderPtr, output_kms->n_encoders);

  crtc_mask = ~(unsigned int) 0;
  for (i = 0; i < output_kms->n_encoders; i++)
    {
      output_kms->encoders[i] = drmModeGetEncoder (manager_kms->fd,
                                                   connector->encoders[i]);
      if (!output_kms->encoders[i])
        continue;

      /* We only list CRTCs as supported if they are supported by all encoders
         for this connectors.

         This is what xf86-video-modesetting does (see drmmode_output_init())
         */
      crtc_mask &= output_kms->encoders[i]->possible_crtcs;

      if (output_kms->encoders[i]->encoder_id == connector->encoder_id)
        output_kms->current_encoder = output_kms->encoders[i];
    }

  crtcs = g_array_new (FALSE, FALSE, sizeof (MetaCrtc*));

  for (i = 0; i < manager->n_crtcs; i++)
    {
      if (crtc_mask & (1 << i))
        {
          MetaCrtc *crtc = &manager->crtcs[i];
          g_array_append_val (crtcs, crtc);
        }
    }

  output->n_possible_crtcs = crtcs->len;
  output->possible_crtcs = (void*)g_array_free (crtcs, FALSE);

  if (output_kms->current_encoder && output_kms->current_encoder->crtc_id != 0)
    {
      for (i = 0; i < manager->n_crtcs; i++)
        {
          if (manager->crtcs[i].crtc_id == output_kms->current_encoder->crtc_id)
            {
              output->crtc = &manager->crtcs[i];
              break;
            }
        }
    }
  else
    {
      output->crtc = NULL;
    }

  if (old_output)
    {
      output->is_primary = old_output->is_primary;
      output->is_presentation = old_output->is_presentation;
    }
  else
    {
      output->is_primary = FALSE;
      output->is_presentation = FALSE;
    }

  output->suggested_x = output_kms->suggested_x;
  output->suggested_y = output_kms->suggested_y;
  output->hotplug_mode_update = output_kms->hotplug_mode_update;

  if (output_kms->edid_blob_id != 0)
    {
      GError *error = NULL;

      edid = read_output_edid (manager_kms, output, &error);
      if (!edid)
        {
          g_warning ("Failed to read EDID blob from %s: %s",
                     output->name, error->message);
          g_error_free (error);
        }
    }
  else
    {
      edid = NULL;
    }

  meta_output_parse_edid (output, edid);
  g_bytes_unref (edid);

  /* MetaConnectorType matches DRM's connector types */
  output->connector_type = (MetaConnectorType) connector->connector_type;

  output_get_tile_info (manager_kms, output);

  /* FIXME: backlight is a very driver specific thing unfortunately,
     every DDX does its own thing, and the dumb KMS API does not include it.

     For example, xf86-video-intel has a list of paths to probe in /sys/class/backlight
     (one for each major HW maker, and then some).
     We can't do the same because we're not root.
     It might be best to leave backlight out of the story and rely on the setuid
     helper in gnome-settings-daemon.
     */
  output->backlight_min = 0;
  output->backlight_max = 0;
  output->backlight = -1;
}

static void
detect_and_setup_output_clones (MetaMonitorManager *manager,
                                drmModeRes         *resources)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  drmModeEncoder **encoders;
  unsigned int i, n_encoders;

  n_encoders = (unsigned int) resources->count_encoders;
  encoders = g_new (drmModeEncoder *, n_encoders);
  for (i = 0; i < n_encoders; i++)
    encoders[i] = drmModeGetEncoder (manager_kms->fd, resources->encoders[i]);

  /*
   * Setup encoder position mask and encoder clone mask.
   */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output;
      MetaOutputKms *output_kms;
      unsigned int j;

      output = &manager->outputs[i];
      output_kms = output->driver_private;

      output_kms->enc_clone_mask = 0xff;
      output_kms->encoder_mask = 0;

      for (j = 0; j < output_kms->n_encoders; j++)
        {
          unsigned int k;

          for (k = 0; k < n_encoders; k++)
            {
              if (output_kms->encoders[j] && encoders[k] &&
                  output_kms->encoders[j]->encoder_id == encoders[k]->encoder_id)
                {
                  output_kms->encoder_mask |= (1 << k);
                  break;
                }
            }

          output_kms->enc_clone_mask &= output_kms->encoders[j]->possible_clones;
        }
    }

  for (i = 0; i < (unsigned)resources->count_encoders; i++)
    drmModeFreeEncoder (encoders[i]);
  g_free (encoders);

  /*
   * Setup MetaOutput <-> MetaOutput clone associations.
   */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output;
      MetaOutputKms *output_kms;
      unsigned int j;

      output = &manager->outputs[i];
      output_kms = output->driver_private;

      if (output_kms->enc_clone_mask == 0)
        continue;

      for (j = 0; j < manager->n_outputs; j++)
        {
          MetaOutput *meta_clone;
          MetaOutputKms *clone_kms;

          meta_clone = &manager->outputs[i];
          clone_kms = meta_clone->driver_private;

          if (meta_clone == output)
            continue;

          if (clone_kms->encoder_mask == 0)
            continue;

          if (clone_kms->encoder_mask == output_kms->enc_clone_mask)
            {
              output->n_possible_clones++;
              output->possible_clones = g_renew (MetaOutput *,
                                                 output->possible_clones,
                                                 output->n_possible_clones);
              output->possible_clones[output->n_possible_clones - 1] = meta_clone;
            }
        }
    }
}

static void
init_connectors (MetaMonitorManager *manager,
                 drmModeRes         *resources)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  unsigned int i;

  manager_kms->n_connectors = resources->count_connectors;
  manager_kms->connectors = g_new (drmModeConnector *, manager_kms->n_connectors);
  for (i = 0; i < manager_kms->n_connectors; i++)
    {
      drmModeConnector *drm_connector;

      drm_connector = drmModeGetConnector (manager_kms->fd,
                                           resources->connectors[i]);
      manager_kms->connectors[i] = drm_connector;
    }
}

static void
init_modes (MetaMonitorManager *manager,
            drmModeRes         *resources)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  GHashTable *modes;
  GHashTableIter iter;
  drmModeModeInfo *drm_mode;
  unsigned int i;
  long mode_id;

  /*
   * Gather all modes on all connected connectors.
   */
  modes = g_hash_table_new (drm_mode_hash, drm_mode_equal);
  for (i = 0; i < manager_kms->n_connectors; i++)
    {
      drmModeConnector *drm_connector;

      drm_connector = manager_kms->connectors[i];
      if (drm_connector && drm_connector->connection == DRM_MODE_CONNECTED)
        {
          unsigned int j;

          for (j = 0; j < (unsigned int) drm_connector->count_modes; j++)
            g_hash_table_add (modes, &drm_connector->modes[j]);
        }
    }

  manager->n_modes = g_hash_table_size (modes) + G_N_ELEMENTS (meta_default_drm_mode_infos);
  manager->modes = g_new0 (MetaCrtcMode, manager->n_modes);

  g_hash_table_iter_init (&iter, modes);
  mode_id = 0;
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &drm_mode))
    {
      MetaCrtcMode *mode;

      mode = &manager->modes[mode_id];
      init_mode (mode, drm_mode, (long) mode_id);

      mode_id++;
    }

  g_hash_table_destroy (modes);

  for (i = 0; i < G_N_ELEMENTS (meta_default_drm_mode_infos); i++)
    {
      MetaCrtcMode *mode;

      mode = &manager->modes[mode_id];
      init_mode (mode, &meta_default_drm_mode_infos[i], (long) mode_id);

      mode_id++;
    }
}

static void
init_crtcs (MetaMonitorManager *manager,
            drmModeRes         *resources)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  unsigned int i;

  manager->n_crtcs = resources->count_crtcs;
  manager->crtcs = g_new0 (MetaCrtc, manager->n_crtcs);

  for (i = 0; i < (unsigned)resources->count_crtcs; i++)
    {
      drmModeCrtc *drm_crtc;
      MetaCrtc *crtc;

      drm_crtc = drmModeGetCrtc (manager_kms->fd, resources->crtcs[i]);

      crtc = &manager->crtcs[i];

      init_crtc (crtc, manager, drm_crtc);
      find_crtc_properties (manager_kms, crtc);
      init_crtc_rotations (manager, crtc, i);

      drmModeFreeCrtc (drm_crtc);
    }
}

static void
init_outputs (MetaMonitorManager *manager,
              drmModeRes         *resources)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  MetaOutput *old_outputs;
  unsigned int n_old_outputs;
  unsigned int n_actual_outputs;
  unsigned int i;

  old_outputs = manager->outputs;
  n_old_outputs = manager->n_outputs;

  manager->outputs = g_new0 (MetaOutput, manager_kms->n_connectors);
  n_actual_outputs = 0;

  for (i = 0; i < manager_kms->n_connectors; i++)
    {
      drmModeConnector *connector;
      MetaOutput *output;

      connector = manager_kms->connectors[i];
      output = &manager->outputs[n_actual_outputs];

      if (connector && connector->connection == DRM_MODE_CONNECTED)
        {
          MetaOutput *old_output;

          old_output = find_output_by_id (old_outputs, n_old_outputs,
                                          output->winsys_id);
          init_output (output, manager, connector, old_output);
          n_actual_outputs++;
        }
    }

  manager->n_outputs = n_actual_outputs;
  manager->outputs = g_renew (MetaOutput, manager->outputs, manager->n_outputs);

  /* Sort the outputs for easier handling in MetaMonitorConfig */
  qsort (manager->outputs, manager->n_outputs, sizeof (MetaOutput),
         compare_outputs);

  detect_and_setup_output_clones (manager, resources);
}

static void
meta_monitor_manager_kms_read_current (MetaMonitorManager *manager)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  drmModeRes *resources;

  resources = drmModeGetResources (manager_kms->fd);

  manager_kms->max_buffer_width = resources->max_width;
  manager_kms->max_buffer_height = resources->max_height;

  manager->power_save_mode = META_POWER_SAVE_ON;

  /* Note: we must not free the public structures (output, crtc, monitor
     mode and monitor info) here, they must be kept alive until the API
     users are done with them after we emit monitors-changed, and thus
     are freed by the platform-independent layer. */
  free_resources (manager_kms);

  init_connectors (manager, resources);
  init_modes (manager, resources);
  init_crtcs (manager, resources);
  init_outputs (manager, resources);

  drmModeFreeResources (resources);
}

static GBytes *
meta_monitor_manager_kms_read_edid (MetaMonitorManager *manager,
                                    MetaOutput         *output)
{
  MetaOutputKms *output_kms = output->driver_private;
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  GError *error = NULL;
  GBytes *edid;

  if (output_kms->edid_blob_id == 0)
    return NULL;

  edid = read_output_edid (manager_kms, output, &error);
  if (!edid)
    {
      g_warning ("Failed to read EDID from '%s': %s",
                 output->name, error->message);
      g_error_free (error);
      return NULL;
    }

  return edid;
}

static void
meta_monitor_manager_kms_set_power_save_mode (MetaMonitorManager *manager,
                                              MetaPowerSave       mode)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  uint64_t state;
  unsigned i;

  switch (mode) {
  case META_POWER_SAVE_ON:
    state = DRM_MODE_DPMS_ON;
    break;
  case META_POWER_SAVE_STANDBY:
    state = DRM_MODE_DPMS_STANDBY;
    break;
  case META_POWER_SAVE_SUSPEND:
    state = DRM_MODE_DPMS_SUSPEND;
    break;
  case META_POWER_SAVE_OFF:
    state = DRM_MODE_DPMS_OFF;
    break;
  default:
    return;
  }

  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output;
      MetaOutputKms *output_kms;

      output = &manager->outputs[i];
      output_kms = output->driver_private;

      if (output_kms->dpms_prop_id != 0)
        {
          int ok = drmModeObjectSetProperty (manager_kms->fd, output->winsys_id,
                                             DRM_MODE_OBJECT_CONNECTOR,
                                             output_kms->dpms_prop_id, state);

          if (ok < 0)
            meta_warning ("Failed to set power save mode for output %s: %s\n",
                          output->name, strerror (errno));
        }
    }
}

static void
set_underscan (MetaMonitorManagerKms *manager_kms,
               MetaOutput *output)
{
  if (!output->crtc)
    return;

  MetaCrtc *crtc = output->crtc;
  MetaCrtcKms *crtc_kms = crtc->driver_private;
  if (!crtc_kms->underscan_prop_id)
    return;

  if (output->is_underscanning)
    {
      drmModeObjectSetProperty (manager_kms->fd, crtc->crtc_id,
                                DRM_MODE_OBJECT_CRTC,
                                crtc_kms->underscan_prop_id, (uint64_t) 1);

      if (crtc_kms->underscan_hborder_prop_id)
        {
          uint64_t value = crtc->current_mode->width * 0.05;
          drmModeObjectSetProperty (manager_kms->fd, crtc->crtc_id,
                                    DRM_MODE_OBJECT_CRTC,
                                    crtc_kms->underscan_hborder_prop_id, value);
        }
      if (crtc_kms->underscan_vborder_prop_id)
        {
          uint64_t value = crtc->current_mode->height * 0.05;
          drmModeObjectSetProperty (manager_kms->fd, crtc->crtc_id,
                                    DRM_MODE_OBJECT_CRTC,
                                    crtc_kms->underscan_vborder_prop_id, value);
        }

    }
  else
    {
      drmModeObjectSetProperty (manager_kms->fd, crtc->crtc_id,
                                DRM_MODE_OBJECT_CRTC,
                                crtc_kms->underscan_prop_id, (uint64_t) 0);
    }
}

static void
meta_monitor_manager_kms_ensure_initial_config (MetaMonitorManager *manager)
{
  MetaMonitorsConfig *config;

  config = meta_monitor_manager_ensure_configured (manager);

  if (meta_is_monitor_config_manager_enabled ())
    meta_monitor_manager_update_logical_state (manager, config);
  else
    meta_monitor_manager_update_logical_state_derived (manager);
}

static void
apply_crtc_assignments (MetaMonitorManager *manager,
                        MetaCrtcInfo       **crtcs,
                        unsigned int         n_crtcs,
                        MetaOutputInfo     **outputs,
                        unsigned int         n_outputs)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  unsigned i;

  for (i = 0; i < n_crtcs; i++)
    {
      MetaCrtcInfo *crtc_info = crtcs[i];
      MetaCrtc *crtc = crtc_info->crtc;
      MetaCrtcKms *crtc_kms = crtc->driver_private;
      MetaMonitorTransform hw_transform;

      crtc->is_dirty = TRUE;

      if (crtc_info->mode == NULL)
        {
          crtc->rect.x = 0;
          crtc->rect.y = 0;
          crtc->rect.width = 0;
          crtc->rect.height = 0;
          crtc->current_mode = NULL;
        }
      else
        {
          MetaCrtcMode *mode;
          unsigned int j;
          int width, height;

          mode = crtc_info->mode;

          if (meta_monitor_transform_is_rotated (crtc_info->transform))
            {
              width = mode->height;
              height = mode->width;
            }
          else
            {
              width = mode->width;
              height = mode->height;
            }

          crtc->rect.x = crtc_info->x;
          crtc->rect.y = crtc_info->y;
          crtc->rect.width = width;
          crtc->rect.height = height;
          crtc->current_mode = mode;
          crtc->transform = crtc_info->transform;

          for (j = 0; j < crtc_info->outputs->len; j++)
            {
              MetaOutput *output = g_ptr_array_index (crtc_info->outputs, j);

              output->is_dirty = TRUE;
              output->crtc = crtc;
            }
        }

      if (crtc_kms->all_hw_transforms & (1 << crtc->transform))
        hw_transform = crtc->transform;
      else
        hw_transform = META_MONITOR_TRANSFORM_NORMAL;

      if (drmModeObjectSetProperty (manager_kms->fd,
                                    crtc_kms->primary_plane_id,
                                    DRM_MODE_OBJECT_PLANE,
                                    crtc_kms->rotation_prop_id,
                                    crtc_kms->rotation_map[hw_transform]) != 0)
        {
          g_warning ("Failed to apply DRM plane transform %d: %m", hw_transform);

          /* Blacklist this HW transform, we want to fallback to our
           * fallbacks in this case.
           */
          crtc_kms->all_hw_transforms &= ~(1 << hw_transform);
        }
    }
  /* Disable CRTCs not mentioned in the list (they have is_dirty == FALSE,
     because they weren't seen in the first loop) */
  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCrtc *crtc = &manager->crtcs[i];

      crtc->logical_monitor = NULL;

      if (crtc->is_dirty)
        {
          crtc->is_dirty = FALSE;
          continue;
        }

      crtc->rect.x = 0;
      crtc->rect.y = 0;
      crtc->rect.width = 0;
      crtc->rect.height = 0;
      crtc->current_mode = NULL;
    }

  for (i = 0; i < n_outputs; i++)
    {
      MetaOutputInfo *output_info = outputs[i];
      MetaOutput *output = output_info->output;

      output->is_primary = output_info->is_primary;
      output->is_presentation = output_info->is_presentation;
      output->is_underscanning = output_info->is_underscanning;

      set_underscan (manager_kms, output);
    }

  /* Disable outputs not mentioned in the list */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output = &manager->outputs[i];

      if (output->is_dirty)
        {
          output->is_dirty = FALSE;
          continue;
        }

      output->crtc = NULL;
      output->is_primary = FALSE;
    }
}

static void
update_screen_size (MetaMonitorManager *manager,
                    MetaMonitorsConfig *config)
{
  GList *l;
  int screen_width = 0;
  int screen_height = 0;

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;
      int right_edge;
      int bottom_edge;

      right_edge = (logical_monitor_config->layout.width +
                    logical_monitor_config->layout.x);
      if (right_edge > screen_width)
        screen_width = right_edge;

      bottom_edge = (logical_monitor_config->layout.height +
                     logical_monitor_config->layout.y);
      if (bottom_edge > screen_height)
        screen_height = bottom_edge;
    }

  manager->screen_width = screen_width;
  manager->screen_height = screen_height;
}

static gboolean
meta_monitor_manager_kms_apply_monitors_config (MetaMonitorManager      *manager,
                                                MetaMonitorsConfig      *config,
                                                MetaMonitorsConfigMethod method,
                                                GError                 **error)
{
  GPtrArray *crtc_infos;
  GPtrArray *output_infos;

  if (!config)
    {
      manager->screen_width = 0;
      manager->screen_height = 0;
      meta_monitor_manager_rebuild (manager, NULL);
      return TRUE;
    }

  if (!meta_monitor_config_manager_assign (manager, config,
                                           &crtc_infos, &output_infos,
                                           error))
    return FALSE;

  if (method == META_MONITORS_CONFIG_METHOD_VERIFY)
    {
      g_ptr_array_free (crtc_infos, TRUE);
      g_ptr_array_free (output_infos, TRUE);
      return TRUE;
    }

  apply_crtc_assignments (manager,
                          (MetaCrtcInfo **) crtc_infos->pdata,
                          crtc_infos->len,
                          (MetaOutputInfo **) output_infos->pdata,
                          output_infos->len);

  g_ptr_array_free (crtc_infos, TRUE);
  g_ptr_array_free (output_infos, TRUE);

  update_screen_size (manager, config);
  meta_monitor_manager_rebuild (manager, config);

  return TRUE;
}

static void
legacy_calculate_screen_size (MetaMonitorManager *manager)
{
  unsigned int i;
  int width = 0, height = 0;

  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCrtc *crtc = &manager->crtcs[i];

      width = MAX (width, crtc->rect.x + crtc->rect.width);
      height = MAX (height, crtc->rect.y + crtc->rect.height);
    }

  manager->screen_width = width;
  manager->screen_height = height;
}

static void
meta_monitor_manager_kms_apply_configuration (MetaMonitorManager *manager,
                                              MetaCrtcInfo      **crtcs,
                                              unsigned int        n_crtcs,
                                              MetaOutputInfo    **outputs,
                                              unsigned int        n_outputs)
{
  apply_crtc_assignments (manager, crtcs, n_crtcs, outputs, n_outputs);

  legacy_calculate_screen_size (manager);
  meta_monitor_manager_rebuild_derived (manager);
}

static void
meta_monitor_manager_kms_get_crtc_gamma (MetaMonitorManager  *manager,
                                         MetaCrtc            *crtc,
                                         gsize               *size,
                                         unsigned short     **red,
                                         unsigned short     **green,
                                         unsigned short     **blue)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  drmModeCrtc *kms_crtc;

  kms_crtc = drmModeGetCrtc (manager_kms->fd, crtc->crtc_id);

  *size = kms_crtc->gamma_size;
  *red = g_new (unsigned short, *size);
  *green = g_new (unsigned short, *size);
  *blue = g_new (unsigned short, *size);

  drmModeCrtcGetGamma (manager_kms->fd, crtc->crtc_id, *size, *red, *green, *blue);

  drmModeFreeCrtc (kms_crtc);
}

static void
meta_monitor_manager_kms_set_crtc_gamma (MetaMonitorManager *manager,
                                         MetaCrtc           *crtc,
                                         gsize               size,
                                         unsigned short     *red,
                                         unsigned short     *green,
                                         unsigned short     *blue)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);

  drmModeCrtcSetGamma (manager_kms->fd, crtc->crtc_id, size, red, green, blue);
}

static void
handle_hotplug_event (MetaMonitorManager *manager)
{
  meta_monitor_manager_read_current_state (manager);
  meta_monitor_manager_on_hotplug (manager);
}

static void
on_uevent (GUdevClient *client,
           const char  *action,
           GUdevDevice *device,
           gpointer     user_data)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (user_data);
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);

  if (!g_udev_device_get_property_as_boolean (device, "HOTPLUG"))
    return;

  handle_hotplug_event (manager);
}

static gboolean
kms_event_check (GSource *source)
{
  MetaKmsSource *kms_source = (MetaKmsSource *) source;

  return g_source_query_unix_fd (source, kms_source->fd_tag) & G_IO_IN;
}

static gboolean
kms_event_dispatch (GSource    *source,
                    GSourceFunc callback,
                    gpointer    user_data)
{
  MetaKmsSource *kms_source = (MetaKmsSource *) source;

  meta_monitor_manager_kms_wait_for_flip (kms_source->manager_kms);

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs kms_event_funcs = {
  NULL,
  kms_event_check,
  kms_event_dispatch
};

static void
meta_monitor_manager_kms_connect_uevent_handler (MetaMonitorManagerKms *manager_kms)
{
  manager_kms->uevent_handler_id = g_signal_connect (manager_kms->udev,
                                                     "uevent",
                                                     G_CALLBACK (on_uevent),
                                                     manager_kms);
}

static void
meta_monitor_manager_kms_disconnect_uevent_handler (MetaMonitorManagerKms *manager_kms)
{
  g_signal_handler_disconnect (manager_kms->udev,
                               manager_kms->uevent_handler_id);
  manager_kms->uevent_handler_id = 0;
}

void
meta_monitor_manager_kms_pause (MetaMonitorManagerKms *manager_kms)
{
  meta_monitor_manager_kms_disconnect_uevent_handler (manager_kms);
}

void
meta_monitor_manager_kms_resume (MetaMonitorManagerKms *manager_kms)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);

  meta_monitor_manager_kms_connect_uevent_handler (manager_kms);
  handle_hotplug_event (manager);
}

static void
meta_monitor_manager_kms_init (MetaMonitorManagerKms *manager_kms)
{
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);
  GSource *source;

  manager_kms->fd = meta_renderer_native_get_kms_fd (renderer_native);

  drmSetClientCap (manager_kms->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

  const char *subsystems[2] = { "drm", NULL };
  manager_kms->udev = g_udev_client_new (subsystems);
  meta_monitor_manager_kms_connect_uevent_handler (manager_kms);

  source = g_source_new (&kms_event_funcs, sizeof (MetaKmsSource));
  manager_kms->source = (MetaKmsSource *) source;
  manager_kms->source->fd_tag = g_source_add_unix_fd (source,
                                                      manager_kms->fd,
                                                      G_IO_IN | G_IO_ERR);
  manager_kms->source->manager_kms = manager_kms;
  g_source_attach (source, NULL);

  manager_kms->desktop_settings = g_settings_new ("org.gnome.desktop.interface");
}

static void
get_crtc_connectors (MetaMonitorManager *manager,
                     MetaCrtc           *crtc,
                     uint32_t          **connectors,
                     unsigned int       *n_connectors)
{
  unsigned int i;
  GArray *connectors_array = g_array_new (FALSE, FALSE, sizeof (uint32_t));

  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output = &manager->outputs[i];

      if (output->crtc == crtc)
        g_array_append_val (connectors_array, output->winsys_id);
    }

  *n_connectors = connectors_array->len;
  *connectors = (uint32_t *) g_array_free (connectors_array, FALSE);
}

gboolean
meta_monitor_manager_kms_apply_crtc_mode (MetaMonitorManagerKms *manager_kms,
                                          MetaCrtc              *crtc,
                                          int                    x,
                                          int                    y,
                                          uint32_t               fb_id)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);
  uint32_t *connectors;
  unsigned int n_connectors;
  drmModeModeInfo *mode;

  get_crtc_connectors (manager, crtc, &connectors, &n_connectors);

  if (connectors)
    mode = crtc->current_mode->driver_private;
  else
    mode = NULL;

  if (drmModeSetCrtc (manager_kms->fd,
                      crtc->crtc_id,
                      fb_id,
                      x, y,
                      connectors, n_connectors,
                      mode) != 0)
    {
      g_warning ("Failed to set CRTC mode %s: %m", crtc->current_mode->name);
      return FALSE;
    }

  g_free (connectors);

  return TRUE;
}

static void
invoke_flip_closure (GClosure *flip_closure)
{
  GValue param = G_VALUE_INIT;

  g_value_init (&param, G_TYPE_POINTER);
  g_value_set_pointer (&param, flip_closure);
  g_closure_invoke (flip_closure, NULL, 1, &param, NULL);
  g_closure_unref (flip_closure);
}

gboolean
meta_monitor_manager_kms_is_crtc_active (MetaMonitorManagerKms *manager_kms,
                                         MetaCrtc              *crtc)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);
  unsigned int i;
  gboolean connected_crtc_found;

  if (manager->power_save_mode != META_POWER_SAVE_ON)
    return FALSE;

  connected_crtc_found = FALSE;
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output = &manager->outputs[i];

      if (output->crtc == crtc)
        {
          connected_crtc_found = TRUE;
          break;
        }
    }

  if (!connected_crtc_found)
    return FALSE;

  return TRUE;
}

gboolean
meta_monitor_manager_kms_flip_crtc (MetaMonitorManagerKms *manager_kms,
                                    MetaCrtc              *crtc,
                                    int                    x,
                                    int                    y,
                                    uint32_t               fb_id,
                                    GClosure              *flip_closure,
                                    gboolean              *fb_in_use)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);
  uint32_t *connectors;
  unsigned int n_connectors;
  int ret = -1;

  g_assert (manager->power_save_mode == META_POWER_SAVE_ON);

  get_crtc_connectors (manager, crtc, &connectors, &n_connectors);
  g_assert (n_connectors > 0);

  if (!manager_kms->page_flips_not_supported)
    {
      ret = drmModePageFlip (manager_kms->fd,
                             crtc->crtc_id,
                             fb_id,
                             DRM_MODE_PAGE_FLIP_EVENT,
                             flip_closure);
      if (ret != 0 && ret != -EACCES)
        {
          g_warning ("Failed to flip: %s", strerror (-ret));
          manager_kms->page_flips_not_supported = TRUE;
        }
    }

  if (manager_kms->page_flips_not_supported)
    {
      if (meta_monitor_manager_kms_apply_crtc_mode (manager_kms,
                                                    crtc,
                                                    x, y,
                                                    fb_id))
        {
          *fb_in_use = TRUE;
          return FALSE;
        }
    }

  if (ret != 0)
    return FALSE;

  *fb_in_use = TRUE;
  g_closure_ref (flip_closure);

  return TRUE;
}

static void
page_flip_handler (int fd,
                   unsigned int frame,
                   unsigned int sec,
                   unsigned int usec,
                   void *data)
{
  GClosure *flip_closure = data;

  invoke_flip_closure (flip_closure);
}

void
meta_monitor_manager_kms_wait_for_flip (MetaMonitorManagerKms *manager_kms)
{
  drmEventContext evctx;

  if (manager_kms->page_flips_not_supported)
    return;

  memset (&evctx, 0, sizeof evctx);
  evctx.version = DRM_EVENT_CONTEXT_VERSION;
  evctx.page_flip_handler = page_flip_handler;
  drmHandleEvent (manager_kms->fd, &evctx);
}

static gboolean
meta_monitor_manager_kms_is_transform_handled (MetaMonitorManager  *manager,
                                               MetaCrtc            *crtc,
                                               MetaMonitorTransform transform)
{
  MetaCrtcKms *crtc_kms = crtc->driver_private;

  if ((1 << crtc->transform) & crtc_kms->all_hw_transforms)
    return TRUE;
  else
    return FALSE;
}

/* The minimum resolution at which we turn on a window-scale of 2 */
#define HIDPI_LIMIT 192

/* The minimum screen height at which we turn on a window-scale of 2;
 * below this there just isn't enough vertical real estate for GNOME
 * apps to work, and it's better to just be tiny */
#define HIDPI_MIN_HEIGHT 1200

/* From http://en.wikipedia.org/wiki/4K_resolution#Resolutions_of_common_formats */
#define SMALLEST_4K_WIDTH 3656

static int
compute_scale (MetaMonitor     *monitor,
               MetaMonitorMode *monitor_mode)
{
  int resolution_width, resolution_height;
  int width_mm, height_mm;
  int scale;

  scale = 1;

  meta_monitor_mode_get_resolution (monitor_mode,
                                    &resolution_width,
                                    &resolution_height);

  if (resolution_height < HIDPI_MIN_HEIGHT)
    goto out;

  /* 4K TV */
  switch (meta_monitor_get_connector_type (monitor))
    {
    case META_CONNECTOR_TYPE_HDMIA:
    case META_CONNECTOR_TYPE_HDMIB:
      if (resolution_width >= SMALLEST_4K_WIDTH)
        goto out;
      break;
    default:
      break;
    }

  meta_monitor_get_physical_dimensions (monitor, &width_mm, &height_mm);

  /*
   * Somebody encoded the aspect ratio (16/9 or 16/10) instead of the physical
   * size.
   */
  if ((width_mm == 160 && height_mm == 90) ||
      (width_mm == 160 && height_mm == 100) ||
      (width_mm == 16 && height_mm == 9) ||
      (width_mm == 16 && height_mm == 10))
    goto out;

  if (width_mm > 0 && height_mm > 0)
    {
      double dpi_x, dpi_y;

      dpi_x = (double) resolution_width / (width_mm / 25.4);
      dpi_y = (double) resolution_height / (height_mm / 25.4);

      /*
       * We don't completely trust these values so both must be high, and never
       * pick higher ratio than 2 automatically.
       */
      if (dpi_x > HIDPI_LIMIT && dpi_y > HIDPI_LIMIT)
        scale = 2;
    }

out:
  return scale;
}

static int
meta_monitor_manager_kms_calculate_monitor_mode_scale (MetaMonitorManager *manager,
                                                       MetaMonitor        *monitor,
                                                       MetaMonitorMode    *monitor_mode)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  int global_scale;

  global_scale = g_settings_get_uint (manager_kms->desktop_settings,
                                      "scaling-factor");
  if (global_scale > 0)
    return global_scale;
  else
    return compute_scale (monitor, monitor_mode);
}

static void
meta_monitor_manager_kms_get_supported_scales (MetaMonitorManager *manager,
                                               float             **scales,
                                               int                *n_scales)
{
  *scales = supported_scales_kms;
  *n_scales = G_N_ELEMENTS (supported_scales_kms);
}

static MetaMonitorManagerCapability
meta_monitor_manager_kms_get_capabilities (MetaMonitorManager *manager)
{
  MetaBackend *backend = meta_get_backend ();
  MetaSettings *settings = meta_backend_get_settings (backend);
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);
  MetaMonitorManagerCapability capabilities =
    META_MONITOR_MANAGER_CAPABILITY_NONE;

  if (meta_settings_is_experimental_feature_enabled (
        settings,
        META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER))
    capabilities |= META_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE;

  switch (meta_renderer_native_get_mode (renderer_native))
    {
    case META_RENDERER_NATIVE_MODE_GBM:
      capabilities |= META_MONITOR_MANAGER_CAPABILITY_MIRRORING;
      break;
#ifdef HAVE_EGL_DEVICE
    case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
      break;
#endif
    }

  return capabilities;
}

static gboolean
meta_monitor_manager_kms_get_max_screen_size (MetaMonitorManager *manager,
                                              int                *max_width,
                                              int                *max_height)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);

  if (meta_is_stage_views_enabled ())
    return FALSE;

  *max_width = manager_kms->max_buffer_width;
  *max_height = manager_kms->max_buffer_height;

  return TRUE;
}

static MetaLogicalMonitorLayoutMode
meta_monitor_manager_kms_get_default_layout_mode (MetaMonitorManager *manager)
{
  MetaBackend *backend = meta_get_backend ();
  MetaSettings *settings = meta_backend_get_settings (backend);

  if (!meta_is_stage_views_enabled ())
    return META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;

  if (meta_settings_is_experimental_feature_enabled (
        settings,
        META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER))
    return META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL;
  else
    return META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
}

static void
meta_monitor_manager_kms_dispose (GObject *object)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (object);

  g_clear_object (&manager_kms->udev);
  g_clear_object (&manager_kms->desktop_settings);

  G_OBJECT_CLASS (meta_monitor_manager_kms_parent_class)->dispose (object);
}

static void
meta_monitor_manager_kms_finalize (GObject *object)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (object);

  free_resources (manager_kms);
  g_source_destroy ((GSource *) manager_kms->source);

  G_OBJECT_CLASS (meta_monitor_manager_kms_parent_class)->finalize (object);
}

static void
meta_monitor_manager_kms_class_init (MetaMonitorManagerKmsClass *klass)
{
  MetaMonitorManagerClass *manager_class = META_MONITOR_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_monitor_manager_kms_dispose;
  object_class->finalize = meta_monitor_manager_kms_finalize;

  manager_class->read_current = meta_monitor_manager_kms_read_current;
  manager_class->read_edid = meta_monitor_manager_kms_read_edid;
  manager_class->ensure_initial_config = meta_monitor_manager_kms_ensure_initial_config;
  manager_class->apply_monitors_config = meta_monitor_manager_kms_apply_monitors_config;
  manager_class->apply_configuration = meta_monitor_manager_kms_apply_configuration;
  manager_class->set_power_save_mode = meta_monitor_manager_kms_set_power_save_mode;
  manager_class->get_crtc_gamma = meta_monitor_manager_kms_get_crtc_gamma;
  manager_class->set_crtc_gamma = meta_monitor_manager_kms_set_crtc_gamma;
  manager_class->is_transform_handled = meta_monitor_manager_kms_is_transform_handled;
  manager_class->calculate_monitor_mode_scale = meta_monitor_manager_kms_calculate_monitor_mode_scale;
  manager_class->get_supported_scales = meta_monitor_manager_kms_get_supported_scales;
  manager_class->get_capabilities = meta_monitor_manager_kms_get_capabilities;
  manager_class->get_max_screen_size = meta_monitor_manager_kms_get_max_screen_size;
  manager_class->get_default_layout_mode = meta_monitor_manager_kms_get_default_layout_mode;
}
