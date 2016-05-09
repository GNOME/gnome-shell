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
#include "meta-backend-private.h"
#include "meta-renderer-native.h"

#include <string.h>
#include <stdlib.h>
#include <clutter/clutter.h>

#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <meta/main.h>
#include <meta/errors.h>

#include <gudev/gudev.h>

#define ALL_TRANSFORMS (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)

typedef struct {
  drmModeConnector *connector;

  unsigned n_encoders;
  drmModeEncoderPtr *encoders;
  drmModeEncoderPtr  current_encoder;

  /* bitmasks of encoder position in the resources array */
  uint32_t encoder_mask;
  uint32_t enc_clone_mask;

  uint32_t dpms_prop_id;
  uint32_t edid_blob_id;
  uint32_t tile_blob_id;

  int suggested_x;
  int suggested_y;
  uint32_t hotplug_mode_update;
} MetaOutputKms;

typedef struct {
  uint32_t underscan_prop_id;
  uint32_t underscan_hborder_prop_id;
  uint32_t underscan_vborder_prop_id;
  uint32_t primary_plane_id;
  uint32_t rotation_prop_id;
  uint32_t rotation_map[ALL_TRANSFORMS];
} MetaCRTCKms;

struct _MetaMonitorManagerKms
{
  MetaMonitorManager parent_instance;

  int fd;

  drmModeConnector **connectors;
  unsigned int       n_connectors;

  GUdevClient *udev;

  GSettings *desktop_settings;
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
    "unknown", "VGA", "DVII", "DVID", "DVID", "Composite",
    "SVIDEO", "LVDS", "Component", "9PinDIN", "DisplayPort",
    "HDMIA", "HDMIB", "TV", "eDP", "Virtual", "DSI"
  };
  const char *connector_type_name;

  if (connector->connector_type < G_N_ELEMENTS (connector_type_names))
    connector_type_name = connector_type_names[connector->connector_type];
  else
    connector_type_name = "unknown";

  return g_strdup_printf ("%s%d", connector_type_name, connector->connector_id);
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
meta_monitor_mode_destroy_notify (MetaMonitorMode *output)
{
  g_slice_free (drmModeModeInfo, output->driver_private);
}

static void
meta_crtc_destroy_notify (MetaCRTC *crtc)
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
      
      drmModeFreeProperty (prop);
    }
}

static void
find_crtc_properties (MetaMonitorManagerKms *manager_kms,
                      MetaCRTC *meta_crtc)
{
  MetaCRTCKms *crtc_kms;
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

static GBytes *
read_output_edid (MetaMonitorManagerKms *manager_kms,
                  MetaOutput            *output)
{
  MetaOutputKms *output_kms = output->driver_private;
  drmModePropertyBlobPtr edid_blob = NULL;

  if (output_kms->edid_blob_id == 0)
    return NULL;

  edid_blob = drmModeGetPropertyBlob (manager_kms->fd, output_kms->edid_blob_id);
  if (!edid_blob)
    {
      meta_warning ("Failed to read EDID of output %s: %s\n", output->name, strerror(errno));
      return NULL;
    }

  if (edid_blob->length > 0)
    {
      return g_bytes_new_with_free_func (edid_blob->data, edid_blob->length,
                                         (GDestroyNotify)drmModeFreePropertyBlob, edid_blob);
    }
  else
    {
      drmModeFreePropertyBlob (edid_blob);
      return NULL;
    }
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

      if (ret != 8)
        return FALSE;
      return TRUE;
    }
  else
    {
      drmModeFreePropertyBlob (tile_blob);
      return FALSE;
    }
}

static MetaMonitorMode *
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

/* The minimum resolution at which we turn on a window-scale of 2 */
#define HIDPI_LIMIT 192

/* The minimum screen height at which we turn on a window-scale of 2;
 * below this there just isn't enough vertical real estate for GNOME
 * apps to work, and it's better to just be tiny */
#define HIDPI_MIN_HEIGHT 1200

