/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Red Hat
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
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#include "config.h"

#include "backends/meta-pointer-constraint.h"

#include <glib-object.h>

G_DEFINE_TYPE (MetaPointerConstraint, meta_pointer_constraint, G_TYPE_OBJECT);

static void
meta_pointer_constraint_init (MetaPointerConstraint *constraint)
{
}

static void
meta_pointer_constraint_class_init (MetaPointerConstraintClass *klass)
{
}

void
meta_pointer_constraint_constrain (MetaPointerConstraint *constraint,
                                   ClutterInputDevice    *device,
                                   guint32                time,
                                   float                  prev_x,
                                   float                  prev_y,
                                   float                  *x,
                                   float                  *y)
{
  META_POINTER_CONSTRAINT_GET_CLASS (constraint)->constrain (constraint,
                                                             device,
                                                             time,
                                                             prev_x, prev_y,
                                                             x, y);
}
