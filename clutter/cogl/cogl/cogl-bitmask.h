/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __COGL_BITMASK_H
#define __COGL_BITMASK_H

#include <glib.h>

G_BEGIN_DECLS

/*
 * CoglBitmask implements a growable array of bits. A CoglBitmask can
 * be allocated on the stack but it must be initialised with
 * _cogl_bitmask_init() before use and then destroyed with
 * _cogl_bitmask_destroy(). A CoglBitmask will try to avoid allocating
 * any memory unless more than 31 bits are needed.
 *
 * Internally a CoglBitmask is a pointer. If the least significant bit
 * of the pointer is 1 then the rest of the bits are directly used as
 * part of the bitmask, otherwise it is a pointer to a GArray of
 * unsigned ints. This relies on the fact the g_malloc will return a
 * pointer aligned to at least two bytes (so that the least
 * significant bit of the address is always 0)
 *
 * If the maximum possible bit number in the set is known at compile
 * time, it may make more sense to use the macros in cogl-flags.h
 * instead of this type.
 */

typedef struct _CoglBitmaskImaginaryType *CoglBitmask;

/* Internal helper macro to determine whether this bitmask has a
   GArray allocated or whether the pointer is just used directly */
#define _cogl_bitmask_has_array(bitmask) \
  (!(GPOINTER_TO_UINT (*bitmask) & 1))

/* Number of bits we can use before needing to allocate an array */
#define COGL_BITMASK_MAX_DIRECT_BITS (sizeof (unsigned int) * 8 - 1)

/*
 * _cogl_bitmask_init:
 * @bitmask: A pointer to a bitmask
 *
 * Initialises the cogl bitmask. This must be called before any other
 * bitmask functions are called. Initially all of the values are
 * zero
 */
/* Set the last significant bit to mark that no array has been
   allocated yet */
#define _cogl_bitmask_init(bitmask) \
  G_STMT_START { *(bitmask) = GUINT_TO_POINTER (1); } G_STMT_END

gboolean
_cogl_bitmask_get_from_array (const CoglBitmask *bitmask,
                              unsigned int bit_num);

void
_cogl_bitmask_set_in_array (CoglBitmask *bitmask,
                            unsigned int bit_num,
                            gboolean value);

void
_cogl_bitmask_set_range_in_array (CoglBitmask *bitmask,
                                  unsigned int n_bits,
                                  gboolean value);

void
_cogl_bitmask_clear_all_in_array (CoglBitmask *bitmask);

/*
 * cogl_bitmask_set_bits:
 * @dst: The bitmask to modify
 * @src: The bitmask to copy bits from
 *
 * This makes sure that all of the bits that are set in @src are also
 * set in @dst. Any unset bits in @src are left alone in @dst.
 */
void
_cogl_bitmask_set_bits (CoglBitmask *dst,
                        const CoglBitmask *src);

/*
 * cogl_bitmask_xor_bits:
 * @dst: The bitmask to modify
 * @src: The bitmask to copy bits from
 *
 * For every bit that is set in src, the corresponding bit in dst is
 * inverted.
 */
void
_cogl_bitmask_xor_bits (CoglBitmask *dst,
                        const CoglBitmask *src);

typedef void (* CoglBitmaskForeachFunc) (int bit_num, gpointer user_data);

/*
 * cogl_bitmask_foreach:
 * @bitmask: A pointer to a bitmask
 * @func: A callback function
 * @user_data: A pointer to pass to the callback
 *
 * This calls @func for each bit that is set in @bitmask.
 */
void
_cogl_bitmask_foreach (const CoglBitmask *bitmask,
                       CoglBitmaskForeachFunc func,
                       gpointer user_data);

/*
 * _cogl_bitmask_get:
 * @bitmask: A pointer to a bitmask
 * @bit_num: A bit number
 *
 * Return value: whether bit number @bit_num is set in @bitmask
 */
static inline gboolean
_cogl_bitmask_get (const CoglBitmask *bitmask, unsigned int bit_num)
{
  if (_cogl_bitmask_has_array (bitmask))
    return _cogl_bitmask_get_from_array (bitmask, bit_num);
  else if (bit_num >= COGL_BITMASK_MAX_DIRECT_BITS)
    return FALSE;
  else
    return !!(GPOINTER_TO_UINT (*bitmask) & (1 << (bit_num + 1)));
}

/*
 * _cogl_bitmask_set:
 * @bitmask: A pointer to a bitmask
 * @bit_num: A bit number
 * @value: The new value
 *
 * Sets or resets a bit number @bit_num in @bitmask according to @value.
 */
static inline void
_cogl_bitmask_set (CoglBitmask *bitmask, unsigned int bit_num, gboolean value)
{
  if (_cogl_bitmask_has_array (bitmask) ||
      bit_num >= COGL_BITMASK_MAX_DIRECT_BITS)
    _cogl_bitmask_set_in_array (bitmask, bit_num, value);
  else if (value)
    *bitmask = GUINT_TO_POINTER (GPOINTER_TO_UINT (*bitmask) |
                                 (1 << (bit_num + 1)));
  else
    *bitmask = GUINT_TO_POINTER (GPOINTER_TO_UINT (*bitmask) &
                                 ~(1 << (bit_num + 1)));
}

/*
 * _cogl_bitmask_set_range:
 * @bitmask: A pointer to a bitmask
 * @n_bits: The number of bits to set
 * @value: The value to set
 *
 * Sets the first @n_bits in @bitmask to @value.
 */
static inline void
_cogl_bitmask_set_range (CoglBitmask *bitmask,
                         unsigned int n_bits,
                         gboolean value)
{
  if (_cogl_bitmask_has_array (bitmask) ||
      n_bits > COGL_BITMASK_MAX_DIRECT_BITS)
    _cogl_bitmask_set_range_in_array (bitmask, n_bits, value);
  else if (value)
    *bitmask = GUINT_TO_POINTER (GPOINTER_TO_UINT (*bitmask) |
                                 ~(~(unsigned int) 1 << n_bits));
  else
    *bitmask = GUINT_TO_POINTER (GPOINTER_TO_UINT (*bitmask) &
                                 ((~(unsigned int) 1 << n_bits) | 1));
}

/*
 * _cogl_bitmask_destroy:
 * @bitmask: A pointer to a bitmask
 *
 * Destroys any resources allocated by the bitmask
 */
static inline void
_cogl_bitmask_destroy (CoglBitmask *bitmask)
{
  if (_cogl_bitmask_has_array (bitmask))
    g_array_free ((GArray *) *bitmask, TRUE);
}

/*
 * _cogl_bitmask_clear_all:
 * @bitmask: A pointer to a bitmask
 *
 * Clears all the bits in a bitmask without destroying any resources.
 */
static inline void
_cogl_bitmask_clear_all (CoglBitmask *bitmask)
{
  if (_cogl_bitmask_has_array (bitmask))
    _cogl_bitmask_clear_all_in_array (bitmask);
  else
    *bitmask = GUINT_TO_POINTER (1);
}

G_END_DECLS

#endif /* __COGL_BITMASK_H */