/* From http://en.wikipedia.org/wiki/4K_resolution#Resolutions_of_common_formats */
#define SMALLEST_4K_WIDTH 3656

/* Based on code from gnome-settings-daemon */
static int
compute_scale (MetaOutput *output)
{
  int scale = 1;

  if (!output->crtc)
    goto out;

  /* Scaling makes no sense */
  if (output->crtc->rect.width < HIDPI_MIN_HEIGHT)
    goto out;

  /* 4K TV */
  if (output->name != NULL && strstr(output->name, "HDMI") != NULL &&
      output->crtc->rect.width >= SMALLEST_4K_WIDTH)
    goto out;

  /* Somebody encoded the aspect ratio (16/9 or 16/10)
   * instead of the physical size */
  if ((output->width_mm == 160 && output->height_mm == 90) ||
      (output->width_mm == 160 && output->height_mm == 100) ||
      (output->width_mm == 16 && output->height_mm == 9) ||
      (output->width_mm == 16 && output->height_mm == 10))
    goto out;

  if (output->width_mm > 0 && output->height_mm > 0)
    {
      double dpi_x, dpi_y;
      dpi_x = (double)output->crtc->rect.width / (output->width_mm / 25.4);
      dpi_y = (double)output->crtc->rect.height / (output->height_mm / 25.4);
      /* We don't completely trust these values so both
         must be high, and never pick higher ratio than
         2 automatically */
      if (dpi_x > HIDPI_LIMIT && dpi_y > HIDPI_LIMIT)
        scale = 2;
    }

out:
  return scale;
}

static int
get_output_scale (MetaMonitorManager *manager,
                  MetaOutput         *output)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  int scale = g_settings_get_uint (manager_kms->desktop_settings, "scaling-factor");
  if (scale > 0)
    return scale;
  else
    return compute_scale (output);
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
                  MetaCRTC           *crtc)
{
  MetaCRTCKms *crtc_kms = crtc->driver_private;
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
          crtc->all_transforms |= 1 << cur;
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
                     MetaCRTC           *crtc,
                     unsigned int        idx)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  drmModeObjectPropertiesPtr props;
  drmModePlaneRes *planes;
  drmModePlane *drm_plane;
  MetaCRTCKms *crtc_kms;
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

  drmModeFreePlaneResources (planes);
}

