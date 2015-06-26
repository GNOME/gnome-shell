/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter X property convenience routines */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat Inc.
 *
 * Some trivial property-unpacking code from Xlib:
 *   Copyright 1987, 1988, 1998  The Open Group
 *   Copyright 1988 by Wyse Technology, Inc., San Jose, Ca,
 *   Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts,
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

/***********************************************************
Copyright 1988 by Wyse Technology, Inc., San Jose, Ca,
Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts,

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL AND WYSE DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL DIGITAL OR WYSE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

/*

Copyright 1987, 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/


#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "xprops.h"
#include <meta/errors.h>
#include "util-private.h"
#include "ui.h"
#include "mutter-Xatomtype.h"
#include "window-private.h"

#include <X11/Xatom.h>
#include <X11/Xlib-xcb.h>

typedef struct
{
  MetaDisplay   *display;
  Window         xwindow;
  Atom           xatom;
  Atom           type;
  int            format;
  unsigned long  n_items;
  unsigned long  bytes_after;
  unsigned char *prop;
} GetPropertyResults;

static gboolean
validate_or_free_results (GetPropertyResults *results,
                          int                 expected_format,
                          Atom                expected_type,
                          gboolean            must_have_items)
{
  char *type_name;
  char *expected_name;
  char *prop_name;
  const char *title;
  const char *res_class;
  const char *res_name;
  MetaWindow *w;

  if (expected_format == results->format &&
      expected_type == results->type &&
      (!must_have_items || results->n_items > 0))
    return TRUE;

  meta_error_trap_push (results->display);
  type_name = XGetAtomName (results->display->xdisplay, results->type);
  expected_name = XGetAtomName (results->display->xdisplay, expected_type);
  prop_name = XGetAtomName (results->display->xdisplay, results->xatom);
  meta_error_trap_pop (results->display);

  w = meta_display_lookup_x_window (results->display, results->xwindow);

  if (w != NULL)
    {
      title = w->title;
      res_class = w->res_class;
      res_name = w->res_name;
    }
  else
    {
      title = NULL;
      res_class = NULL;
      res_name = NULL;
    }

  if (title == NULL)
    title = "unknown";

  if (res_class == NULL)
    res_class = "unknown";

  if (res_name == NULL)
    res_name = "unknown";

  meta_warning ("Window 0x%lx has property %s\nthat was expected to have type %s format %d\nand actually has type %s format %d n_items %d.\nThis is most likely an application bug, not a window manager bug.\nThe window has title=\"%s\" class=\"%s\" name=\"%s\"\n",
                results->xwindow,
                prop_name ? prop_name : "(bad atom)",
                expected_name ? expected_name : "(bad atom)",
                expected_format,
                type_name ? type_name : "(bad atom)",
                results->format, (int) results->n_items,
                title, res_class, res_name);

  if (type_name)
    XFree (type_name);
  if (expected_name)
    XFree (expected_name);
  if (prop_name)
    XFree (prop_name);

  if (results->prop)
    {
      g_free (results->prop);
      results->prop = NULL;
    }

  return FALSE;
}

static xcb_get_property_cookie_t
async_get_property (xcb_connection_t *xcb_conn, Window xwindow,
                    Atom xatom, Atom required_type)
{
  return xcb_get_property (xcb_conn, False, xwindow,
                           xatom, required_type, 0, G_MAXUINT32);
}

static gboolean
async_get_property_finish (xcb_connection_t          *xcb_conn,
                           xcb_get_property_cookie_t  cookie,
                           GetPropertyResults        *results)
{
  xcb_get_property_reply_t *reply;
  xcb_generic_error_t *error;

  reply = xcb_get_property_reply (xcb_conn, cookie, &error);
  if (error)
    {
      free (error);
      return FALSE;
    }

  results->n_items = reply->value_len;
  results->type = reply->type;
  results->bytes_after = reply->bytes_after;
  results->format = reply->format;
  results->prop = NULL;

  if (results->type != None)
    results->prop = g_memdup (xcb_get_property_value (reply),
                              xcb_get_property_value_length (reply));

  free (reply);
  return (results->prop != NULL);
}

static gboolean
get_property (MetaDisplay        *display,
              Window              xwindow,
              Atom                xatom,
              Atom                req_type,
              GetPropertyResults *results)
{
  xcb_get_property_cookie_t cookie;
  xcb_connection_t *xcb_conn = XGetXCBConnection (display->xdisplay);

  results->display = display;
  results->xwindow = xwindow;
  results->xatom = xatom;
  results->prop = NULL;
  results->n_items = 0;
  results->type = None;
  results->bytes_after = 0;
  results->format = 0;

  cookie = async_get_property (xcb_conn, xwindow, xatom, req_type);
  return async_get_property_finish (xcb_conn, cookie, results);
}

static gboolean
atom_list_from_results (GetPropertyResults *results,
                        uint32_t          **atoms_p,
                        int                *n_atoms_p)
{
  if (!validate_or_free_results (results, 32, XA_ATOM, FALSE))
    return FALSE;

  *atoms_p = (uint32_t*) results->prop;
  *n_atoms_p = results->n_items;
  results->prop = NULL;

  return TRUE;
}

static gboolean
cardinal_list_from_results (GetPropertyResults *results,
                            uint32_t          **cardinals_p,
                            int                *n_cardinals_p)
{
  if (!validate_or_free_results (results, 32, XA_CARDINAL, FALSE))
    return FALSE;

  *cardinals_p = (uint32_t *) results->prop;
  *n_cardinals_p = results->n_items;
  results->prop = NULL;

  return TRUE;
}

gboolean
meta_prop_get_cardinal_list (MetaDisplay *display,
                             Window       xwindow,
                             Atom         xatom,
                             uint32_t   **cardinals_p,
                             int         *n_cardinals_p)
{
  GetPropertyResults results;

  *cardinals_p = NULL;
  *n_cardinals_p = 0;

  if (!get_property (display, xwindow, xatom, XA_CARDINAL,
                     &results))
    return FALSE;

  return cardinal_list_from_results (&results, cardinals_p, n_cardinals_p);
}

static gboolean
motif_hints_from_results (GetPropertyResults *results,
                          MotifWmHints      **hints_p)
{
  *hints_p = NULL;

  if (results->type == None || results->n_items <= 0)
    {
      meta_verbose ("Motif hints had unexpected type or n_items\n");
      return FALSE;
    }

  /* The issue here is that some old crufty code will set a smaller
   * MotifWmHints than the one we expect, apparently.  I'm not sure of
   * the history behind it. See bug #89841 for example.
   */
  *hints_p = calloc (1, sizeof (MotifWmHints));
  if (*hints_p == NULL)
    return FALSE;

  memcpy(*hints_p, results->prop, MIN (sizeof (MotifWmHints),
                                       results->n_items * sizeof (uint32_t)));
  return TRUE;
}

