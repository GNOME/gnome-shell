/*
 * st-stylable.h: Interface for stylable objects
 *
 * Copyright 2008, 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 *             Thomas Wood <thomas@linux.intel.com>
 *
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_STYLABLE_H__
#define __ST_STYLABLE_H__

#include <glib-object.h>
#include <st/st-style.h>

G_BEGIN_DECLS

#define ST_TYPE_STYLABLE              (st_stylable_get_type ())
#define ST_STYLABLE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), ST_TYPE_STYLABLE, StStylable))
#define ST_IS_STYLABLE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ST_TYPE_STYLABLE))
#define ST_STYLABLE_IFACE(iface)      (G_TYPE_CHECK_CLASS_CAST ((iface), ST_TYPE_STYLABLE, StStylableIface))
#define ST_IS_STYLABLE_IFACE(iface)   (G_TYPE_CHECK_CLASS_TYPE ((iface), ST_TYPE_STYLABLE))
#define ST_STYLABLE_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), ST_TYPE_STYLABLE, StStylableIface))

/* StStylableIface is defined in st-style.h */

struct _StStylableIface
{
  GTypeInterface g_iface;

  /* virtual functions */
  StStyle *  (* get_style) (StStylable *stylable);
  void       (* set_style) (StStylable *stylable,
                            StStyle    *style);

  /* context virtual functions */
  StStylable   *(*get_container)    (StStylable  *stylable);
  StStylable   *(*get_base_style)   (StStylable  *stylable);
  const gchar  *(*get_style_id)     (StStylable  *stylable);
  const gchar  *(*get_style_type)   (StStylable  *stylable);
  const gchar  *(*get_style_class)  (StStylable  *stylable);
  const gchar  *(*get_pseudo_class) (StStylable  *stylable);
  gchar        *(*get_attribute)    (StStylable  *stylable,
                                     const gchar *name);
  gboolean      (*get_viewport)     (StStylable *stylable,
                                     gint        *x,
                                     gint        *y,
                                     gint        *width,
                                     gint        *height);

  /* signals, not vfuncs */
  void (* style_notify)     (StStylable *stylable,
                             GParamSpec *pspec);
  void (* style_changed)    (StStylable *stylable);

  void (* stylable_changed) (StStylable *stylable);
};

GType st_stylable_get_type (void) G_GNUC_CONST;

void st_stylable_iface_install_property (StStylableIface *iface,
                                         GType            owner_type,
                                         GParamSpec      *pspec);

void         st_stylable_freeze_notify     (StStylable  *stylable);
void         st_stylable_notify            (StStylable  *stylable,
                                            const gchar *property_name);
void         st_stylable_thaw_notify       (StStylable  *stylable);
GParamSpec **st_stylable_list_properties   (StStylable  *stylable,
                                            guint       *n_props);
GParamSpec * st_stylable_find_property     (StStylable  *stylable,
                                            const gchar *property_name);
void         st_stylable_set_style         (StStylable  *stylable,
                                            StStyle     *style);
StStyle *    st_stylable_get_style         (StStylable  *stylable);

void         st_stylable_get               (StStylable  *stylable,
                                            const gchar *first_property_name,
                                            ...) G_GNUC_NULL_TERMINATED;
void         st_stylable_get_property      (StStylable  *stylable,
                                            const gchar *property_name,
                                            GValue      *value);
gboolean     st_stylable_get_default_value (StStylable  *stylable,
                                            const gchar *property_name,
                                            GValue      *value_out);

StStylable*  st_stylable_get_container     (StStylable  *stylable);
StStylable*  st_stylable_get_base_style    (StStylable  *stylable);
const gchar* st_stylable_get_style_id      (StStylable  *stylable);
const gchar* st_stylable_get_style_type    (StStylable  *stylable);
const gchar* st_stylable_get_style_class   (StStylable  *stylable);
const gchar* st_stylable_get_pseudo_class  (StStylable  *stylable);
gchar*       st_stylable_get_attribute     (StStylable  *stylable,
                                            const gchar *name);
gboolean     st_stylable_get_viewport      (StStylable  *stylable,
                                            gint        *x,
                                            gint        *y,
                                            gint        *width,
                                            gint        *height);

void st_stylable_changed (StStylable *stylable);

G_END_DECLS

#endif /* __ST_STYLABLE_H__ */
