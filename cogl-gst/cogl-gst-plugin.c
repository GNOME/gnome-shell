/*
 * Cogl-GStreamer.
 *
 * GStreamer integration library for Cogl.
 *
 * cogl-gst-video-sink.c - Gstreamer Video Sink that renders to a
 *                         Cogl Pipeline.
 *
 * Authored by Jonathan Matthew  <jonathan@kaolin.wh9.net>,
 *             Chris Lord        <chris@openedhand.com>
 *             Damien Lespiau    <damien.lespiau@intel.com>
 *             Matthew Allum     <mallum@openedhand.com>
 *             Plamena Manolova  <plamena.n.manolova@intel.com>
 *
 * Copyright (C) 2007, 2008 OpenedHand
 * Copyright (C) 2009, 2010, 2013 Intel Corporation
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "cogl-gst-video-sink.h"

#define PACKAGE "CoglGst"

static CoglBool
_plugin_init (GstPlugin *coglgstvideosink)
{
  return gst_element_register (coglgstvideosink,
                               "coglsink",
                               GST_RANK_PRIMARY,
                               COGL_GST_TYPE_VIDEO_SINK);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
                   GST_VERSION_MINOR,
                   cogl,
                   "Sends video data from GStreamer to a Cogl pipeline",
                   _plugin_init,
                   COGL_VERSION_STRING,
                   "LGPL",
                   PACKAGE,
                   "http://cogl3d.org/")