gboolean
meta_prop_get_motif_hints (MetaDisplay   *display,
                           Window         xwindow,
                           Atom           xatom,
                           MotifWmHints **hints_p)
{
  GetPropertyResults results;

  *hints_p = NULL;

  if (!get_property (display, xwindow, xatom, AnyPropertyType,
                     &results))
    return FALSE;

  return motif_hints_from_results (&results, hints_p);
}

static gboolean
latin1_string_from_results (GetPropertyResults *results,
                            char              **str_p)
{
  *str_p = NULL;

  if (!validate_or_free_results (results, 8, XA_STRING, FALSE))
    return FALSE;

  *str_p = g_strndup ((char *) results->prop, results->n_items);

  return TRUE;
}

gboolean
meta_prop_get_latin1_string (MetaDisplay *display,
                             Window       xwindow,
                             Atom         xatom,
                             char       **str_p)
{
  GetPropertyResults results;

  *str_p = NULL;

  if (!get_property (display, xwindow, xatom, XA_STRING,
                     &results))
    return FALSE;

  return latin1_string_from_results (&results, str_p);
}

static gboolean
utf8_string_from_results (GetPropertyResults *results,
                          char              **str_p)
{
  *str_p = NULL;

  if (!validate_or_free_results (results, 8,
                                 results->display->atom_UTF8_STRING, FALSE))
    return FALSE;

  if (results->n_items > 0 &&
      !g_utf8_validate ((gchar *)results->prop, results->n_items, NULL))
    {
      char *name;

      name = XGetAtomName (results->display->xdisplay, results->xatom);
      meta_warning ("Property %s on window 0x%lx contained invalid UTF-8\n",
                    name, results->xwindow);
      meta_XFree (name);
      g_free (results->prop);
      results->prop = NULL;

      return FALSE;
    }

  *str_p = g_strndup ((char *) results->prop, results->n_items);

  return TRUE;
}

