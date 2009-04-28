/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Alt-Tab abstraction: default implementation */

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
#include "frame-private.h"
#include "window-private.h"

static void meta_alt_tab_handler_default_interface_init (MetaAltTabHandlerInterface *handler_iface);

G_DEFINE_TYPE_WITH_CODE (MetaAltTabHandlerDefault, meta_alt_tab_handler_default, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (META_TYPE_ALT_TAB_HANDLER,
                                                meta_alt_tab_handler_default_interface_init))

enum {
  PROP_SCREEN = 1,
  PROP_IMMEDIATE
};

static void
meta_alt_tab_handler_default_init (MetaAltTabHandlerDefault *hd)
{
  hd->entries = g_array_new (FALSE, FALSE, sizeof (MetaTabEntry));
}

static void
meta_alt_tab_handler_default_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  MetaAltTabHandlerDefault *hd = META_ALT_TAB_HANDLER_DEFAULT (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      hd->screen = g_value_get_object (value);
      break;
    case PROP_IMMEDIATE:
      hd->immediate_mode = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_alt_tab_handler_default_finalize (GObject *object)
{
  MetaAltTabHandlerDefault *hd = META_ALT_TAB_HANDLER_DEFAULT (object);

  g_array_free (hd->entries, TRUE);

  if (hd->tab_popup)
    meta_ui_tab_popup_free (hd->tab_popup);

  G_OBJECT_CLASS (meta_alt_tab_handler_default_parent_class)->finalize (object);
}

static void
meta_alt_tab_handler_default_add_window (MetaAltTabHandler *handler,
                                         MetaWindow        *window)
{
  MetaAltTabHandlerDefault *hd = META_ALT_TAB_HANDLER_DEFAULT (handler);
  MetaTabEntry entry;
  MetaRectangle r;

  entry.key = (MetaTabEntryKey) window;
  entry.title = window->title;
  entry.icon = window->icon;
  entry.blank = FALSE;
  entry.hidden = !meta_window_showing_on_its_workspace (window);
  entry.demands_attention = window->wm_state_demands_attention;

  if (hd->immediate_mode || !entry.hidden ||
      !meta_window_get_icon_geometry (window, &r))
    meta_window_get_outer_rect (window, &r);
  entry.rect = r;

  /* Find inside of highlight rectangle to be used when window is
   * outlined for tabbing.  This should be the size of the
   * east/west frame, and the size of the south frame, on those
   * sides.  On the top it should be the size of the south frame
   * edge.
   */
#define OUTLINE_WIDTH 5
  /* Top side */
  if (!entry.hidden &&
      window->frame && window->frame->bottom_height > 0 &&
      window->frame->child_y >= window->frame->bottom_height)
    entry.inner_rect.y = window->frame->bottom_height;
  else
    entry.inner_rect.y = OUTLINE_WIDTH;

  /* Bottom side */
  if (!entry.hidden &&
      window->frame && window->frame->bottom_height != 0)
    entry.inner_rect.height = r.height
      - entry.inner_rect.y - window->frame->bottom_height;
  else
    entry.inner_rect.height = r.height
      - entry.inner_rect.y - OUTLINE_WIDTH;

  /* Left side */
  if (!entry.hidden && window->frame && window->frame->child_x != 0)
    entry.inner_rect.x = window->frame->child_x;
  else
    entry.inner_rect.x = OUTLINE_WIDTH;

  /* Right side */
  if (!entry.hidden &&
      window->frame && window->frame->right_width != 0)
    entry.inner_rect.width = r.width
      - entry.inner_rect.x - window->frame->right_width;
  else
    entry.inner_rect.width = r.width
      - entry.inner_rect.x - OUTLINE_WIDTH;

  g_array_append_val (hd->entries, entry);
}

static void
meta_alt_tab_handler_default_show (MetaAltTabHandler *handler,
                                   MetaWindow        *initial_selection)
{
  MetaAltTabHandlerDefault *hd = META_ALT_TAB_HANDLER_DEFAULT (handler);

  if (hd->tab_popup)
    return;

  hd->tab_popup = meta_ui_tab_popup_new ((MetaTabEntry *)hd->entries->data,
                                         hd->screen->number,
                                         hd->entries->len,
                                         5, /* FIXME */
                                         TRUE);
  meta_ui_tab_popup_select (hd->tab_popup, (MetaTabEntryKey) initial_selection);
  meta_ui_tab_popup_set_showing (hd->tab_popup, !hd->immediate_mode);
}

static void
meta_alt_tab_handler_default_destroy (MetaAltTabHandler *handler)
{
  MetaAltTabHandlerDefault *hd = META_ALT_TAB_HANDLER_DEFAULT (handler);

  if (hd->tab_popup)
    {
      meta_ui_tab_popup_free (hd->tab_popup);
      hd->tab_popup = NULL;
    }
}

static void
meta_alt_tab_handler_default_forward (MetaAltTabHandler *handler)
{
  MetaAltTabHandlerDefault *hd = META_ALT_TAB_HANDLER_DEFAULT (handler);

  if (hd->tab_popup)
    meta_ui_tab_popup_forward (hd->tab_popup);
}

static void
meta_alt_tab_handler_default_backward (MetaAltTabHandler *handler)
{
  MetaAltTabHandlerDefault *hd = META_ALT_TAB_HANDLER_DEFAULT (handler);

  if (hd->tab_popup)
    meta_ui_tab_popup_backward (hd->tab_popup);
}

static MetaWindow *
meta_alt_tab_handler_default_get_selected (MetaAltTabHandler *handler)
{
  MetaAltTabHandlerDefault *hd = META_ALT_TAB_HANDLER_DEFAULT (handler);

  if (hd->tab_popup)
    return (MetaWindow *)meta_ui_tab_popup_get_selected (hd->tab_popup);
  else
    return NULL;
}

static void
meta_alt_tab_handler_default_class_init (MetaAltTabHandlerDefaultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = meta_alt_tab_handler_default_set_property;
  object_class->finalize     = meta_alt_tab_handler_default_finalize;

  g_object_class_override_property (object_class, PROP_SCREEN, "screen");
  g_object_class_override_property (object_class, PROP_IMMEDIATE, "immediate");
}

static void
meta_alt_tab_handler_default_interface_init (MetaAltTabHandlerInterface *handler_iface)
{
  handler_iface->add_window   = meta_alt_tab_handler_default_add_window;
  handler_iface->show         = meta_alt_tab_handler_default_show;
  handler_iface->destroy      = meta_alt_tab_handler_default_destroy;
  handler_iface->forward      = meta_alt_tab_handler_default_forward;
  handler_iface->backward     = meta_alt_tab_handler_default_backward;
  handler_iface->get_selected = meta_alt_tab_handler_default_get_selected;
}
