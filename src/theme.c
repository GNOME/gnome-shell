/* Metacity Default Theme Engine */

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

#include "theme.h"
#include <pango.h>
#include <pangox.h>

typedef struct _DefaultFrameData DefaultFrameData;

struct _DefaultFrameData
{
  PangoLayout *layout;


};

static gpointer
default_acquire_frame (MetaFrameInfo *info)
{
  DefaultFrameData *d;

  d = g_new (DefaultFrameData, 1);

  d->layout = NULL;
  
  return d;
}

void
default_release_frame (MetaFrameInfo *info,
                       gpointer       frame_data)
{
  DefaultFrameData *d;

  d = frame_data;

  if (d->layout)
    g_object_unref (G_OBJECT (d->layout));

  g_free (d);
}

void
default_fill_frame_geometry (MetaFrameInfo *info,
                             gpointer       frame_data)
{


}

void
default_expose_frame (MetaFrameInfo *info,
                      int x, int y,
                      int width, int height,
                      gpointer frame_data)
{


}

MetaFrameControl
default_get_control (MetaFrameInfo *info,
                     int x, int y,
                     gpointer frame_data)
{


}

MetaThemeEngine meta_default_engine = {
  NULL,
  default_acquire_frame,
  default_release_frame,
  default_fill_frame_geometry,
  default_expose_frame,
  default_get_control
};

#endif
