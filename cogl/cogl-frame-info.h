/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Owen Taylor <otaylor@redhat.com>
 */
#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_FRAME_INFO_H
#define __COGL_FRAME_INFO_H

#include <cogl/cogl-types.h>
#include <cogl/cogl-output.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _CoglFrameInfo CoglFrameInfo;
#define COGL_FRAME_INFO(X) ((CoglFrameInfo *)(X))

/**
 * cogl_is_frame_info:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglFrameInfo.
 *
 * Return value: %TRUE if the object references a #CoglFrameInfo
 *   and %FALSE otherwise.
 * Since: 2.0
 * Stability: unstable
 */
CoglBool
cogl_is_frame_info (void *object);

/**
 * cogl_frame_info_get_frame_counter:
 * @info: a #CoglFrameInfo object
 *
 * Gets the frame counter for the #CoglOnscreen that corresponds
 * to this frame.
 *
 * Return value: The frame counter value
 * Since: 1.14
 * Stability: unstable
 */
int64_t cogl_frame_info_get_frame_counter (CoglFrameInfo *info);

/**
 * cogl_frame_info_get_presentation_time:
 * @info: a #CoglFrameInfo object
 *
 * Gets the presentation time for the frame. This is the time at which
 * the frame became visible to the user.
 *
 * The presentation time measured in nanoseconds is based on a
 * monotonic time source. The time source is not necessarily
 * correlated with system/wall clock time and may represent the time
 * elapsed since some undefined system event such as when the system
 * last booted.
 *
 * <note>Linux kernel version less that 3.8 can result in
 * non-monotonic timestamps being reported when using a drm based
 * OpenGL driver. Also some buggy Mesa drivers up to 9.0.1 may also
 * incorrectly report non-monotonic timestamps.</note>
 *
 * Return value: the presentation time for the frame
 * Since: 1.14
 * Stability: unstable
 */
int64_t cogl_frame_info_get_presentation_time (CoglFrameInfo *info);

/**
 * cogl_frame_info_get_refresh_rate:
 * @info: a #CoglFrameInfo object
 *
 * Gets the refresh rate in Hertz for the output that the frame was on
 * at the time the frame was presented.
 *
 * <note>Some platforms can't associate a #CoglOutput with a
 * #CoglFrameInfo object but are able to report a refresh rate via
 * this api. Therefore if you need this information then this api is
 * more reliable than using cogl_frame_info_get_output() followed by
 * cogl_output_get_refresh_rate().</note>
 *
 * Return value: the refresh rate in Hertz
 * Since: 1.14
 * Stability: unstable
 */
float cogl_frame_info_get_refresh_rate (CoglFrameInfo *info);

/**
 * cogl_frame_info_get_output:
 * @info: a #CoglFrameInfo object
 *
 * Gets the #CoglOutput that the swapped frame was presented to.
 *
 * Return value: (transfer none): The #CoglOutput that the frame was
 *        presented to, or %NULL if this could not be determined.
 * Since: 1.14
 * Stability: unstable
 */
CoglOutput *
cogl_frame_info_get_output (CoglFrameInfo *info);

G_END_DECLS

#endif /* __COGL_FRAME_INFO_H */
