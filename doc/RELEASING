RELEASING
=========

When making a new release;

 - Verify that you don't have uncommitted and unpublished
   changes, i.e. both this:

     $ git status

   and this:

     $ git diff --stat master origin/master

   should be empty. Commit and push before the next step.

 - Clean your work directory:

     $ git clean -xdf

   This ensures that you don't have stale files lying around.

 - Run:

     $ ./autogen.sh --enable-gtk-doc --enable-docs
     $ make all
     $ make check

   And verify that the code builds from a clean Git snapshot.

 - Update the release documentation:

     - NEWS: new feature details, bugs fixed, acknowledgements
     - README: dependencies, any behavioural changes relevant to
       developers;

   then commit the changes.

 - Bump clutter_micro_version to the next even number; if this is a stable
   release, bump up clutter_interface_version by one as well. Then commit
   the changes.

 - Run:

     $ make release-publish

   which will:

     - do sanity checks on the build
     - distcheck the release
     - tag the repository with the version number
     - upload the tarball to the remote server (needs SSH account)

 - Bump clutter_micro_version to the next odd number; if this is a stable
   release, bump up clutter_interface_version by one as well. Then commit
   the changes.

 - Push the branch and then the tag, e.g.:

     $ git push origin master
     $ git push origin 1.2.4

 - Announce release to the waiting world on the blog and mailing lists. Use
   the template printed by `make release-publish`.
