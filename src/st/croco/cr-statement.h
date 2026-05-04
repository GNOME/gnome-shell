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
 * Author: Dodji Seketeli
 * See COPYRIGHTS file for copyright information.
 */

#include <stdio.h>
#include "cr-utils.h"
#include "cr-term.h"
#include "cr-selector.h"
#include "cr-declaration.h"

#pragma once

G_BEGIN_DECLS

/**
 *@file
 *Declaration of the #CRStatement class.
 */

/*
 *forward declaration of CRStyleSheet which is defined in
 *cr-stylesheet.h
 */

struct _CRStatement ;

/*
 *typedef struct _CRStatement CRStatement ;
 *this is forward declared in
 *cr-declaration.h already.
 */

struct _CRAtMediaRule ;
typedef struct _CRAtMediaRule CRAtMediaRule ;

typedef struct _CRRuleSet CRRuleSet ;

/**
 *The abstraction of a css ruleset.
 *A ruleset is made of a list of selectors,
 *followed by a list of declarations.
 */
struct _CRRuleSet
{
	/**A list of instances of #CRSimpeSel*/
	CRSelector *sel_list ;

	/**A list of instances of #CRDeclaration*/
	CRDeclaration *decl_list ;
} ;

/*
 *a forward declaration of CRStylesheet.
 *CRStylesheet is actually declared in
 *cr-stylesheet.h
 */
struct _CRStyleSheet ;
typedef struct _CRStyleSheet CRStyleSheet;


/**The \@import rule abstraction.*/
typedef struct _CRAtImportRule CRAtImportRule ;
struct _CRAtImportRule
{
	/**the url of the import rule*/
	CRString *url ;

        GList *media_list ;

	/**
	 *the stylesheet fetched from the url, if any.
	 *this is not "owned" by #CRAtImportRule which means
	 *it is not destroyed by the destructor of #CRAtImportRule.
	 */
	CRStyleSheet * sheet;
};


/**abstraction of an \@media rule*/
struct _CRAtMediaRule
{
	GList *media_list ;
	CRStatement *rulesets ;
} ;


typedef struct _CRAtPageRule CRAtPageRule ;
/**The \@page rule abstraction*/
struct _CRAtPageRule
{
	/**a list of instances of #CRDeclaration*/
	CRDeclaration *decl_list ;

	/**page selector. Is a pseudo selector*/
	CRString *name ;
	CRString *pseudo ;
} ;

/**The \@charset rule abstraction*/
typedef struct _CRAtCharsetRule CRAtCharsetRule ;
struct _CRAtCharsetRule
{
	CRString * charset ;
};

/**The abstraction of the \@font-face rule.*/
typedef struct _CRAtFontFaceRule CRAtFontFaceRule ;
struct _CRAtFontFaceRule
{
	/*a list of instanaces of #CRDeclaration*/
	CRDeclaration *decl_list ;
} ;


/**
 *The possible types of css2 statements.
 */
enum CRStatementType
{
	/**
	 *A generic css at-rule
	 *each unknown at-rule will
	 *be of this type.
	 */

        /**A css at-rule*/
	AT_RULE_STMT = 0,

	/*A css ruleset*/
	RULESET_STMT,

	/**A css2 import rule*/
	AT_IMPORT_RULE_STMT,

	/**A css2 media rule*/
	AT_MEDIA_RULE_STMT,

	/**A css2 page rule*/
	AT_PAGE_RULE_STMT,

	/**A css2 charset rule*/
	AT_CHARSET_RULE_STMT,

	/**A css2 font face rule*/
	AT_FONT_FACE_RULE_STMT
} ;


/**
 *The abstraction of css statement as defined
 *in the chapter 4 and appendix D.1 of the css2 spec.
 *A statement is actually a double chained list of
 *statements.A statement can be a ruleset, an \@import
 *rule, an \@page rule etc ...
 */
struct _CRStatement
{
	/**
	 *The type of the statement.
	 */
	enum CRStatementType type ;

	union
	{
		CRRuleSet *ruleset ;
		CRAtImportRule *import_rule ;
		CRAtMediaRule *media_rule ;
		CRAtPageRule *page_rule ;
		CRAtCharsetRule *charset_rule ;
		CRAtFontFaceRule *font_face_rule ;
	} kind ;

        /*
         *the specificity of the selector
         *that matched this statement.
         *This is only used by the cascading
         *order determination algorithm.
         */
        gulong specificity ;

        /*
         *the style sheet that contains
         *this css statement.
         */
        CRStyleSheet *parent_sheet ;
	CRStatement *next ;
	CRStatement *prev ;


        /**
         *a custom pointer useable by
         *applications that use libcroco.
         *libcroco itself will never modify
         *this pointer.
         */
        gpointer app_data ;
} ;


CRStatement*
cr_statement_new_ruleset (CRStyleSheet *a_sheet,
                          CRSelector *a_sel_list,
			  CRDeclaration *a_decl_list,
			  CRStatement *a_media_rule) ;

CRStatement*
cr_statement_new_at_import_rule (CRStyleSheet *a_container_sheet,
                                 CRString *a_url,
                                 GList *a_media_list,
				 CRStyleSheet *a_imported_sheet) ;

CRStatement *
cr_statement_new_at_media_rule (CRStyleSheet *a_sheet,
                                CRStatement *a_ruleset,
				GList *a_media) ;

CRStatement *
cr_statement_new_at_charset_rule (CRStyleSheet *a_sheet,
                                  CRString *a_charset) ;

CRStatement *
cr_statement_new_at_font_face_rule (CRStyleSheet *a_sheet,
                                    CRDeclaration *a_font_decls) ;

CRStatement *
cr_statement_new_at_page_rule (CRStyleSheet *a_sheet,
                               CRDeclaration *a_decl_list,
			       CRString *a_name,
			       CRString *a_pseudo) ;

enum CRStatus
cr_statement_set_parent_sheet (CRStatement *a_this,
                               CRStyleSheet *a_sheet) ;

CRStatement *
cr_statement_append (CRStatement *a_this,
		     CRStatement *a_new) ;

gchar *
cr_statement_to_string (CRStatement const * a_this, gulong a_indent) ;

gchar*
cr_statement_list_to_string (CRStatement const *a_this, gulong a_indent) ;

void
cr_statement_destroy (CRStatement *a_this) ;

G_END_DECLS
