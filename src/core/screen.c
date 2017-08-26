/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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

/**
 * SECTION:screen
 * @title: MetaScreen
 * @short_description: Mutter X screen handler
 */

#include <config.h>
#include "screen-private.h"
#include <meta/main.h>
#include "util-private.h"
#include <meta/errors.h>
#include "window-private.h"
#include "frame.h"
#include <meta/prefs.h>
#include "workspace-private.h"
#include "keybindings-private.h"
#include "stack.h"
#include <meta/compositor.h>
#include <meta/meta-enum-types.h>
#include "core.h"
#include "meta-cursor-tracker-private.h"
#include "boxes-private.h"
#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"

#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xcomposite.h>

#include <X11/Xatom.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "x11/meta-x11-display-private.h"
#include "x11/window-x11.h"
#include "x11/xprops.h"

#include "backends/x11/meta-backend-x11.h"
#include "backends/meta-cursor-sprite-xcursor.h"

G_DEFINE_TYPE (MetaScreen, meta_screen, G_TYPE_OBJECT);

static void
meta_screen_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
#if 0
  MetaScreen *screen = META_SCREEN (object);
#endif

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_screen_get_property (GObject      *object,
                          guint         prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
#if 0
  MetaScreen *screen = META_SCREEN (object);
#endif

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_screen_finalize (GObject *object)
{
  /* Actual freeing done in meta_screen_free() for now */
}

static void
meta_screen_class_init (MetaScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = meta_screen_get_property;
  object_class->set_property = meta_screen_set_property;
  object_class->finalize = meta_screen_finalize;
}

static void
meta_screen_init (MetaScreen *screen)
{
}

MetaScreen*
meta_screen_new (MetaDisplay *display,
                 guint32      timestamp)
{
  MetaScreen *screen;
  int number;
  Window xroot = meta_x11_display_get_xroot (display->x11_display);

  number = meta_ui_get_screen_number ();

  meta_verbose ("Trying screen %d on display '%s'\n",
                number, display->x11_display->name);

  screen = g_object_new (META_TYPE_SCREEN, NULL);
  screen->closing = 0;

  screen->display = display;

  meta_verbose ("Added screen %d ('%s') root 0x%lx\n",
                number, display->x11_display->screen_name,
                xroot);

  return screen;
}

void
meta_screen_free (MetaScreen *screen,
                  guint32     timestamp)
{
  screen->closing += 1;

  g_object_unref (screen);
}

/**
 * meta_screen_get_display:
 * @screen: A #MetaScreen
 *
 * Retrieve the display associated with screen.
 *
 * Returns: (transfer none): Display
 */
MetaDisplay *
meta_screen_get_display (MetaScreen *screen)
{
  return screen->display;
}
