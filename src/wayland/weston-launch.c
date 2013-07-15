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

#include <pwd.h>
#include <grp.h>
#include <security/pam_appl.h>

#include <xf86drm.h>

#ifdef HAVE_SYSTEMD_LOGIN
#include <systemd/sd-login.h>
#endif

#include "weston-launch.h"

#define MAX_ARGV_SIZE 256

struct weston_launch {
	struct pam_conv pc;
	pam_handle_t *ph;
	int tty;
	int ttynr;
	int sock[2];
	struct passwd *pw;

	int signalfd;

	pid_t child;
	int verbose;
	char *new_user;
};

union cmsg_data { unsigned char b[4]; int fd; };

static gid_t *
read_groups(void)
{
	int n;
	gid_t *groups;
	
	n = getgroups(0, NULL);

	if (n < 0) {
		fprintf(stderr, "Unable to retrieve groups: %m\n");
		return NULL;
	}

	groups = malloc(n * sizeof(gid_t));
	if (!groups)
		return NULL;

	if (getgroups(n, groups) < 0) {
		fprintf(stderr, "Unable to retrieve groups: %m\n");
		free(groups);
		return NULL;
	}
	return groups;
}

static int
weston_launch_allowed(struct weston_launch *wl)
{
	struct group *gr;
	gid_t *groups;
	int i;
#ifdef HAVE_SYSTEMD_LOGIN
	char *session, *seat;
	int err;
#endif

	if (getuid() == 0)
		return 1;

	gr = getgrnam("weston-launch");
	if (gr) {
		groups = read_groups();
		if (groups) {
			for (i = 0; groups[i]; ++i) {
				if (groups[i] == gr->gr_gid) {
					free(groups);
					return 1;
				}
			}
			free(groups);
		}
	}

#ifdef HAVE_SYSTEMD_LOGIN
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
#endif
	
	return 0;
}

static int
pam_conversation_fn(int msg_count,
		    const struct pam_message **messages,
		    struct pam_response **responses,
		    void *user_data)
{
	return PAM_SUCCESS;
}

static int
setup_pam(struct weston_launch *wl)
{
	int err;

	wl->pc.conv = pam_conversation_fn;
	wl->pc.appdata_ptr = wl;

	err = pam_start("login", wl->pw->pw_name, &wl->pc, &wl->ph);
	if (err != PAM_SUCCESS) {
		fprintf(stderr, "failed to start pam transaction: %d: %s\n",
			err, pam_strerror(wl->ph, err));
		return -1;
	}

	err = pam_set_item(wl->ph, PAM_TTY, ttyname(wl->tty));
	if (err != PAM_SUCCESS) {
		fprintf(stderr, "failed to set PAM_TTY item: %d: %s\n",
			err, pam_strerror(wl->ph, err));
		return -1;
	}

	err = pam_open_session(wl->ph, 0);
	if (err != PAM_SUCCESS) {
		fprintf(stderr, "failed to open pam session: %d: %s\n",
			err, pam_strerror(wl->ph, err));
		return -1;
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
handle_setmaster(struct weston_launch *wl, struct msghdr *msg, ssize_t len)
{
	int ret = -1;
	struct cmsghdr *cmsg;
	struct weston_launcher_set_master *message;
	union cmsg_data *data;

	if (len != sizeof(*message)) {
		error(0, 0, "missing value in setmaster request");
		goto out;
	}

	message = msg->msg_iov->iov_base;

	cmsg = CMSG_FIRSTHDR(msg);
	if (!cmsg ||
	    cmsg->cmsg_level != SOL_SOCKET ||
	    cmsg->cmsg_type != SCM_RIGHTS) {
		error(0, 0, "invalid control message");
		goto out;
	}

	data = (union cmsg_data *) CMSG_DATA(cmsg);
	if (data->fd == -1) {
		error(0, 0, "missing drm fd in socket request");
		goto out;
	}

	if (message->set_master)
		ret = drmSetMaster(data->fd);
	else
		ret = drmDropMaster(data->fd);

	close(data->fd);

	if (wl->verbose)
		fprintf(stderr, "weston-launch: %sMaster, ret: %d, fd: %d\n",
			message->set_master ? "set" : "drop", ret, data->fd);

out:
	do {
		len = send(wl->sock[0], &ret, sizeof ret, 0);
	} while (len < 0 && errno == EINTR);
	if (len < 0)
		return -1;

	return 0;
}

static int
handle_open(struct weston_launch *wl, struct msghdr *msg, ssize_t len)
{
	int fd = -1, ret = -1;
	char control[CMSG_SPACE(sizeof(fd))];
	struct cmsghdr *cmsg;
	struct stat s;
	struct msghdr nmsg;
	struct iovec iov;
	struct weston_launcher_open *message;
	union cmsg_data *data;

	message = msg->msg_iov->iov_base;
	if ((size_t)len < sizeof(*message))
		goto err0;

	/* Ensure path is null-terminated */
	((char *) message)[len-1] = '\0';

	if (stat(message->path, &s) < 0)
		goto err0;

	fd = open(message->path, message->flags);
	if (fd < 0) {
		fprintf(stderr, "Error opening device %s: %m\n",
			message->path);
		goto err0;
	}

	if (major(s.st_rdev) != INPUT_MAJOR) {
		close(fd);
		fd = -1;
		fprintf(stderr, "Device %s is not an input device\n",
			message->path);
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
		ret = 0;
	}
	iov.iov_base = &ret;
	iov.iov_len = sizeof ret;

	if (wl->verbose)
		fprintf(stderr, "weston-launch: opened %s: ret: %d, fd: %d\n",
			message->path, ret, fd);
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
	case WESTON_LAUNCHER_DRM_SET_MASTER:
		ret = handle_setmaster(wl, &msg, len);
		break;
	}

	return ret;
}

static void
quit(struct weston_launch *wl, int status)
{
	int err;

	close(wl->signalfd);
	close(wl->sock[0]);

	if (wl->new_user) {
		err = pam_close_session(wl->ph, 0);
		if (err)
			fprintf(stderr, "pam_close_session failed: %d: %s\n",
				err, pam_strerror(wl->ph, err));
		pam_end(wl->ph, err);
	}

	exit(status);
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
	default:
		return -1;
	}

	return 0;
}

