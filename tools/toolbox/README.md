# Toolbox tools

[Toolbox][toolbox] is a container tool for Linux, which allows the use of
interactive command line environments for development, without having to
install software on the host.

It is suitable for gnome-shell development, and we maintain a number of scripts
to make its use easier and more convenient.

When things go wrong, please check out the [troubleshooting section][troubleshooting] in the [GNOME Project Handbook][handbook].

## create-toolbox.sh
Create a new toolbox for gnome-shell development.

The new toolbox uses a custom container image that includes all dependencies
except [mutter]. Mutter is developed in lock-step with gnome-shell, and
therefore needs to be updated more regularly than the image. The toolbox
includes the `update-mutter` command that takes care of this, which is usually
run by the script when creating the toolbox.

When called with the `--builder` option, the script also sets up build
configuration for the [GNOME Builder IDE][builder], which allows building
and running gnome-shell from within the app.

For other options, run the script with `--help`.

## meson-build.sh

Build and install a meson project in a toolbox.

gnome-shell uses the [meson] build system, and doesn't require any build
steps other than the standard meson commands. It is still convenient to
have a single command to enter a toolbox and setup, compile and install
the project.

Run the script with `--help` to see available options.

## meson-test.sh

Run a meson project's test suite in a toolbox.

The script wraps meson's `test` command to make invoking it inside a
toolbox more convenient.

Run the script with `--help` to see available options.

## run-gnome-shell.sh

Run gnome-shell from a toolbox.

The script can be used both from within a graphical session and from a TTY.
It takes care of isolating the nested shell from the host session, and setting
up the environment to allow apps to launch inside the nested instance.

It also provides useful options for development, like running from `gdb`, or
simulating multiple monitors or a greeter session.

Run the script with `--help` to see all available options.

## toolbox-sysext-install.sh

Extract a prepared system extension directory from the toolbox and
install it as [systemd-sysext] extension on the host.

The `meson-build.sh` script has a `--sysext` option to easily add
projects to the extension directory in the toolbox:

```sh
  $ cd path/to/mutter; meson-build.sh --sysext
  $ cd path/to/gnome-shell; meson-build.sh --sysext
  $ toolbox-sysext-install.sh
```

System extensions require that the host operating system matches
the extension. For toolboxes created with the `create-toolbox.sh`
script this means the most recent Fedora release.

### Important note

gnome-shell is tightly coupled with mutter, so an extension that
only contains gnome-shell is very likely to fail. The `update-mutter`
command inside the toolbox already takes care of this, but when
building mutter manually, it is the responsibility of the caller
to ensure that the system extension contains a recent-enough
mutter build.

Run the script with `--help` to see available options.

[toolbox]: https://containertoolbx.org/
[mutter]: https://gitlab.gnome.org/GNOME/mutter
[builder]: https://apps.gnome.org/Builder/
[meson]: https://mesonbuild.com
[handbook]: https://handbook.gnome.org/
[troubleshooting]: https://handbook.gnome.org/development/toolbx.html#when-things-go-wrong
[systemd-sysext]: https://www.freedesktop.org/software/systemd/man/latest/systemd-sysext.html
