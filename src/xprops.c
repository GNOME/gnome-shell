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
                       int          format,
                       Atom         type)
{
  char *type_name;
  char *expected_name;
  char *prop_name;

  if (expected_format == format &&
      expected_type == type)
    return TRUE;
  
  meta_error_trap_push (display);
  type_name = XGetAtomName (display->xdisplay, type);
  expected_name = XGetAtomName (display->xdisplay, expected_type);
  prop_name = XGetAtomName (display->xdisplay, xatom);
  meta_error_trap_pop (display);

  meta_warning (_("Window 0x%lx has property %s that was expected to have type %s format %d and actually has type %s format %d\n"),
                xwindow,
                prop_name ? prop_name : "(bad atom)",
                expected_name ? expected_name : "(bad atom)",
                expected_format,
                type_name ? type_name : "(bad atom)",
                format);

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
                          &bytes_after, (guchar **)&atoms) != Success)
    {
      meta_error_trap_pop (display);
      return FALSE;
    }

  if (meta_error_trap_pop (display) != Success)
    return FALSE;
    
  if (!check_type_and_format (display, xwindow, xatom, 32, XA_ATOM,
                              format, type))
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
                          &bytes_after, (guchar **)&cardinals) != Success)
    {
      meta_error_trap_pop (display);
      return FALSE;
    }

  if (meta_error_trap_pop (display) != Success)
    return FALSE;

  if (!check_type_and_format (display, xwindow, xatom, 32, XA_CARDINAL,
                              format, type))
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
                          &bytes_after, (guchar **)&hints) != Success)
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