/* this one freakishly returns g_malloc memory */
static gboolean
utf8_list_from_results (GetPropertyResults *results,
                        char             ***str_p,
                        int                *n_str_p)
{
  int i;
  int n_strings;
  char **retval;
  const char *p;

  *str_p = NULL;
  *n_str_p = 0;

  if (!validate_or_free_results (results, 8,
                                 results->display->atom_UTF8_STRING, FALSE))
    return FALSE;

  /* I'm not sure this is right, but I'm guessing the
   * property is nul-separated
   */
  i = 0;
  n_strings = 0;
  while (i < (int) results->n_items)
    {
      if (results->prop[i] == '\0')
        ++n_strings;
      ++i;
    }

  if (results->prop[results->n_items - 1] != '\0')
    ++n_strings;

  /* we're guaranteed that results->prop has a nul on the end
   * by XGetWindowProperty
   */

  retval = g_new0 (char*, n_strings + 1);

  p = (char *)results->prop;
  i = 0;
  while (i < n_strings)
    {
      if (!g_utf8_validate (p, -1, NULL))
        {
          char *name;

          meta_error_trap_push (results->display);
          name = XGetAtomName (results->display->xdisplay, results->xatom);
          meta_error_trap_pop (results->display);
          meta_warning ("Property %s on window 0x%lx contained invalid UTF-8 for item %d in the list\n",
                        name, results->xwindow, i);
          meta_XFree (name);
          g_free (results->prop);
          results->prop = NULL;

          g_strfreev (retval);
          return FALSE;
        }

      retval[i] = g_strdup (p);

      p = p + strlen (p) + 1;
      ++i;
    }

  *str_p = retval;
  *n_str_p = i;

  g_free (results->prop);
  results->prop = NULL;

  return TRUE;
}

/* returns g_malloc not Xmalloc memory */
gboolean
meta_prop_get_utf8_list (MetaDisplay   *display,
                         Window         xwindow,
                         Atom           xatom,
                         char        ***str_p,
                         int           *n_str_p)
{
  GetPropertyResults results;

  *str_p = NULL;

  if (!get_property (display, xwindow, xatom,
                     display->atom_UTF8_STRING,
                     &results))
    return FALSE;

  return utf8_list_from_results (&results, str_p, n_str_p);
}

void
meta_prop_set_utf8_string_hint (MetaDisplay *display,
                                Window xwindow,
                                Atom atom,
                                const char *val)
{
  meta_error_trap_push (display);
  XChangeProperty (display->xdisplay,
                   xwindow, atom,
                   display->atom_UTF8_STRING,
                   8, PropModeReplace, (guchar*) val, strlen (val));
  meta_error_trap_pop (display);
}

static gboolean
window_from_results (GetPropertyResults *results,
                     Window             *window_p)
{
  if (!validate_or_free_results (results, 32, XA_WINDOW, TRUE))
    return FALSE;

  *window_p = *(Window*) results->prop;
  g_free (results->prop);
  results->prop = NULL;

  return TRUE;
}

static gboolean
counter_from_results (GetPropertyResults *results,
                      XSyncCounter       *counter_p)
{
  if (!validate_or_free_results (results, 32,
                                 XA_CARDINAL,
                                 TRUE))
    return FALSE;

  *counter_p = *(XSyncCounter*) results->prop;
  g_free (results->prop);
  results->prop = NULL;

  return TRUE;
}

