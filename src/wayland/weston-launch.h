/*
 * Copyright © 2012 Benjamin Franzke
 *             2013 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WESTON_LAUNCH_H_
#define _WESTON_LAUNCH_H_

enum weston_launcher_message_type {
	WESTON_LAUNCHER_REQUEST,
	WESTON_LAUNCHER_EVENT,
};

enum weston_launcher_opcode {
	WESTON_LAUNCHER_OPEN              = (1 << 1 | WESTON_LAUNCHER_REQUEST),
	WESTON_LAUNCHER_DRM_SET_FD        = (2 << 1 | WESTON_LAUNCHER_REQUEST),
	WESTON_LAUNCHER_ACTIVATE_VT       = (3 << 1 | WESTON_LAUNCHER_REQUEST),
	WESTON_LAUNCHER_CONFIRM_VT_SWITCH = (4 << 1 | WESTON_LAUNCHER_REQUEST),
};

enum weston_launcher_server_opcode {
	WESTON_LAUNCHER_SERVER_REQUEST_VT_SWITCH = (1 << 1 | WESTON_LAUNCHER_EVENT),
	WESTON_LAUNCHER_SERVER_VT_ENTER          = (2 << 1 | WESTON_LAUNCHER_EVENT),
};

struct weston_launcher_message {
	int opcode;
};

struct weston_launcher_open {
	struct weston_launcher_message header;
	int flags;
	char path[0];
};

struct weston_launcher_activate_vt {
	struct weston_launcher_message header;
	signed char vt;
};

struct weston_launcher_reply {
	struct weston_launcher_message header;
	int ret;
};

struct weston_launcher_event {
	struct weston_launcher_message header;
	int detail; /* unused, but makes sure replies and events are serialized the same */
};

#endif
