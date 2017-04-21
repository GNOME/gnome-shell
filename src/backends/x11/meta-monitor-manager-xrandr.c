/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "meta-monitor-manager-xrandr.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <clutter/clutter.h>

#include <X11/Xatom.h>
#include <X11/Xlibint.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/extutil.h>
#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>

#include "meta-backend-x11.h"
#include <meta/main.h>
#include <meta/errors.h>
#include "meta-monitor-config.h"
#include "backends/meta-monitor-config-manager.h"
#include "backends/meta-logical-monitor.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

/* Look for DPI_FALLBACK in:
 * http://git.gnome.org/browse/gnome-settings-daemon/tree/plugins/xsettings/gsd-xsettings-manager.c
 * for the reasoning */
#define DPI_FALLBACK 96.0

static float supported_scales_xrandr[] = {
  1.0, 2.0
};

struct _MetaMonitorManagerXrandr
{
  MetaMonitorManager parent_instance;

  Display *xdisplay;
  XRRScreenResources *resources;
  int rr_event_base;
  int rr_error_base;
  gboolean has_randr15;

  xcb_timestamp_t last_xrandr_set_timestamp;

#ifdef HAVE_XRANDR15
  GHashTable *tiled_monitor_atoms;
#endif /* HAVE_XRANDR15 */

  int max_screen_width;
  int max_screen_height;
};

struct _MetaMonitorManagerXrandrClass
{
  MetaMonitorManagerClass parent_class;
};

G_DEFINE_TYPE (MetaMonitorManagerXrandr, meta_monitor_manager_xrandr, META_TYPE_MONITOR_MANAGER);

#ifdef HAVE_XRANDR15
typedef struct _MetaMonitorXrandrData
{
  Atom xrandr_name;
} MetaMonitorXrandrData;

GQuark quark_meta_monitor_xrandr_data;
#endif /* HAVE_RANDR15 */

static MetaMonitorTransform
meta_monitor_transform_from_xrandr (Rotation rotation)
{
  static const MetaMonitorTransform y_reflected_map[4] = {
    META_MONITOR_TRANSFORM_FLIPPED_180,
    META_MONITOR_TRANSFORM_FLIPPED_90,
    META_MONITOR_TRANSFORM_FLIPPED,
    META_MONITOR_TRANSFORM_FLIPPED_270
  };
  MetaMonitorTransform ret;

  switch (rotation & 0x7F)
    {
    default:
    case RR_Rotate_0:
      ret = META_MONITOR_TRANSFORM_NORMAL;
      break;
    case RR_Rotate_90:
      ret = META_MONITOR_TRANSFORM_90;
      break;
    case RR_Rotate_180:
      ret = META_MONITOR_TRANSFORM_180;
      break;
    case RR_Rotate_270:
      ret = META_MONITOR_TRANSFORM_270;
      break;
    }

  if (rotation & RR_Reflect_X)
    return ret + 4;
  else if (rotation & RR_Reflect_Y)
    return y_reflected_map[ret];
  else
    return ret;
}

#define ALL_ROTATIONS (RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_180 | RR_Rotate_270)

