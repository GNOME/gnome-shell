/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Alt-Tab abstraction */

/*
 * Copyright (C) 2009 Red Hat, Inc.
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

#include <config.h>
#include "alttabhandlerdefault.h"
#include "screen-private.h"

static GType handler_type = G_TYPE_INVALID;

GType meta_alt_tab_handler_default_get_type (void);

void
meta_alt_tab_handler_register (GType type)
{
  handler_type = type;
}

MetaAltTabHandler *
meta_alt_tab_handler_new (MetaScreen *screen,
                          gboolean    immediate)
{
  if (handler_type == G_TYPE_INVALID)
    handler_type = meta_alt_tab_handler_default_get_type ();

  return g_object_new (handler_type,
                       "screen", screen,
                       "immediate", immediate,
                       NULL);
}

static void meta_alt_tab_handler_class_init (GObjectClass *object_class);

GType
meta_alt_tab_handler_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      const GTypeInfo type_info =
      {
        sizeof (MetaAltTabHandlerInterface), /* class_size */
	NULL,           /* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc)meta_alt_tab_handler_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };
      GType g_define_type_id =
	g_type_register_static (G_TYPE_INTERFACE, "MetaAltTabHandler",
				&type_info, 0);

      g_type_interface_add_prerequisite (g_define_type_id, G_TYPE_OBJECT);

      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

static void
meta_alt_tab_handler_class_init (GObjectClass *object_class)
{
  g_object_interface_install_property (object_class,
                                       g_param_spec_object ("screen",
                                                            "Screen",
                                                            "MetaScreen this is the switcher for",
                                                            META_TYPE_SCREEN,
                                                            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_interface_install_property (object_class,
                                       g_param_spec_boolean ("immediate",
                                                             "Immediate mode",
                                                             "Whether or not to select windows immediately",
                                                             FALSE,
                                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

void
meta_alt_tab_handler_add_window (MetaAltTabHandler *handler,
				 MetaWindow        *window)
{
  META_ALT_TAB_HANDLER_GET_INTERFACE (handler)->add_window (handler, window);
}

void
meta_alt_tab_handler_show (MetaAltTabHandler *handler,
                           MetaWindow        *initial_selection)
{
  META_ALT_TAB_HANDLER_GET_INTERFACE (handler)->show (handler,
                                                      initial_selection);
}

void
meta_alt_tab_handler_destroy (MetaAltTabHandler *handler)
{
  META_ALT_TAB_HANDLER_GET_INTERFACE (handler)->destroy (handler);
}

void
meta_alt_tab_handler_forward (MetaAltTabHandler *handler)
{
  META_ALT_TAB_HANDLER_GET_INTERFACE (handler)->forward (handler);
}

void
meta_alt_tab_handler_backward (MetaAltTabHandler *handler)
{
  META_ALT_TAB_HANDLER_GET_INTERFACE (handler)->backward (handler);
}

MetaWindow *
meta_alt_tab_handler_get_selected (MetaAltTabHandler *handler)
{
  return META_ALT_TAB_HANDLER_GET_INTERFACE (handler)->get_selected (handler);
}
