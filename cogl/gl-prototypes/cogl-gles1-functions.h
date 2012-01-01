/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009, 2011 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

/* The functions in this file are part of the core GL,GLES1 and GLES2 apis */
#include "cogl-core-functions.h"

/* The functions in this file are core to GLES1 and GLES2 but not core
 * to GL but they may be extensions available for GL */
#include "cogl-in-gles-core-functions.h"

/* The functions in this file are core to GLES1 only but
 * may be extensions for GLES2 and GL */
#include "cogl-in-gles1-core-functions.h"

/* These are fixed-function APIs core to GL and GLES1 */
#include "cogl-fixed-functions.h"
