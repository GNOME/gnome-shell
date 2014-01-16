/*
 * Copyright © 2012 Benjamin Franzke
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>

#include <error.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include <termios.h>
#include <linux/vt.h>
#include <linux/major.h>
#include <linux/kd.h>

#include <pwd.h>
#include <grp.h>

#include <xf86drm.h>

#include <systemd/sd-login.h>

#include "weston-launch.h"

#define MAX_ARGV_SIZE 256
#define DRM_MAJOR     226

enum vt_state {
	VT_HAS_VT,
	VT_PENDING_CONFIRM,
	VT_NOT_HAVE_VT,
};

struct weston_launch {
	int tty;
	int ttynr;
	int sock[2];
	struct passwd *pw;

	int signalfd;

	pid_t child;
	int verbose;

	struct termios terminal_attributes;
	int kb_mode;
	enum vt_state vt_state;

        int drm_fd;
};

union cmsg_data { unsigned char b[4]; int fd; };

static void quit (struct weston_launch *wl, int status);

static int
weston_launch_allowed(struct weston_launch *wl)
{
	char *session, *seat;
	int err;

	if (getuid() == 0)
		return 1;

	err = sd_pid_get_session(getpid(), &session);
	if (err == 0 && session) {
		if (sd_session_is_active(session) &&
		    sd_session_get_seat(session, &seat) == 0) {
			free(seat);
			free(session);
			return 1;
		}
		free(session);
	}
	
	return 0;
}

static int
setup_launcher_socket(struct weston_launch *wl)
{
	if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, wl->sock) < 0)
		error(1, errno, "socketpair failed");
	
	fcntl(wl->sock[0], F_SETFD, O_CLOEXEC);

	return 0;
}

static int
setup_signals(struct weston_launch *wl)
{
	int ret;
	sigset_t mask;
	struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
	ret = sigaction(SIGCHLD, &sa, NULL);
	assert(ret == 0);

	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigaction(SIGHUP, &sa, NULL);

	ret = sigemptyset(&mask);
	assert(ret == 0);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGUSR1);
	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
	assert(ret == 0);

	wl->signalfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (wl->signalfd < 0)
		return -errno;

	return 0;
}

static void
setenv_fd(const char *env, int fd)
{
	char buf[32];

	snprintf(buf, sizeof buf, "%d", fd);
	setenv(env, buf, 1);
}

static int
handle_setdrmfd(struct weston_launch *wl, struct msghdr *msg, ssize_t len)
{
        struct weston_launcher_reply reply;
	struct cmsghdr *cmsg;
	union cmsg_data *data;
	struct stat s;

	reply.header.opcode = WESTON_LAUNCHER_DRM_SET_FD;
	reply.ret = -1;

	if (wl->drm_fd != -1) {
		error(0, 0, "DRM FD already set");
		reply.ret = -EINVAL;
		goto out;
	}

	cmsg = CMSG_FIRSTHDR(msg);
	if (!cmsg ||
	    cmsg->cmsg_level != SOL_SOCKET ||
	    cmsg->cmsg_type != SCM_RIGHTS) {
		error(0, 0, "invalid control message");
		reply.ret = -EINVAL;
		goto out;
	}

	data = (union cmsg_data *) CMSG_DATA(cmsg);
	if (data->fd < 0) {
		error(0, 0, "missing drm fd in socket request");
		reply.ret = -EINVAL;
		goto out;
	}

	if (fstat(data->fd, &s) < 0) {
		reply.ret = -errno;
		goto out;
	}

	if (major(s.st_rdev) != DRM_MAJOR) {
		fprintf(stderr, "FD is not for DRM\n");
		reply.ret = -EPERM;
		goto out;
	}

	wl->drm_fd = data->fd;
	reply.ret = drmSetMaster(data->fd);
	if (reply.ret < 0)
		reply.ret = -errno;

	if (wl->verbose)
		fprintf(stderr, "mutter-launch: set drm FD, ret: %d, fd: %d\n",
			reply.ret, data->fd);

out:
	do {
		len = send(wl->sock[0], &reply, sizeof reply, 0);
	} while (len < 0 && errno == EINTR);
	if (len < 0)
		return -1;

	return 0;
}

static int
handle_confirm_vt_switch(struct weston_launch *wl, struct msghdr *msg, ssize_t len)
{
        struct weston_launcher_reply reply;

	reply.header.opcode = WESTON_LAUNCHER_CONFIRM_VT_SWITCH;
	reply.ret = -1;

	if (wl->vt_state != VT_PENDING_CONFIRM) {
		error(0, 0, "unexpected CONFIRM_VT_SWITCH");
		goto out;
	}

	if (wl->drm_fd != -1) {
		int ret;

		ret = drmDropMaster(wl->drm_fd);
		if (ret < 0) {
			fprintf(stderr, "failed to drop DRM master: %m\n");
		} else if (wl->verbose) {
			fprintf(stderr, "dropped DRM master for VT switch\n");
		}
	}

	wl->vt_state = VT_NOT_HAVE_VT;
	ioctl(wl->tty, VT_RELDISP, 1);

	if (wl->verbose)
		fprintf(stderr, "mutter-launcher: confirmed VT switch\n");

	reply.ret = 0;

out:
	do {
		len = send(wl->sock[0], &reply, sizeof reply, 0);
	} while (len < 0 && errno == EINTR);
	if (len < 0)
		return -1;

	return 0;
}

static int
handle_activate_vt(struct weston_launch *wl, struct msghdr *msg, ssize_t len)
{
        struct weston_launcher_reply reply;
	struct weston_launcher_activate_vt *message;

	reply.header.opcode = WESTON_LAUNCHER_ACTIVATE_VT;
	reply.ret = -1;

	if (len != sizeof(*message)) {
		error(0, 0, "missing value in activate_vt request");
		goto out;
	}

	message = msg->msg_iov->iov_base;

	reply.ret = ioctl(wl->tty, VT_ACTIVATE, message->vt);
	if (reply.ret < 0)
		reply.ret = -errno;

	if (wl->verbose)
		fprintf(stderr, "mutter-launch: activate VT, ret: %d\n", reply.ret);

out:
	do {
		len = send(wl->sock[0], &reply, sizeof reply, 0);
	} while (len < 0 && errno == EINTR);
	if (len < 0)
		return -1;

	return 0;
}


static int
handle_open(struct weston_launch *wl, struct msghdr *msg, ssize_t len)
{
        struct weston_launcher_reply reply;
	int fd = -1;
	char control[CMSG_SPACE(sizeof(fd))];
	struct cmsghdr *cmsg;
	struct stat s;
	struct msghdr nmsg;
	struct iovec iov;
	struct weston_launcher_open *message;
	union cmsg_data *data;

	reply.header.opcode = WESTON_LAUNCHER_OPEN;
	reply.ret = -1;

	message = msg->msg_iov->iov_base;
	if ((size_t)len < sizeof(*message))
		goto err0;

	/* Ensure path is null-terminated */
	((char *) message)[len-1] = '\0';

	if (stat(message->path, &s) < 0) {
		reply.ret = -errno;
		goto err0;
	}

	fd = open(message->path, message->flags);
	if (fd < 0) {
		fprintf(stderr, "Error opening device %s: %m\n",
			message->path);
		reply.ret = -errno;
		goto err0;
	}

	if (major(s.st_rdev) != INPUT_MAJOR) {
		close(fd);
		fd = -1;
		fprintf(stderr, "Device %s is not an input device\n",
			message->path);
		reply.ret = -EPERM;
		goto err0;
	}

