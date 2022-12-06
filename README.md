# GNOME Shell
GNOME Shell provides core user interface functions for the GNOME desktop,
like switching to windows and launching applications. GNOME Shell takes
advantage of the capabilities of modern graphics hardware and introduces
innovative user interface concepts to provide a visually attractive and
easy to use experience.

For more information about GNOME Shell, including instructions on how
to build GNOME Shell from source and how to get involved with the project,
see the [project wiki][project-wiki].

Bugs should be reported to the GNOME [bug tracking system][bug-tracker].
Please refer to the [*Schedule* wiki page][schedule] to see the supported versions.

## Contributing

To contribute, open merge requests at https://gitlab.gnome.org/GNOME/gnome-shell.

Commit messages should follow the [GNOME commit message
guidelines](https://wiki.gnome.org/Git/CommitMessages). If a merge request
fixes an existing issue, it is good practice to append the full issue URL
to each commit message. Try to always prefix commit subjects with a relevant
topic, such as `panel:` or `status/network:`, and it's always better to write
too much in the commit message body than too little.

## Default branch

The default development branch is `main`. If you still have a local
checkout under the old name, use:
```sh
git checkout master
git branch -m master main
git fetch
git branch --unset-upstream
git branch -u origin/main
git symbolic-ref refs/remotes/origin/HEAD refs/remotes/origin/main
```

## License
GNOME Shell is distributed under the terms of the GNU General Public License,
version 2 or later. See the [COPYING][license] file for details.

[project-wiki]: https://wiki.gnome.org/Projects/GnomeShell
[bug-tracker]: https://gitlab.gnome.org/GNOME/gnome-shell/issues
[schedule]: https://wiki.gnome.org/Schedule
[license]: COPYING
