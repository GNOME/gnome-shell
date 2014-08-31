/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Endless Mobile
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include <clutter/clutter.h>

#ifndef __SHELL_WOBBLY_EFFECT_H__
#define __SHELL_WOBBLY_EFFECT_H__

#define SHELL_TYPE_WOBBLY_EFFECT             (shell_wobbly_effect_get_type ())
#define SHELL_WOBBLY_EFFECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_WOBBLY_EFFECT, ShellWobblyEffect))
#define SHELL_WOBBLY_EFFECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  SHELL_TYPE_WOBBLY_EFFECT, ShellWobblyEffectClass))
#define SHELL_IS_WOBBLY_EFFECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_WOBBLY_EFFECT))
#define SHELL_IS_WOBBLY_EFFECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  SHELL_TYPE_WOBBLY_EFFECT))
#define SHELL_WOBBLY_EFFECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  SHELL_TYPE_WOBBLY_EFFECT, ShellWobblyEffectClass))

typedef struct _ShellWobblyEffect        ShellWobblyEffect;
typedef struct _ShellWobblyEffectClass   ShellWobblyEffectClass;

struct _ShellWobblyEffect
{
  ClutterOffscreenEffect parent;
};

struct _ShellWobblyEffectClass
{
  ClutterOffscreenEffectClass parent_class;
};

GType shell_wobbly_effect_get_type (void) G_GNUC_CONST;

void shell_wobbly_effect_set_bend_x (ShellWobblyEffect *self,
                                     int                bend_x);

#endif /* __SHELL_WOBBLY_EFFECT_H__ */