err0:
	memset(&nmsg, 0, sizeof nmsg);
	nmsg.msg_iov = &iov;
	nmsg.msg_iovlen = 1;
	if (fd != -1) {
		nmsg.msg_control = control;
		nmsg.msg_controllen = sizeof control;
		cmsg = CMSG_FIRSTHDR(&nmsg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
		data = (union cmsg_data *) CMSG_DATA(cmsg);
		data->fd = fd;
		nmsg.msg_controllen = cmsg->cmsg_len;
		reply.ret = 0;
	}
	iov.iov_base = &reply;
	iov.iov_len = sizeof reply;

	if (wl->verbose)
		fprintf(stderr, "mutter-launch: opened %s: ret: %d, fd: %d\n",
			message->path, reply.ret, fd);
	do {
		len = sendmsg(wl->sock[0], &nmsg, 0);
	} while (len < 0 && errno == EINTR);

	close(fd);

	if (len < 0)
		return -1;

	return 0;
}

static int
handle_socket_msg(struct weston_launch *wl)
{
	char control[CMSG_SPACE(sizeof(int))];
	char buf[BUFSIZ];
	struct msghdr msg;
	struct iovec iov;
	int ret = -1;
	ssize_t len;
	struct weston_launcher_message *message;

	memset(&msg, 0, sizeof(msg));
	iov.iov_base = buf;
	iov.iov_len  = sizeof buf;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof control;

	do {
		len = recvmsg(wl->sock[0], &msg, 0);
	} while (len < 0 && errno == EINTR);

	if (len < 1)
		return -1;

	message = (void *) buf;
	switch (message->opcode) {
	case WESTON_LAUNCHER_OPEN:
		ret = handle_open(wl, &msg, len);
		break;
	case WESTON_LAUNCHER_DRM_SET_FD:
		ret = handle_setdrmfd(wl, &msg, len);
		break;
	case WESTON_LAUNCHER_CONFIRM_VT_SWITCH:
		ret = handle_confirm_vt_switch(wl, &msg, len);
		break;
	case WESTON_LAUNCHER_ACTIVATE_VT:
		ret = handle_activate_vt(wl, &msg, len);
		break;
	}

	return ret;
}