static gboolean
counter_list_from_results (GetPropertyResults *results,
                           uint32_t          **counters_p,
                           int                *n_counters_p)
{
  if (!validate_or_free_results (results, 32,
                                 XA_CARDINAL,
                                 FALSE))
    return FALSE;

  *counters_p = (uint32_t *) results->prop;
  *n_counters_p = results->n_items;
  results->prop = NULL;

  return TRUE;
}

gboolean
meta_prop_get_window (MetaDisplay *display,
                      Window       xwindow,
                      Atom         xatom,
                      Window      *window_p)
{
  GetPropertyResults results;

  *window_p = None;

  if (!get_property (display, xwindow, xatom, XA_WINDOW,
                     &results))
    return FALSE;

  return window_from_results (&results, window_p);
}

gboolean
meta_prop_get_cardinal (MetaDisplay   *display,
                        Window         xwindow,
                        Atom           xatom,
                        uint32_t      *cardinal_p)
{
  return meta_prop_get_cardinal_with_atom_type (display, xwindow, xatom,
                                                XA_CARDINAL, cardinal_p);
}

static gboolean
cardinal_with_atom_type_from_results (GetPropertyResults *results,
                                      Atom                prop_type,
                                      uint32_t           *cardinal_p)
{
  if (!validate_or_free_results (results, 32, prop_type, TRUE))
    return FALSE;

  *cardinal_p = *((uint32_t *) results->prop);
  g_free (results->prop);
  results->prop = NULL;

  return TRUE;
}

gboolean
meta_prop_get_cardinal_with_atom_type (MetaDisplay   *display,
                                       Window         xwindow,
                                       Atom           xatom,
                                       Atom           prop_type,
                                       uint32_t      *cardinal_p)
{
  GetPropertyResults results;

  *cardinal_p = 0;

  if (!get_property (display, xwindow, xatom, prop_type,
                     &results))
    return FALSE;

  return cardinal_with_atom_type_from_results (&results, prop_type, cardinal_p);
}

static char *
text_property_to_utf8 (Display *xdisplay,
                       const XTextProperty *prop)
{
  char *ret = NULL;
  char **local_list = NULL;
  int count = 0;
  int res;

  res = XmbTextPropertyToTextList (xdisplay, prop, &local_list, &count);
  if (res == XNoMemory || res == XLocaleNotSupported || res == XConverterNotFound)
    goto out;

  if (count == 0)
    goto out;

  ret = g_strdup (local_list[0]);

 out:
  XFreeStringList (local_list);
  return ret;
}

static gboolean
text_property_from_results (GetPropertyResults *results,
                            char              **utf8_str_p)
{
  XTextProperty tp;

  *utf8_str_p = NULL;

  tp.value = results->prop;
  results->prop = NULL;
  tp.encoding = results->type;
  tp.format = results->format;
  tp.nitems = results->n_items;

  *utf8_str_p = text_property_to_utf8 (results->display->xdisplay, &tp);

  if (tp.value != NULL)
    XFree (tp.value);

  return *utf8_str_p != NULL;
}

static gboolean
wm_hints_from_results (GetPropertyResults *results,
                       XWMHints          **hints_p)
{
  XWMHints *hints;
  xPropWMHints *raw;

  *hints_p = NULL;

  if (!validate_or_free_results (results, 32, XA_WM_HINTS, TRUE))
    return FALSE;

  /* pre-R3 bogusly truncated window_group, don't fail on them */
  if (results->n_items < (NumPropWMHintsElements - 1))
    {
      meta_verbose ("WM_HINTS property too short: %d should be %d\n",
                    (int) results->n_items, NumPropWMHintsElements - 1);
      if (results->prop)
        {
          g_free (results->prop);
          results->prop = NULL;
        }
      return FALSE;
    }

  hints = calloc (1, sizeof (XWMHints));

  raw = (xPropWMHints*) results->prop;

  hints->flags = raw->flags;
  hints->input = (raw->input ? True : False);
  hints->initial_state = raw->initialState;
  hints->icon_pixmap = raw->iconPixmap;
  hints->icon_window = raw->iconWindow;
  hints->icon_x = raw->iconX;
  hints->icon_y = raw->iconY;
  hints->icon_mask = raw->iconMask;
  if (results->n_items >= NumPropWMHintsElements)
    hints->window_group = raw->windowGroup;
  else
    hints->window_group = 0;

  if (results->prop)
    {
      g_free (results->prop);
      results->prop = NULL;
    }

  *hints_p = hints;

  return TRUE;
}

