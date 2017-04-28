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
 */

#ifndef META_ORIENTATION_MANAGER_H
#define META_ORIENTATION_MANAGER_H

typedef enum
{
  META_ORIENTATION_UNDEFINED,
  META_ORIENTATION_NORMAL,
  META_ORIENTATION_BOTTOM_UP,
  META_ORIENTATION_LEFT_UP,
  META_ORIENTATION_RIGHT_UP
} MetaOrientation;

#define META_TYPE_ORIENTATION_MANAGER (meta_orientation_manager_get_type ())
G_DECLARE_FINAL_TYPE (MetaOrientationManager, meta_orientation_manager,
                      META, ORIENTATION_MANAGER, GObject)

MetaOrientation meta_orientation_manager_get_orientation (MetaOrientationManager *self);

#endif  /* META_ORIENTATION_MANAGER_H */
