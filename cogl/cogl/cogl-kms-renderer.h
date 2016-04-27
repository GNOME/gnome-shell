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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_KMS_RENDERER_H__
#define __COGL_KMS_RENDERER_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-renderer.h>

COGL_BEGIN_DECLS

/**
 * cogl_kms_renderer_set_kms_fd:
 * @renderer: A #CoglRenderer
 * @fd: The fd to kms to use
 *
 * Sets the file descriptor Cogl should use to communicate
 * to the kms driver. If -1 (the default), then Cogl will
 * open its own FD by trying to open "/dev/dri/card0".
 *
 * Since: 1.18
 * Stability: unstable
 */
void
cogl_kms_renderer_set_kms_fd (CoglRenderer *renderer,
                              int fd);

/**
 * cogl_kms_renderer_get_kms_fd:
 * @renderer: A #CoglRenderer
 *
 * Queries the file descriptor Cogl is using internally for
 * communicating with the kms driver.
 *
 * Return value: The kms file descriptor or -1 if no kms file
 *               desriptor has been opened by Cogl.
 * Stability: unstable
 */
int
cogl_kms_renderer_get_kms_fd (CoglRenderer *renderer);

struct gbm_device *
cogl_kms_renderer_get_gbm (CoglRenderer *renderer);
COGL_END_DECLS
#endif /* __COGL_KMS_RENDERER_H__ */
