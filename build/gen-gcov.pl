#!/usr/bin/perl

use strict;
use warnings;

our $gcov_file = $ARGV[0] or undef;

open my $g, '<', $gcov_file
    or die("Unable to open '$gcov_file': $!");

my ($actual, $covered, $uncovered, $percent) = (0, 0, 0, 0);

while (<$g>) {
    my $report_line = $_;

    chomp($report_line);

    $actual += 1;
    $actual -= 1 if $report_line =~ / -:/;

    $uncovered += 1 if $report_line =~ /#####:/;
}

close($g);

$covered = $actual - $uncovered;
$percent = int(($covered * 100) / $actual);

$gcov_file =~ s/^\.\///g;
$gcov_file =~ s/\.gcov$//g;

my $cover_file    = "$gcov_file:";
my $cover_literal = "$covered / $actual";
my $cover_percent = "$percent%";

format ReportLine =
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @>>>>>>>>>>>>>  @>>>>>
$cover_file,                                $cover_literal, $cover_percent
.

$~ = 'ReportLine';
write;

0;