static gboolean
class_hint_from_results (GetPropertyResults *results,
                         XClassHint         *class_hint)
{
  int len_name, len_class;

  class_hint->res_class = NULL;
  class_hint->res_name = NULL;

  if (!validate_or_free_results (results, 8, XA_STRING, FALSE))
    return FALSE;

  len_name = strlen ((char *) results->prop);
  if (! (class_hint->res_name = malloc (len_name+1)))
    {
      g_free (results->prop);
      results->prop = NULL;
      return FALSE;
    }

  strcpy (class_hint->res_name, (char *)results->prop);

  if (len_name == (int) results->n_items)
    len_name--;

  len_class = strlen ((char *)results->prop + len_name + 1);

  if (! (class_hint->res_class = malloc(len_class+1)))
    {
      XFree(class_hint->res_name);
      class_hint->res_name = NULL;
      g_free (results->prop);
      results->prop = NULL;
      return FALSE;
    }

  strcpy (class_hint->res_class, (char *)results->prop + len_name + 1);

  g_free (results->prop);
  results->prop = NULL;

  return TRUE;
}

static gboolean
size_hints_from_results (GetPropertyResults *results,
                         XSizeHints        **hints_p,
                         gulong             *flags_p)
{
  xPropSizeHints *raw;
  XSizeHints *hints;

  *hints_p = NULL;
  *flags_p = 0;

  if (!validate_or_free_results (results, 32, XA_WM_SIZE_HINTS, FALSE))
    return FALSE;

  if (results->n_items < OldNumPropSizeElements)
    return FALSE;

  raw = (xPropSizeHints*) results->prop;

  hints = malloc (sizeof (XSizeHints));

  hints->flags = raw->flags;
  hints->x = raw->x;
  hints->y = raw->y;
  hints->width = raw->width;
  hints->height = raw->height;
  hints->min_width  = raw->minWidth;
  hints->min_height = raw->minHeight;
  hints->max_width  = raw->maxWidth;
  hints->max_height = raw->maxHeight;
  hints->width_inc  = raw->widthInc;
  hints->height_inc = raw->heightInc;
  hints->min_aspect.x = raw->minAspectX;
  hints->min_aspect.y = raw->minAspectY;
  hints->max_aspect.x = raw->maxAspectX;
  hints->max_aspect.y = raw->maxAspectY;

  *flags_p = (USPosition | USSize | PAllHints);
  if (results->n_items >= NumPropSizeElements)
    {
      hints->base_width = raw->baseWidth;
      hints->base_height = raw->baseHeight;
      hints->win_gravity = raw->winGravity;
      *flags_p |= (PBaseSize | PWinGravity);
    }

  hints->flags &= (*flags_p);	/* get rid of unwanted bits */

  g_free (results->prop);
  results->prop = NULL;

  *hints_p = hints;

  return TRUE;
}

static char*
latin1_to_utf8 (const char *text)
{
  GString *str;
  const char *p;

  str = g_string_new ("");

  p = text;
  while (*p)
    {
      g_string_append_unichar (str, *p);
      ++p;
    }

  return g_string_free (str, FALSE);
}

