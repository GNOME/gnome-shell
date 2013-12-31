/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "dbus-utils.h"

#include <glib.h>

/* Stolen from tp_escape_as_identifier, from tp-glib,
 * which follows the same escaping convention as systemd.
 */
static inline gboolean
_esc_ident_bad (gchar c, gboolean is_first)
{
  return ((c < 'a' || c > 'z') &&
          (c < 'A' || c > 'Z') &&
          (c < '0' || c > '9' || is_first));
}

static gchar *
escape_dbus_component (const gchar *name)
{
  gboolean bad = FALSE;
  size_t len = 0;
  GString *op;
  const gchar *ptr, *first_ok;

  g_return_val_if_fail (name != NULL, NULL);

  /* fast path for empty name */
  if (name[0] == '\0')
    return g_strdup ("_");

  for (ptr = name; *ptr; ptr++)
    {
      if (_esc_ident_bad (*ptr, ptr == name))
        {
          bad = TRUE;
          len += 3;
        }
      else
        len++;
    }

  /* fast path if it's clean */
  if (!bad)
    return g_strdup (name);

  /* If strictly less than ptr, first_ok is the first uncopied safe character.
   */
  first_ok = name;
  op = g_string_sized_new (len);
  for (ptr = name; *ptr; ptr++)
    {
      if (_esc_ident_bad (*ptr, ptr == name))
        {
          /* copy preceding safe characters if any */
          if (first_ok < ptr)
            {
              g_string_append_len (op, first_ok, ptr - first_ok);
            }
          /* escape the unsafe character */
          g_string_append_printf (op, "_%02x", (unsigned char)(*ptr));
          /* restart after it */
          first_ok = ptr + 1;
        }
    }
  /* copy trailing safe characters if any */
  if (first_ok < ptr)
    {
      g_string_append_len (op, first_ok, ptr - first_ok);
    }
  return g_string_free (op, FALSE);
}

char *
get_escaped_dbus_path (const char *prefix,
                       const char *component)
{
  char *escaped_component = escape_dbus_component (component);
  char *path = g_strconcat (prefix, "/", escaped_component, NULL);

  g_free (escaped_component);
  return path;
}
