/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013-2017 Red Hat Inc.
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

#include "backends/x11/meta-output-xrandr.h"

#include <glib.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>

#include "backends/meta-crtc.h"
#include "backends/x11/meta-monitor-manager-xrandr.h"
#include "meta/util.h"

static Display *
xdisplay_from_output (MetaOutput *output)
{
  MetaGpu *gpu = meta_output_get_gpu (output);
  MetaMonitorManager *monitor_manager = meta_gpu_get_monitor_manager (gpu);
  MetaMonitorManagerXrandr *monitor_manager_xrandr =
    META_MONITOR_MANAGER_XRANDR (monitor_manager);

  return meta_monitor_manager_xrandr_get_xdisplay (monitor_manager_xrandr);
}

static void
output_set_presentation_xrandr (MetaOutput *output,
                                gboolean    presentation)
{
  Display *xdisplay = xdisplay_from_output (output);
  Atom atom;
  int value = presentation;

  atom = XInternAtom (xdisplay, "_MUTTER_PRESENTATION_OUTPUT", False);

  xcb_randr_change_output_property (XGetXCBConnection (xdisplay),
                                    (XID) output->winsys_id,
                                    atom, XCB_ATOM_CARDINAL, 32,
                                    XCB_PROP_MODE_REPLACE,
                                    1, &value);
}

static void
output_set_underscanning_xrandr (MetaOutput *output,
                                 gboolean    underscanning)
{
  Display *xdisplay = xdisplay_from_output (output);
  Atom prop, valueatom;
  const char *value;

  prop = XInternAtom (xdisplay, "underscan", False);

  value = underscanning ? "on" : "off";
  valueatom = XInternAtom (xdisplay, value, False);

  xcb_randr_change_output_property (XGetXCBConnection (xdisplay),
                                    (XID) output->winsys_id,
                                    prop, XCB_ATOM_ATOM, 32,
                                    XCB_PROP_MODE_REPLACE,
                                    1, &valueatom);

  /* Configure the border at the same time. Currently, we use a
   * 5% of the width/height of the mode. In the future, we should
   * make the border configurable. */
  if (underscanning)
    {
      uint32_t border_value;

      prop = XInternAtom (xdisplay, "underscan hborder", False);
      border_value = output->crtc->current_mode->width * 0.05;

      xcb_randr_change_output_property (XGetXCBConnection (xdisplay),
                                        (XID) output->winsys_id,
                                        prop, XCB_ATOM_INTEGER, 32,
                                        XCB_PROP_MODE_REPLACE,
                                        1, &border_value);

      prop = XInternAtom (xdisplay, "underscan vborder", False);
      border_value = output->crtc->current_mode->height * 0.05;

      xcb_randr_change_output_property (XGetXCBConnection (xdisplay),
                                        (XID) output->winsys_id,
                                        prop, XCB_ATOM_INTEGER, 32,
                                        XCB_PROP_MODE_REPLACE,
                                        1, &border_value);
    }
}

void
meta_output_xrandr_apply_mode (MetaOutput *output)
{
  Display *xdisplay = xdisplay_from_output (output);

  if (output->is_primary)
    {
      XRRSetOutputPrimary (xdisplay, DefaultRootWindow (xdisplay),
                           (XID) output->winsys_id);
    }

  output_set_presentation_xrandr (output, output->is_presentation);

  if (output->supports_underscanning)
    output_set_underscanning_xrandr (output, output->is_underscanning);
}

static int
normalize_backlight (MetaOutput *output,
                     int         hw_value)
{
  return round ((double)(hw_value - output->backlight_min) /
                (output->backlight_max - output->backlight_min) * 100.0);
}

void
meta_output_xrandr_change_backlight (MetaOutput *output,
                                     int         value)
{
  Display *xdisplay = xdisplay_from_output (output);
  Atom atom;
  int hw_value;

  hw_value = round ((double) value / 100.0 * output->backlight_max +
                    output->backlight_min);

  atom = XInternAtom (xdisplay, "Backlight", False);

  xcb_randr_change_output_property (XGetXCBConnection (xdisplay),
                                    (XID)output->winsys_id,
                                    atom, XCB_ATOM_INTEGER, 32,
                                    XCB_PROP_MODE_REPLACE,
                                    1, &hw_value);

  /* We're not selecting for property notifies, so update the value immediately */
  output->backlight = normalize_backlight (output, hw_value);
}