static MetaMonitorTransform
meta_monitor_transform_from_xrandr_all (Rotation rotation)
{
  unsigned ret;

  /* Handle the common cases first (none or all) */
  if (rotation == 0 || rotation == RR_Rotate_0)
    return (1 << META_MONITOR_TRANSFORM_NORMAL);

  /* All rotations and one reflection -> all of them by composition */
  if ((rotation & ALL_ROTATIONS) &&
      ((rotation & RR_Reflect_X) || (rotation & RR_Reflect_Y)))
    return ALL_TRANSFORMS;

  ret = 1 << META_MONITOR_TRANSFORM_NORMAL;
  if (rotation & RR_Rotate_90)
    ret |= 1 << META_MONITOR_TRANSFORM_90;
  if (rotation & RR_Rotate_180)
    ret |= 1 << META_MONITOR_TRANSFORM_180;
  if (rotation & RR_Rotate_270)
    ret |= 1 << META_MONITOR_TRANSFORM_270;
  if (rotation & (RR_Rotate_0 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED;
  if (rotation & (RR_Rotate_90 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED_90;
  if (rotation & (RR_Rotate_180 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED_180;
  if (rotation & (RR_Rotate_270 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED_270;

  return ret;
}

static gboolean
output_get_integer_property (MetaMonitorManagerXrandr *manager_xrandr,
                             MetaOutput *output, const char *propname,
                             gint *value)
{
  gboolean exists = FALSE;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *buffer;

  atom = XInternAtom (manager_xrandr->xdisplay, propname, False);
  XRRGetOutputProperty (manager_xrandr->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_INTEGER,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  exists = (actual_type == XA_INTEGER && actual_format == 32 && nitems == 1);

  if (exists && value != NULL)
    *value = ((int*)buffer)[0];

  XFree (buffer);
  return exists;
}

static gboolean
output_get_property_exists (MetaMonitorManagerXrandr *manager_xrandr,
                            MetaOutput *output, const char *propname)
{
  gboolean exists = FALSE;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *buffer;

  atom = XInternAtom (manager_xrandr->xdisplay, propname, False);
  XRRGetOutputProperty (manager_xrandr->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, AnyPropertyType,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  exists = (actual_type != None);

  XFree (buffer);
  return exists;
}

static gboolean
output_get_boolean_property (MetaMonitorManagerXrandr *manager_xrandr,
                             MetaOutput *output, const char *propname)
{
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;

  atom = XInternAtom (manager_xrandr->xdisplay, propname, False);
  XRRGetOutputProperty (manager_xrandr->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_CARDINAL,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_CARDINAL || actual_format != 32 || nitems < 1)
    return FALSE;

  return ((int*)buffer)[0];
}

static gboolean
output_get_presentation_xrandr (MetaMonitorManagerXrandr *manager_xrandr,
                                MetaOutput *output)
{
  return output_get_boolean_property (manager_xrandr, output, "_MUTTER_PRESENTATION_OUTPUT");
}

static gboolean
output_get_underscanning_xrandr (MetaMonitorManagerXrandr *manager_xrandr,
                                 MetaOutput               *output)
{
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;
  g_autofree char *str = NULL;

  atom = XInternAtom (manager_xrandr->xdisplay, "underscan", False);
  XRRGetOutputProperty (manager_xrandr->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    return FALSE;

  str = XGetAtomName (manager_xrandr->xdisplay, *(Atom *)buffer);
  return (strcmp (str, "on") == 0);
}

static gboolean
output_get_supports_underscanning_xrandr (MetaMonitorManagerXrandr *manager_xrandr,
                                          MetaOutput               *output)
{
  Atom atom, actual_type;
  int actual_format, i;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;
  XRRPropertyInfo *property_info;
  Atom *values;
  gboolean supports_underscanning = FALSE;

  atom = XInternAtom (manager_xrandr->xdisplay, "underscan", False);
  XRRGetOutputProperty (manager_xrandr->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    return FALSE;

  property_info = XRRQueryOutputProperty (manager_xrandr->xdisplay,
                                          (XID) output->winsys_id,
                                          atom);
  values = (Atom *) property_info->values;

  for (i = 0; i < property_info->num_values; i++)
    {
      /* The output supports underscanning if "on" is a valid value
       * for the underscan property.
       */
      char *name = XGetAtomName (manager_xrandr->xdisplay, values[i]);
      if (strcmp (name, "on") == 0)
        supports_underscanning = TRUE;

      XFree (name);
    }

  XFree (property_info);

  return supports_underscanning;
}

static int
normalize_backlight (MetaOutput *output,
                     int         hw_value)
{
  return round ((double)(hw_value - output->backlight_min) /
                (output->backlight_max - output->backlight_min) * 100.0);
}

static int
output_get_backlight_xrandr (MetaMonitorManagerXrandr *manager_xrandr,
                             MetaOutput               *output)
{
  int value = -1;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;

  atom = XInternAtom (manager_xrandr->xdisplay, "Backlight", False);
  XRRGetOutputProperty (manager_xrandr->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_INTEGER,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_INTEGER || actual_format != 32 || nitems < 1)
    return FALSE;

  value = ((int*)buffer)[0];
  if (value > 0)
    return normalize_backlight (output, value);
  else
    return -1;
}

static void
output_get_backlight_limits_xrandr (MetaMonitorManagerXrandr *manager_xrandr,
                                    MetaOutput               *output)
{
  Atom atom;
  xcb_connection_t *xcb_conn;
  g_autofree xcb_randr_query_output_property_reply_t *reply;

  atom = XInternAtom (manager_xrandr->xdisplay, "Backlight", False);

  xcb_conn = XGetXCBConnection (manager_xrandr->xdisplay);
  reply = xcb_randr_query_output_property_reply (xcb_conn,
                                                 xcb_randr_query_output_property (xcb_conn,
                                                                                  (xcb_randr_output_t) output->winsys_id,
                                                                                  (xcb_atom_t) atom),
                                                 NULL);

  /* This can happen on systems without backlights. */
  if (reply == NULL)
    return;

  if (!reply->range || reply->length != 2)
    {
      meta_verbose ("backlight %s was not range\n", output->name);
      return;
    }

  int32_t *values = xcb_randr_query_output_property_valid_values (reply);
  output->backlight_min = values[0];
  output->backlight_max = values[1];
}

static int
compare_outputs (const void *one,
                 const void *two)
{
  const MetaOutput *o_one = one, *o_two = two;

  return strcmp (o_one->name, o_two->name);
}

static guint8 *
get_edid_property (Display  *dpy,
                   RROutput  output,
                   Atom      atom,
                   gsize    *len)
{
  unsigned char *prop;
  int actual_format;
  unsigned long nitems, bytes_after;
  Atom actual_type;
  guint8 *result;

  XRRGetOutputProperty (dpy, output, atom,
                        0, 100, False, False,
                        AnyPropertyType,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &prop);

  if (actual_type == XA_INTEGER && actual_format == 8)
    {
      result = g_memdup (prop, nitems);
      if (len)
        *len = nitems;
    }
  else
    {
      result = NULL;
    }

  XFree (prop);

  return result;
}

static GBytes *
read_output_edid (MetaMonitorManagerXrandr *manager_xrandr,
                  XID                       winsys_id)
{
  Atom edid_atom;
  guint8 *result;
  gsize len;

  edid_atom = XInternAtom (manager_xrandr->xdisplay, "EDID", FALSE);
  result = get_edid_property (manager_xrandr->xdisplay, winsys_id, edid_atom, &len);

  if (!result)
    {
      edid_atom = XInternAtom (manager_xrandr->xdisplay, "EDID_DATA", FALSE);
      result = get_edid_property (manager_xrandr->xdisplay, winsys_id, edid_atom, &len);
    }

  if (result)
    {
      if (len > 0 && len % 128 == 0)
        return g_bytes_new_take (result, len);
      else
        g_free (result);
    }

  return NULL;
}

static void
output_get_tile_info (MetaMonitorManagerXrandr *manager_xrandr,
                      MetaOutput *output)
{
  Atom tile_atom;
  unsigned char *prop;
  unsigned long nitems, bytes_after;
  int actual_format;
  Atom actual_type;

  if (manager_xrandr->has_randr15 == FALSE)
    return;

  tile_atom = XInternAtom (manager_xrandr->xdisplay, "TILE", FALSE);
  XRRGetOutputProperty (manager_xrandr->xdisplay,
                        output->winsys_id,
                        tile_atom, 0, 100, False,
                        False, AnyPropertyType,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &prop);

  if (actual_type == XA_INTEGER && actual_format == 32 && nitems == 8)
    {
      long *values = (long *)prop;
      output->tile_info.group_id = values[0];
      output->tile_info.flags = values[1];
      output->tile_info.max_h_tiles = values[2];
      output->tile_info.max_v_tiles = values[3];
      output->tile_info.loc_h_tile = values[4];
      output->tile_info.loc_v_tile = values[5];
      output->tile_info.tile_w = values[6];
      output->tile_info.tile_h = values[7];
    }
  XFree (prop);
}

static gboolean
output_get_hotplug_mode_update (MetaMonitorManagerXrandr *manager_xrandr,
                                MetaOutput               *output)
{
  return output_get_property_exists (manager_xrandr, output, "hotplug_mode_update");
}

static gint
output_get_suggested_x (MetaMonitorManagerXrandr *manager_xrandr,
                        MetaOutput               *output)
{
  gint val;
  if (output_get_integer_property (manager_xrandr, output, "suggested X", &val))
    return val;

  return -1;
}

static gint
output_get_suggested_y (MetaMonitorManagerXrandr *manager_xrandr,
                        MetaOutput               *output)
{
  gint val;
  if (output_get_integer_property (manager_xrandr, output, "suggested Y", &val))
    return val;

  return -1;
}

static MetaConnectorType
connector_type_from_atom (MetaMonitorManagerXrandr *manager_xrandr,
                          Atom                      atom)
{
  Display *xdpy = manager_xrandr->xdisplay;

  if (atom == XInternAtom (xdpy, "HDMI", True))
    return META_CONNECTOR_TYPE_HDMIA;
  if (atom == XInternAtom (xdpy, "VGA", True))
    return META_CONNECTOR_TYPE_VGA;
  /* Doesn't have a DRM equivalent, but means an internal panel.
   * We could pick either LVDS or eDP here. */
  if (atom == XInternAtom (xdpy, "Panel", True))
    return META_CONNECTOR_TYPE_LVDS;
  if (atom == XInternAtom (xdpy, "DVI", True) || atom == XInternAtom (xdpy, "DVI-I", True))
    return META_CONNECTOR_TYPE_DVII;
  if (atom == XInternAtom (xdpy, "DVI-A", True))
    return META_CONNECTOR_TYPE_DVIA;
  if (atom == XInternAtom (xdpy, "DVI-D", True))
    return META_CONNECTOR_TYPE_DVID;
  if (atom == XInternAtom (xdpy, "DisplayPort", True))
    return META_CONNECTOR_TYPE_DisplayPort;

  if (atom == XInternAtom (xdpy, "TV", True))
    return META_CONNECTOR_TYPE_TV;
  if (atom == XInternAtom (xdpy, "TV-Composite", True))
    return META_CONNECTOR_TYPE_Composite;
  if (atom == XInternAtom (xdpy, "TV-SVideo", True))
    return META_CONNECTOR_TYPE_SVIDEO;
  /* Another set of mismatches. */
  if (atom == XInternAtom (xdpy, "TV-SCART", True))
    return META_CONNECTOR_TYPE_TV;
  if (atom == XInternAtom (xdpy, "TV-C4", True))
    return META_CONNECTOR_TYPE_TV;

  return META_CONNECTOR_TYPE_Unknown;
}

static MetaConnectorType
output_get_connector_type_from_prop (MetaMonitorManagerXrandr *manager_xrandr,
                                     MetaOutput               *output)
{
  Atom atom, actual_type, connector_type_atom;
  int actual_format;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;

  atom = XInternAtom (manager_xrandr->xdisplay, "ConnectorType", False);
  XRRGetOutputProperty (manager_xrandr->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    return META_CONNECTOR_TYPE_Unknown;

  connector_type_atom = ((Atom *) buffer)[0];
  return connector_type_from_atom (manager_xrandr, connector_type_atom);
}

static MetaConnectorType
output_get_connector_type_from_name (MetaMonitorManagerXrandr *manager_xrandr,
                                     MetaOutput               *output)
{
  const char *name = output->name;

  /* drmmode_display.c, which was copy/pasted across all the FOSS
   * xf86-video-* drivers, seems to name its outputs based on the
   * connector type, so look for that....
   *
   * SNA has its own naming scheme, because what else did you expect
   * from SNA, but it's not too different, so we can thankfully use
   * that with minor changes.
   *
   * http://cgit.freedesktop.org/xorg/xserver/tree/hw/xfree86/drivers/modesetting/drmmode_display.c#n953
   * http://cgit.freedesktop.org/xorg/driver/xf86-video-intel/tree/src/sna/sna_display.c#n3486
   */

  if (g_str_has_prefix (name, "DVI"))
    return META_CONNECTOR_TYPE_DVII;
  if (g_str_has_prefix (name, "LVDS"))
    return META_CONNECTOR_TYPE_LVDS;
  if (g_str_has_prefix (name, "HDMI"))
    return META_CONNECTOR_TYPE_HDMIA;
  if (g_str_has_prefix (name, "VGA"))
    return META_CONNECTOR_TYPE_VGA;
  /* SNA uses DP, not DisplayPort. Test for both. */
  if (g_str_has_prefix (name, "DP") || g_str_has_prefix (name, "DisplayPort"))
    return META_CONNECTOR_TYPE_DisplayPort;
  if (g_str_has_prefix (name, "eDP"))
    return META_CONNECTOR_TYPE_eDP;
  if (g_str_has_prefix (name, "Virtual"))
    return META_CONNECTOR_TYPE_VIRTUAL;
  if (g_str_has_prefix (name, "Composite"))
    return META_CONNECTOR_TYPE_Composite;
  if (g_str_has_prefix (name, "S-video"))
    return META_CONNECTOR_TYPE_SVIDEO;
  if (g_str_has_prefix (name, "TV"))
    return META_CONNECTOR_TYPE_TV;
  if (g_str_has_prefix (name, "CTV"))
    return META_CONNECTOR_TYPE_Composite;
  if (g_str_has_prefix (name, "DSI"))
    return META_CONNECTOR_TYPE_DSI;
  if (g_str_has_prefix (name, "DIN"))
    return META_CONNECTOR_TYPE_9PinDIN;

  return META_CONNECTOR_TYPE_Unknown;
}

static MetaConnectorType
output_get_connector_type (MetaMonitorManagerXrandr *manager_xrandr,
                           MetaOutput               *output)
{
  MetaConnectorType ret;

  /* The "ConnectorType" property is considered mandatory since RandR 1.3,
   * but none of the FOSS drivers support it, because we're a bunch of
   * professional software developers.
   *
   * Try poking it first, without any expectations that it will work.
   * If it's not there, we thankfully have other bonghits to try next.
   */
  ret = output_get_connector_type_from_prop (manager_xrandr, output);
  if (ret != META_CONNECTOR_TYPE_Unknown)
    return ret;

  /* Fall back to heuristics based on the output name. */
  ret = output_get_connector_type_from_name (manager_xrandr, output);
  if (ret != META_CONNECTOR_TYPE_Unknown)
    return ret;

  return META_CONNECTOR_TYPE_Unknown;
}

static void
output_get_modes (MetaMonitorManager *manager,
                  MetaOutput         *meta_output,
                  XRROutputInfo      *output)
{
  guint j, k;
  guint n_actual_modes;

  meta_output->modes = g_new0 (MetaCrtcMode *, output->nmode);

  n_actual_modes = 0;
  for (j = 0; j < (guint)output->nmode; j++)
    {
      for (k = 0; k < manager->n_modes; k++)
        {
          if (output->modes[j] == (XID)manager->modes[k].mode_id)
            {
              meta_output->modes[n_actual_modes] = &manager->modes[k];
              n_actual_modes += 1;
              break;
            }
        }
    }
  meta_output->n_modes = n_actual_modes;
  if (n_actual_modes > 0)
    meta_output->preferred_mode = meta_output->modes[0];
}

static void
output_get_crtcs (MetaMonitorManager *manager,
                  MetaOutput         *meta_output,
                  XRROutputInfo      *output)
{
  guint j, k;
  guint n_actual_crtcs;

  meta_output->possible_crtcs = g_new0 (MetaCrtc *, output->ncrtc);

  n_actual_crtcs = 0;
  for (j = 0; j < (unsigned)output->ncrtc; j++)
    {
      for (k = 0; k < manager->n_crtcs; k++)
        {
          if ((XID)manager->crtcs[k].crtc_id == output->crtcs[j])
            {
              meta_output->possible_crtcs[n_actual_crtcs] = &manager->crtcs[k];
              n_actual_crtcs += 1;
              break;
            }
        }
    }
  meta_output->n_possible_crtcs = n_actual_crtcs;

  meta_output->crtc = NULL;
  for (j = 0; j < manager->n_crtcs; j++)
    {
      if ((XID)manager->crtcs[j].crtc_id == output->crtc)
        {
          meta_output->crtc = &manager->crtcs[j];
          break;
        }
    }
}

static char *
get_xmode_name (XRRModeInfo *xmode)
{
  int width = xmode->width;
  int height = xmode->height;

  return g_strdup_printf ("%dx%d", width, height);
}

static void
meta_monitor_manager_xrandr_read_current (MetaMonitorManager *manager)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (manager);
  XRRScreenResources *resources;
  RROutput primary_output;
  unsigned int i, j, k;
  unsigned int n_actual_outputs;
  int min_width, min_height;
  Screen *screen;
  BOOL dpms_capable, dpms_enabled;
  CARD16 dpms_state;

  if (manager_xrandr->resources)
    XRRFreeScreenResources (manager_xrandr->resources);
  manager_xrandr->resources = NULL;

  dpms_capable = DPMSCapable (manager_xrandr->xdisplay);

  if (dpms_capable &&
      DPMSInfo (manager_xrandr->xdisplay, &dpms_state, &dpms_enabled) &&
      dpms_enabled)
    {
      switch (dpms_state)
        {
        case DPMSModeOn:
          manager->power_save_mode = META_POWER_SAVE_ON;
          break;
        case DPMSModeStandby:
          manager->power_save_mode = META_POWER_SAVE_STANDBY;
          break;
        case DPMSModeSuspend:
          manager->power_save_mode = META_POWER_SAVE_SUSPEND;
          break;
        case DPMSModeOff:
          manager->power_save_mode = META_POWER_SAVE_OFF;
          break;
        default:
          manager->power_save_mode = META_POWER_SAVE_UNSUPPORTED;
          break;
        }
    }
  else
    {
      manager->power_save_mode = META_POWER_SAVE_UNSUPPORTED;
    }

  XRRGetScreenSizeRange (manager_xrandr->xdisplay, DefaultRootWindow (manager_xrandr->xdisplay),
			 &min_width,
			 &min_height,
			 &manager_xrandr->max_screen_width,
			 &manager_xrandr->max_screen_height);

  screen = ScreenOfDisplay (manager_xrandr->xdisplay,
			    DefaultScreen (manager_xrandr->xdisplay));
  /* This is updated because we called RRUpdateConfiguration below */
  manager->screen_width = WidthOfScreen (screen);
  manager->screen_height = HeightOfScreen (screen);

  resources = XRRGetScreenResourcesCurrent (manager_xrandr->xdisplay,
					    DefaultRootWindow (manager_xrandr->xdisplay));
  if (!resources)
    return;

  manager_xrandr->resources = resources;
  manager->n_outputs = resources->noutput;
  manager->n_crtcs = resources->ncrtc;
  manager->n_modes = resources->nmode;
  manager->outputs = g_new0 (MetaOutput, manager->n_outputs);
  manager->modes = g_new0 (MetaCrtcMode, manager->n_modes);
  manager->crtcs = g_new0 (MetaCrtc, manager->n_crtcs);

  for (i = 0; i < (unsigned)resources->nmode; i++)
    {
      XRRModeInfo *xmode = &resources->modes[i];
      MetaCrtcMode *mode;

      mode = &manager->modes[i];

      mode->mode_id = xmode->id;
      mode->width = xmode->width;
      mode->height = xmode->height;
      mode->refresh_rate = (xmode->dotClock /
			    ((float)xmode->hTotal * xmode->vTotal));
      mode->flags = xmode->modeFlags;
      mode->name = get_xmode_name (xmode);
    }

  for (i = 0; i < (unsigned)resources->ncrtc; i++)
    {
      XRRCrtcInfo *crtc;
      MetaCrtc *meta_crtc;

      crtc = XRRGetCrtcInfo (manager_xrandr->xdisplay, resources, resources->crtcs[i]);

      meta_crtc = &manager->crtcs[i];

      meta_crtc->crtc_id = resources->crtcs[i];
      meta_crtc->rect.x = crtc->x;
      meta_crtc->rect.y = crtc->y;
      meta_crtc->rect.width = crtc->width;
      meta_crtc->rect.height = crtc->height;
      meta_crtc->is_dirty = FALSE;
      meta_crtc->transform = meta_monitor_transform_from_xrandr (crtc->rotation);
      meta_crtc->all_transforms = meta_monitor_transform_from_xrandr_all (crtc->rotations);

      for (j = 0; j < (unsigned)resources->nmode; j++)
	{
	  if (resources->modes[j].id == crtc->mode)
	    {
	      meta_crtc->current_mode = &manager->modes[j];
	      break;
	    }
	}

      XRRFreeCrtcInfo (crtc);
    }

  primary_output = XRRGetOutputPrimary (manager_xrandr->xdisplay,
					DefaultRootWindow (manager_xrandr->xdisplay));

  n_actual_outputs = 0;
  for (i = 0; i < (unsigned)resources->noutput; i++)
    {
      XRROutputInfo *output;
      MetaOutput *meta_output;

      output = XRRGetOutputInfo (manager_xrandr->xdisplay, resources, resources->outputs[i]);
      if (!output)
        continue;

      meta_output = &manager->outputs[n_actual_outputs];

      if (output->connection != RR_Disconnected)
	{
          GBytes *edid;

	  meta_output->winsys_id = resources->outputs[i];
	  meta_output->name = g_strdup (output->name);

          edid = read_output_edid (manager_xrandr, meta_output->winsys_id);
          meta_output_parse_edid (meta_output, edid);
          g_bytes_unref (edid);

	  meta_output->width_mm = output->mm_width;
	  meta_output->height_mm = output->mm_height;
	  meta_output->subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
          meta_output->hotplug_mode_update = output_get_hotplug_mode_update (manager_xrandr, meta_output);
	  meta_output->suggested_x = output_get_suggested_x (manager_xrandr, meta_output);
	  meta_output->suggested_y = output_get_suggested_y (manager_xrandr, meta_output);
          meta_output->connector_type = output_get_connector_type (manager_xrandr, meta_output);

	  output_get_tile_info (manager_xrandr, meta_output);
	  output_get_modes (manager, meta_output, output);
          output_get_crtcs (manager, meta_output, output);

	  meta_output->n_possible_clones = output->nclone;
	  meta_output->possible_clones = g_new0 (MetaOutput *, meta_output->n_possible_clones);
	  /* We can build the list of clones now, because we don't have the list of outputs
	     yet, so temporarily set the pointers to the bare XIDs, and then we'll fix them
	     in a second pass
	  */
	  for (j = 0; j < (unsigned)output->nclone; j++)
	    {
	      meta_output->possible_clones[j] = GINT_TO_POINTER (output->clones[j]);
	    }

	  meta_output->is_primary = ((XID)meta_output->winsys_id == primary_output);
	  meta_output->is_presentation = output_get_presentation_xrandr (manager_xrandr, meta_output);
	  meta_output->is_underscanning = output_get_underscanning_xrandr (manager_xrandr, meta_output);
          meta_output->supports_underscanning = output_get_supports_underscanning_xrandr (manager_xrandr, meta_output);
	  output_get_backlight_limits_xrandr (manager_xrandr, meta_output);

	  if (!(meta_output->backlight_min == 0 && meta_output->backlight_max == 0))
	    meta_output->backlight = output_get_backlight_xrandr (manager_xrandr, meta_output);
	  else
	    meta_output->backlight = -1;

          if (meta_output->n_modes == 0 || meta_output->n_possible_crtcs == 0)
            meta_monitor_manager_clear_output (meta_output);
          else
            n_actual_outputs++;
	}

      XRRFreeOutputInfo (output);
    }

  manager->n_outputs = n_actual_outputs;

  /* Sort the outputs for easier handling in MetaMonitorConfig */
  qsort (manager->outputs, manager->n_outputs, sizeof (MetaOutput), compare_outputs);

  /* Now fix the clones */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *meta_output;

      meta_output = &manager->outputs[i];

      for (j = 0; j < meta_output->n_possible_clones; j++)
	{
	  RROutput clone = GPOINTER_TO_INT (meta_output->possible_clones[j]);

	  for (k = 0; k < manager->n_outputs; k++)
	    {
	      if (clone == (XID)manager->outputs[k].winsys_id)
		{
		  meta_output->possible_clones[j] = &manager->outputs[k];
		  break;
		}
	    }
	}
    }
}

static GBytes *
meta_monitor_manager_xrandr_read_edid (MetaMonitorManager *manager,
                                       MetaOutput         *output)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (manager);

  return read_output_edid (manager_xrandr, output->winsys_id);
}

static void
meta_monitor_manager_xrandr_set_power_save_mode (MetaMonitorManager *manager,
						 MetaPowerSave       mode)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (manager);
  CARD16 state;

  switch (mode) {
  case META_POWER_SAVE_ON:
    state = DPMSModeOn;
    break;
  case META_POWER_SAVE_STANDBY:
    state = DPMSModeStandby;
    break;
  case META_POWER_SAVE_SUSPEND:
    state = DPMSModeSuspend;
    break;
  case META_POWER_SAVE_OFF:
    state = DPMSModeOff;
    break;
  default:
    return;
  }

  DPMSForceLevel (manager_xrandr->xdisplay, state);
  DPMSSetTimeouts (manager_xrandr->xdisplay, 0, 0, 0);
}

static xcb_randr_rotation_t
meta_monitor_transform_to_xrandr (MetaMonitorTransform transform)
{
  switch (transform)
    {
    case META_MONITOR_TRANSFORM_NORMAL:
      return XCB_RANDR_ROTATION_ROTATE_0;
    case META_MONITOR_TRANSFORM_90:
      return XCB_RANDR_ROTATION_ROTATE_90;
    case META_MONITOR_TRANSFORM_180:
      return XCB_RANDR_ROTATION_ROTATE_180;
    case META_MONITOR_TRANSFORM_270:
      return XCB_RANDR_ROTATION_ROTATE_270;
    case META_MONITOR_TRANSFORM_FLIPPED:
      return XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_0;
    case META_MONITOR_TRANSFORM_FLIPPED_90:
      return XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_90;
    case META_MONITOR_TRANSFORM_FLIPPED_180:
      return XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_180;
    case META_MONITOR_TRANSFORM_FLIPPED_270:
      return XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_270;
    }

  g_assert_not_reached ();
}

static void
output_set_presentation_xrandr (MetaMonitorManagerXrandr *manager_xrandr,
                                MetaOutput               *output,
                                gboolean                  presentation)
{
  Atom atom;
  int value = presentation;

  atom = XInternAtom (manager_xrandr->xdisplay, "_MUTTER_PRESENTATION_OUTPUT", False);

  xcb_randr_change_output_property (XGetXCBConnection (manager_xrandr->xdisplay),
                                    (XID)output->winsys_id,
                                    atom, XCB_ATOM_CARDINAL, 32,
                                    XCB_PROP_MODE_REPLACE,
                                    1, &value);
}

static void
output_set_underscanning_xrandr (MetaMonitorManagerXrandr *manager_xrandr,
                                 MetaOutput               *output,
                                 gboolean                  underscanning)
{
  Atom prop, valueatom;
  const char *value;

  prop = XInternAtom (manager_xrandr->xdisplay, "underscan", False);

  value = underscanning ? "on" : "off";
  valueatom = XInternAtom (manager_xrandr->xdisplay, value, False);

  xcb_randr_change_output_property (XGetXCBConnection (manager_xrandr->xdisplay),
                                    (XID)output->winsys_id,
                                    prop, XCB_ATOM_ATOM, 32,
                                    XCB_PROP_MODE_REPLACE,
                                    1, &valueatom);

  /* Configure the border at the same time. Currently, we use a
   * 5% of the width/height of the mode. In the future, we should
   * make the border configurable. */
  if (underscanning)
    {
      uint32_t border_value;

      prop = XInternAtom (manager_xrandr->xdisplay, "underscan hborder", False);
      border_value = output->crtc->current_mode->width * 0.05;

      xcb_randr_change_output_property (XGetXCBConnection (manager_xrandr->xdisplay),
                                        (XID)output->winsys_id,
                                        prop, XCB_ATOM_INTEGER, 32,
                                        XCB_PROP_MODE_REPLACE,
                                        1, &border_value);

      prop = XInternAtom (manager_xrandr->xdisplay, "underscan vborder", False);
      border_value = output->crtc->current_mode->height * 0.05;

      xcb_randr_change_output_property (XGetXCBConnection (manager_xrandr->xdisplay),
                                        (XID)output->winsys_id,
                                        prop, XCB_ATOM_INTEGER, 32,
                                        XCB_PROP_MODE_REPLACE,
                                        1, &border_value);
    }
}

static gboolean
xrandr_set_crtc_config (MetaMonitorManagerXrandr *manager_xrandr,
                        gboolean                  save_timestamp,
                        xcb_randr_crtc_t          crtc,
                        xcb_timestamp_t           timestamp,
                        int                       x,
                        int                       y,
                        xcb_randr_mode_t          mode,
                        xcb_randr_rotation_t      rotation,
                        xcb_randr_output_t       *outputs,
                        int                       n_outputs)
{
  xcb_connection_t *xcb_conn;
  xcb_timestamp_t config_timestamp;
  xcb_randr_set_crtc_config_cookie_t cookie;
  xcb_randr_set_crtc_config_reply_t *reply;
  xcb_generic_error_t *xcb_error = NULL;

  xcb_conn = XGetXCBConnection (manager_xrandr->xdisplay);
  config_timestamp = manager_xrandr->resources->configTimestamp;
  cookie = xcb_randr_set_crtc_config (xcb_conn,
                                      crtc,
                                      timestamp,
                                      config_timestamp,
                                      x, y,
                                      mode,
                                      rotation,
                                      n_outputs,
                                      outputs);
  reply = xcb_randr_set_crtc_config_reply (xcb_conn,
                                           cookie,
                                           &xcb_error);
  if (xcb_error || !reply)
    {
      free (xcb_error);
      free (reply);
      return FALSE;
    }

  if (save_timestamp)
    manager_xrandr->last_xrandr_set_timestamp = reply->timestamp;

  free (reply);

  return TRUE;
}

static gboolean
is_crtc_assignment_changed (MetaCrtc      *crtc,
                            MetaCrtcInfo **crtc_infos,
                            unsigned int   n_crtc_infos)
{
  unsigned int i;

  for (i = 0; i < n_crtc_infos; i++)
    {
      MetaCrtcInfo *crtc_info = crtc_infos[i];
      unsigned int j;

      if (crtc_info->crtc != crtc)
        continue;

      if (crtc->current_mode != crtc_info->mode)
        return TRUE;

      if (crtc->rect.x != crtc_info->x)
        return TRUE;

      if (crtc->rect.y != crtc_info->y)
        return TRUE;

      if (crtc->transform != crtc_info->transform)
        return TRUE;

      for (j = 0; j < crtc_info->outputs->len; j++)
        {
          MetaOutput *output = ((MetaOutput**) crtc_info->outputs->pdata)[j];

          if (output->crtc != crtc)
            return TRUE;
        }

      return FALSE;
    }

  return crtc->current_mode != NULL;
}

static gboolean
is_output_assignment_changed (MetaOutput      *output,
                              MetaCrtcInfo   **crtc_infos,
                              unsigned int     n_crtc_infos,
                              MetaOutputInfo **output_infos,
                              unsigned int     n_output_infos)
{
  gboolean output_is_found = FALSE;
  unsigned int i;

  for (i = 0; i < n_output_infos; i++)
    {
      MetaOutputInfo *output_info = output_infos[i];

      if (output_info->output != output)
        continue;

      if (output->is_primary != output_info->is_primary)
        return TRUE;

      if (output->is_presentation != output_info->is_presentation)
        return TRUE;

      if (output->is_underscanning != output_info->is_underscanning)
        return TRUE;

      output_is_found = TRUE;
    }

  if (!output_is_found)
    return output->crtc != NULL;

  for (i = 0; i < n_crtc_infos; i++)
    {
      MetaCrtcInfo *crtc_info = crtc_infos[i];
      unsigned int j;

      for (j = 0; j < crtc_info->outputs->len; j++)
        {
          MetaOutput *crtc_info_output =
            ((MetaOutput**) crtc_info->outputs->pdata)[j];

          if (crtc_info_output == output &&
              crtc_info->crtc == output->crtc)
            return FALSE;
        }
    }

  return TRUE;
}

static gboolean
is_assignments_changed (MetaMonitorManager *manager,
                        MetaCrtcInfo      **crtc_infos,
                        unsigned int        n_crtc_infos,
                        MetaOutputInfo    **output_infos,
                        unsigned int        n_output_infos)
{
  unsigned int i;

  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCrtc *crtc = &manager->crtcs[i];

      if (is_crtc_assignment_changed (crtc, crtc_infos, n_crtc_infos))
        return TRUE;
    }

  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output = &manager->outputs[i];

      if (is_output_assignment_changed (output,
                                        crtc_infos,
                                        n_crtc_infos,
                                        output_infos,
                                        n_output_infos))
        return TRUE;
    }

  return FALSE;
}

static void
apply_crtc_assignments (MetaMonitorManager *manager,
                        gboolean            save_timestamp,
                        MetaCrtcInfo      **crtcs,
                        unsigned int        n_crtcs,
                        MetaOutputInfo    **outputs,
                        unsigned int        n_outputs)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (manager);
  unsigned i;
  int width, height, width_mm, height_mm;

  XGrabServer (manager_xrandr->xdisplay);

  /* First compute the new size of the screen (framebuffer) */
  width = 0; height = 0;
  for (i = 0; i < n_crtcs; i++)
    {
      MetaCrtcInfo *crtc_info = crtcs[i];
      MetaCrtc *crtc = crtc_info->crtc;
      crtc->is_dirty = TRUE;

      if (crtc_info->mode == NULL)
        continue;

      if (meta_monitor_transform_is_rotated (crtc_info->transform))
        {
          width = MAX (width, crtc_info->x + crtc_info->mode->height);
          height = MAX (height, crtc_info->y + crtc_info->mode->width);
        }
      else
        {
          width = MAX (width, crtc_info->x + crtc_info->mode->width);
          height = MAX (height, crtc_info->y + crtc_info->mode->height);
        }
    }

  /* Second disable all newly disabled CRTCs, or CRTCs that in the previous
     configuration would be outside the new framebuffer (otherwise X complains
     loudly when resizing)
     CRTC will be enabled again after resizing the FB
  */
  for (i = 0; i < n_crtcs; i++)
    {
      MetaCrtcInfo *crtc_info = crtcs[i];
      MetaCrtc *crtc = crtc_info->crtc;

      if (crtc_info->mode == NULL ||
          crtc->rect.x + crtc->rect.width > width ||
          crtc->rect.y + crtc->rect.height > height)
        {
          xrandr_set_crtc_config (manager_xrandr,
                                  save_timestamp,
                                  (xcb_randr_crtc_t) crtc->crtc_id,
                                  XCB_CURRENT_TIME,
                                  0, 0, XCB_NONE,
                                  XCB_RANDR_ROTATION_ROTATE_0,
                                  NULL, 0);

          crtc->rect.x = 0;
          crtc->rect.y = 0;
          crtc->rect.width = 0;
          crtc->rect.height = 0;
          crtc->current_mode = NULL;
        }
    }

  /* Disable CRTCs not mentioned in the list */
  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCrtc *crtc = &manager->crtcs[i];

      if (crtc->is_dirty)
        {
          crtc->is_dirty = FALSE;
          continue;
        }
      if (crtc->current_mode == NULL)
        continue;

      xrandr_set_crtc_config (manager_xrandr,
                              save_timestamp,
                              (xcb_randr_crtc_t) crtc->crtc_id,
                              XCB_CURRENT_TIME,
                              0, 0, XCB_NONE,
                              XCB_RANDR_ROTATION_ROTATE_0,
                              NULL, 0);

      crtc->rect.x = 0;
      crtc->rect.y = 0;
      crtc->rect.width = 0;
      crtc->rect.height = 0;
      crtc->current_mode = NULL;
    }

  g_assert (width > 0 && height > 0);
  /* The 'physical size' of an X screen is meaningless if that screen
   * can consist of many monitors. So just pick a size that make the
   * dpi 96.
   *
   * Firefox and Evince apparently believe what X tells them.
   */
  width_mm = (width / DPI_FALLBACK) * 25.4 + 0.5;
  height_mm = (height / DPI_FALLBACK) * 25.4 + 0.5;
  XRRSetScreenSize (manager_xrandr->xdisplay, DefaultRootWindow (manager_xrandr->xdisplay),
                    width, height, width_mm, height_mm);

  for (i = 0; i < n_crtcs; i++)
    {
      MetaCrtcInfo *crtc_info = crtcs[i];
      MetaCrtc *crtc = crtc_info->crtc;

      if (crtc_info->mode != NULL)
        {
          MetaCrtcMode *mode;
          g_autofree xcb_randr_output_t *output_ids = NULL;
          unsigned int j, n_output_ids;
          xcb_randr_rotation_t rotation;

          mode = crtc_info->mode;

          n_output_ids = crtc_info->outputs->len;
          output_ids = g_new (xcb_randr_output_t, n_output_ids);

          for (j = 0; j < n_output_ids; j++)
            {
              MetaOutput *output;

              output = ((MetaOutput**)crtc_info->outputs->pdata)[j];

              output->is_dirty = TRUE;
              output->crtc = crtc;

              output_ids[j] = output->winsys_id;
            }

          rotation = meta_monitor_transform_to_xrandr (crtc_info->transform);
          if (!xrandr_set_crtc_config (manager_xrandr,
                                       save_timestamp,
                                       (xcb_randr_crtc_t) crtc->crtc_id,
                                       XCB_CURRENT_TIME,
                                       crtc_info->x, crtc_info->y,
                                       (xcb_randr_mode_t) mode->mode_id,
                                       rotation,
                                       output_ids, n_output_ids))
            {
              meta_warning ("Configuring CRTC %d with mode %d (%d x %d @ %f) at position %d, %d and transform %u failed\n",
                            (unsigned)(crtc->crtc_id), (unsigned)(mode->mode_id),
                            mode->width, mode->height, (float)mode->refresh_rate,
                            crtc_info->x, crtc_info->y, crtc_info->transform);
              continue;
            }

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
        }
    }

  for (i = 0; i < n_outputs; i++)
    {
      MetaOutputInfo *output_info = outputs[i];
      MetaOutput *output = output_info->output;

      if (output_info->is_primary)
        {
          XRRSetOutputPrimary (manager_xrandr->xdisplay,
                               DefaultRootWindow (manager_xrandr->xdisplay),
                               (XID)output_info->output->winsys_id);
        }

      output_set_presentation_xrandr (manager_xrandr,
                                      output_info->output,
                                      output_info->is_presentation);

      if (output_get_supports_underscanning_xrandr (manager_xrandr, output_info->output))
        output_set_underscanning_xrandr (manager_xrandr,
                                         output_info->output,
                                         output_info->is_underscanning);

      output->is_primary = output_info->is_primary;
      output->is_presentation = output_info->is_presentation;
      output->is_underscanning = output_info->is_underscanning;
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

  XUngrabServer (manager_xrandr->xdisplay);
  XFlush (manager_xrandr->xdisplay);
}

static void
meta_monitor_manager_xrandr_ensure_initial_config (MetaMonitorManager *manager)
{
  MetaMonitorManagerDeriveFlag flags =
    META_MONITOR_MANAGER_DERIVE_FLAG_NONE;

  meta_monitor_manager_ensure_configured (manager);

  /*
   * Normally we don't rebuild our data structures until we see the
   * RRScreenNotify event, but at least at startup we want to have the right
   * configuration immediately.
   */
  meta_monitor_manager_read_current_state (manager);

  if (meta_is_monitor_config_manager_enabled ())
    flags |= META_MONITOR_MANAGER_DERIVE_FLAG_CONFIGURED_SCALE;

  meta_monitor_manager_update_logical_state_derived (manager, flags);
}

static gboolean
meta_monitor_manager_xrandr_apply_monitors_config (MetaMonitorManager      *manager,
                                                   MetaMonitorsConfig      *config,
                                                   MetaMonitorsConfigMethod method,
                                                   GError                 **error)
{
  GPtrArray *crtc_infos;
  GPtrArray *output_infos;

  if (!config)
    {
      MetaMonitorManagerDeriveFlag flags =
        META_MONITOR_MANAGER_DERIVE_FLAG_NONE;

      meta_monitor_manager_rebuild_derived (manager, flags);
      return TRUE;
    }

  if (!meta_monitor_config_manager_assign (manager, config,
                                           &crtc_infos, &output_infos,
                                           error))
    return FALSE;

  if (method != META_MONITORS_CONFIG_METHOD_VERIFY)
    {
      /*
       * If the assignment has not changed, we won't get any notification about
       * any new configuration from the X server; but we still need to update
       * our own configuration, as something not applicable in Xrandr might
       * have changed locally, such as the logical monitors scale. This means we
       * must check that our new assignment actually changes anything, otherwise
       * just update the logical state.
       */
      if (is_assignments_changed (manager,
                                  (MetaCrtcInfo **) crtc_infos->pdata,
                                  crtc_infos->len,
                                  (MetaOutputInfo **) output_infos->pdata,
                                  output_infos->len))
        {
          apply_crtc_assignments (manager,
                                  TRUE,
                                  (MetaCrtcInfo **) crtc_infos->pdata,
                                  crtc_infos->len,
                                  (MetaOutputInfo **) output_infos->pdata,
                                  output_infos->len);
        }
      else
        {
          MetaMonitorManagerDeriveFlag flags;

          flags = (META_MONITOR_MANAGER_DERIVE_FLAG_NONE |
                   META_MONITOR_MANAGER_DERIVE_FLAG_CONFIGURED_SCALE);
          meta_monitor_manager_rebuild_derived (manager, flags);
        }
    }

  g_ptr_array_free (crtc_infos, TRUE);
  g_ptr_array_free (output_infos, TRUE);

  return TRUE;
}

static void
meta_monitor_manager_xrandr_apply_configuration (MetaMonitorManager *manager,
						 MetaCrtcInfo      **crtcs,
						 unsigned int        n_crtcs,
						 MetaOutputInfo    **outputs,
						 unsigned int        n_outputs)
{
  apply_crtc_assignments (manager, FALSE, crtcs, n_crtcs, outputs, n_outputs);
}

static void
meta_monitor_manager_xrandr_change_backlight (MetaMonitorManager *manager,
					      MetaOutput         *output,
					      gint                value)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (manager);
  Atom atom;
  int hw_value;

  hw_value = round ((double)value / 100.0 * output->backlight_max + output->backlight_min);

  atom = XInternAtom (manager_xrandr->xdisplay, "Backlight", False);

  xcb_randr_change_output_property (XGetXCBConnection (manager_xrandr->xdisplay),
                                    (XID)output->winsys_id,
                                    atom, XCB_ATOM_INTEGER, 32,
                                    XCB_PROP_MODE_REPLACE,
                                    1, &hw_value);

  /* We're not selecting for property notifies, so update the value immediately */
  output->backlight = normalize_backlight (output, hw_value);
}

static void
meta_monitor_manager_xrandr_get_crtc_gamma (MetaMonitorManager  *manager,
					    MetaCrtc            *crtc,
					    gsize               *size,
					    unsigned short     **red,
					    unsigned short     **green,
					    unsigned short     **blue)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (manager);
  XRRCrtcGamma *gamma;

  gamma = XRRGetCrtcGamma (manager_xrandr->xdisplay, (XID)crtc->crtc_id);

  *size = gamma->size;
  *red = g_memdup (gamma->red, sizeof (unsigned short) * gamma->size);
  *green = g_memdup (gamma->green, sizeof (unsigned short) * gamma->size);
  *blue = g_memdup (gamma->blue, sizeof (unsigned short) * gamma->size);

  XRRFreeGamma (gamma);
}

static void
meta_monitor_manager_xrandr_set_crtc_gamma (MetaMonitorManager *manager,
					    MetaCrtc           *crtc,
					    gsize               size,
					    unsigned short     *red,
					    unsigned short     *green,
					    unsigned short     *blue)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (manager);
  XRRCrtcGamma *gamma;

  gamma = XRRAllocGamma (size);
  memcpy (gamma->red, red, sizeof (unsigned short) * size);
  memcpy (gamma->green, green, sizeof (unsigned short) * size);
  memcpy (gamma->blue, blue, sizeof (unsigned short) * size);

  XRRSetCrtcGamma (manager_xrandr->xdisplay, (XID)crtc->crtc_id, gamma);

  XRRFreeGamma (gamma);
}

#ifdef HAVE_XRANDR15
static MetaMonitorXrandrData *
meta_monitor_xrandr_data_from_monitor (MetaMonitor *monitor)
{
  MetaMonitorXrandrData *monitor_xrandr_data;

  monitor_xrandr_data = g_object_get_qdata (G_OBJECT (monitor),
                                            quark_meta_monitor_xrandr_data);
  if (monitor_xrandr_data)
    return monitor_xrandr_data;

  monitor_xrandr_data = g_new0 (MetaMonitorXrandrData, 1);
  g_object_set_qdata_full (G_OBJECT (monitor),
                           quark_meta_monitor_xrandr_data,
                           monitor_xrandr_data,
                           g_free);

  return monitor_xrandr_data;
}

static void
meta_monitor_manager_xrandr_increase_monitor_count (MetaMonitorManagerXrandr *manager_xrandr,
                                                    Atom                      name_atom)
{
  int count;

  count =
    GPOINTER_TO_INT (g_hash_table_lookup (manager_xrandr->tiled_monitor_atoms,
                                          GSIZE_TO_POINTER (name_atom)));

  count++;
  g_hash_table_insert (manager_xrandr->tiled_monitor_atoms,
                       GSIZE_TO_POINTER (name_atom),
                       GINT_TO_POINTER (count));
}

static int
meta_monitor_manager_xrandr_decrease_monitor_count (MetaMonitorManagerXrandr *manager_xrandr,
                                                    Atom                      name_atom)
{
  int count;

  count =
    GPOINTER_TO_SIZE (g_hash_table_lookup (manager_xrandr->tiled_monitor_atoms,
                                           GSIZE_TO_POINTER (name_atom)));
  g_assert (count > 0);

  count--;
  g_hash_table_insert (manager_xrandr->tiled_monitor_atoms,
                       GSIZE_TO_POINTER (name_atom),
                       GINT_TO_POINTER (count));

  return count;
}

static void
meta_monitor_manager_xrandr_tiled_monitor_added (MetaMonitorManager *manager,
                                                 MetaMonitor        *monitor)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (manager);
  MetaMonitorTiled *monitor_tiled = META_MONITOR_TILED (monitor);
  const char *product;
  char *name;
  uint32_t tile_group_id;
  MetaMonitorXrandrData *monitor_xrandr_data;
  Atom name_atom;
  XRRMonitorInfo *xrandr_monitor_info;
  GList *outputs;
  GList *l;
  int i;

  if (manager_xrandr->has_randr15 == FALSE)
    return;

  product = meta_monitor_get_product (monitor);
  tile_group_id = meta_monitor_tiled_get_tile_group_id (monitor_tiled);

  if (product)
    name = g_strdup_printf ("%s-%d", product, tile_group_id);
  else
    name = g_strdup_printf ("Tiled-%d", tile_group_id);

  name_atom = XInternAtom (manager_xrandr->xdisplay, name, False);
  g_free (name);

  monitor_xrandr_data = meta_monitor_xrandr_data_from_monitor (monitor);
  monitor_xrandr_data->xrandr_name = name_atom;

  meta_monitor_manager_xrandr_increase_monitor_count (manager_xrandr,
                                                      name_atom);

  outputs = meta_monitor_get_outputs (monitor);
  xrandr_monitor_info = XRRAllocateMonitor (manager_xrandr->xdisplay,
                                            g_list_length (outputs));
  xrandr_monitor_info->name = name_atom;
  xrandr_monitor_info->primary = meta_monitor_is_primary (monitor);
  xrandr_monitor_info->automatic = True;
  for (l = outputs, i = 0; l; l = l->next, i++)
    {
      MetaOutput *output = l->data;

      xrandr_monitor_info->outputs[i] = output->winsys_id;
    }

  XRRSetMonitor (manager_xrandr->xdisplay,
                 DefaultRootWindow (manager_xrandr->xdisplay),
                 xrandr_monitor_info);
  XRRFreeMonitors (xrandr_monitor_info);
}

static void
meta_monitor_manager_xrandr_tiled_monitor_removed (MetaMonitorManager *manager,
                                                   MetaMonitor        *monitor)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (manager);
  MetaMonitorXrandrData *monitor_xrandr_data;
  Atom monitor_name;

  int monitor_count;

  if (manager_xrandr->has_randr15 == FALSE)
    return;

  monitor_xrandr_data = meta_monitor_xrandr_data_from_monitor (monitor);
  monitor_name = monitor_xrandr_data->xrandr_name;
  monitor_count =
    meta_monitor_manager_xrandr_decrease_monitor_count (manager_xrandr,
                                                        monitor_name);

  if (monitor_count == 0)
    XRRDeleteMonitor (manager_xrandr->xdisplay,
                      DefaultRootWindow (manager_xrandr->xdisplay),
                      monitor_name);
}

static void
meta_monitor_manager_xrandr_init_monitors (MetaMonitorManagerXrandr *manager_xrandr)
{
  XRRMonitorInfo *m;
  int n, i;

  if (manager_xrandr->has_randr15 == FALSE)
    return;

  /* delete any tiled monitors setup, as mutter will want to recreate
     things in its image */
  m = XRRGetMonitors (manager_xrandr->xdisplay,
                      DefaultRootWindow (manager_xrandr->xdisplay),
                      FALSE, &n);
  if (n == -1)
    return;

  for (i = 0; i < n; i++)
    {
      if (m[i].noutput > 1)
        XRRDeleteMonitor (manager_xrandr->xdisplay,
                          DefaultRootWindow (manager_xrandr->xdisplay),
                          m[i].name);
    }
  XRRFreeMonitors (m);
}
#endif

static gboolean
meta_monitor_manager_xrandr_is_transform_handled (MetaMonitorManager  *manager,
                                                  MetaCrtc            *crtc,
                                                  MetaMonitorTransform transform)
{
  g_warn_if_fail (crtc->all_transforms & transform);

  return TRUE;
}

static int
meta_monitor_manager_xrandr_calculate_monitor_mode_scale (MetaMonitorManager *manager,
                                                          MetaMonitor        *monitor,
                                                          MetaMonitorMode    *monitor_mode)
{
  return meta_monitor_calculate_mode_scale (monitor, monitor_mode);
}

static void
meta_monitor_manager_xrandr_get_supported_scales (MetaMonitorManager *manager,
                                                  float             **scales,
                                                  int                *n_scales)
{
  *scales = supported_scales_xrandr;
  *n_scales = G_N_ELEMENTS (supported_scales_xrandr);
}

static MetaMonitorManagerCapability
meta_monitor_manager_xrandr_get_capabilities (MetaMonitorManager *manager)
{
  return (META_MONITOR_MANAGER_CAPABILITY_MIRRORING |
          META_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED);
}

static gboolean
meta_monitor_manager_xrandr_get_max_screen_size (MetaMonitorManager *manager,
                                                 int                *max_width,
                                                 int                *max_height)
{
  MetaMonitorManagerXrandr *manager_xrandr =
    META_MONITOR_MANAGER_XRANDR (manager);

  *max_width = manager_xrandr->max_screen_width;
  *max_height = manager_xrandr->max_screen_height;

  return TRUE;
}

static MetaLogicalMonitorLayoutMode
meta_monitor_manager_xrandr_get_default_layout_mode (MetaMonitorManager *manager)
{
  return META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
}

static void
meta_monitor_manager_xrandr_init (MetaMonitorManagerXrandr *manager_xrandr)
{
  MetaBackendX11 *backend = META_BACKEND_X11 (meta_get_backend ());

  manager_xrandr->xdisplay = meta_backend_x11_get_xdisplay (backend);

  if (!XRRQueryExtension (manager_xrandr->xdisplay,
			  &manager_xrandr->rr_event_base,
			  &manager_xrandr->rr_error_base))
    {
      return;
    }
  else
    {
      int major_version, minor_version;
      /* We only use ScreenChangeNotify, but GDK uses the others,
	 and we don't want to step on its toes */
      XRRSelectInput (manager_xrandr->xdisplay,
		      DefaultRootWindow (manager_xrandr->xdisplay),
		      RRScreenChangeNotifyMask
		      | RRCrtcChangeNotifyMask
		      | RROutputPropertyNotifyMask);

      manager_xrandr->has_randr15 = FALSE;
      XRRQueryVersion (manager_xrandr->xdisplay, &major_version,
                       &minor_version);
#ifdef HAVE_XRANDR15
      if (major_version > 1 ||
          (major_version == 1 &&
           minor_version >= 5))
        {
          manager_xrandr->has_randr15 = TRUE;
          manager_xrandr->tiled_monitor_atoms = g_hash_table_new (NULL, NULL);
        }
      meta_monitor_manager_xrandr_init_monitors (manager_xrandr);
#endif
    }
}

static void
meta_monitor_manager_xrandr_finalize (GObject *object)
{
  MetaMonitorManagerXrandr *manager_xrandr = META_MONITOR_MANAGER_XRANDR (object);

  if (manager_xrandr->resources)
    XRRFreeScreenResources (manager_xrandr->resources);
  manager_xrandr->resources = NULL;

  g_hash_table_destroy (manager_xrandr->tiled_monitor_atoms);

  G_OBJECT_CLASS (meta_monitor_manager_xrandr_parent_class)->finalize (object);
}

static void
meta_monitor_manager_xrandr_class_init (MetaMonitorManagerXrandrClass *klass)
{
  MetaMonitorManagerClass *manager_class = META_MONITOR_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_monitor_manager_xrandr_finalize;

  manager_class->read_current = meta_monitor_manager_xrandr_read_current;
  manager_class->read_edid = meta_monitor_manager_xrandr_read_edid;
  manager_class->ensure_initial_config = meta_monitor_manager_xrandr_ensure_initial_config;
  manager_class->apply_monitors_config = meta_monitor_manager_xrandr_apply_monitors_config;
  manager_class->apply_configuration = meta_monitor_manager_xrandr_apply_configuration;
  manager_class->set_power_save_mode = meta_monitor_manager_xrandr_set_power_save_mode;
  manager_class->change_backlight = meta_monitor_manager_xrandr_change_backlight;
  manager_class->get_crtc_gamma = meta_monitor_manager_xrandr_get_crtc_gamma;
  manager_class->set_crtc_gamma = meta_monitor_manager_xrandr_set_crtc_gamma;
#ifdef HAVE_XRANDR15
  manager_class->tiled_monitor_added = meta_monitor_manager_xrandr_tiled_monitor_added;
  manager_class->tiled_monitor_removed = meta_monitor_manager_xrandr_tiled_monitor_removed;
#endif
  manager_class->is_transform_handled = meta_monitor_manager_xrandr_is_transform_handled;
  manager_class->calculate_monitor_mode_scale = meta_monitor_manager_xrandr_calculate_monitor_mode_scale;
  manager_class->get_supported_scales = meta_monitor_manager_xrandr_get_supported_scales;
  manager_class->get_capabilities = meta_monitor_manager_xrandr_get_capabilities;
  manager_class->get_max_screen_size = meta_monitor_manager_xrandr_get_max_screen_size;
  manager_class->get_default_layout_mode = meta_monitor_manager_xrandr_get_default_layout_mode;

  quark_meta_monitor_xrandr_data =
    g_quark_from_static_string ("-meta-monitor-xrandr-data");
}

gboolean
meta_monitor_manager_xrandr_handle_xevent (MetaMonitorManagerXrandr *manager_xrandr,
					   XEvent                   *event)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_xrandr);
  gboolean is_hotplug;
  gboolean is_our_configuration;

  if ((event->type - manager_xrandr->rr_event_base) != RRScreenChangeNotify)
    return FALSE;

  XRRUpdateConfiguration (event);

  meta_monitor_manager_read_current_state (manager);


  is_hotplug = (manager_xrandr->resources->timestamp <
                manager_xrandr->resources->configTimestamp);
  is_our_configuration = (manager_xrandr->resources->timestamp ==
                          manager_xrandr->last_xrandr_set_timestamp);
  if (is_hotplug)
    {
      meta_monitor_manager_on_hotplug (manager);
    }
  else
    {
      MetaMonitorManagerDeriveFlag flags =
        META_MONITOR_MANAGER_DERIVE_FLAG_NONE;

      if (is_our_configuration)
        flags |= META_MONITOR_MANAGER_DERIVE_FLAG_CONFIGURED_SCALE;

      meta_monitor_manager_rebuild_derived (manager, flags);
    }

  return TRUE;
}
