/*
 * Wayland Support
 *
 * Copyright (C) 2012,2013 Intel Corporation
 *               2013 Red Hat, Inc.
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
 */

#ifndef META_WAYLAND_VERSIONS_H
#define META_WAYLAND_VERSIONS_H

/* Protocol objects, will never change version */
/* #define META_WL_DISPLAY_VERSION  1 */
/* #define META_WL_REGISTRY_VERSION 1 */
#define META_WL_CALLBACK_VERSION 1

/* Not handled by mutter-wayland directly */
/* #define META_WL_SHM_VERSION        1 */
/* #define META_WL_SHM_POOL_VERSION   1 */
/* #define META_WL_DRM_VERSION        1 */
/* #define META_WL_BUFFER_VERSION     1 */

/* Global/master objects (version exported by wl_registry and negotiated through bind) */
#define META_WL_COMPOSITOR_VERSION          3
#define META_WL_DATA_DEVICE_MANAGER_VERSION 1
#define META_WL_SEAT_VERSION                2 /* 3 not implemented yet */
#define META_WL_OUTPUT_VERSION              2
#define META_XSERVER_VERSION                1
#define META_GTK_SHELL_VERSION              1
#define META_WL_SUBCOMPOSITOR_VERSION       1

/* Slave objects (version inherited from a master object) */
#define META_WL_DATA_OFFER_VERSION          1 /* from wl_data_device */
#define META_WL_DATA_SOURCE_VERSION         1 /* from wl_data_device */
#define META_WL_DATA_DEVICE_VERSION         1 /* from wl_data_device_manager */
#define META_WL_SURFACE_VERSION             3 /* from wl_compositor */
#define META_WL_POINTER_VERSION             2 /* from wl_seat; 3 not implemented yet */
#define META_WL_KEYBOARD_VERSION            2 /* from wl_seat; 3 not implemented yet */
#define META_WL_TOUCH_VERSION               0 /* from wl_seat; wl_touch not supported */
#define META_WL_REGION_VERSION              1 /* from wl_compositor */
#define META_GTK_SURFACE_VERSION            1 /* from gtk_shell */
#define META_XDG_SURFACE_VERSION            1 /* from xdg_shell */
#define META_XDG_POPUP_VERSION              1 /* from xdg_shell */
#define META_WL_SUBSURFACE_VERSION          1 /* from wl_subcompositor */

/* The first version to implement a specific event */
#define META_WL_SEAT_HAS_NAME               2
#define META_WL_OUTPUT_HAS_DONE             2

#endif