static int
setup_tty(struct weston_launch *wl, const char *tty)
{
	struct stat buf;
	char *t;

	if (!wl->new_user) {
		wl->tty = STDIN_FILENO;
	} else if (tty) {
		t = ttyname(STDIN_FILENO);
		if (t && strcmp(t, tty) == 0)
			wl->tty = STDIN_FILENO;
		else
			wl->tty = open(tty, O_RDWR | O_NOCTTY);
	} else {
		int tty0 = open("/dev/tty0", O_WRONLY | O_CLOEXEC);
		char filename[16];

		if (tty0 < 0)
			error(1, errno, "could not open tty0");

		if (ioctl(tty0, VT_OPENQRY, &wl->ttynr) < 0 || wl->ttynr == -1)
			error(1, errno, "failed to find non-opened console"); 

		snprintf(filename, sizeof filename, "/dev/tty%d", wl->ttynr);
		wl->tty = open(filename, O_RDWR | O_NOCTTY);
		close(tty0);
	}

	if (wl->tty < 0)
		error(1, errno, "failed to open tty");

	if (tty) {
		if (fstat(wl->tty, &buf) < 0)
			error(1, errno, "stat %s failed", tty);

		if (major(buf.st_rdev) != TTY_MAJOR)
			error(1, 0, "invalid tty device: %s", tty);

		wl->ttynr = minor(buf.st_rdev);
	}

	return 0;
}

static void
setup_session(struct weston_launch *wl)
{
	char **env;
	char *term;
	int i;

	if (wl->tty != STDIN_FILENO) {
		if (setsid() < 0)
			error(1, errno, "setsid failed");
		if (ioctl(wl->tty, TIOCSCTTY, 0) < 0)
			error(1, errno, "TIOCSCTTY failed - tty is in use");
	}

	term = getenv("TERM");
	clearenv();
	if (term)
		setenv("TERM", term, 1);
	setenv("USER", wl->pw->pw_name, 1);
	setenv("LOGNAME", wl->pw->pw_name, 1);
	setenv("HOME", wl->pw->pw_dir, 1);
	setenv("SHELL", wl->pw->pw_shell, 1);

	env = pam_getenvlist(wl->ph);
	if (env) {
		for (i = 0; env[i]; ++i) {
			if (putenv(env[i]) < 0)
				error(0, 0, "putenv %s failed", env[i]);
		}
		free(env);
	}
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
	if (wl->new_user)
		setup_session(wl);

	drop_privileges(wl);

	if (wl->tty != STDIN_FILENO)
		setenv_fd("WESTON_TTY_FD", wl->tty);

	setenv_fd("WESTON_LAUNCHER_SOCK", wl->sock[1]);

	unsetenv("DISPLAY");

	/* Do not give our signal mask to the new process. */
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGINT);
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
	fprintf(stderr, "  -t, --tty       Start session on alternative tty\n");
	fprintf(stderr, "  -v, --verbose   Be verbose\n");
	fprintf(stderr, "  -h, --help      Display this help message\n");
}

int
main(int argc, char *argv[])
{
	struct weston_launch wl;
	int i, c;
	char *tty = NULL;
	struct option opts[] = {
		{ "user",    required_argument, NULL, 'u' },
		{ "tty",     required_argument, NULL, 't' },
		{ "verbose", no_argument,       NULL, 'v' },
		{ "help",    no_argument,       NULL, 'h' },
		{ 0,         0,                 NULL,  0  }
	};	

	memset(&wl, 0, sizeof wl);

	while ((c = getopt_long(argc, argv, "u:t::vh", opts, &i)) != -1) {
		switch (c) {
		case 'u':
			wl.new_user = optarg;
			if (getuid() != 0)
				error(1, 0, "Permission denied. -u allowed for root only");
			break;
		case 't':
			tty = optarg;
			break;
		case 'v':
			wl.verbose = 1;
			break;
		case 'h':
			help("weston-launch");
			exit(EXIT_FAILURE);
		}
	}

	if ((argc - optind) > (MAX_ARGV_SIZE - 6))
		error(1, E2BIG, "Too many arguments to pass to weston");

	if (strcmp (argv[optind], "mutter-wayland") &&
	    strcmp (argv[optind], "gnome-shell-wayland"))
	  error(1, 0, "mutter-launch can only be used to launch mutter or gnome-shell");

	if (wl.new_user)
		wl.pw = getpwnam(wl.new_user);
	else
		wl.pw = getpwuid(getuid());
	if (wl.pw == NULL)
		error(1, errno, "failed to get username");

	if (!weston_launch_allowed(&wl))
		error(1, 0, "Permission denied. You should either:\n"
#ifdef HAVE_SYSTEMD_LOGIN
		      " - run from an active and local (systemd) session.\n"
#else
		      " - enable systemd session support for weston-launch.\n"
#endif
		      " - or add yourself to the 'weston-launch' group.");

	if (setup_tty(&wl, tty) < 0)
		exit(EXIT_FAILURE);

	if (wl.new_user && setup_pam(&wl) < 0)
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
	if (wl.tty != STDIN_FILENO)
		close(wl.tty);

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
