/* Metacity X property convenience routines */

/* 
 * Copyright (C) 2001 Havoc Pennington
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
#include "xprops.h"
#include "errors.h"
#include "util.h"
#include <X11/Xatom.h>

static gboolean
check_type_and_format (MetaDisplay *display,
                       Window       xwindow,
                       Atom         xatom,
                       int          expected_format,
                       Atom         expected_type,
                       int          n_items, /* -1 to not check this */
                       int          format,
                       Atom         type)
{
  char *type_name;
  char *expected_name;
  char *prop_name;

  if (expected_format == format &&
      expected_type == type &&
      (n_items < 0 || n_items > 0))
    return TRUE;  
  
  meta_error_trap_push (display);
  type_name = XGetAtomName (display->xdisplay, type);
  expected_name = XGetAtomName (display->xdisplay, expected_type);
  prop_name = XGetAtomName (display->xdisplay, xatom);
  meta_error_trap_pop (display);

  meta_warning (_("Window 0x%lx has property %s that was expected to have type %s format %d and actually has type %s format %d n_items %d\n"),
                xwindow,
                prop_name ? prop_name : "(bad atom)",
                expected_name ? expected_name : "(bad atom)",
                expected_format,
                type_name ? type_name : "(bad atom)",
                format, n_items);

  if (type_name)
    XFree (type_name);
  if (expected_name)
    XFree (expected_name);
  if (prop_name)
    XFree (prop_name);

  return FALSE;
}

gboolean
meta_prop_get_atom_list (MetaDisplay *display,
                         Window       xwindow,
                         Atom         xatom,
                         Atom       **atoms_p,
                         int         *n_atoms_p)
{
  Atom type;
  int format;
  gulong n_atoms;
  gulong bytes_after;
  Atom *atoms;

  *atoms_p = NULL;
  *n_atoms_p = 0;
  
  meta_error_trap_push (display);
  if (XGetWindowProperty (display->xdisplay, xwindow, xatom,
                          0, G_MAXLONG,
                          False, XA_ATOM, &type, &format, &n_atoms,
                          &bytes_after, (guchar **)&atoms) != Success ||
      type == None)
    {
      meta_error_trap_pop (display);
      return FALSE;
    }

  if (meta_error_trap_pop (display) != Success)
    return FALSE;
    
  if (!check_type_and_format (display, xwindow, xatom, 32, XA_ATOM,
                              -1, format, type))
    {
      XFree (atoms);
      return FALSE;
    }

  *atoms_p = atoms;
  *n_atoms_p = n_atoms;

  return TRUE;
}

gboolean
meta_prop_get_cardinal_list (MetaDisplay *display,
                             Window       xwindow,
                             Atom         xatom,
                             gulong     **cardinals_p,
                             int         *n_cardinals_p)
{
  Atom type;
  int format;
  gulong n_cardinals;
  gulong bytes_after;
  gulong *cardinals;

  *cardinals_p = NULL;
  *n_cardinals_p = 0;
  
  meta_error_trap_push (display);
  if (XGetWindowProperty (display->xdisplay, xwindow, xatom,
                          0, G_MAXLONG,
                          False, XA_CARDINAL, &type, &format, &n_cardinals,
                          &bytes_after, (guchar **)&cardinals) != Success ||
      type == None)
    {
      meta_error_trap_pop (display);
      return FALSE;
    }

  if (meta_error_trap_pop (display) != Success)
    return FALSE;

  if (!check_type_and_format (display, xwindow, xatom, 32, XA_CARDINAL,
                              -1, format, type))
    {
      XFree (cardinals);
      return FALSE;
    }

  *cardinals_p = cardinals;
  *n_cardinals_p = n_cardinals;

  return TRUE;
}

gboolean
meta_prop_get_motif_hints (MetaDisplay   *display,
                           Window         xwindow,
                           Atom           xatom,
                           MotifWmHints **hints_p)
{
  Atom type;
  int format;
  gulong bytes_after;
  MotifWmHints *hints;
  gulong n_items;

#define EXPECTED_ITEMS sizeof (MotifWmHints)/sizeof (gulong)
  
  *hints_p = NULL;
  
  meta_error_trap_push (display);
  if (XGetWindowProperty (display->xdisplay, xwindow, xatom,
                          0, EXPECTED_ITEMS,
                          False, AnyPropertyType, &type, &format, &n_items,
                          &bytes_after, (guchar **)&hints) != Success ||
      type == None)
    {
      meta_error_trap_pop (display);
      return FALSE;
    }

  if (meta_error_trap_pop (display) != Success)
    return FALSE;
  
  if (type == None || n_items != EXPECTED_ITEMS)
    {
      meta_verbose ("Motif hints had unexpected type or n_items\n");
      XFree (hints);
      return FALSE;
    }

  *hints_p = hints;
  
  return TRUE;
}

