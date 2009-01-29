#!/usr/bin/perl

use strict;
use warnings;

use Text::Wrap;
use Pod::Usage;
use Getopt::Long;
use POSIX qw( strftime );

$Text::Wrap::columns = 74;

# git commands
our $GIT_LOG       = 'git log --pretty=format:"%at|%an|<%ae>|%h|%s"';
our $GIT_DIFF_TREE = 'git diff-tree --name-only -r';

my $help;
my $result = GetOptions(
    "h|help"    => \$help,
);

pod2usage(1) if $help;

our $revs = $ARGV[0] or undef;

my $log_cmd = $GIT_LOG;
$log_cmd .= ' ' . $revs if defined $revs;

open my $git_log, '-|', $log_cmd
    or die("Unable to invoke git-log: $!\n");

while (<$git_log>) {
    my $log_line = $_;

    chomp($log_line);

    my ($timestamp, $committer, $email, $commit_hash, $subject) =
        split /\|/, $log_line, 5;

    # use a shorter date line
    my $date = strftime("%Y-%m-%d", localtime($timestamp));

    print STDOUT $date, "  ", $committer, "  ", $email, "\n\n";

    # list the file changes
    if ($commit_hash) {
        my $diff_cmd = $GIT_DIFF_TREE . " " . $commit_hash;

        open my $git_diff, '-|', $diff_cmd
            or die("Unable to invoke git-diff-tree: $!\n");

        while (<$git_diff>) {
            my $diff_line = $_;

            chomp($diff_line);

            next if $diff_line =~ /^$commit_hash/;
            print STDOUT "\t* ", $diff_line, ":\n";
        }

        close($git_diff);
    }
    else {
        print STDOUT "\t* *:\n";
    }

    print STDOUT "\n";

    # no need to use the full body, the subject will do
    if (defined $subject) {
        $subject =~ s/\t//g;

        print STDOUT wrap("\t", "\t", $subject), "\n";
    }

    print STDOUT "\n";
}

close($git_log);

0;
__END__

=pod

=head1 NAME

gen-changelog - Creates a ChangeLog from a git log

=head1 SYNOPSIS

  gen-changelog <options>

=head1 DESCRIPTION

B<gen-changelog> is a small Perl script that reads the output of git log
and creates a file using the GNU ChangeLog format. It should be used when
creating a tarball of a project, to provide a full log of the changes to
the users.

=head1 OPTIONS

=over 4

=item -h, --help

Prints a brief help message

=item E<lt>sinceE<gt>..E<lt>untilE<gt>

Show only commits between the named two commits. When either E<lt>sinceE<gt>
or E<lt>untilE<gt> is omitted, it defaults to `HEAD`, i.e. the tip of the
current branch. For a more complete list of ways to spell E<lt>sinceE<gt>
and E<lt>untilE<gt>, see "SPECIFYING REVISIONS" section in git rev-parse.

=back

=head1 CAVEATS

B<gen-changelog> is very simple and should be tweaked to fit your use case.
It does fit the author's, but he'll gladly accept patches and requests.

=head1 EXAMPLES

=over 4

=item Print the full log and redirect it to a file

  gen-changelog > ChangeLog

=item Print the changelog of the local changes

  gen-changelog origin..HEAD

=back

=head1 AUTHOR

Emmanuele Bassi  E<lt>ebassi (at) gnome.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2009  Emmanuele Bassi

This program is free software. It can be distributed and/or modified under
the terms of Perl itself. See L<perlartistic> for further details.

=cut
