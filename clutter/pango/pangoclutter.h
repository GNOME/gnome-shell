/* Pango
 * pangoclutter.h: Clutter/Freetype2 backend
 *
 * Copyright (C) 1999 Red Hat Software
 * Copyright (C) 2000 Tor Lillqvist
 * Copyright (C) 2006 Marc Lehmann <pcg@goof.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef PANGOCLUTTER_H__
#define PANGOCLUTTER_H__

#define PANGO_ENABLE_BACKEND

/* we always want to disable cast checks */
#ifndef G_DISABLE_CAST_CHECKS
#define G_DISABLE_CAST_CHECKS
#endif

#include <glib-object.h>
#include <pango/pango.h>
#include <fontconfig/fontconfig.h>
#include <clutter/clutter-color.h>

G_BEGIN_DECLS

#define PANGO_TYPE_CLUTTER_FONT_MAP                                   \
            (pango_clutter_font_map_get_type ())

#define PANGO_CLUTTER_FONT_MAP(object)                                \
            (G_TYPE_CHECK_INSTANCE_CAST ((object),                    \
                                         PANGO_TYPE_CLUTTER_FONT_MAP, \
                                         PangoClutterFontMap))

#define PANGO_CLUTTER_IS_FONT_MAP(object)                             \
           (G_TYPE_CHECK_INSTANCE_TYPE ((object), PANGO_TYPE_CLUTTER_FONT_MAP))

typedef struct _PangoClutterFontMap      PangoClutterFontMap;
typedef struct _PangoClutterFontMapClass PangoClutterFontMapClass;

typedef void (*PangoClutterSubstituteFunc) (FcPattern *pattern, gpointer data);

GType pango_clutter_font_map_get_type (void);

PangoFontMap*
pango_clutter_font_map_new (void);

void  
pango_clutter_font_map_set_default_substitute 
                           (PangoClutterFontMap        *fontmap,
			    PangoClutterSubstituteFunc  func,
			    gpointer                    data,
			    GDestroyNotify              notify);

void
pango_clutter_font_map_set_resolution (PangoClutterFontMap *fontmap,
                                       double               dpi);

void          
pango_clutter_font_map_substitute_changed (PangoClutterFontMap *fontmap);

PangoContext *
pango_clutter_font_map_create_context (PangoClutterFontMap *fontmap);

#define FLAG_INVERSE 1
#define FLAG_OUTLINE 2 // not yet implemented

void 
pango_clutter_render_layout_subpixel (PangoLayout *layout,
				      int           x, 
				      int           y,
				      ClutterColor *color,
				      int           flags);
void 
pango_clutter_render_layout (PangoLayout  *layout,
			     int           x, 
			     int           y,
			     ClutterColor *color,
			     int           flags);

void 
pango_clutter_render_layout_line (PangoLayoutLine *line,
				  int              x,
				  int              y,
				  ClutterColor    *color);

void 
pango_clutter_render_clear_caches ();

G_END_DECLS

#endif
