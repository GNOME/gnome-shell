/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Thomas Thurman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/** \file  Schema bindings generator.
 *
 * This program simply takes the items given in the binding lists in
 * window-bindings.h and turns them into a portion of
 * the GConf .schemas file.
 */

#include <stdio.h>
#include <glib.h>

#define _(x) x

char *liberal;

const char* window_string = \
	"    <schema>\n" \
	"      <key>/schemas/apps/metacity/%s_keybindings/%s%s</key>\n" \
	"      <applyto>/apps/metacity/%s_keybindings/%s%s</applyto>\n" \
	"      <owner>metacity</owner>\n" \
	"      <type>string</type>\n" \
	"      <default>%s</default>\n" \
	"      <locale name=\"C\">\n" \
	"         <short>%s</short>\n" \
	"         <long>\n" \
	"          %s  %s\n" \
	"         </long>\n" \
	"      </locale>\n" \
	"    </schema>\n\n\n";

void
produce_window_bindings ()
{
  /* Escaped versions of each of these fields (i.e. > => &gt;, etc) */
  gchar *esc_short, *esc_long, *esc_key;

#define item(name, suffix, param, short, long, keystroke) \
	esc_short = g_markup_escape_text (short, -1);\
	esc_long = g_markup_escape_text (long, -1);\
	if (keystroke) esc_key = g_markup_escape_text (keystroke, -1);\
	printf(\
		window_string, \
		"window", #name, suffix? suffix:"", \
		"window", #name, suffix? suffix:"",\
		keystroke? esc_key: "disabled", \
		esc_short, esc_long, liberal);\
	g_free (esc_short);\
	g_free (esc_long);\
	if (keystroke) g_free (esc_key);
#include "../src/core/window-bindings.h"
#undef item
}

int
main ()
{
  liberal = g_markup_escape_text(_("The parser is fairly liberal and allows "\
  	"lower or upper case, and also abbreviations such as \"<Ctl>\" and " \
	"\"<Ctrl>\". If you set the option to the special string " \
	"\"disabled\", then there will be no keybinding for this action."), -1);

  produce_window_bindings ();
  
  g_free (liberal);
}

/* eof schema-bindings.c */

