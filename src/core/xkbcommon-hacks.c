/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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

#include "config.h"

#include "xkbcommon-hacks.h"

/* Terrible, gross hackery to provide an implementation for xkb_keymap_mod_get_mask.
 * Delete when https://github.com/xkbcommon/libxkbcommon/pull/10 is pushed. */

/* The structures here have been pulled from libxkbcommon. */

enum xkb_action_controls {
    CONTROL_REPEAT = (1 << 0),
    CONTROL_SLOW = (1 << 1),
    CONTROL_DEBOUNCE = (1 << 2),
    CONTROL_STICKY = (1 << 3),
    CONTROL_MOUSEKEYS = (1 << 4),
    CONTROL_MOUSEKEYS_ACCEL = (1 << 5),
    CONTROL_AX = (1 << 6),
    CONTROL_AX_TIMEOUT = (1 << 7),
    CONTROL_AX_FEEDBACK = (1 << 8),
    CONTROL_BELL = (1 << 9),
    CONTROL_IGNORE_GROUP_LOCK = (1 << 10),
    CONTROL_ALL = \
        (CONTROL_REPEAT | CONTROL_SLOW | CONTROL_DEBOUNCE | CONTROL_STICKY | \
         CONTROL_MOUSEKEYS | CONTROL_MOUSEKEYS_ACCEL | CONTROL_AX | \
         CONTROL_AX_TIMEOUT | CONTROL_AX_FEEDBACK | CONTROL_BELL | \
         CONTROL_IGNORE_GROUP_LOCK)
};

typedef uint32_t xkb_atom_t;

/* Don't allow more modifiers than we can hold in xkb_mod_mask_t. */
#define XKB_MAX_MODS ((xkb_mod_index_t) (sizeof(xkb_mod_mask_t) * 8))

/* These should all go away. */
enum mod_type {
    MOD_REAL = (1 << 0),
    MOD_VIRT = (1 << 1),
    MOD_BOTH = (MOD_REAL | MOD_VIRT),
};

struct xkb_mod {
    xkb_atom_t name;
    enum mod_type type;
    xkb_mod_mask_t mapping; /* vmod -> real mod mapping */
};

struct xkb_mod_set {
    struct xkb_mod mods[XKB_MAX_MODS];
    unsigned int num_mods;
};

/* Common keyboard description structure */
struct xkb_keymap_real {
    struct xkb_context *ctx;

    int refcnt;
    enum xkb_keymap_compile_flags flags;
    enum xkb_keymap_format format;

    enum xkb_action_controls enabled_ctrls;

    xkb_keycode_t min_key_code;
    xkb_keycode_t max_key_code;
    void *keys;

    /* aliases in no particular order */
    unsigned int num_key_aliases;
    void *key_aliases;

    void *types;
    unsigned int num_types;

    unsigned int num_sym_interprets;
    void *sym_interprets;

    struct xkb_mod_set mods;
};

xkb_mod_mask_t
my_xkb_keymap_mod_get_mask(struct xkb_keymap *_keymap, xkb_mod_index_t idx)
{
    struct xkb_keymap_real *keymap = (struct xkb_keymap_real *) _keymap;

    if (idx >= keymap->mods.num_mods)
        return 0;

    return keymap->mods.mods[idx].mapping;
}
