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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include <clutter/clutter.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

#include "monitor-private.h"

struct _MetaMonitorManager
{
  GObject parent_instance;

  /* Outputs refers to physical screens,
     while monitor_infos refer to logical ones (aka CRTC)
     They can be different if two outputs are
     in clone mode
  */
  MetaOutput *outputs;
  int primary_monitor_index;
  int n_outputs;

  MetaMonitorInfo *monitor_infos;
  int n_monitor_infos;

#ifdef HAVE_RANDR
  Display *xdisplay;
#endif
};

struct _MetaMonitorManagerClass
{
  GObjectClass parent_class;
};

enum {
  MONITORS_CHANGED,
  SIGNALS_LAST
};

static int signals[SIGNALS_LAST];

G_DEFINE_TYPE (MetaMonitorManager, meta_monitor_manager, G_TYPE_OBJECT);

static void
make_dummy_monitor_config (MetaMonitorManager *manager)
{
  manager->monitor_infos = g_new0 (MetaMonitorInfo, 1);
  manager->n_monitor_infos = 1;

  manager->monitor_infos[0].number = 0;
  manager->monitor_infos[0].xinerama_index = 0;
  manager->monitor_infos[0].rect.x = 0;
  manager->monitor_infos[0].rect.y = 0;
  if (manager->xdisplay)
    {
      Screen *screen = ScreenOfDisplay (manager->xdisplay,
                                        DefaultScreen (manager->xdisplay));

      manager->monitor_infos[0].rect.width = WidthOfScreen (screen);
      manager->monitor_infos[0].rect.height = HeightOfScreen (screen);
    }
  else
    {
      manager->monitor_infos[0].rect.width = 1024;
      manager->monitor_infos[0].rect.height = 768;
    }
  manager->monitor_infos[0].refresh_rate = 60.0f;
  manager->monitor_infos[0].is_primary = TRUE;
  manager->monitor_infos[0].in_fullscreen = -1;
  manager->monitor_infos[0].output_id = 1;

  manager->outputs = g_new0 (MetaOutput, 1);
  manager->n_outputs = 1;

  manager->outputs[0].monitor = &manager->monitor_infos[0];
  manager->outputs[0].name = g_strdup ("LVDS");
  manager->outputs[0].width_mm = 222;
  manager->outputs[0].height_mm = 125;
  manager->outputs[0].subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
}

#ifdef HAVE_RANDR

/* In the case of multiple outputs of a single crtc (mirroring), we consider one of the
 * outputs the "main". This is the one we consider "owning" the windows, so if
 * the mirroring is changed to a dual monitor setup then the windows are moved to the
 * crtc that now has that main output. If one of the outputs is the primary that is
 * always the main, otherwise we just use the first.
 */
static void
find_main_output_for_crtc (MetaMonitorManager         *manager,
                           XRRScreenResources *resources,
                           XRRCrtcInfo        *crtc,
                           MetaMonitorInfo    *info,
                           GArray             *outputs)
{
  XRROutputInfo *output;
  RROutput primary_output;
  int i;

  primary_output = XRRGetOutputPrimary (manager->xdisplay,
                                        DefaultRootWindow (manager->xdisplay));

  for (i = 0; i < crtc->noutput; i++)
    {
      output = XRRGetOutputInfo (manager->xdisplay, resources, crtc->outputs[i]);

      if (output->connection != RR_Disconnected)
        {
          MetaOutput meta_output;

          meta_output.name = g_strdup (output->name);
          meta_output.width_mm = output->mm_width;
          meta_output.height_mm = output->mm_height;
          meta_output.subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;

          g_array_append_val (outputs, meta_output);

          if (crtc->outputs[i] == primary_output)
            {
              info->output_id = crtc->outputs[i];
              info->is_primary = TRUE;
              manager->primary_monitor_index = info->number;
            }
          else if (info->output_id == 0)
            {
              info->output_id = crtc->outputs[i];
            }
        }

      XRRFreeOutputInfo (output);
    }
}

