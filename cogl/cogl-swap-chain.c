/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
