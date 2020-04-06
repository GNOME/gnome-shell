/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme-context.c: holds global information about a tree of styled objects
 *
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2009 Florian Müllner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ST_THEME_CONTEXT_H__
#define __ST_THEME_CONTEXT_H__

#include <clutter/clutter.h>
#include <pango/pango.h>
#include "st-theme-node.h"

G_BEGIN_DECLS

/**
 * SECTION:st-theme-context
 * @short_description: holds global information about a tree of styled objects
 *
 * #StThemeContext is responsible for managing information global to a tree of styled objects,
 * such as the set of stylesheets or the default font. In normal usage, a #StThemeContext
 * is bound to a #ClutterStage; a singleton #StThemeContext can be obtained for a #ClutterStage
 * by using st_theme_context_get_for_stage().
 */

#define ST_TYPE_THEME_CONTEXT             (st_theme_context_get_type ())
G_DECLARE_FINAL_TYPE (StThemeContext, st_theme_context,
                      ST, THEME_CONTEXT, GObject)

StThemeContext *st_theme_context_new           (void);
StThemeContext *st_theme_context_get_for_stage (ClutterStage *stage);

void                        st_theme_context_set_theme      (StThemeContext             *context,
                                                             StTheme                    *theme);
StTheme *                   st_theme_context_get_theme      (StThemeContext             *context);

void                        st_theme_context_set_font       (StThemeContext             *context,
                                                             const PangoFontDescription *font);
const PangoFontDescription *st_theme_context_get_font       (StThemeContext             *context);

StThemeNode *               st_theme_context_get_root_node  (StThemeContext             *context);

StThemeNode *               st_theme_context_intern_node    (StThemeContext             *context,
                                                             StThemeNode                *node);

int st_theme_context_get_scale_factor (StThemeContext *context);

G_END_DECLS

#endif /* __ST_THEME_CONTEXT_H__ */
