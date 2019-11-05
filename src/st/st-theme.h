/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme.h: A set of CSS stylesheets used for rule matching
 *
 * Copyright 2008, 2009 Red Hat, Inc.
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
#ifndef __ST_THEME_H__
#define __ST_THEME_H__

#include <glib-object.h>

#include "st-theme-node.h"

G_BEGIN_DECLS

/**
 * SECTION:st-theme
 * @short_description: a set of stylesheets
 *
 * #StTheme holds a set of stylesheets. (The "cascade" of the name
 * Cascading Stylesheets.) A #StTheme can be set to apply to all the actors
 * in a stage using st_theme_context_set_theme().
 */

#define ST_TYPE_THEME              (st_theme_get_type ())
G_DECLARE_FINAL_TYPE (StTheme, st_theme, ST, THEME, GObject)

StTheme *st_theme_new (GFile *application_stylesheet,
                       GFile *theme_stylesheet,
                       GFile *default_stylesheet);

gboolean  st_theme_load_stylesheet        (StTheme *theme, GFile *file, GError **error);
void      st_theme_unload_stylesheet      (StTheme *theme, GFile *file);
GSList   *st_theme_get_custom_stylesheets (StTheme *theme);

G_END_DECLS

#endif /* __ST_THEME_H__ */
