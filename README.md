# GNOME Shell

GNOME Shell provides core user interface functions for the GNOME desktop,
like switching to windows and launching applications. GNOME Shell takes
advantage of the capabilities of modern graphics hardware and introduces
innovative user interface concepts to provide a visually attractive and
easy to use experience.

All interactions with the project should follow the [Code of Conduct][conduct].

[conduct]: https://conduct.gnome.org/

## Supported versions

Upstream gnome-shell only supports the most recent stable release series,
the previous stable release series, and the current development release
series. Any older stable release series are no longer supported, although
they may still receive backported security updates in long-term support
distributions. Such support is up to the distributions, though.

Please refer to the [schedule] to see when a new version will be released.

[schedule]: https://release.gnome.org/calendar

## Reporting bugs

Bugs should be reported to the [issue tracking system][bug-tracker].

The [GNOME handbook][bug-handbook] has useful information for creating
effective issue reports.

If you are using extensions, please confirm that an issue still happens
without extensions. To properly disable extensions you can use the
[extensions-app] and then restart your session. Disabling extensions
without a restart is not sufficient to rule out extensions as the
cause of a bug. If an issue can only be reproduced with a certain
extension, please file an issue report against that extension first.

Please note that the issue tracker is meant to be used for
actionable issues only.

For support questions, feedback on changes or general discussions,
you can use:

 - the [#gnome-shell matrix room][matrix-room]
 - the `Desktop` category or `shell` tag on [GNOME Discourse][discourse]

[bug-tracker]: https://gitlab.gnome.org/GNOME/gnome-shell/issues
[bug-handbook]: https://handbook.gnome.org/issues/reporting.html
[extensions-app]: https://apps.gnome.org/Extensions
[matrix-room]: https://matrix.to/#/#gnome-shell:gnome.org
[discourse]: https://discourse.gnome.org

## Contributing

To contribute, open merge requests at https://gitlab.gnome.org/GNOME/gnome-shell.

Commit messages should follow the [commit message guidelines][commit-messages].

[commit-messages]: docs/commit-messages.md

## License

GNOME Shell is distributed under the terms of the GNU General Public License,
version 2 or later. See the [COPYING][license] file for details.

[license]: COPYING