static void
meta_monitor_manager_kms_read_current (MetaMonitorManager *manager)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  drmModeRes *resources;
  drmModeEncoder **encoders;
  GHashTable *modes;
  GHashTableIter iter;
  drmModeModeInfo *mode;
  unsigned int i, j, k;
  unsigned int n_actual_outputs;
  int width, height;
  MetaOutput *old_outputs;
  unsigned int n_old_outputs;

  resources = drmModeGetResources(manager_kms->fd);
  modes = g_hash_table_new (drm_mode_hash, drm_mode_equal);

  manager->max_screen_width = resources->max_width;
  manager->max_screen_height = resources->max_height;

  manager->power_save_mode = META_POWER_SAVE_ON;

  old_outputs = manager->outputs;
  n_old_outputs = manager->n_outputs;

  /* Note: we must not free the public structures (output, crtc, monitor
     mode and monitor info) here, they must be kept alive until the API
     users are done with them after we emit monitors-changed, and thus
     are freed by the platform-independent layer. */
  free_resources (manager_kms);

  manager_kms->n_connectors = resources->count_connectors;
  manager_kms->connectors = g_new (drmModeConnector *, manager_kms->n_connectors);
  for (i = 0; i < manager_kms->n_connectors; i++)
    {
      drmModeConnector *connector;

      connector = drmModeGetConnector (manager_kms->fd, resources->connectors[i]);
      manager_kms->connectors[i] = connector;

      if (connector && connector->connection == DRM_MODE_CONNECTED)
        {
          /* Collect all modes for this connector */
          for (j = 0; j < (unsigned)connector->count_modes; j++)
            g_hash_table_add (modes, &connector->modes[j]);
        }
    }

  encoders = g_new (drmModeEncoder *, resources->count_encoders);
  for (i = 0; i < (unsigned)resources->count_encoders; i++)
    encoders[i] = drmModeGetEncoder (manager_kms->fd, resources->encoders[i]);

  manager->n_modes = g_hash_table_size (modes);
  manager->modes = g_new0 (MetaMonitorMode, manager->n_modes);
  g_hash_table_iter_init (&iter, modes);
  i = 0;
  while (g_hash_table_iter_next (&iter, NULL, (gpointer)&mode))
    {
      MetaMonitorMode *meta_mode;

      meta_mode = &manager->modes[i];

      meta_mode->mode_id = i;
      meta_mode->name = g_strndup (mode->name, DRM_DISPLAY_MODE_LEN);
      meta_mode->width = mode->hdisplay;
      meta_mode->height = mode->vdisplay;
      meta_mode->flags = mode->flags;

      /* Calculate refresh rate in milliHz first for extra precision. */
      meta_mode->refresh_rate = (mode->clock * 1000000LL) / mode->htotal;
      meta_mode->refresh_rate += (mode->vtotal / 2);
      meta_mode->refresh_rate /= mode->vtotal;
      if (mode->flags & DRM_MODE_FLAG_INTERLACE)
	meta_mode->refresh_rate *= 2;
      if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
        meta_mode->refresh_rate /= 2;
      if (mode->vscan > 1)
        meta_mode->refresh_rate /= mode->vscan;
      meta_mode->refresh_rate /= 1000.0;

      meta_mode->driver_private = g_slice_dup (drmModeModeInfo, mode);
      meta_mode->driver_notify = (GDestroyNotify)meta_monitor_mode_destroy_notify;

      i++;
    }
  g_hash_table_destroy (modes);

  manager->n_crtcs = resources->count_crtcs;
  manager->crtcs = g_new0 (MetaCRTC, manager->n_crtcs);
  width = 0; height = 0;
  for (i = 0; i < (unsigned)resources->count_crtcs; i++)
    {
      drmModeCrtc *crtc;
      MetaCRTC *meta_crtc;

      crtc = drmModeGetCrtc (manager_kms->fd, resources->crtcs[i]);

      meta_crtc = &manager->crtcs[i];

      meta_crtc->crtc_id = crtc->crtc_id;
      meta_crtc->rect.x = crtc->x;
      meta_crtc->rect.y = crtc->y;
      meta_crtc->rect.width = crtc->width;
      meta_crtc->rect.height = crtc->height;
      meta_crtc->is_dirty = FALSE;
      meta_crtc->transform = META_MONITOR_TRANSFORM_NORMAL;
      /* FIXME: implement! */
      meta_crtc->all_transforms = 1 << META_MONITOR_TRANSFORM_NORMAL;

      if (crtc->mode_valid)
        {
          for (j = 0; j < manager->n_modes; j++)
            {
              if (drm_mode_equal (&crtc->mode, manager->modes[j].driver_private))
                {
                  meta_crtc->current_mode = &manager->modes[j];
                  break;
                }
            }

          width = MAX (width, meta_crtc->rect.x + meta_crtc->rect.width);
          height = MAX (height, meta_crtc->rect.y + meta_crtc->rect.height);
        }

      meta_crtc->driver_private = g_new0 (MetaCRTCKms, 1);
      meta_crtc->driver_notify = (GDestroyNotify) meta_crtc_destroy_notify;
      find_crtc_properties (manager_kms, meta_crtc);
      init_crtc_rotations (manager, meta_crtc, i);

      drmModeFreeCrtc (crtc);
    }

  manager->screen_width = width;
  manager->screen_height = height;

  manager->outputs = g_new0 (MetaOutput, manager_kms->n_connectors);
  n_actual_outputs = 0;

  for (i = 0; i < manager_kms->n_connectors; i++)
    {
      MetaOutput *meta_output, *old_output;
      MetaOutputKms *output_kms;
      drmModeConnector *connector;
      GArray *crtcs;
      unsigned int crtc_mask;
      GBytes *edid;

      connector = manager_kms->connectors[i];
      meta_output = &manager->outputs[n_actual_outputs];

      if (connector && connector->connection == DRM_MODE_CONNECTED)
	{
          meta_output->driver_private = output_kms = g_slice_new0 (MetaOutputKms);
          meta_output->driver_notify = (GDestroyNotify)meta_output_destroy_notify;

	  meta_output->winsys_id = connector->connector_id;
	  meta_output->name = make_output_name (connector);
	  meta_output->width_mm = connector->mmWidth;
	  meta_output->height_mm = connector->mmHeight;

          switch (connector->subpixel)
            {
            case DRM_MODE_SUBPIXEL_NONE:
              meta_output->subpixel_order = COGL_SUBPIXEL_ORDER_NONE;
              break;
            case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
              meta_output->subpixel_order = COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB;
              break;
            case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
              meta_output->subpixel_order = COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR;
              break;
            case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
              meta_output->subpixel_order = COGL_SUBPIXEL_ORDER_VERTICAL_RGB;
              break;
            case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
              meta_output->subpixel_order = COGL_SUBPIXEL_ORDER_VERTICAL_BGR;
              break;
            case DRM_MODE_SUBPIXEL_UNKNOWN:
            default:
              meta_output->subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
              break;
            }

	  meta_output->preferred_mode = NULL;
	  meta_output->n_modes = connector->count_modes;
	  meta_output->modes = g_new0 (MetaMonitorMode *, meta_output->n_modes);
	  for (j = 0; j < meta_output->n_modes; j++) {
            meta_output->modes[j] = find_meta_mode (manager, &connector->modes[j]);
            if (connector->modes[j].type & DRM_MODE_TYPE_PREFERRED)
              meta_output->preferred_mode = meta_output->modes[j];
          }

          if (!meta_output->preferred_mode)
            meta_output->preferred_mode = meta_output->modes[0];

          output_kms->connector = connector;
          output_kms->n_encoders = connector->count_encoders;
          output_kms->encoders = g_new0 (drmModeEncoderPtr, output_kms->n_encoders);

          crtc_mask = ~(unsigned int)0;
	  for (j = 0; j < output_kms->n_encoders; j++)
	    {
              output_kms->encoders[j] = drmModeGetEncoder (manager_kms->fd, connector->encoders[j]);
              if (!output_kms->encoders[j])
                continue;

              /* We only list CRTCs as supported if they are supported by all encoders
                 for this connectors.

                 This is what xf86-video-modesetting does (see drmmode_output_init())
              */
              crtc_mask &= output_kms->encoders[j]->possible_crtcs;

              if (output_kms->encoders[j]->encoder_id == connector->encoder_id)
                output_kms->current_encoder = output_kms->encoders[j];
            }

          crtcs = g_array_new (FALSE, FALSE, sizeof (MetaCRTC*));

          for (j = 0; j < manager->n_crtcs; j++)
            {
              if (crtc_mask & (1 << j))
                {
                  MetaCRTC *crtc = &manager->crtcs[j];
                  g_array_append_val (crtcs, crtc);
		}
	    }

	  meta_output->n_possible_crtcs = crtcs->len;
	  meta_output->possible_crtcs = (void*)g_array_free (crtcs, FALSE);

          if (output_kms->current_encoder && output_kms->current_encoder->crtc_id != 0)
            {
              for (j = 0; j < manager->n_crtcs; j++)
                {
                  if (manager->crtcs[j].crtc_id == output_kms->current_encoder->crtc_id)
                    {
                      meta_output->crtc = &manager->crtcs[j];
                      break;
                    }
                }
            }
          else
            meta_output->crtc = NULL;

          old_output = find_output_by_id (old_outputs, n_old_outputs,
                                          meta_output->winsys_id);
          if (old_output)
            {
              meta_output->is_primary = old_output->is_primary;
              meta_output->is_presentation = old_output->is_presentation;
            }
          else
            {
              meta_output->is_primary = FALSE;
              meta_output->is_presentation = FALSE;
            }

          find_connector_properties (manager_kms, output_kms);
          meta_output->suggested_x = output_kms->suggested_x;
          meta_output->suggested_y = output_kms->suggested_y;
          meta_output->hotplug_mode_update = output_kms->hotplug_mode_update;
          
          edid = read_output_edid (manager_kms, meta_output);
          meta_output_parse_edid (meta_output, edid);
          g_bytes_unref (edid);

          /* MetaConnectorType matches DRM's connector types */
          meta_output->connector_type = (MetaConnectorType) connector->connector_type;

          meta_output->scale = get_output_scale (manager, meta_output);

          output_get_tile_info (manager_kms, meta_output);

          /* FIXME: backlight is a very driver specific thing unfortunately,
             every DDX does its own thing, and the dumb KMS API does not include it.

             For example, xf86-video-intel has a list of paths to probe in /sys/class/backlight
             (one for each major HW maker, and then some).
             We can't do the same because we're not root.
             It might be best to leave backlight out of the story and rely on the setuid
             helper in gnome-settings-daemon.
          */
	  meta_output->backlight_min = 0;
          meta_output->backlight_max = 0;
          meta_output->backlight = -1;

	  n_actual_outputs++;
	}
    }

  manager->n_outputs = n_actual_outputs;
  manager->outputs = g_renew (MetaOutput, manager->outputs, manager->n_outputs);

  /* Sort the outputs for easier handling in MetaMonitorConfig */
  qsort (manager->outputs, manager->n_outputs, sizeof (MetaOutput), compare_outputs);

  /* Now fix the clones.
     Code mostly inspired by xf86-video-modesetting. */

  /* XXX: intel hardware doesn't usually have clones, but I only have laptops with
     intel cards, so this code was never tested! */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *meta_output;
      MetaOutputKms *output_kms;

      meta_output = &manager->outputs[i];
      output_kms = meta_output->driver_private;

      output_kms->enc_clone_mask = 0xff;
      output_kms->encoder_mask = 0;

      for (j = 0; j < output_kms->n_encoders; j++)
	{
	  for (k = 0; k < (unsigned)resources->count_encoders; k++)
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

  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *meta_output;
      MetaOutputKms *output_kms;

      meta_output = &manager->outputs[i];
      output_kms = meta_output->driver_private;

      if (output_kms->enc_clone_mask == 0)
        continue;

      for (j = 0; j < manager->n_outputs; j++)
        {
          MetaOutput *meta_clone;
          MetaOutputKms *clone_kms;

          meta_clone = &manager->outputs[i];
          clone_kms = meta_clone->driver_private;

          if (meta_clone == meta_output)
            continue;

          if (clone_kms->encoder_mask == 0)
            continue;

          if (clone_kms->encoder_mask == output_kms->enc_clone_mask)
            {
              meta_output->n_possible_clones++;
              meta_output->possible_clones = g_renew (MetaOutput *,
                                                      meta_output->possible_clones,
                                                      meta_output->n_possible_clones);
              meta_output->possible_clones[meta_output->n_possible_clones - 1] = meta_clone;
            }
        }
    }

  for (i = 0; i < (unsigned)resources->count_encoders; i++)
    drmModeFreeEncoder (encoders[i]);
  g_free (encoders);

  drmModeFreeResources (resources);
}

