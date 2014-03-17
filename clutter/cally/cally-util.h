/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2008 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CALLY_UTIL_H__
#define __CALLY_UTIL_H__

#if !defined(__CALLY_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cally/cally.h> can be included directly."
#endif

#include <clutter/clutter.h>
#include <atk/atk.h>

G_BEGIN_DECLS

#define CALLY_TYPE_UTIL            (cally_util_get_type ())
#define CALLY_UTIL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CALLY_TYPE_UTIL, CallyUtil))
#define CALLY_UTIL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CALLY_TYPE_UTIL, CallyUtilClass))
#define CALLY_IS_UTIL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CALLY_TYPE_UTIL))
#define CALLY_IS_UTIL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CALLY_TYPE_UTIL))
#define CALLY_UTIL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CALLY_TYPE_UTIL, CallyUtilClass))

typedef struct _CallyUtil        CallyUtil;
typedef struct _CallyUtilClass   CallyUtilClass;
typedef struct _CallyUtilPrivate CallyUtilPrivate;

/**
 * CallyUtil:
 *
 * The <structname>CallyUtil</structname> structure contains only
 * private data and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _CallyUtil
{
  /*< private >*/
  AtkUtil parent;

  CallyUtilPrivate *priv;
};

/**
 * CallyUtilClass:
 *
 * The <structname>CallyUtilClass</structname> structure contains only
 * private data
 *
 * Since: 1.4
 */
struct _CallyUtilClass
{
  /*< private >*/
  AtkUtilClass parent_class;

  /* padding for future expansion */
  gpointer _padding_dummy[8];
};

CLUTTER_AVAILABLE_IN_1_4
GType cally_util_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CALLY_UTIL_H__ */
