# Building and Running

## Building gnome-shell

gnome-shell uses the [meson] build system, and can be compiled
with the following commands:

```sh
$ meson setup _build
$ meson compile -C _build
```

Unfortunately gnome-shell has a non-trivial number of dependencies
that cannot always be satisfied by distribution packages.

This is particular true for [mutter], which is developed in lock-step
with gnome-shell and always has to be built from source.

[meson]: https://mesonbuild.com/
[mutter]: https://mutter.gnome.org

## Toolbox

[Toolbox][toolbox] is a container tool for Linux, which allows the use of
interactive command line environments for development, without having to
install software on the host.

It is suitable for gnome-shell development, and we maintain a number of scripts
to make its use easier and more convenient.

You can set up a new container that satisfies all build- and runtime
dependencies with the following script:

```sh
$ tools/toolbox/create-toolbox.sh
```

The script will download the container image and build mutter, so it is
expected that it will take a while.

Once you have a working container, the following scripts can be
used to build and run gnome-shell:

```sh
$ tools/toolbox/meson-build.sh
$ tools/toolbox/run-gnome-shell.sh
```

If building or running fails with errors about `meta`, `clutter` or `cogl`,
there was probably an incompatible change in mutter. You can update the
dependency with the following command:

```sh
$ toolbox run --container gnome-shell-devel update-mutter
```

Refer to the [README][toolbox-tools] for further information on the scripts.

[toolbox]: https://containertoolbx.org/
[toolbox-tools]: ../tools/toolbox/README.md

## Running a nested instance

It is possible to run gnome-shell as nested instance using Devkit.

The `run-gnome-shell` script will automatically do that when run from
a graphical session, or you can run the following command:

```sh
$ dbus-run-session gnome-shell --wayland --devkit
```

## Native

Sometimes it's necessary to run the "native backend", on real display hardware.

To do that, switch to a tty and either use the `run-gnome-shell` script
or run the following command:

```sh
$ dbus-run-session gnome-shell --wayland
```

Some functionality is not available when running gnome-shell outside a GNOME
session, including logout. To exit gnome-shell, bring up the run dialog with
<kbd>Alt</kbd> <kbd>F2</kbd> and enter `debugexit`.

## Running under valgrind with a full session

Sometimes it is necessary to run gnome-shell under valgrind within a full GNOME
session. This can be achieved by overriding the `ExecStart` command of the
systemd service file used to launch gnome-shell with a drop-in config file.
Starting gnome-shell under valgrind can also take some time which requires
adjusting the timeouts of the service as well. This command can be used to
create such a drop-in file for the current user:

```sh
$ systemctl --user edit org.gnome.Shell@wayland.service --drop-in valgrind
```

This opens an editor in which the following content has to be added:

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/valgrind --log-file=/tmp/gs-valgrind.txt --enable-debuginfod=no --leak-check=full --show-leak-kinds=definite /usr/bin/gnome-shell
TimeoutStartSec=300
TimeoutStopSec=300
```

Then the next time when logging into a session as the current user, gnome-shell
will be running under valgrind and create a log file under
`/tmp/gs-valgrind.txt`.

After ending the valgrind session and obtaining the log file, the drop-in file
needs to be removed again before starting the next session. Otherwise the log
will get overwritten. This can be done using following command from a VT:

```sh
$ systemctl --user revert org.gnome.Shell@wayland.service
```

For X11 sessions use `org.gnome.Shell@x11.service` in these commands instead.
