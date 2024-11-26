/*
 * Copyright 2024 Red Hat
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

#pragma once

#include <glib-object.h>
#include <clutter/clutter.h>

#define ST_TYPE_SPINNER_CONTENT (st_spinner_content_get_type ())
G_DECLARE_FINAL_TYPE (StSpinnerContent, st_spinner_content,
                      ST, SPINNER_CONTENT, GObject)

ClutterContent *st_spinner_content_new (void);
