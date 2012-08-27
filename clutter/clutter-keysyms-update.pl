#!/usr/bin/env perl

# Author  : Simos Xenitellis <simos at gnome dot org>.
# Authos  : Bastien Nocera <hadess@hadess.net>
# Version : 1.2
#
# Notes   : It downloads keysymdef.h from the Internet, if not found locally,
# Notes   : and creates an updated clutter-keysyms.h

use strict;

my $update_url = 'http://git.clutter-project.org/clutter/plain/clutter/clutter-keysyms-update.pl';

# Used for reading the keysymdef symbols.
my @keysymelements;

my $keysymdef_url = 'http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h';

if ( ! -f "keysymdef.h" )
{
	print "Trying to download keysymdef.h from\n", $keysymdef_url, "\n";
	die "Unable to download keysymdef.h: $!" 
		unless system("wget -c -O keysymdef.h \"$keysymdef_url\"") == 0;
	print " done.\n\n";
}
else
{
	print "We are using existing keysymdef.h found in this directory.\n";
	print "It is assumed that you took care and it is a recent version\n";
}

my $XF86keysym_url = 'http://cgit.freedesktop.org/xorg/proto/x11proto/plain/XF86keysym.h';

if ( ! -f "XF86keysym.h" )
{
	print "Trying to download XF86keysym.h from\n", $XF86keysym_url, "\n";
	die "Unable to download keysymdef.h: $!\n" 
		unless system("wget -c -O XF86keysym.h \"$XF86keysym_url\"") == 0;
	print " done.\n\n";
}
else
{
	print "We are using existing XF86keysym.h found in this directory.\n";
	print "It is assumed that you took care and it is a recent version\n";
}

if ( -f "clutter-keysyms.h" )
{
	print "There is already a clutter-keysyms.h file in this directory. We are not overwriting it.\n";
	print "Please move it somewhere else in order to run this script.\n";
	die "Exiting...\n\n";
}

die "Could not open file keysymdef.h: $!\n"
    unless open(IN_KEYSYMDEF, "<:utf8", "keysymdef.h");

# Output: clutter/clutter/clutter-keysyms.h
die "Could not open file clutter-keysyms.h: $!\n"
    unless open(OUT_KEYSYMS, ">:utf8", "clutter-keysyms.h");

my $LICENSE_HEADER= <<EOF;
/* Clutter
 *
 * Copyright (C) 2006, 2007, 2008  OpenedHand Ltd
 * Copyright (C) 2009, 2010  Intel Corp
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses>.
 */

EOF

print OUT_KEYSYMS $LICENSE_HEADER;

print OUT_KEYSYMS<<EOF;

/*
 * File auto-generated from script at:
 *  $update_url
 *
 * using the input files:
 *  $keysymdef_url
 * and
 *  $XF86keysym_url
 */

#ifndef __CLUTTER_KEYSYMS_H__
#define __CLUTTER_KEYSYMS_H__

EOF

while (<IN_KEYSYMDEF>)
{
	next if ( ! /^#define / );

	@keysymelements = split(/\s+/);
	die "Internal error, no \@keysymelements: $_\n" unless @keysymelements;

	$_ = $keysymelements[1];
	die "Internal error, was expecting \"XC_*\", found: $_\n" if ( ! /^XK_/ );
	
	$_ = $keysymelements[2];
	die "Internal error, was expecting \"0x*\", found: $_\n" if ( ! /^0x/ );

	my $element = $keysymelements[1];
	my $binding = $element;
	$binding =~ s/^XK_/CLUTTER_KEY_/g;

	printf OUT_KEYSYMS "#define %s 0x%03x\n", $binding, hex($keysymelements[2]);
}

close IN_KEYSYMDEF;

#$cluttersyms{"0"} = "0000";

# Source: http://gitweb.freedesktop.org/?p=xorg/proto/x11proto.git;a=blob;f=XF86keysym.h
die "Could not open file XF86keysym.h: $!\n" unless open(IN_XF86KEYSYM, "<:utf8", "XF86keysym.h");

while (<IN_XF86KEYSYM>)
{
	next if ( ! /^#define / );

	@keysymelements = split(/\s+/);
	die "Internal error, no \@keysymelements: $_\n" unless @keysymelements;

	$_ = $keysymelements[1];
	die "Internal error, was expecting \"XF86XK_*\", found: $_\n" if ( ! /^XF86XK_/ );

	# Work-around https://bugs.freedesktop.org/show_bug.cgi?id=11193
	if ($_ eq "XF86XK_XF86BackForward") {
		$keysymelements[1] = "XF86XK_AudioForward";
	}
	# XF86XK_Clear could end up a dupe of XK_Clear
	# XF86XK_Select could end up a dupe of XK_Select
	if ($_ eq "XF86XK_Clear") {
		$keysymelements[1] = "XF86XK_WindowClear";
	}
	if ($_ eq "XF86XK_Select") {
		$keysymelements[1] = "XF86XK_SelectButton";
	}

	# Ignore XF86XK_Q
	next if ( $_ eq "XF86XK_Q");
	# XF86XK_Calculater is misspelled, and a dupe
	next if ( $_ eq "XF86XK_Calculater");

	$_ = $keysymelements[2];
	die "Internal error, was expecting \"0x*\", found: $_\n" if ( ! /^0x/ );

	my $element = $keysymelements[1];
	my $binding = $element;
	$binding =~ s/^XF86XK_/CLUTTER_KEY_/g;

	printf OUT_KEYSYMS "#define %s 0x%03x\n", $binding, hex($keysymelements[2]);
}

close IN_XF86KEYSYM;


print OUT_KEYSYMS<<EOF;

#endif /* __CLUTTER_KEYSYMS_H__ */
EOF

foreach my $f (qw/ keysymdef.h XF86keysym.h /) {
    unlink $f or die "Unable to delete $f: $!";
}

printf "We just finished converting keysymdef.h to clutter-keysyms.h "
     . "and deprecated/clutter-keysyms.h\nThank you\n";