static void
read_monitor_infos_from_xrandr (MetaMonitorManager *manager)
{
    XRRScreenResources *resources;
    GArray *outputs;
    int i, j;

    resources = XRRGetScreenResourcesCurrent (manager->xdisplay,
                                              DefaultRootWindow (manager->xdisplay));
    if (!resources)
      return make_dummy_monitor_config (manager);

    outputs = g_array_new (FALSE, TRUE, sizeof (MetaOutput));

    manager->n_outputs = 0;
    manager->n_monitor_infos = resources->ncrtc;

    manager->monitor_infos = g_new0 (MetaMonitorInfo, manager->n_monitor_infos);

    for (i = 0; i < resources->ncrtc; i++)
      {
        XRRCrtcInfo *crtc;
        MetaMonitorInfo *info;

        crtc = XRRGetCrtcInfo (manager->xdisplay, resources, resources->crtcs[i]);

        info = &manager->monitor_infos[i];

        info->number = i;
        info->rect.x = crtc->x;
        info->rect.y = crtc->y;
        info->rect.width = crtc->width;
        info->rect.height = crtc->height;
        info->in_fullscreen = -1;

        for (j = 0; j < resources->nmode; j++)
          {
            if (resources->modes[j].id == crtc->mode)
              info->refresh_rate = (resources->modes[j].dotClock /
                                    ((float)resources->modes[j].hTotal *
                                     resources->modes[j].vTotal));
          }

        find_main_output_for_crtc (manager, resources, crtc, info, outputs);

        XRRFreeCrtcInfo (crtc);
      }

    manager->n_outputs = outputs->len;
    manager->outputs = (void*)g_array_free (outputs, FALSE);

    XRRFreeScreenResources (resources);
}

#endif

/*
 * meta_has_dummy_output:
 *
 * Returns TRUE if the only available monitor is the dummy one
 * backing the ClutterStage window.
 */
static gboolean
has_dummy_output (void)
{
  return FALSE;
}

static void
meta_monitor_manager_init (MetaMonitorManager *manager)
{
}

static void
read_current_config (MetaMonitorManager *manager)
{
  if (has_dummy_output ())
    return make_dummy_monitor_config (manager);

#ifdef HAVE_RANDR
  return read_monitor_infos_from_xrandr (manager);
#endif
}

static MetaMonitorManager *
meta_monitor_manager_new (Display *display)
{
  MetaMonitorManager *manager;

  manager = g_object_new (META_TYPE_MONITOR_MANAGER, NULL);

  manager->xdisplay = display;

  read_current_config (manager);
  return manager;
}

static void
free_output_array (MetaOutput *old_outputs,
                   int         n_old_outputs)
{
  int i;

  for (i = 0; i < n_old_outputs; i++)
    g_free (old_outputs[i].name);
  g_free (old_outputs);
}

static void
meta_monitor_manager_finalize (GObject *object)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (object);

  free_output_array (manager->outputs, manager->n_outputs);
  g_free (manager->monitor_infos);

  G_OBJECT_CLASS (meta_monitor_manager_parent_class)->finalize (object);
}

static void
meta_monitor_manager_class_init (MetaMonitorManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_monitor_manager_finalize;

  signals[MONITORS_CHANGED] =
    g_signal_new ("monitors-changed",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  0,
                  NULL, NULL, NULL,
		  G_TYPE_NONE, 0);
}

static MetaMonitorManager *global_manager;

void
meta_monitor_manager_initialize (Display *display)
{
  global_manager = meta_monitor_manager_new (display);
}

MetaMonitorManager *
meta_monitor_manager_get (void)
{
  g_assert (global_manager != NULL);

  return global_manager;
}

MetaMonitorInfo *
meta_monitor_manager_get_monitor_infos (MetaMonitorManager *manager,
                                        int                *n_infos)
{
  *n_infos = manager->n_monitor_infos;
  return manager->monitor_infos;
}

MetaOutput *
meta_monitor_manager_get_outputs (MetaMonitorManager *manager,
                                  int                *n_outputs)
{
  *n_outputs = manager->n_outputs;
  return manager->outputs;
}

int
meta_monitor_manager_get_primary_index (MetaMonitorManager *manager)
{
  return manager->primary_monitor_index;
}

void
meta_monitor_manager_invalidate (MetaMonitorManager *manager)
{
  MetaOutput *old_outputs;
  MetaMonitorInfo *old_monitor_infos;
  int n_old_outputs;

  /* Save the old monitor infos, so they stay valid during the update */
  old_outputs = manager->outputs;
  n_old_outputs = manager->n_outputs;
  old_monitor_infos = manager->monitor_infos;

  read_current_config (manager);

  g_signal_emit (manager, signals[MONITORS_CHANGED], 0);

  g_free (old_monitor_infos);
  free_output_array (old_outputs, n_old_outputs);
}

