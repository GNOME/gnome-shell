# Shell External Windows

Shew is a small support library for dealing with external windows.

The code for creating external windows from a handle that are suitable for
setting a transient parent was copied from [xdg-desktop-portal-gtk].

The code for exporting a handle for a GtkWindow is losely based on code
from GTK/[libportal].

## Stability
This is an unstable library with no API or ABI guarantees, and no
soname versioning.

It is not recommended to use it outside the gnome-shell project, and
may only be used as a subproject.

## License
shew is distributed under the terms of the GNU Lesser General Public License
version 2.1 or later. See the [COPYING][license] file for details.

[xdg-desktop-portal-gtk]: https://github.com/flatpak/xdg-desktop-portal-gtk
[libportal]: https://github.com/flatpak/libportal
[license]: COPYING
