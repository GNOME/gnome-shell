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

#include "backends/native/meta-output-kms.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "backends/meta-crtc.h"
#include "backends/native/meta-crtc-kms.h"
#include "backends/native/meta-default-modes.h"
#include "backends/native/meta-monitor-manager-kms.h"

#define SYNC_TOLERANCE 0.01    /* 1 percent */

typedef struct _MetaOutputKms
{
  MetaOutput parent;

  drmModeConnector *connector;

  unsigned int n_encoders;
  drmModeEncoderPtr *encoders;
  drmModeEncoderPtr current_encoder;

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

void
meta_output_kms_set_underscan (MetaOutput *output)
{
  if (!output->crtc)
    return;

  meta_crtc_kms_set_underscan (output->crtc,
                               output->is_underscanning);
}

void
meta_output_kms_set_power_save_mode (MetaOutput *output,
                                     uint64_t    state)
{
  MetaOutputKms *output_kms = output->driver_private;
  MetaMonitorManager *monitor_manager =
    meta_output_get_monitor_manager (output);
  MetaMonitorManagerKms *monitor_manager_kms =
    META_MONITOR_MANAGER_KMS (monitor_manager);

  if (output_kms->dpms_prop_id != 0)
    {
      int fd;

      fd = meta_monitor_manager_kms_get_fd (monitor_manager_kms);
      if (drmModeObjectSetProperty (fd, output->winsys_id,
                                    DRM_MODE_OBJECT_CONNECTOR,
                                    output_kms->dpms_prop_id, state) < 0)
        g_warning ("Failed to set power save mode for output %s: %s",
                   output->name, strerror (errno));
    }
}

gboolean
meta_output_kms_can_clone (MetaOutput *output,
                           MetaOutput *other_output)
{
  MetaOutputKms *output_kms = output->driver_private;
  MetaOutputKms *other_output_kms = other_output->driver_private;

  if (output_kms->enc_clone_mask == 0 ||
      other_output_kms->enc_clone_mask == 0)
    return FALSE;

  if (output_kms->encoder_mask != other_output_kms->enc_clone_mask)
    return FALSE;

  return TRUE;
}

static drmModePropertyBlobPtr
read_edid_blob (MetaMonitorManagerKms *monitor_manager_kms,
                uint32_t               edid_blob_id,
                GError               **error)
{
  int fd;
  drmModePropertyBlobPtr edid_blob = NULL;

  fd = meta_monitor_manager_kms_get_fd (monitor_manager_kms);
  edid_blob = drmModeGetPropertyBlob (fd, edid_blob_id);
  if (!edid_blob)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                   "%s", strerror (errno));
      return NULL;
    }

  return edid_blob;
}