static GBytes *
meta_monitor_manager_kms_read_edid (MetaMonitorManager *manager,
                                    MetaOutput         *output)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);

  return read_output_edid (manager_kms, output);
}

static void
meta_monitor_manager_kms_set_power_save_mode (MetaMonitorManager *manager,
                                              MetaPowerSave       mode)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  MetaBackend *backend;
  MetaRenderer *renderer;
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
      MetaOutput *meta_output;
      MetaOutputKms *output_kms;

      meta_output = &manager->outputs[i];
      output_kms = meta_output->driver_private;

      if (output_kms->dpms_prop_id != 0)
        {
          int ok = drmModeObjectSetProperty (manager_kms->fd, meta_output->winsys_id,
                                             DRM_MODE_OBJECT_CONNECTOR,
                                             output_kms->dpms_prop_id, state);

          if (ok < 0)
            meta_warning ("Failed to set power save mode for output %s: %s\n",
                          meta_output->name, strerror (errno));
        }
    }

  backend = meta_get_backend ();
  renderer = meta_backend_get_renderer (backend);

  for (i = 0; i < manager->n_crtcs; i++)
    meta_renderer_native_set_ignore_crtc (META_RENDERER_NATIVE (renderer),
                                          manager->crtcs[i].crtc_id,
                                          mode != META_POWER_SAVE_ON);
}