static void
tty_reset(struct weston_launch *wl)
{
	struct vt_mode mode = { 0 };

	if (ioctl(wl->tty, KDSKBMODE, wl->kb_mode))
		fprintf(stderr, "failed to restore keyboard mode: %m\n");

	if (ioctl(wl->tty, KDSETMODE, KD_TEXT))
		fprintf(stderr, "failed to set KD_TEXT mode on tty: %m\n");

	if (tcsetattr(wl->tty, TCSANOW, &wl->terminal_attributes) < 0)
		fprintf(stderr, "could not restore terminal to canonical mode\n");

	mode.mode = VT_AUTO;
	if (ioctl(wl->tty, VT_SETMODE, &mode) < 0)
		fprintf(stderr, "could not reset vt handling\n");
}

static void
quit(struct weston_launch *wl, int status)
{
	if (wl->child > 0)
		kill(wl->child, SIGKILL);

	close(wl->signalfd);
	close(wl->sock[0]);

	if (wl->drm_fd > 0)
		close(wl->drm_fd);

	tty_reset(wl);

	exit(status);
}

static int
handle_vt_switch(struct weston_launch *wl)
{
	struct weston_launcher_event message;
	ssize_t len;

	if (wl->vt_state == VT_HAS_VT) {
		wl->vt_state = VT_PENDING_CONFIRM;
		message.header.opcode = WESTON_LAUNCHER_SERVER_REQUEST_VT_SWITCH;
	} else if (wl->vt_state == VT_NOT_HAVE_VT) {
		wl->vt_state = VT_HAS_VT;
		ioctl(wl->tty, VT_RELDISP, VT_ACKACQ);

		if (wl->drm_fd != -1) {
			int ret;

			ret = drmSetMaster(wl->drm_fd);
			if (ret < 0) {
				fprintf(stderr, "failed to become DRM master: %m\n");
				/* This is very, very bad, and the compositor will crash soon,
				   but oh well... */
			} else if (wl->verbose) {
				fprintf(stderr, "became DRM master after VT switch\n");
			}
		}

		message.header.opcode = WESTON_LAUNCHER_SERVER_VT_ENTER;
	} else
		return -1;

	message.detail = 0;

	do {
		len = send(wl->sock[0], &message, sizeof(message), 0);
	} while (len < 0 && errno == EINTR);

	return 0;
}


