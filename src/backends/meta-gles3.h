/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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
 */

#ifndef META_GLES3_H
#define META_GLES3_H

#include <glib-object.h>

#include "backends/meta-egl.h"

typedef struct _MetaGles3Table MetaGles3Table;

#define META_TYPE_GLES3 (meta_gles3_get_type ())
G_DECLARE_FINAL_TYPE (MetaGles3, meta_gles3, META, GLES3, GObject)

MetaGles3Table * meta_gles3_get_table (MetaGles3 *gles3);

void meta_gles3_clear_error (MetaGles3 *gles3);

gboolean meta_gles3_validate (MetaGles3 *gles3,
                              GError   **error);

void meta_gles3_ensure_loaded (MetaGles3  *gles,
                               gpointer   *func,
                               const char *name);

gboolean meta_gles3_has_extensions (MetaGles3 *gles3,
                                    char    ***missing_extensions,
                                    char      *first_extension,
                                    ...);

MetaGles3 * meta_gles3_new (MetaEgl *egl);

#define GLBAS(gles3, func, args)                                               \
{                                                                              \
  GError *_error = NULL;                                                       \
                                                                               \
  func args;                                                                   \
                                                                               \
  if (!meta_gles3_validate (gles3, &_error))                                   \
    {                                                                          \
      g_warning ("%s %s failed: %s", #func, #args, _error->message);           \
      g_error_free (_error);                                                   \
    }                                                                          \
}

#define GLEXT(gles3, func, args)                                               \
{                                                                              \
  GError *_error = NULL;                                                       \
  MetaGles3Table *table;                                                       \
                                                                               \
  table = meta_gles3_get_table (gles3);                                        \
  meta_gles3_ensure_loaded (gles3, (gpointer *) &table->func, #func);          \
                                                                               \
  table->func args;                                                            \
                                                                               \
  if (!meta_gles3_validate (gles3, &_error))                                   \
    {                                                                          \
      g_warning ("%s %s failed: %s", #func, #args, _error->message);           \
      g_error_free (_error);                                                   \
    }                                                                          \
}

#endif /* META_GLES3_H */
