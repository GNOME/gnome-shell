/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat Inc.
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
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#ifndef META_EGL_H
#define META_EGL_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <glib-object.h>

#define META_TYPE_EGL (meta_egl_get_type ())
G_DECLARE_FINAL_TYPE (MetaEgl, meta_egl, META, EGL, GObject)

gboolean meta_egl_has_extensions (MetaEgl   *egl,
                                  EGLDisplay display,
                                  char    ***missing_extensions,
                                  char      *first_extension,
                                  ...);

gboolean meta_egl_initialize (MetaEgl   *egl,
                              EGLDisplay display,
                              GError   **error);

gboolean meta_egl_choose_config (MetaEgl      *egl,
                                 EGLDisplay    display,
                                 const EGLint *attrib_list,
                                 EGLConfig    *chosen_config,
                                 GError      **error);

EGLImageKHR meta_egl_create_image (MetaEgl        *egl,
                                   EGLDisplay      display,
                                   EGLContext      context,
                                   EGLenum         target,
                                   EGLClientBuffer buffer,
                                   const EGLint   *attrib_list,
                                   GError        **error);

gboolean meta_egl_destroy_image (MetaEgl    *egl,
                                 EGLDisplay  display,
                                 EGLImageKHR image,
                                 GError    **error);

EGLSurface meta_egl_create_pbuffer_surface (MetaEgl      *egl,
                                            EGLDisplay    display,
                                            EGLConfig     config,
                                            const EGLint *attrib_list,
                                            GError      **error);

EGLDisplay meta_egl_get_platform_display (MetaEgl      *egl,
                                          EGLenum       platform,
                                          void         *native_display,
                                          const EGLint *attrib_list,
                                          GError      **error);

gboolean meta_egl_query_wayland_buffer (MetaEgl            *egl,
                                        EGLDisplay          display,
                                        struct wl_resource *buffer,
                                        EGLint              attribute,
                                        EGLint             *value,
                                        GError            **error);

gboolean meta_egl_query_devices (MetaEgl      *egl,
                                 EGLint        max_devices,
                                 EGLDeviceEXT *devices,
                                 EGLint       *num_devices,
                                 GError      **error);

const char * meta_egl_query_device_string (MetaEgl     *egl,
                                           EGLDeviceEXT device,
                                           EGLint       name,
                                           GError     **error);

gboolean meta_egl_egl_device_has_extensions (MetaEgl      *egl,
                                             EGLDeviceEXT device,
                                             char      ***missing_extensions,
                                             char        *first_extension,
                                             ...);

gboolean meta_egl_get_output_layers (MetaEgl           *egl,
                                     EGLDisplay         display,
                                     const EGLAttrib   *attrib_list,
                                     EGLOutputLayerEXT *layers,
                                     EGLint             max_layers,
                                     EGLint            *num_layers,
                                     GError           **error);

gboolean meta_egl_query_output_layer_attrib (MetaEgl          *egl,
                                             EGLDisplay        display,
                                             EGLOutputLayerEXT layer,
                                             EGLint            attribute,
                                             EGLAttrib        *value,
                                             GError          **error);

EGLStreamKHR meta_egl_create_stream (MetaEgl      *egl,
                                     EGLDisplay    display,
                                     const EGLint *attrib_list,
                                     GError      **error);

gboolean meta_egl_destroy_stream (MetaEgl     *egl,
                                  EGLDisplay   display,
                                  EGLStreamKHR stream,
                                  GError      **error);

gboolean meta_egl_query_stream (MetaEgl     *egl,
                                EGLDisplay   display,
                                EGLStreamKHR stream,
                                EGLenum      attribute,
                                EGLint      *value,
                                GError     **error);

EGLStreamKHR meta_egl_create_stream_attrib (MetaEgl         *egl,
                                            EGLDisplay       display,
                                            const EGLAttrib *attrib_list,
                                            GError         **error);

EGLSurface meta_egl_create_stream_producer_surface (MetaEgl     *egl,
                                                    EGLDisplay   display,
                                                    EGLConfig    config,
                                                    EGLStreamKHR stream,
                                                    const EGLint *attrib_list,
                                                    GError      **error);

gboolean meta_egl_stream_consumer_output (MetaEgl          *egl,
                                          EGLDisplay        display,
                                          EGLStreamKHR      stream,
                                          EGLOutputLayerEXT layer,
                                          GError          **error);

gboolean meta_egl_stream_consumer_acquire_attrib (MetaEgl     *egl,
                                                  EGLDisplay   display,
                                                  EGLStreamKHR stream,
                                                  EGLAttrib   *attrib_list,
                                                  GError     **error);

gboolean meta_egl_stream_consumer_acquire (MetaEgl     *egl,
                                           EGLDisplay   display,
                                           EGLStreamKHR stream,
                                           GError     **error);

gboolean meta_egl_stream_consumer_gl_texture_external (MetaEgl     *egl,
                                                       EGLDisplay   display,
                                                       EGLStreamKHR stream,
                                                       GError     **error);

EGLStreamKHR meta_egl_create_stream_from_file_descriptor (MetaEgl                   *egl,
                                                          EGLDisplay                 display,
                                                          EGLNativeFileDescriptorKHR file_descriptor,
                                                          GError                   **error);

#endif /* META_EGL_H */