void
meta_prop_get_values (MetaDisplay   *display,
                      Window         xwindow,
                      MetaPropValue *values,
                      int            n_values)
{
  int i;
  xcb_get_property_cookie_t *tasks;
  xcb_connection_t *xcb_conn = XGetXCBConnection (display->xdisplay);

  meta_verbose ("Requesting %d properties of 0x%lx at once\n",
                n_values, xwindow);

  if (n_values == 0)
    return;

  tasks = g_new0 (xcb_get_property_cookie_t, n_values);

  /* Start up tasks. The "values" array can have values
   * with atom == None, which means to ignore that element.
   */
  i = 0;
  while (i < n_values)
    {
      if (values[i].required_type == None)
        {
          switch (values[i].type)
            {
            case META_PROP_VALUE_INVALID:
              /* This means we don't really want a value, e.g. got
               * property notify on an atom we don't care about.
               */
              if (values[i].atom != None)
                meta_bug ("META_PROP_VALUE_INVALID requested in %s\n", G_STRFUNC);
              break;
            case META_PROP_VALUE_UTF8_LIST:
            case META_PROP_VALUE_UTF8:
              values[i].required_type = display->atom_UTF8_STRING;
              break;
            case META_PROP_VALUE_STRING:
            case META_PROP_VALUE_STRING_AS_UTF8:
              values[i].required_type = XA_STRING;
              break;
            case META_PROP_VALUE_MOTIF_HINTS:
              values[i].required_type = AnyPropertyType;
              break;
            case META_PROP_VALUE_CARDINAL_LIST:
            case META_PROP_VALUE_CARDINAL:
              values[i].required_type = XA_CARDINAL;
              break;
            case META_PROP_VALUE_WINDOW:
              values[i].required_type = XA_WINDOW;
              break;
            case META_PROP_VALUE_ATOM_LIST:
              values[i].required_type = XA_ATOM;
              break;
            case META_PROP_VALUE_TEXT_PROPERTY:
              values[i].required_type = AnyPropertyType;
              break;
            case META_PROP_VALUE_WM_HINTS:
              values[i].required_type = XA_WM_HINTS;
              break;
            case META_PROP_VALUE_CLASS_HINT:
              values[i].required_type = XA_STRING;
              break;
            case META_PROP_VALUE_SIZE_HINTS:
              values[i].required_type = XA_WM_SIZE_HINTS;
              break;
            case META_PROP_VALUE_SYNC_COUNTER:
            case META_PROP_VALUE_SYNC_COUNTER_LIST:
	      values[i].required_type = XA_CARDINAL;
              break;
            }
        }

      if (values[i].atom != None)
        tasks[i] = async_get_property (xcb_conn, xwindow, values[i].atom, values[i].required_type);
      ++i;
    }

  /* Get replies for all our tasks */
  meta_topic (META_DEBUG_SYNC, "Syncing to get %d GetProperty replies in %s\n",
              n_values, G_STRFUNC);
  XSync (display->xdisplay, False);

  /* Collect results, should arrive in order requested */
  i = 0;
  while (i < n_values)
    {
      GetPropertyResults results;

      /* We're relying on the fact that sequence numbers can never be zero
       * in Xorg. This is a bit disgusting... */
      if (tasks[i].sequence == 0)
        {
          /* Probably values[i].type was None, or ag_task_create()
           * returned NULL.
           */
          values[i].type = META_PROP_VALUE_INVALID;
          goto next;
        }

      results.display = display;
      results.xwindow = xwindow;
      results.xatom = values[i].atom;
      results.prop = NULL;
      results.n_items = 0;
      results.type = None;
      results.bytes_after = 0;
      results.format = 0;

      if (!async_get_property_finish (xcb_conn, tasks[i], &results))
        {
          values[i].type = META_PROP_VALUE_INVALID;
          goto next;
        }

      switch (values[i].type)
        {
        case META_PROP_VALUE_INVALID:
          g_assert_not_reached ();
          break;
        case META_PROP_VALUE_UTF8_LIST:
          if (!utf8_list_from_results (&results,
                                       &values[i].v.string_list.strings,
                                       &values[i].v.string_list.n_strings))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_UTF8:
          if (!utf8_string_from_results (&results,
                                         &values[i].v.str))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_STRING:
          if (!latin1_string_from_results (&results,
                                           &values[i].v.str))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_STRING_AS_UTF8:
          if (!latin1_string_from_results (&results,
                                           &values[i].v.str))
            values[i].type = META_PROP_VALUE_INVALID;
          else
            {
              char *new_str;
              new_str = latin1_to_utf8 (values[i].v.str);
              free (values[i].v.str);
              values[i].v.str = new_str;
            }
          break;
        case META_PROP_VALUE_MOTIF_HINTS:
          if (!motif_hints_from_results (&results,
                                         &values[i].v.motif_hints))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_CARDINAL_LIST:
          if (!cardinal_list_from_results (&results,
                                           &values[i].v.cardinal_list.cardinals,
                                           &values[i].v.cardinal_list.n_cardinals))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_CARDINAL:
          if (!cardinal_with_atom_type_from_results (&results,
                                                     values[i].required_type,
                                                     &values[i].v.cardinal))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_WINDOW:
          if (!window_from_results (&results,
                                    &values[i].v.xwindow))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_ATOM_LIST:
          if (!atom_list_from_results (&results,
                                       &values[i].v.atom_list.atoms,
                                       &values[i].v.atom_list.n_atoms))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_TEXT_PROPERTY:
          if (!text_property_from_results (&results, &values[i].v.str))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_WM_HINTS:
          if (!wm_hints_from_results (&results, &values[i].v.wm_hints))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_CLASS_HINT:
          if (!class_hint_from_results (&results, &values[i].v.class_hint))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_SIZE_HINTS:
          if (!size_hints_from_results (&results,
                                        &values[i].v.size_hints.hints,
                                        &values[i].v.size_hints.flags))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_SYNC_COUNTER:
          if (!counter_from_results (&results,
                                     &values[i].v.xcounter))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        case META_PROP_VALUE_SYNC_COUNTER_LIST:
          if (!counter_list_from_results (&results,
                                          &values[i].v.xcounter_list.counters,
                                          &values[i].v.xcounter_list.n_counters))
            values[i].type = META_PROP_VALUE_INVALID;
          break;
        }

    next:
      ++i;
    }

  g_free (tasks);
}