static int
handle_signal(struct weston_launch *wl)
{
	struct signalfd_siginfo sig;
	int pid, status, ret;

	if (read(wl->signalfd, &sig, sizeof sig) != sizeof sig) {
		error(0, errno, "reading signalfd failed");
		return -1;
	}

	switch (sig.ssi_signo) {
	case SIGCHLD:
		pid = waitpid(-1, &status, 0);
		if (pid == wl->child) {
			wl->child = 0;
			if (WIFEXITED(status))
				ret = WEXITSTATUS(status);
			else if (WIFSIGNALED(status))
				/*
				 * If weston dies because of signal N, we
				 * return 10+N. This is distinct from
				 * weston-launch dying because of a signal
				 * (128+N).
				 */
				ret = 10 + WTERMSIG(status);
			else
				ret = 0;
			quit(wl, ret);
		}
		break;
	case SIGTERM:
	case SIGINT:
		if (wl->child)
			kill(wl->child, sig.ssi_signo);
		break;
	case SIGUSR1:
		return handle_vt_switch(wl);
	default:
		return -1;
	}

	return 0;
}

static int
setup_tty(struct weston_launch *wl)
{
	struct stat buf;
	struct termios raw_attributes;
	struct vt_mode mode = { 0 };
	char *session, *tty;
	char path[PATH_MAX];
	int ok;

	ok = sd_pid_get_session(getpid(), &session);
	if (ok < 0)
	  error(1, -ok, "could not determine current session");

	ok = sd_session_get_tty(session, &tty);
	if (ok == 0) {
		/* Old systemd only has the tty name in the TTY
		   field, new one has the full char device path.

		   Check what we have and fix it properly.
		*/
		if (strncmp(tty, "/dev", strlen("/dev")) == 0) {
			strncpy(path, tty, PATH_MAX);
			path[PATH_MAX-1] = 0;
		} else {
			snprintf(path, PATH_MAX, "/dev/%s", tty);
		}

		wl->tty = open(path, O_RDWR | O_NOCTTY | O_CLOEXEC);
		free(tty);
#ifdef HAVE_SD_SESSION_GET_VT
	} else if (ok == -ENOENT) {
		unsigned vt;

		/* Negative errnos are cool, right?
		   So cool that we can't distinguish "session not found"
		   from "key does not exist in the session file"!
		   Let's assume the latter, as we got the value
		   from sd_pid_get_session()...
		*/

		ok = sd_session_get_vt(session, &vt);
		if (ok < 0)
			error(1, -ok, "could not determine current TTY");

		snprintf(path, PATH_MAX, "/dev/tty%u", vt);
		wl->tty = open(path, O_RDWR | O_NOCTTY | O_CLOEXEC);
		free(tty);
#endif
	} else
		error(1, -ok, "could not determine current TTY");

	if (wl->tty < 0)
		error(1, errno, "failed to open tty");

	if (fstat(wl->tty, &buf) < 0)
		error(1, errno, "stat %s failed", path);

	if (major(buf.st_rdev) != TTY_MAJOR)
		error(1, 0, "invalid tty device: %s", path);

	wl->ttynr = minor(buf.st_rdev);

	if (tcgetattr(wl->tty, &wl->terminal_attributes) < 0)
		error(1, errno, "could not get terminal attributes");

	/* Ignore control characters and disable echo */
	raw_attributes = wl->terminal_attributes;
	cfmakeraw(&raw_attributes);

	/* Fix up line endings to be normal (cfmakeraw hoses them) */
	raw_attributes.c_oflag |= OPOST | OCRNL;
	/* Don't generate ttou signals */
	raw_attributes.c_oflag &= ~TOSTOP;

	if (tcsetattr(wl->tty, TCSANOW, &raw_attributes) < 0)
	        error(1, errno, "could not put terminal into raw mode");

	ioctl(wl->tty, KDGKBMODE, &wl->kb_mode);
	ok = ioctl(wl->tty, KDSKBMODE, K_OFF);
	if (ok < 0) {
		ok = ioctl(wl->tty, KDSKBMODE, K_RAW);
		if (ok < 0)
			error(1, errno, "failed to set keyboard mode on tty");
	}

	ok = ioctl(wl->tty, KDSETMODE, KD_GRAPHICS);
	if (ok < 0)
		error(1, errno, "failed to set KD_GRAPHICS mode on tty");

	wl->vt_state = VT_HAS_VT;
	mode.mode = VT_PROCESS;
	mode.relsig = SIGUSR1;
	mode.acqsig = SIGUSR1;
	ok = ioctl(wl->tty, VT_SETMODE, &mode);
	if (ok < 0)
		error(1, errno, "failed to take control of vt handling");

	return 0;
}

