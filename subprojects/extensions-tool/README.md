# gnome-extensions-tool
gnome-extensions-tool is a command line utility for managing
GNOME Shell extensions. It is usually built as part of gnome-shell,
but can be used as a stand-alone project as well (for example to
create an extension bundle as part of continuous integration).

Bugs should be reported to the GNOME [bug tracking system][bug-tracker].

## Building
Before the project can be built stand-alone, the po directory has
to be populated with translations (from gnome-shell).

To do that, simply run the included script:
```sh
$ ./generate-translations.sh
```

## License
gnome-extensions-tool is distributed under the terms of the GNU General Public
License, version 3 or later. See the [COPYING][license] file for details.

[bug-tracker]: https://gitlab.gnome.org/GNOME/gnome-shell/issues
[license]: COPYING
