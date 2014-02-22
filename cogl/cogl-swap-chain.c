/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-object.h"

#include "cogl-swap-chain-private.h"
#include "cogl-swap-chain.h"

static void _cogl_swap_chain_free (CoglSwapChain *swap_chain);

COGL_OBJECT_DEFINE (SwapChain, swap_chain);

static void
_cogl_swap_chain_free (CoglSwapChain *swap_chain)
{
  g_slice_free (CoglSwapChain, swap_chain);
}

CoglSwapChain *
cogl_swap_chain_new (void)
{
  CoglSwapChain *swap_chain = g_slice_new0 (CoglSwapChain);

  swap_chain->length = -1; /* no preference */

  return _cogl_swap_chain_object_new (swap_chain);
}

void
cogl_swap_chain_set_has_alpha (CoglSwapChain *swap_chain,
                               CoglBool has_alpha)
{
  swap_chain->has_alpha = has_alpha;
}

void
cogl_swap_chain_set_length (CoglSwapChain *swap_chain,
                            int length)
{
  swap_chain->length = length;
}