static void
drop_privileges(struct weston_launch *wl)
{
	if (setgid(wl->pw->pw_gid) < 0 ||
#ifdef HAVE_INITGROUPS
	    initgroups(wl->pw->pw_name, wl->pw->pw_gid) < 0 ||
#endif
	    setuid(wl->pw->pw_uid) < 0)
		error(1, errno, "dropping privileges failed");
}

static void
launch_compositor(struct weston_launch *wl, int argc, char *argv[])
{
	char command[PATH_MAX];
	char *child_argv[MAX_ARGV_SIZE];
	sigset_t mask;
	int i;

	if (wl->verbose)
		printf("weston-launch: spawned weston with pid: %d\n", getpid());

	drop_privileges(wl);

	setenv_fd("WESTON_LAUNCHER_SOCK", wl->sock[1]);
	setenv("LD_LIBRARY_PATH", LIBDIR, 1);
	unsetenv("DISPLAY");

	/* Do not give our signal mask to the new process. */
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGUSR1);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	snprintf (command, PATH_MAX, "%s \"$@\"", argv[0]);

	child_argv[0] = wl->pw->pw_shell;
	child_argv[1] = "-l";
	child_argv[2] = "-c";
	child_argv[3] = command;
	for (i = 0; i < argc; ++i)
		child_argv[4 + i] = argv[i];
	child_argv[4 + i] = NULL;

	execv(child_argv[0], child_argv);
	error(1, errno, "exec failed");
}

static void
help(const char *name)
{
	fprintf(stderr, "Usage: %s [args...] [-- [weston args..]]\n", name);
	fprintf(stderr, "  -u, --user      Start session as specified username\n");
	fprintf(stderr, "  -v, --verbose   Be verbose\n");
	fprintf(stderr, "  -h, --help      Display this help message\n");
}

int
main(int argc, char *argv[])
{
	struct weston_launch wl;
	int i, c;
	struct option opts[] = {
		{ "verbose", no_argument,       NULL, 'v' },
		{ "help",    no_argument,       NULL, 'h' },
		{ 0,         0,                 NULL,  0  }
	};	

	memset(&wl, 0, sizeof wl);
	wl.drm_fd = -1;

	while ((c = getopt_long(argc, argv, "u:t::vh", opts, &i)) != -1) {
		switch (c) {
		case 'v':
			wl.verbose = 1;
			break;
		case 'h':
			help("mutter-launch");
			exit(EXIT_FAILURE);
		}
	}

	if ((argc - optind) > (MAX_ARGV_SIZE - 6))
		error(1, E2BIG, "Too many arguments to pass to weston");

	if (optind >= argc)
		error(1, 0, "Expected program argument");

	wl.pw = getpwuid(getuid());
	if (wl.pw == NULL)
		error(1, errno, "failed to get username");

	if (!weston_launch_allowed(&wl))
		error(1, 0, "Permission denied. You must run from an active and local (systemd) session.");

	if (setup_tty(&wl) < 0)
		exit(EXIT_FAILURE);

	if (setup_launcher_socket(&wl) < 0)
		exit(EXIT_FAILURE);

	if (setup_signals(&wl) < 0)
		exit(EXIT_FAILURE);

	wl.child = fork();
	if (wl.child == -1) {
		error(1, errno, "fork failed");
		exit(EXIT_FAILURE);
	}

	if (wl.child == 0)
		launch_compositor(&wl, argc - optind, argv + optind);

	close(wl.sock[1]);

	while (1) {
		struct pollfd fds[2];
		int n;

		fds[0].fd = wl.sock[0];
		fds[0].events = POLLIN;
		fds[1].fd = wl.signalfd;
		fds[1].events = POLLIN;

		n = poll(fds, 2, -1);
		if (n < 0)
			error(0, errno, "poll failed");
		if (fds[0].revents & POLLIN)
			handle_socket_msg(&wl);
		if (fds[1].revents)
			handle_signal(&wl);
	}

	return 0;
}