static void
crtc_free (CoglKmsCrtc *crtc)
{
  g_free (crtc->connectors);
  g_slice_free (CoglKmsCrtc, crtc);
}

static void
set_underscan (MetaMonitorManagerKms *manager_kms,
               MetaOutput *output)
{
  if (!output->crtc)
    return;

  MetaCRTC *crtc = output->crtc;
  MetaCRTCKms *crtc_kms = crtc->driver_private;
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
meta_monitor_manager_kms_apply_configuration (MetaMonitorManager *manager,
                                              MetaCRTCInfo       **crtcs,
                                              unsigned int         n_crtcs,
                                              MetaOutputInfo     **outputs,
                                              unsigned int         n_outputs)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  MetaBackend *backend;
  MetaRenderer *renderer;
  unsigned i;
  GPtrArray *cogl_crtcs;
  int screen_width, screen_height;
  gboolean ok;
  GError *error;

  cogl_crtcs = g_ptr_array_new_full (manager->n_crtcs, (GDestroyNotify)crtc_free);
  screen_width = 0; screen_height = 0;
  for (i = 0; i < n_crtcs; i++)
    {
      MetaCRTCInfo *crtc_info = crtcs[i];
      MetaCRTC *crtc = crtc_info->crtc;
      MetaCRTCKms *crtc_kms = crtc->driver_private;
      CoglKmsCrtc *cogl_crtc;

      crtc->is_dirty = TRUE;

      cogl_crtc = g_slice_new0 (CoglKmsCrtc);
      g_ptr_array_add (cogl_crtcs, cogl_crtc);

      if (crtc_info->mode == NULL)
        {
          cogl_crtc->id = crtc->crtc_id;
          cogl_crtc->x = 0;
          cogl_crtc->y = 0;
          cogl_crtc->count = 0;
          memset (&cogl_crtc->mode, 0, sizeof (drmModeModeInfo));
          cogl_crtc->connectors = NULL;
          cogl_crtc->count = 0;

          crtc->rect.x = 0;
          crtc->rect.y = 0;
          crtc->rect.width = 0;
          crtc->rect.height = 0;
          crtc->current_mode = NULL;
        }
      else
        {
          MetaMonitorMode *mode;
          uint32_t *connectors;
          unsigned int j, n_connectors;
          int width, height;

          mode = crtc_info->mode;

          cogl_crtc->id = crtc->crtc_id;
          cogl_crtc->x = crtc_info->x;
          cogl_crtc->y = crtc_info->y;
          cogl_crtc->count = n_connectors = crtc_info->outputs->len;
          cogl_crtc->connectors = connectors = g_new (uint32_t, n_connectors);

          for (j = 0; j < n_connectors; j++)
            {
              MetaOutput *output = g_ptr_array_index (crtc_info->outputs, j);

              connectors[j] = output->winsys_id;

              output->is_dirty = TRUE;
              output->crtc = crtc;
            }

          memcpy (&cogl_crtc->mode, crtc_info->mode->driver_private,
                  sizeof (drmModeModeInfo));

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

          screen_width = MAX (screen_width, crtc_info->x + width);
          screen_height = MAX (screen_height, crtc_info->y + height);

          crtc->rect.x = crtc_info->x;
          crtc->rect.y = crtc_info->y;
          crtc->rect.width = width;
          crtc->rect.height = height;
          crtc->current_mode = mode;
          crtc->transform = crtc_info->transform;
        }

      if (crtc->all_transforms & (1 << crtc->transform))
        drmModeObjectSetProperty (manager_kms->fd,
                                  crtc_kms->primary_plane_id,
                                  DRM_MODE_OBJECT_PLANE,
                                  crtc_kms->rotation_prop_id,
                                  crtc_kms->rotation_map[crtc->transform]);
    }

  /* Disable CRTCs not mentioned in the list (they have is_dirty == FALSE,
     because they weren't seen in the first loop) */
  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCRTC *crtc = &manager->crtcs[i];
      CoglKmsCrtc *cogl_crtc;

      crtc->logical_monitor = NULL;

      if (crtc->is_dirty)
        {
          crtc->is_dirty = FALSE;
          continue;
        }

      cogl_crtc = g_slice_new0 (CoglKmsCrtc);
      g_ptr_array_add (cogl_crtcs, cogl_crtc);

      cogl_crtc->id = crtc->crtc_id;
      cogl_crtc->x = 0;
      cogl_crtc->y = 0;
      cogl_crtc->count = 0;
      memset (&cogl_crtc->mode, 0, sizeof (drmModeModeInfo));
      cogl_crtc->connectors = NULL;
      cogl_crtc->count = 0;

      crtc->rect.x = 0;
      crtc->rect.y = 0;
      crtc->rect.width = 0;
      crtc->rect.height = 0;
      crtc->current_mode = NULL;
    }

  backend = meta_get_backend ();
  renderer = meta_backend_get_renderer (backend);

  error = NULL;
  ok = meta_renderer_native_set_layout (META_RENDERER_NATIVE (renderer),
                                        screen_width, screen_height,
                                        (CoglKmsCrtc**)cogl_crtcs->pdata,
                                        cogl_crtcs->len,
                                        &error);
  g_ptr_array_unref (cogl_crtcs);

  if (!ok)
    {
      meta_warning ("Applying display configuration failed: %s\n", error->message);
      g_error_free (error);
      return;
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

  manager->screen_width = screen_width;
  manager->screen_height = screen_height;

  meta_monitor_manager_rebuild_derived (manager);
}

