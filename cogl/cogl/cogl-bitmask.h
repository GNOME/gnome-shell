/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __COGL_BITMASK_H
#define __COGL_BITMASK_H

#include <glib.h>
#include "cogl-util.h"

COGL_BEGIN_DECLS

/*
 * CoglBitmask implements a growable array of bits. A CoglBitmask can
 * be allocated on the stack but it must be initialised with
 * _cogl_bitmask_init() before use and then destroyed with
 * _cogl_bitmask_destroy(). A CoglBitmask will try to avoid allocating
 * any memory unless more than the number of bits in a long - 1 bits
 * are needed.
 *
 * Internally a CoglBitmask is a pointer. If the least significant bit
 * of the pointer is 1 then the rest of the bits are directly used as
 * part of the bitmask, otherwise it is a pointer to a GArray of
 * unsigned ints. This relies on the fact the g_malloc will return a
 * pointer aligned to at least two bytes (so that the least
 * significant bit of the address is always 0). It also assumes that
 * the size of a pointer is always greater than or equal to the size
 * of a long (although there is a compile time assert to verify this).
 *
 * If the maximum possible bit number in the set is known at compile
 * time, it may make more sense to use the macros in cogl-flags.h
 * instead of this type.
 */

typedef struct _CoglBitmaskImaginaryType *CoglBitmask;

/* These are internal helper macros */
#define _cogl_bitmask_to_number(bitmask) \
  ((unsigned long) (*bitmask))
#define _cogl_bitmask_to_bits(bitmask) \
  (_cogl_bitmask_to_number (bitmask) >> 1UL)
/* The least significant bit is set to mark that no array has been
   allocated yet */
#define _cogl_bitmask_from_bits(bits) \
  ((void *) ((((unsigned long) (bits)) << 1UL) | 1UL))

/* Internal helper macro to determine whether this bitmask has a
   GArray allocated or whether the pointer is just used directly */
#define _cogl_bitmask_has_array(bitmask) \
  (!(_cogl_bitmask_to_number (bitmask) & 1UL))

/* Number of bits we can use before needing to allocate an array */
#define COGL_BITMASK_MAX_DIRECT_BITS (sizeof (unsigned long) * 8 - 1)

/*
 * _cogl_bitmask_init:
 * @bitmask: A pointer to a bitmask
 *
 * Initialises the cogl bitmask. This must be called before any other
 * bitmask functions are called. Initially all of the values are
 * zero
 */
#define _cogl_bitmask_init(bitmask) \
  G_STMT_START { *(bitmask) = _cogl_bitmask_from_bits (0); } G_STMT_END

CoglBool
_cogl_bitmask_get_from_array (const CoglBitmask *bitmask,
                              unsigned int bit_num);

void
_cogl_bitmask_set_in_array (CoglBitmask *bitmask,
                            unsigned int bit_num,
                            CoglBool value);

void
_cogl_bitmask_set_range_in_array (CoglBitmask *bitmask,
                                  unsigned int n_bits,
                                  CoglBool value);

void
_cogl_bitmask_clear_all_in_array (CoglBitmask *bitmask);

void
_cogl_bitmask_set_flags_array (const CoglBitmask *bitmask,
                               unsigned long *flags);

int
_cogl_bitmask_popcount_in_array (const CoglBitmask *bitmask);

int
_cogl_bitmask_popcount_upto_in_array (const CoglBitmask *bitmask,
                                      int upto);

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

/* The foreach function can return FALSE to stop iteration */
typedef CoglBool (* CoglBitmaskForeachFunc) (int bit_num, void *user_data);

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
                       void *user_data);

/*
 * _cogl_bitmask_get:
 * @bitmask: A pointer to a bitmask
 * @bit_num: A bit number
 *
 * Return value: whether bit number @bit_num is set in @bitmask
 */
static inline CoglBool
_cogl_bitmask_get (const CoglBitmask *bitmask, unsigned int bit_num)
{
  if (_cogl_bitmask_has_array (bitmask))
    return _cogl_bitmask_get_from_array (bitmask, bit_num);
  else if (bit_num >= COGL_BITMASK_MAX_DIRECT_BITS)
    return FALSE;
  else
    return !!(_cogl_bitmask_to_bits (bitmask) & (1UL << bit_num));
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
_cogl_bitmask_set (CoglBitmask *bitmask, unsigned int bit_num, CoglBool value)
{
  if (_cogl_bitmask_has_array (bitmask) ||
      bit_num >= COGL_BITMASK_MAX_DIRECT_BITS)
    _cogl_bitmask_set_in_array (bitmask, bit_num, value);
  else if (value)
    *bitmask = _cogl_bitmask_from_bits (_cogl_bitmask_to_bits (bitmask) |
                                        (1UL << bit_num));
  else
    *bitmask = _cogl_bitmask_from_bits (_cogl_bitmask_to_bits (bitmask) &
                                        ~(1UL << bit_num));
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
                         CoglBool value)
{
  if (_cogl_bitmask_has_array (bitmask) ||
      n_bits > COGL_BITMASK_MAX_DIRECT_BITS)
    _cogl_bitmask_set_range_in_array (bitmask, n_bits, value);
  else if (value)
    *bitmask = _cogl_bitmask_from_bits (_cogl_bitmask_to_bits (bitmask) |
                                        ~(~0UL << n_bits));
  else
    *bitmask = _cogl_bitmask_from_bits (_cogl_bitmask_to_bits (bitmask) &
                                        (~0UL << n_bits));
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
    *bitmask = _cogl_bitmask_from_bits (0);
}

/*
 * _cogl_bitmask_set_flags:
 * @bitmask: A pointer to a bitmask
 * @flags: An array of flags
 *
 * Bitwise or's the bits from @bitmask into the flags array (see
 * cogl-flags) pointed to by @flags.
 */
static inline void
_cogl_bitmask_set_flags (const CoglBitmask *bitmask,
                         unsigned long *flags)
{
  if (_cogl_bitmask_has_array (bitmask))
    _cogl_bitmask_set_flags_array (bitmask, flags);
  else
    flags[0] |= _cogl_bitmask_to_bits (bitmask);
}

/*
 * _cogl_bitmask_popcount:
 * @bitmask: A pointer to a bitmask
 *
 * Counts the number of bits that are set in the bitmask.
 *
 * Return value: the number of bits set in @bitmask.
 */
static inline int
_cogl_bitmask_popcount (const CoglBitmask *bitmask)
{
  return (_cogl_bitmask_has_array (bitmask) ?
          _cogl_bitmask_popcount_in_array (bitmask) :
          _cogl_util_popcountl (_cogl_bitmask_to_bits (bitmask)));
}

/*
 * _cogl_bitmask_popcount:
 * @Bitmask: A pointer to a bitmask
 * @upto: The maximum bit index to consider
 *
 * Counts the number of bits that are set and have an index which is
 * less than @upto.
 *
 * Return value: the number of bits set in @bitmask that are less than @upto.
 */
static inline int
_cogl_bitmask_popcount_upto (const CoglBitmask *bitmask,
                             int upto)
{
  if (_cogl_bitmask_has_array (bitmask))
    return _cogl_bitmask_popcount_upto_in_array (bitmask, upto);
  else if (upto >= COGL_BITMASK_MAX_DIRECT_BITS)
    return _cogl_util_popcountl (_cogl_bitmask_to_bits (bitmask));
  else
    return _cogl_util_popcountl (_cogl_bitmask_to_bits (bitmask) &
                                 ((1UL << upto) - 1));
}

COGL_END_DECLS

#endif /* __COGL_BITMASK_H */
