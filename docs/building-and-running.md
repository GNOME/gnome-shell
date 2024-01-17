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

It is possible to run gnome-shell as "nested" instance in a window.

The `run-gnome-shell` script will automatically do that when run from
a graphical session, or you can run the following command:

```sh
$ WAYLAND_DISPLAY=shell-test-1 dbus-run-session \
    gnome-shell --wayland-display=shell-test-1 --nested
```

There are limitations to the nested instance, such as keyboard shortcuts
usually not getting to the nested compositor.

In order to still bring up the [Looking Glass][lg] debugger, you can
use an [extension][lg-button] as workaround.

[lg]: ./looking-glass.md
[lg-button]: https://extensions.gnome.org/extension/2296/looking-glass-button/

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
