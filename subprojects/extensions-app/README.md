# ![logo] GNOME Extensions
GNOME Extensions is a small app for managing GNOME Shell
extensions. It is usually built as part of gnome-shell, but can be
used as a stand-alone project as well.

Bugs should be reported to the GNOME [bug tracking system][bug-tracker].

## Installation
If Extensions is not already installed on your GNOME system, we
recommend getting it from [flathub].

<a href='https://flathub.org/apps/details/org.gnome.Extensions'>
  <img width='240' alt='Download on Flathub' src='https://flathub.org/assets/badges/flathub-badge-en.png'/>
</a>

## Building
Before the project can be built stand-alone, the po directory has
to be populated with translations (from gnome-shell).

To do that, simply run the included script:
```sh
$ ./generate-translations.sh
```

## License
gnome-extensions-app is distributed under the terms of the GNU General Public
License, version 2 or later. See the [COPYING][license] file for details.

[logo]: logo.png
[bug-tracker]: https://gitlab.gnome.org/GNOME/gnome-shell/issues
[flathub]: https://flathub.org
[license]: COPYING
