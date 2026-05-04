/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 * This file is part of The Croco Library
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * See the COPYRIGHTS file for copyright information.
 */

#pragma once

#include <stdio.h>
#include "cr-utils.h"
#include "cr-term.h"
#include "cr-parsing-location.h"

G_BEGIN_DECLS

/**
 *@file
 *The declaration of the #CRDeclaration class.
 */

/*forward declaration of what is defined in cr-statement.h*/
typedef struct _CRStatement CRStatement ;

/**
 *The abstraction of a css declaration defined by the
 *css2 spec in chapter 4.
 *It is actually a chained list of property/value pairs.
 */
typedef struct _CRDeclaration CRDeclaration ;
struct _CRDeclaration
{
	/**The property.*/
	CRString *property ;

	/**The value of the property.*/
	CRTerm *value ;

	/*the ruleset that contains this declaration*/
	CRStatement *parent_statement ;

	/*the next declaration*/
	CRDeclaration *next ;

	/*the previous one declaration*/
	CRDeclaration *prev ;

	/*does the declaration have the important keyword ?*/
	gboolean important ;

	glong ref_count ;

	CRParsingLocation location ;
} ;


CRDeclaration * cr_declaration_new (CRStatement *a_statement,
				    CRString *a_property,
				    CRTerm *a_value) ;


CRDeclaration * cr_declaration_parse_list_from_buf (const guchar *a_str);

CRDeclaration * cr_declaration_append (CRDeclaration *a_this,
				       CRDeclaration *a_new) ;

gchar * cr_declaration_to_string (CRDeclaration const *a_this,
				  gulong a_indent) ;

guchar * cr_declaration_list_to_string2 (CRDeclaration const *a_this,
					 gulong a_indent,
					 gboolean a_one_decl_per_line) ;

void  cr_declaration_ref (CRDeclaration *a_this) ;

gboolean cr_declaration_unref (CRDeclaration *a_this) ;

void cr_declaration_destroy (CRDeclaration *a_this) ;

G_END_DECLS