gboolean
meta_prop_get_latin1_string (MetaDisplay   *display,
                             Window         xwindow,
                             Atom           xatom,
                             char         **str_p)
{
  Atom type;
  int format;
  gulong bytes_after;
  guchar *str;
  gulong n_items;
  
  *str_p = NULL;
  
  meta_error_trap_push (display);
  if (XGetWindowProperty (display->xdisplay, xwindow, xatom,
                          0, G_MAXLONG,
                          False, XA_STRING, &type, &format, &n_items,
                          &bytes_after, (guchar **)&str) != Success ||
      type == None)
    {
      meta_error_trap_pop (display);
      return FALSE;
    }

  if (meta_error_trap_pop (display) != Success)
    return FALSE;

  if (!check_type_and_format (display, xwindow, xatom, 8, XA_STRING,
                              -1, format, type))
    {
      XFree (str);
      return FALSE;
    }

  *str_p = str;

  return TRUE;
}

gboolean
meta_prop_get_utf8_string (MetaDisplay   *display,
                           Window         xwindow,
                           Atom           xatom,
                           char         **str_p)
{
  Atom type;
  int format;
  gulong bytes_after;
  guchar *str;
  gulong n_items;
  
  *str_p = NULL;
  
  meta_error_trap_push (display);
  if (XGetWindowProperty (display->xdisplay, xwindow, xatom,
                          0, G_MAXLONG,
                          False, display->atom_utf8_string,
                          &type, &format, &n_items,
                          &bytes_after, (guchar **)&str) != Success ||
      type == None)
    {
      meta_error_trap_pop (display);
      return FALSE;
    }

  if (meta_error_trap_pop (display) != Success)
    return FALSE;

  if (!check_type_and_format (display, xwindow, xatom, 8,
                              display->atom_utf8_string,
                              -1, format, type))
    {
      XFree (str);
      return FALSE;
    }

  if (!g_utf8_validate (str, n_items, NULL))
    {
      char *name;

      name = XGetAtomName (display->xdisplay, xatom);
      meta_warning (_("Property %s on window 0x%lx contained invalid UTF-8\n"),
                    name, xwindow);
      XFree (name);
      XFree (str);

      return FALSE;
    }
  
  *str_p = str;

  return TRUE;
}

gboolean
meta_prop_get_window (MetaDisplay   *display,
                      Window         xwindow,
                      Atom           xatom,
                      Window        *window_p)
{
  Atom type;
  int format;
  gulong bytes_after;
  Window *window;
  gulong n_items;

  *window_p = None;
  
  meta_error_trap_push (display);
  if (XGetWindowProperty (display->xdisplay, xwindow, xatom,
                          0, G_MAXLONG,
                          False, XA_WINDOW, &type, &format, &n_items,
                          &bytes_after, (guchar **)&window) != Success ||
      type == None)
    {
      meta_error_trap_pop (display);
      return FALSE;
    }

  if (meta_error_trap_pop (display) != Success)
    return FALSE;

  if (!check_type_and_format (display, xwindow, xatom, 32, XA_WINDOW,
                              -1, format, type))
    {
      XFree (window);
      return FALSE;
    }

  *window_p = *window;

  XFree (window);
  
  return TRUE;
}

gboolean
meta_prop_get_cardinal (MetaDisplay   *display,
                        Window         xwindow,
                        Atom           xatom,
                        gulong        *cardinal_p)

{
  return meta_prop_get_cardinal_with_atom_type (display, xwindow, xatom,
                                                XA_CARDINAL, cardinal_p);
}

gboolean
meta_prop_get_cardinal_with_atom_type (MetaDisplay   *display,
                                       Window         xwindow,
                                       Atom           xatom,
                                       Atom           prop_type,
                                       gulong        *cardinal_p)
{
  Atom type;
  int format;
  gulong bytes_after;
  gulong *cardinal;
  gulong n_items;

  *cardinal_p = 0;
  
  meta_error_trap_push (display);
  if (XGetWindowProperty (display->xdisplay, xwindow, xatom,
                          0, G_MAXLONG,
                          False, prop_type, &type, &format, &n_items,
                          &bytes_after, (guchar **)&cardinal) != Success ||
      type == None)
    {
      meta_error_trap_pop (display);
      return FALSE;
    }

  if (meta_error_trap_pop (display) != Success)
    return FALSE;

  if (!check_type_and_format (display, xwindow, xatom, 32, prop_type,
                              -1, format, type))
    {
      XFree (cardinal);
      return FALSE;
    }

  *cardinal_p = *cardinal;

  XFree (cardinal);
  
  return TRUE;
}