static gboolean
output_get_integer_property (MetaOutput *output,
                             const char *propname,
                             gint       *value)
{
  Display *xdisplay = xdisplay_from_output (output);
  gboolean exists = FALSE;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *buffer;

  atom = XInternAtom (xdisplay, propname, False);
  XRRGetOutputProperty (xdisplay,
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
output_get_property_exists (MetaOutput *output,
                            const char *propname)
{
  Display *xdisplay = xdisplay_from_output (output);
  gboolean exists = FALSE;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *buffer;

  atom = XInternAtom (xdisplay, propname, False);
  XRRGetOutputProperty (xdisplay,
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
output_get_boolean_property (MetaOutput *output,
                             const char *propname)
{
  Display *xdisplay = xdisplay_from_output (output);
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;

  atom = XInternAtom (xdisplay, propname, False);
  XRRGetOutputProperty (xdisplay,
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
output_get_presentation_xrandr (MetaOutput *output)
{
  return output_get_boolean_property (output, "_MUTTER_PRESENTATION_OUTPUT");
}

static gboolean
output_get_underscanning_xrandr (MetaOutput *output)
{
  Display *xdisplay = xdisplay_from_output (output);
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;
  g_autofree char *str = NULL;

  atom = XInternAtom (xdisplay, "underscan", False);
  XRRGetOutputProperty (xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    return FALSE;

  str = XGetAtomName (xdisplay, *(Atom *)buffer);
  return (strcmp (str, "on") == 0);
}

static gboolean
output_get_supports_underscanning_xrandr (MetaOutput *output)
{
  Display *xdisplay = xdisplay_from_output (output);
  Atom atom, actual_type;
  int actual_format, i;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;
  XRRPropertyInfo *property_info;
  Atom *values;
  gboolean supports_underscanning = FALSE;

  atom = XInternAtom (xdisplay, "underscan", False);
  XRRGetOutputProperty (xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    return FALSE;

  property_info = XRRQueryOutputProperty (xdisplay,
                                          (XID) output->winsys_id,
                                          atom);
  values = (Atom *) property_info->values;

  for (i = 0; i < property_info->num_values; i++)
    {
      /* The output supports underscanning if "on" is a valid value
       * for the underscan property.
       */
      char *name = XGetAtomName (xdisplay, values[i]);
      if (strcmp (name, "on") == 0)
        supports_underscanning = TRUE;

      XFree (name);
    }

  XFree (property_info);

  return supports_underscanning;
}

static int
output_get_backlight_xrandr (MetaOutput *output)
{
  Display *xdisplay = xdisplay_from_output (output);
  int value = -1;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;

  atom = XInternAtom (xdisplay, "Backlight", False);
  XRRGetOutputProperty (xdisplay,
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
output_get_backlight_limits_xrandr (MetaOutput *output)
{
  Display *xdisplay = xdisplay_from_output (output);
  Atom atom;
  xcb_connection_t *xcb_conn;
  xcb_randr_query_output_property_cookie_t cookie;
  g_autofree xcb_randr_query_output_property_reply_t *reply = NULL;

  atom = XInternAtom (xdisplay, "Backlight", False);

  xcb_conn = XGetXCBConnection (xdisplay);
  cookie = xcb_randr_query_output_property (xcb_conn,
                                            (xcb_randr_output_t) output->winsys_id,
                                            (xcb_atom_t) atom);
  reply = xcb_randr_query_output_property_reply (xcb_conn,
                                                 cookie,
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

static guint8 *
get_edid_property (Display  *xdisplay,
                   RROutput  output,
                   Atom      atom,
                   gsize    *len)
{
  unsigned char *prop;
  int actual_format;
  unsigned long nitems, bytes_after;
  Atom actual_type;
  guint8 *result;

  XRRGetOutputProperty (xdisplay, output, atom,
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

GBytes *
meta_output_xrandr_read_edid (MetaOutput *output)
{
  Display *xdisplay = xdisplay_from_output (output);
  Atom edid_atom;
  guint8 *result;
  gsize len;

  edid_atom = XInternAtom (xdisplay, "EDID", FALSE);
  result = get_edid_property (xdisplay, output->winsys_id, edid_atom, &len);

  if (!result)
    {
      edid_atom = XInternAtom (xdisplay, "EDID_DATA", FALSE);
      result = get_edid_property (xdisplay, output->winsys_id, edid_atom, &len);
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

static gboolean
output_get_hotplug_mode_update (MetaOutput *output)
{
  return output_get_property_exists (output, "hotplug_mode_update");
}

static gint
output_get_suggested_x (MetaOutput *output)
{
  gint val;
  if (output_get_integer_property (output, "suggested X", &val))
    return val;

  return -1;
}

static gint
output_get_suggested_y (MetaOutput *output)
{
  gint val;
  if (output_get_integer_property (output, "suggested Y", &val))
    return val;

  return -1;
}

static MetaConnectorType
connector_type_from_atom (Display *xdisplay,
                          Atom     atom)
{
  if (atom == XInternAtom (xdisplay, "HDMI", True))
    return META_CONNECTOR_TYPE_HDMIA;
  if (atom == XInternAtom (xdisplay, "VGA", True))
    return META_CONNECTOR_TYPE_VGA;
  /* Doesn't have a DRM equivalent, but means an internal panel.
   * We could pick either LVDS or eDP here. */
  if (atom == XInternAtom (xdisplay, "Panel", True))
    return META_CONNECTOR_TYPE_LVDS;
  if (atom == XInternAtom (xdisplay, "DVI", True) ||
      atom == XInternAtom (xdisplay, "DVI-I", True))
    return META_CONNECTOR_TYPE_DVII;
  if (atom == XInternAtom (xdisplay, "DVI-A", True))
    return META_CONNECTOR_TYPE_DVIA;
  if (atom == XInternAtom (xdisplay, "DVI-D", True))
    return META_CONNECTOR_TYPE_DVID;
  if (atom == XInternAtom (xdisplay, "DisplayPort", True))
    return META_CONNECTOR_TYPE_DisplayPort;

  if (atom == XInternAtom (xdisplay, "TV", True))
    return META_CONNECTOR_TYPE_TV;
  if (atom == XInternAtom (xdisplay, "TV-Composite", True))
    return META_CONNECTOR_TYPE_Composite;
  if (atom == XInternAtom (xdisplay, "TV-SVideo", True))
    return META_CONNECTOR_TYPE_SVIDEO;
  /* Another set of mismatches. */
  if (atom == XInternAtom (xdisplay, "TV-SCART", True))
    return META_CONNECTOR_TYPE_TV;
  if (atom == XInternAtom (xdisplay, "TV-C4", True))
    return META_CONNECTOR_TYPE_TV;

  return META_CONNECTOR_TYPE_Unknown;
}

static MetaConnectorType
output_get_connector_type_from_prop (MetaOutput *output)
{
  Display *xdisplay = xdisplay_from_output (output);
  Atom atom, actual_type, connector_type_atom;
  int actual_format;
  unsigned long nitems, bytes_after;
  g_autofree unsigned char *buffer = NULL;

  atom = XInternAtom (xdisplay, "ConnectorType", False);
  XRRGetOutputProperty (xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    return META_CONNECTOR_TYPE_Unknown;

  connector_type_atom = ((Atom *) buffer)[0];
  return connector_type_from_atom (xdisplay, connector_type_atom);
}

static MetaConnectorType
output_get_connector_type_from_name (MetaOutput *output)
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
output_get_connector_type (MetaOutput *output)
{
  MetaConnectorType ret;

  /* The "ConnectorType" property is considered mandatory since RandR 1.3,
   * but none of the FOSS drivers support it, because we're a bunch of
   * professional software developers.
   *
   * Try poking it first, without any expectations that it will work.
   * If it's not there, we thankfully have other bonghits to try next.
   */
  ret = output_get_connector_type_from_prop (output);
  if (ret != META_CONNECTOR_TYPE_Unknown)
    return ret;

  /* Fall back to heuristics based on the output name. */
  ret = output_get_connector_type_from_name (output);
  if (ret != META_CONNECTOR_TYPE_Unknown)
    return ret;

  return META_CONNECTOR_TYPE_Unknown;
}

static void
output_get_tile_info (MetaOutput *output)
{
  MetaGpu *gpu = meta_output_get_gpu (output);
  MetaMonitorManager *monitor_manager = meta_gpu_get_monitor_manager (gpu);
  MetaMonitorManagerXrandr *monitor_manager_xrandr =
    META_MONITOR_MANAGER_XRANDR (monitor_manager);
  Display *xdisplay = xdisplay_from_output (output);
  Atom tile_atom;
  unsigned char *prop;
  unsigned long nitems, bytes_after;
  int actual_format;
  Atom actual_type;

  if (!meta_monitor_manager_xrandr_has_randr15 (monitor_manager_xrandr))
    return;

  tile_atom = XInternAtom (xdisplay, "TILE", FALSE);
  XRRGetOutputProperty (xdisplay,
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


static void
output_get_modes (MetaOutput    *output,
                  XRROutputInfo *xrandr_output)
{
  MetaGpu *gpu = meta_output_get_gpu (output);
  unsigned int i;
  unsigned int n_actual_modes;

  output->modes = g_new0 (MetaCrtcMode *, xrandr_output->nmode);

  n_actual_modes = 0;
  for (i = 0; i < (unsigned int) xrandr_output->nmode; i++)
    {
      GList *l;

      for (l = meta_gpu_get_modes (gpu); l; l = l->next)
        {
          MetaCrtcMode *mode = l->data;

          if (xrandr_output->modes[i] == (XID) mode->mode_id)
            {
              output->modes[n_actual_modes] = mode;
              n_actual_modes += 1;
              break;
            }
        }
    }
  output->n_modes = n_actual_modes;
  if (n_actual_modes > 0)
    output->preferred_mode = output->modes[0];
}

static void
output_get_crtcs (MetaOutput    *output,
                  XRROutputInfo *xrandr_output)
{
  MetaGpu *gpu = meta_output_get_gpu (output);
  unsigned int i;
  unsigned int n_actual_crtcs;
  GList *l;

  output->possible_crtcs = g_new0 (MetaCrtc *, xrandr_output->ncrtc);

  n_actual_crtcs = 0;
  for (i = 0; i < (unsigned int) xrandr_output->ncrtc; i++)
    {
      for (l = meta_gpu_get_crtcs (gpu); l; l = l->next)
        {
          MetaCrtc *crtc = l->data;

          if ((XID) crtc->crtc_id == xrandr_output->crtcs[i])
            {
              output->possible_crtcs[n_actual_crtcs] = crtc;
              n_actual_crtcs += 1;
              break;
            }
        }
    }
  output->n_possible_crtcs = n_actual_crtcs;

  output->crtc = NULL;
  for (l = meta_gpu_get_crtcs (gpu); l; l = l->next)
    {
      MetaCrtc *crtc = l->data;

      if ((XID) crtc->crtc_id == xrandr_output->crtc)
        {
          output->crtc = crtc;
          break;
        }
    }
}

MetaOutput *
meta_create_xrandr_output (MetaGpuXrandr *gpu_xrandr,
                           XRROutputInfo *xrandr_output,
                           RROutput       output_id,
                           RROutput       primary_output)
{
  MetaOutput *output;
  GBytes *edid;
  unsigned int i;

  output = g_object_new (META_TYPE_OUTPUT, NULL);
  output->gpu = META_GPU (gpu_xrandr);
  output->winsys_id = output_id;
  output->name = g_strdup (xrandr_output->name);

  edid = meta_output_xrandr_read_edid (output);
  meta_output_parse_edid (output, edid);
  g_bytes_unref (edid);

  output->width_mm = xrandr_output->mm_width;
  output->height_mm = xrandr_output->mm_height;
  output->subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
  output->hotplug_mode_update = output_get_hotplug_mode_update (output);
  output->suggested_x = output_get_suggested_x (output);
  output->suggested_y = output_get_suggested_y (output);
  output->connector_type = output_get_connector_type (output);

  output_get_tile_info (output);
  output_get_modes (output, xrandr_output);
  output_get_crtcs (output, xrandr_output);

  output->n_possible_clones = xrandr_output->nclone;
  output->possible_clones = g_new0 (MetaOutput *,
                                    output->n_possible_clones);
  /*
   * We can build the list of clones now, because we don't have the list of
   * outputs yet, so temporarily set the pointers to the bare XIDs, and then
   * we'll fix them in a second pass.
   */
  for (i = 0; i < (unsigned int) xrandr_output->nclone; i++)
    {
      output->possible_clones[i] = GINT_TO_POINTER (xrandr_output->clones[i]);
    }

  output->is_primary = ((XID) output->winsys_id == primary_output);
  output->is_presentation = output_get_presentation_xrandr (output);
  output->is_underscanning = output_get_underscanning_xrandr (output);
  output->supports_underscanning =
    output_get_supports_underscanning_xrandr (output);
  output_get_backlight_limits_xrandr (output);

  if (!(output->backlight_min == 0 && output->backlight_max == 0))
    output->backlight = output_get_backlight_xrandr (output);
  else
    output->backlight = -1;

  if (output->n_modes == 0 || output->n_possible_crtcs == 0)
    {
      g_object_unref (output);
      return NULL;
    }
  else
    {
      return output;
    }
}