static GBytes *
read_output_edid (MetaMonitorManagerKms *monitor_manager_kms,
                  MetaOutput            *output,
                  GError               **error)
{
  MetaOutputKms *output_kms = output->driver_private;
  drmModePropertyBlobPtr edid_blob;

  g_assert (output_kms->edid_blob_id != 0);

  edid_blob = read_edid_blob (monitor_manager_kms, output_kms->edid_blob_id, error);
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
output_get_tile_info (MetaMonitorManagerKms *monitor_manager_kms,
                      MetaOutput            *output)
{
  MetaOutputKms *output_kms = output->driver_private;
  int fd;
  drmModePropertyBlobPtr tile_blob = NULL;

  if (output_kms->tile_blob_id == 0)
    return FALSE;

  fd = meta_monitor_manager_kms_get_fd (monitor_manager_kms);
  tile_blob = drmModeGetPropertyBlob (fd, output_kms->tile_blob_id);
  if (!tile_blob)
    {
      g_warning ("Failed to read TILE of output %s: %s",
                 output->name, strerror (errno));
      return FALSE;
    }

  if (tile_blob->length > 0)
    {
      int ret;

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
          g_warning ("Couldn't understand output tile property blob");
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

GBytes *
meta_output_kms_read_edid (MetaOutput *output)
{
  MetaOutputKms *output_kms = output->driver_private;
  MetaMonitorManager *monitor_manager =
    meta_output_get_monitor_manager (output);
  MetaMonitorManagerKms *monitor_manager_kms =
    META_MONITOR_MANAGER_KMS (monitor_manager);
  GError *error = NULL;
  GBytes *edid;

  if (output_kms->edid_blob_id == 0)
    return NULL;

  edid = read_output_edid (monitor_manager_kms, output, &error);
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
find_connector_properties (MetaMonitorManagerKms *monitor_manager_kms,
                           MetaOutputKms         *output_kms)
{
  drmModeConnector *connector = output_kms->connector;
  int fd;
  int i;

  fd = meta_monitor_manager_kms_get_fd (monitor_manager_kms);

  output_kms->hotplug_mode_update = 0;
  output_kms->suggested_x = -1;
  output_kms->suggested_y = -1;

  for (i = 0; i < connector->count_props; i++)
    {
      drmModePropertyPtr prop = drmModeGetProperty (fd, connector->props[i]);
      if (!prop)
        continue;

      if ((prop->flags & DRM_MODE_PROP_ENUM) &&
          strcmp (prop->name, "DPMS") == 0)
        output_kms->dpms_prop_id = prop->prop_id;
      else if ((prop->flags & DRM_MODE_PROP_BLOB) &&
               strcmp (prop->name, "EDID") == 0)
        output_kms->edid_blob_id = connector->prop_values[i];
      else if ((prop->flags & DRM_MODE_PROP_BLOB) &&
               strcmp (prop->name, "TILE") == 0)
        output_kms->tile_blob_id = connector->prop_values[i];
      else if ((prop->flags & DRM_MODE_PROP_RANGE) &&
               strcmp (prop->name, "suggested X") == 0)
        output_kms->suggested_x = connector->prop_values[i];
      else if ((prop->flags & DRM_MODE_PROP_RANGE) &&
               strcmp (prop->name, "suggested Y") == 0)
        output_kms->suggested_y = connector->prop_values[i];
      else if ((prop->flags & DRM_MODE_PROP_RANGE) &&
               strcmp (prop->name, "hotplug_mode_update") == 0)
        output_kms->hotplug_mode_update = connector->prop_values[i];
      else if (strcmp (prop->name, "scaling mode") == 0)
        output_kms->has_scaling = TRUE;

      drmModeFreeProperty (prop);
    }
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
add_common_modes (MetaOutput            *output,
                  MetaMonitorManagerKms *monitor_manager_kms)
{
  GPtrArray *array;
  unsigned i;
  unsigned max_hdisplay = 0;
  unsigned max_vdisplay = 0;
  float max_refresh_rate = 0.0;

  for (i = 0; i < output->n_modes; i++)
    {
      const drmModeModeInfo *drm_mode;
      float refresh_rate;

      drm_mode = output->modes[i]->driver_private;
      refresh_rate = meta_calculate_drm_mode_refresh_rate (drm_mode);
      max_hdisplay = MAX (max_hdisplay, drm_mode->hdisplay);
      max_vdisplay = MAX (max_vdisplay, drm_mode->vdisplay);
      max_refresh_rate = MAX (max_refresh_rate, refresh_rate);
    }

  max_refresh_rate = MAX (max_refresh_rate, 60.0);
  max_refresh_rate *= (1 + SYNC_TOLERANCE);

  array = g_ptr_array_new ();
  for (i = 0; i < G_N_ELEMENTS (meta_default_drm_mode_infos); i++)
    {
      const drmModeModeInfo *drm_mode;
      float refresh_rate;
      MetaCrtcMode *crtc_mode;

      drm_mode = &meta_default_drm_mode_infos[i];
      refresh_rate = meta_calculate_drm_mode_refresh_rate (drm_mode);
      if (drm_mode->hdisplay > max_hdisplay ||
          drm_mode->vdisplay > max_vdisplay ||
          refresh_rate > max_refresh_rate)
        continue;

      crtc_mode = meta_monitor_manager_kms_get_mode_from_drm_mode (monitor_manager_kms,
                                                                   drm_mode);
      g_ptr_array_add (array, crtc_mode);
    }

  output->modes = g_renew (MetaCrtcMode *, output->modes,
                           output->n_modes + array->len);
  memcpy (output->modes + output->n_modes, array->pdata,
          array->len * sizeof (MetaCrtcMode *));
  output->n_modes += array->len;

  g_ptr_array_free (array, TRUE);
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

static void
init_output_modes (MetaOutput            *output,
                   MetaMonitorManagerKms *monitor_manager_kms)
{
  MetaOutputKms *output_kms = output->driver_private;
  unsigned int i;

  output->preferred_mode = NULL;
  output->n_modes = output_kms->connector->count_modes;
  output->modes = g_new0 (MetaCrtcMode *, output->n_modes);
  for (i = 0; i < output->n_modes; i++)
    {
      drmModeModeInfo *drm_mode;
      MetaCrtcMode *crtc_mode;

      drm_mode = &output_kms->connector->modes[i];
      crtc_mode =
        meta_monitor_manager_kms_get_mode_from_drm_mode (monitor_manager_kms,
                                                         drm_mode);
      output->modes[i] = crtc_mode;
      if (output_kms->connector->modes[i].type & DRM_MODE_TYPE_PREFERRED)
        output->preferred_mode = output->modes[i];
    }

  if (!output->preferred_mode)
    output->preferred_mode = output->modes[0];
}

MetaOutput *
meta_create_kms_output (MetaMonitorManager *monitor_manager,
                        drmModeConnector   *connector,
                        MetaKmsResources   *resources,
                        MetaOutput         *old_output)
{
  MetaMonitorManagerKms *monitor_manager_kms =
    META_MONITOR_MANAGER_KMS (monitor_manager);
  MetaOutput *output;
  MetaOutputKms *output_kms;
  GArray *crtcs;
  GBytes *edid;
  GList *l;
  unsigned int i;
  unsigned int crtc_mask;
  int fd;

  output = g_object_new (META_TYPE_OUTPUT, NULL);

  output_kms = g_slice_new0 (MetaOutputKms);
  output->driver_private = output_kms;
  output->driver_notify = (GDestroyNotify) meta_output_destroy_notify;

  output->monitor_manager = monitor_manager;
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

  output_kms->connector = connector;
  find_connector_properties (monitor_manager_kms, output_kms);

  init_output_modes (output, monitor_manager_kms);

  /* FIXME: MSC feature bit? */
  /* Presume that if the output supports scaling, then we have
   * a panel fitter capable of adjusting any mode to suit.
   */
  if (output_kms->has_scaling)
    add_common_modes (output, monitor_manager_kms);

  qsort (output->modes, output->n_modes,
         sizeof (MetaCrtcMode *), compare_modes);

  output_kms->n_encoders = connector->count_encoders;
  output_kms->encoders = g_new0 (drmModeEncoderPtr, output_kms->n_encoders);

  fd = meta_monitor_manager_kms_get_fd (monitor_manager_kms);

  crtc_mask = ~(unsigned int) 0;
  for (i = 0; i < output_kms->n_encoders; i++)
    {
      output_kms->encoders[i] = drmModeGetEncoder (fd, connector->encoders[i]);
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

  for (l = monitor_manager->crtcs, i = 0; l; l = l->next, i++)
    {
      if (crtc_mask & (1 << i))
        {
          MetaCrtc *crtc = l->data;

          g_array_append_val (crtcs, crtc);
        }
    }

  output->n_possible_crtcs = crtcs->len;
  output->possible_crtcs = (void*)g_array_free (crtcs, FALSE);

  if (output_kms->current_encoder && output_kms->current_encoder->crtc_id != 0)
    {
      for (l = monitor_manager->crtcs; l; l = l->next)
        {
          MetaCrtc *crtc = l->data;

          if (crtc->crtc_id == output_kms->current_encoder->crtc_id)
            {
              output->crtc = crtc;
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

      edid = read_output_edid (monitor_manager_kms, output, &error);
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

  output_get_tile_info (monitor_manager_kms, output);

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

  output_kms->enc_clone_mask = 0xff;
  output_kms->encoder_mask = 0;

  for (i = 0; i < output_kms->n_encoders; i++)
    {
      drmModeEncoder *output_encoder = output_kms->encoders[i];
      unsigned int j;

      for (j = 0; j < resources->n_encoders; j++)
        {
          drmModeEncoder *encoder = resources->encoders[j];

          if (output_encoder && encoder &&
              output_encoder->encoder_id == encoder->encoder_id)
            {
              output_kms->encoder_mask |= (1 << j);
              break;
            }
        }

      output_kms->enc_clone_mask &= output_encoder->possible_clones;
    }

  return output;
}
