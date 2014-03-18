/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Asynchronous X property getting hack */

/*
 * Copyright (C) 2002 Havoc Pennington
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from The Open Group.
 */

#ifndef ASYNC_GETPROP_H
#define ASYNC_GETPROP_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct _AgGetPropertyTask AgGetPropertyTask;

AgGetPropertyTask* ag_task_create             (Display            *display,
                                               Window              window,
                                               Atom                property,
                                               long                offset,
                                               long                length,
                                               Bool                delete,
                                               Atom                req_type);
Status             ag_task_get_reply_and_free (AgGetPropertyTask  *task,
                                               Atom               *actual_type,
                                               int                *actual_format,
                                               unsigned long      *nitems,
                                               unsigned long      *bytesafter,
                                               unsigned char     **prop);

Bool     ag_task_have_reply   (AgGetPropertyTask *task);
Atom     ag_task_get_property (AgGetPropertyTask *task);
Window   ag_task_get_window   (AgGetPropertyTask *task);
Display* ag_task_get_display  (AgGetPropertyTask *task);

AgGetPropertyTask* ag_get_next_completed_task (Display *display);

/* so other headers don't have to include internal Xlib goo */
void*    ag_Xmalloc  (unsigned long bytes);
void*    ag_Xmalloc0 (unsigned long bytes);

#endif