static void
meta_monitor_manager_kms_get_crtc_gamma (MetaMonitorManager  *manager,
                                         MetaCRTC            *crtc,
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
                                         MetaCRTC           *crtc,
                                         gsize               size,
                                         unsigned short     *red,
                                         unsigned short     *green,
                                         unsigned short     *blue)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);

  drmModeCrtcSetGamma (manager_kms->fd, crtc->crtc_id, size, red, green, blue);
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

  meta_monitor_manager_read_current_config (manager);

  meta_monitor_manager_on_hotplug (manager);
}

static void
meta_monitor_manager_kms_init (MetaMonitorManagerKms *manager_kms)
{
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);

  manager_kms->fd = meta_renderer_native_get_kms_fd (renderer_native);

  drmSetClientCap (manager_kms->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

  const char *subsystems[2] = { "drm", NULL };
  manager_kms->udev = g_udev_client_new (subsystems);
  g_signal_connect (manager_kms->udev, "uevent",
                    G_CALLBACK (on_uevent), manager_kms);

  manager_kms->desktop_settings = g_settings_new ("org.gnome.desktop.interface");
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
  manager_class->apply_configuration = meta_monitor_manager_kms_apply_configuration;
  manager_class->set_power_save_mode = meta_monitor_manager_kms_set_power_save_mode;
  manager_class->get_crtc_gamma = meta_monitor_manager_kms_get_crtc_gamma;
  manager_class->set_crtc_gamma = meta_monitor_manager_kms_set_crtc_gamma;
}

