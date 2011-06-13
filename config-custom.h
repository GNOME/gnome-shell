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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* The contents of this file get #included by config.h so it is
   intended for extra configuration that needs to be included by all
   Cogl source files. */

/* The windows headers #define 'near' and 'far' to be blank. We
   commonly want to use these variable names for doing perspective
   transformation so rather than having to workaround this mis-feature
   in Windows in the code we just #undef them here. We need to do this
   after including windows.h */
#ifdef _WIN32
#include <windows.h>
#undef near
#undef far
#endif /* _WIN32 */