static void
free_value (MetaPropValue *value)
{
  switch (value->type)
    {
    case META_PROP_VALUE_INVALID:
      break;
    case META_PROP_VALUE_UTF8:
    case META_PROP_VALUE_STRING:
      free (value->v.str);
      break;
    case META_PROP_VALUE_STRING_AS_UTF8:
      g_free (value->v.str);
      break;
    case META_PROP_VALUE_MOTIF_HINTS:
      free (value->v.motif_hints);
      break;
    case META_PROP_VALUE_CARDINAL:
      break;
    case META_PROP_VALUE_WINDOW:
      break;
    case META_PROP_VALUE_ATOM_LIST:
      free (value->v.atom_list.atoms);
      break;
    case META_PROP_VALUE_TEXT_PROPERTY:
      free (value->v.str);
      break;
    case META_PROP_VALUE_WM_HINTS:
      free (value->v.wm_hints);
      break;
    case META_PROP_VALUE_CLASS_HINT:
      free (value->v.class_hint.res_class);
      free (value->v.class_hint.res_name);
      break;
    case META_PROP_VALUE_SIZE_HINTS:
      free (value->v.size_hints.hints);
      break;
    case META_PROP_VALUE_UTF8_LIST:
      g_strfreev (value->v.string_list.strings);
      break;
    case META_PROP_VALUE_CARDINAL_LIST:
      free (value->v.cardinal_list.cardinals);
      break;
    case META_PROP_VALUE_SYNC_COUNTER:
      break;
    case META_PROP_VALUE_SYNC_COUNTER_LIST:
      free (value->v.xcounter_list.counters);
      break;
    }
}

void
meta_prop_free_values (MetaPropValue *values,
                       int            n_values)
{
  int i;

  i = 0;
  while (i < n_values)
    {
      free_value (&values[i]);
      ++i;
    }

  /* Zero the whole thing to quickly detect breakage */
  memset (values, '\0', sizeof (MetaPropValue) * n_values);
}
