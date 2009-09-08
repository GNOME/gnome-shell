/*
 * nbtk-stylable.h: Interface for stylable objects
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

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_STYLABLE_H__
#define __NBTK_STYLABLE_H__

#include <glib-object.h>
#include <nbtk/nbtk-style.h>

G_BEGIN_DECLS

#define NBTK_TYPE_STYLABLE              (nbtk_stylable_get_type ())
#define NBTK_STYLABLE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_STYLABLE, NbtkStylable))
#define NBTK_IS_STYLABLE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_STYLABLE))
#define NBTK_STYLABLE_IFACE(iface)      (G_TYPE_CHECK_CLASS_CAST ((iface), NBTK_TYPE_STYLABLE, NbtkStylableIface))
#define NBTK_IS_STYLABLE_IFACE(iface)   (G_TYPE_CHECK_CLASS_TYPE ((iface), NBTK_TYPE_STYLABLE))
#define NBTK_STYLABLE_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NBTK_TYPE_STYLABLE, NbtkStylableIface))

/* NbtkStylableIface is defined in nbtk-style.h */

struct _NbtkStylableIface
{
  GTypeInterface g_iface;

  /* virtual functions */
  NbtkStyle *(* get_style) (NbtkStylable *stylable);
  void       (* set_style) (NbtkStylable *stylable,
                            NbtkStyle    *style);

  /* context virtual functions */
  NbtkStylable *(*get_container)    (NbtkStylable *stylable);
  NbtkStylable *(*get_base_style)   (NbtkStylable *stylable);
  const gchar  *(*get_style_id)     (NbtkStylable *stylable);
  const gchar  *(*get_style_type)   (NbtkStylable *stylable);
  const gchar  *(*get_style_class)  (NbtkStylable *stylable);
  const gchar  *(*get_pseudo_class) (NbtkStylable *stylable);
  gchar        *(*get_attribute)    (NbtkStylable *stylable,
                                     const gchar  *name);
  gboolean      (*get_viewport)     (NbtkStylable *stylable,
                                     gint         *x,
                                     gint         *y,
                                     gint         *width,
                                     gint         *height);

  /* signals, not vfuncs */
  void (* style_notify)     (NbtkStylable *stylable,
                             GParamSpec   *pspec);
  void (* style_changed)    (NbtkStylable *stylable);

  void (* stylable_changed) (NbtkStylable *stylable);
};

GType        nbtk_stylable_get_type               (void) G_GNUC_CONST;

void         nbtk_stylable_iface_install_property (NbtkStylableIface *iface,
                                                   GType              owner_type,
                                                   GParamSpec        *pspec);

void         nbtk_stylable_freeze_notify          (NbtkStylable      *stylable);
void         nbtk_stylable_notify                 (NbtkStylable      *stylable,
                                                   const gchar       *property_name);
void         nbtk_stylable_thaw_notify            (NbtkStylable      *stylable);
GParamSpec **nbtk_stylable_list_properties        (NbtkStylable      *stylable,
                                                   guint             *n_props);
GParamSpec * nbtk_stylable_find_property          (NbtkStylable      *stylable,
                                                   const gchar       *property_name);
void         nbtk_stylable_set_style              (NbtkStylable      *stylable,
                                                   NbtkStyle         *style);
NbtkStyle *  nbtk_stylable_get_style              (NbtkStylable      *stylable);

void         nbtk_stylable_get                    (NbtkStylable      *stylable,
                                                   const gchar       *first_property_name,
                                                   ...) G_GNUC_NULL_TERMINATED;
void         nbtk_stylable_get_property           (NbtkStylable      *stylable,
                                                   const gchar       *property_name,
                                                   GValue            *value);
gboolean     nbtk_stylable_get_default_value      (NbtkStylable      *stylable,
                                                   const gchar       *property_name,
                                                   GValue            *value_out);

NbtkStylable* nbtk_stylable_get_container     (NbtkStylable *stylable);
NbtkStylable* nbtk_stylable_get_base_style    (NbtkStylable *stylable);
const gchar*  nbtk_stylable_get_style_id      (NbtkStylable *stylable);
const gchar*  nbtk_stylable_get_style_type    (NbtkStylable *stylable);
const gchar*  nbtk_stylable_get_style_class   (NbtkStylable *stylable);
const gchar*  nbtk_stylable_get_pseudo_class  (NbtkStylable *stylable);
gchar*        nbtk_stylable_get_attribute     (NbtkStylable *stylable,
                                               const gchar  *name);
gboolean      nbtk_stylable_get_viewport      (NbtkStylable *stylable,
                                               gint         *x,
                                               gint         *y,
                                               gint         *width,
                                               gint         *height);

void nbtk_stylable_changed (NbtkStylable *stylable);
G_END_DECLS

#endif /* __NBTK_STYLABLE_H__ */
