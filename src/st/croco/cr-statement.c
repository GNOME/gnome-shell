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
 * Author: Dodji Seketeli.
 * See COPYRIGHTS files for copyrights information.
 */

#include <string.h>
#include "cr-statement.h"
#include "cr-parser.h"

/**
 *@file
 *Definition of the #CRStatement class.
 */

#define DECLARATION_INDENT_NB 2

static void
cr_statement_clear (CRStatement * a_this)
{
        g_return_if_fail (a_this);

        switch (a_this->type) {
        case AT_RULE_STMT:
                break;
        case RULESET_STMT:
                if (!a_this->kind.ruleset)
                        return;
                if (a_this->kind.ruleset->sel_list) {
                        cr_selector_unref (a_this->kind.ruleset->sel_list);
                        a_this->kind.ruleset->sel_list = NULL;
                }
                if (a_this->kind.ruleset->decl_list) {
                        cr_declaration_destroy
                                (a_this->kind.ruleset->decl_list);
                        a_this->kind.ruleset->decl_list = NULL;
                }
                g_free (a_this->kind.ruleset);
                a_this->kind.ruleset = NULL;
                break;

        case AT_IMPORT_RULE_STMT:
                if (!a_this->kind.import_rule)
                        return;
                if (a_this->kind.import_rule->url) {
                        cr_string_destroy
                                (a_this->kind.import_rule->url) ;
                        a_this->kind.import_rule->url = NULL;
                }
                g_free (a_this->kind.import_rule);
                a_this->kind.import_rule = NULL;
                break;

        case AT_MEDIA_RULE_STMT:
                if (!a_this->kind.media_rule)
                        return;
                if (a_this->kind.media_rule->rulesets) {
                        cr_statement_destroy
                                (a_this->kind.media_rule->rulesets);
                        a_this->kind.media_rule->rulesets = NULL;
                }
                if (a_this->kind.media_rule->media_list) {
                        GList *cur = NULL;

                        for (cur = a_this->kind.media_rule->media_list;
                             cur; cur = cur->next) {
                                if (cur->data) {
                                        cr_string_destroy ((CRString *) cur->data);
                                        cur->data = NULL;
                                }

                        }
                        g_clear_list (&a_this->kind.media_rule->media_list, NULL);
                }
                g_free (a_this->kind.media_rule);
                a_this->kind.media_rule = NULL;
                break;

        case AT_PAGE_RULE_STMT:
                if (!a_this->kind.page_rule)
                        return;

                if (a_this->kind.page_rule->decl_list) {
                        cr_declaration_destroy
                                (a_this->kind.page_rule->decl_list);
                        a_this->kind.page_rule->decl_list = NULL;
                }
                if (a_this->kind.page_rule->name) {
                        cr_string_destroy
                                (a_this->kind.page_rule->name);
                        a_this->kind.page_rule->name = NULL;
                }
                if (a_this->kind.page_rule->pseudo) {
                        cr_string_destroy
                                (a_this->kind.page_rule->pseudo);
                        a_this->kind.page_rule->pseudo = NULL;
                }
                g_free (a_this->kind.page_rule);
                a_this->kind.page_rule = NULL;
                break;

        case AT_CHARSET_RULE_STMT:
                if (!a_this->kind.charset_rule)
                        return;

                if (a_this->kind.charset_rule->charset) {
                        cr_string_destroy
                                (a_this->kind.charset_rule->charset);
                        a_this->kind.charset_rule->charset = NULL;
                }
                g_free (a_this->kind.charset_rule);
                a_this->kind.charset_rule = NULL;
                break;

        case AT_FONT_FACE_RULE_STMT:
                if (!a_this->kind.font_face_rule)
                        return;

                if (a_this->kind.font_face_rule->decl_list) {
                        cr_declaration_unref
                                (a_this->kind.font_face_rule->decl_list);
                        a_this->kind.font_face_rule->decl_list = NULL;
                }
                g_free (a_this->kind.font_face_rule);
                a_this->kind.font_face_rule = NULL;
                break;

        default:
                break;
        }
}

/**
 * cr_statement_ruleset_to_string:
 *
 *@a_this: the current instance of #CRStatement
 *@a_indent: the number of whitespace to use for indentation
 *
 *Serializes the ruleset statement into a string
 *
 *Returns the newly allocated serialised string. Must be freed
 *by the caller, using g_free().
 */
static gchar *
cr_statement_ruleset_to_string (CRStatement const * a_this, glong a_indent)
{
        GString *stringue = NULL;
        gchar *tmp_str = NULL,
                *result = NULL;

        g_return_val_if_fail (a_this && a_this->type == RULESET_STMT, NULL);

        stringue = g_string_new (NULL);

        if (a_this->kind.ruleset->sel_list) {
                if (a_indent)
                        cr_utils_dump_n_chars2 (' ', stringue, a_indent);

                tmp_str =
                        (gchar *) cr_selector_to_string (a_this->kind.ruleset->
                                               sel_list);
                if (tmp_str) {
                        g_string_append (stringue, tmp_str);
                        g_free (tmp_str);
                        tmp_str = NULL;
                }
        }
        g_string_append (stringue, " {\n");
        if (a_this->kind.ruleset->decl_list) {
                tmp_str = (gchar *) cr_declaration_list_to_string2
                        (a_this->kind.ruleset->decl_list,
                         a_indent + DECLARATION_INDENT_NB, TRUE);
                if (tmp_str) {
                        g_string_append (stringue, tmp_str);
                        g_free (tmp_str);
                        tmp_str = NULL;
                }
                g_string_append (stringue, "\n");
                cr_utils_dump_n_chars2 (' ', stringue, a_indent);
        }
        g_string_append (stringue, "}");
        result = g_string_free_and_steal (stringue);

        g_clear_pointer (&tmp_str, g_free);
        return result;
}


/**
 * cr_statement_font_face_rule_to_string:
 *
 *@a_this: the current instance of #CRStatement to consider
 *It must be a font face rule statement.
 *@a_indent: the number of white spaces of indentation.
 *
 *Serializes a font face rule statement into a string.
 *
 *Returns the serialized string. Must be deallocated by the caller
 *using g_free().
 */
static gchar *
cr_statement_font_face_rule_to_string (CRStatement const * a_this,
                                       glong a_indent)
{
        gchar *result = NULL, *tmp_str = NULL ;
        GString *stringue = NULL ;

        g_return_val_if_fail (a_this
                              && a_this->type == AT_FONT_FACE_RULE_STMT,
                              NULL);

        if (a_this->kind.font_face_rule->decl_list) {
                stringue = g_string_new (NULL) ;
                g_return_val_if_fail (stringue, NULL) ;
                if (a_indent)
                        cr_utils_dump_n_chars2 (' ', stringue,
                                        a_indent);
                g_string_append (stringue, "@font-face {\n");
                tmp_str = (gchar *) cr_declaration_list_to_string2
                        (a_this->kind.font_face_rule->decl_list,
                         a_indent + DECLARATION_INDENT_NB, TRUE) ;
                if (tmp_str) {
                        g_string_append (stringue,
                                         tmp_str) ;
                        g_free (tmp_str) ;
                        tmp_str = NULL ;
                }
                g_string_append (stringue, "\n}");
        }
        if (stringue) {
                result = g_string_free_and_steal (stringue);
                stringue = NULL ;
        }
        return result ;
}


/**
 * cr_statement_charset_to_string:
 *
 *Serialises an \@charset statement into a string.
 *@a_this: the statement to serialize.
 *@a_indent: the number of indentation spaces
 *
 *Returns the serialized charset statement. Must be
 *freed by the caller using g_free().
 */
static gchar *
cr_statement_charset_to_string (CRStatement const *a_this,
                                gulong a_indent)
{
        gchar *str = NULL ;
        GString *stringue = NULL ;

        g_return_val_if_fail (a_this
                              && a_this->type == AT_CHARSET_RULE_STMT,
                              NULL) ;

        if (a_this->kind.charset_rule
            && a_this->kind.charset_rule->charset
            && a_this->kind.charset_rule->charset->stryng
            && a_this->kind.charset_rule->charset->stryng->str) {
                str = g_strndup (a_this->kind.charset_rule->charset->stryng->str,
                                 a_this->kind.charset_rule->charset->stryng->len);
                g_return_val_if_fail (str, NULL);
                stringue = g_string_new (NULL) ;
                g_return_val_if_fail (stringue, NULL) ;
                cr_utils_dump_n_chars2 (' ', stringue, a_indent);
                g_string_append_printf (stringue,
                                        "@charset \"%s\" ;", str);
                g_clear_pointer (&str, g_free);
        }
        if (stringue) {
                str = g_string_free_and_steal (stringue);
        }
        return str ;
}


/**
 * cr_statement_at_page_rule_to_string:
 *
 *Serialises the at page rule statement into a string
 *@a_this: the current instance of #CRStatement. Must
 *be an "\@page" rule statement.
 *
 *Returns the serialized string. Must be freed by the caller
 */
static gchar *
cr_statement_at_page_rule_to_string (CRStatement const *a_this,
                                     gulong a_indent)
{
        GString *stringue = NULL;
        gchar *result = NULL ;

        stringue = g_string_new (NULL) ;

        cr_utils_dump_n_chars2 (' ', stringue, a_indent) ;
        g_string_append (stringue, "@page");
	if (a_this->kind.page_rule->name
	    && a_this->kind.page_rule->name->stryng) {
		g_string_append_printf
		  (stringue, " %s",
		   a_this->kind.page_rule->name->stryng->str) ;
        } else {
                g_string_append (stringue, " ");
        }
	if (a_this->kind.page_rule->pseudo
	    && a_this->kind.page_rule->pseudo->stryng) {
		g_string_append_printf
		  (stringue,  " :%s",
		   a_this->kind.page_rule->pseudo->stryng->str) ;
        }
        if (a_this->kind.page_rule->decl_list) {
                gchar *str = NULL ;
                g_string_append (stringue, " {\n");
                str = (gchar *) cr_declaration_list_to_string2
                        (a_this->kind.page_rule->decl_list,
                         a_indent + DECLARATION_INDENT_NB, TRUE) ;
                if (str) {
                        g_string_append (stringue, str) ;
                        g_free (str) ;
                        str = NULL ;
                }
                g_string_append (stringue, "\n}\n");
        }
        result = g_string_free_and_steal (stringue) ;
        stringue = NULL ;
        return result ;
}


/**
 *Serializes an \@media statement.
 *@param a_this the current instance of #CRStatement
 *@param a_indent the number of spaces of indentation.
 *@return the serialized \@media statement. Must be freed
 *by the caller using g_free().
 */
static gchar *
cr_statement_media_rule_to_string (CRStatement const *a_this,
                                   gulong a_indent)
{
        gchar *str = NULL ;
        GString *stringue = NULL ;
        GList const *cur = NULL;

        g_return_val_if_fail (a_this->type == AT_MEDIA_RULE_STMT,
                              NULL);

        if (a_this->kind.media_rule) {
                stringue = g_string_new (NULL) ;
                cr_utils_dump_n_chars2 (' ', stringue, a_indent);
                g_string_append (stringue, "@media");

                for (cur = a_this->kind.media_rule->media_list; cur;
                     cur = cur->next) {
                        if (cur->data) {
                                gchar *str2 = cr_string_dup2
                                        ((CRString const *) cur->data);

                                if (str2) {
                                        if (cur->prev) {
                                                g_string_append
                                                        (stringue,
                                                         ",");
                                        }
                                        g_string_append_printf
                                                (stringue,
                                                 " %s", str2);
                                        g_free (str2);
                                        str2 = NULL;
                                }
                        }
                }
                g_string_append (stringue, " {\n");
                str = cr_statement_list_to_string
                        (a_this->kind.media_rule->rulesets,
                         a_indent + DECLARATION_INDENT_NB) ;
                if (str) {
                        g_string_append (stringue, str) ;
                        g_free (str) ;
                        str = NULL ;
                }
                g_string_append (stringue, "\n}");
        }
        if (stringue) {
                str = g_string_free_and_steal (stringue) ;
        }
        return str ;
}


static gchar *
cr_statement_import_rule_to_string (CRStatement const *a_this,
                                    gulong a_indent)
{
        GString *stringue = NULL ;
        gchar *str = NULL;

        g_return_val_if_fail (a_this
                              && a_this->type == AT_IMPORT_RULE_STMT
                              && a_this->kind.import_rule,
                              NULL) ;

        if (a_this->kind.import_rule->url
            && a_this->kind.import_rule->url->stryng) {
                stringue = g_string_new (NULL) ;
                g_return_val_if_fail (stringue, NULL) ;
                str = g_strndup (a_this->kind.import_rule->url->stryng->str,
                                 a_this->kind.import_rule->url->stryng->len);
                cr_utils_dump_n_chars2 (' ', stringue, a_indent);
                if (str) {
                        g_string_append_printf (stringue,
                                                "@import url(\"%s\")",
                                                str);
                        g_free (str);
                        str = NULL ;
                } else          /*there is no url, so no import rule, get out! */
                        return NULL;

                if (a_this->kind.import_rule->media_list) {
                        GList const *cur = NULL;

                        for (cur = a_this->kind.import_rule->media_list;
                             cur; cur = cur->next) {
                                if (cur->data) {
                                        CRString const *crstr = cur->data;

                                        if (cur->prev) {
                                                g_string_append
                                                        (stringue, ", ");
                                        }
                                        if (crstr
                                            && crstr->stryng
                                            && crstr->stryng->str) {
                                                g_string_append_len
                                                        (stringue,
                                                         crstr->stryng->str,
                                                         crstr->stryng->len) ;
                                        }
                                }
                        }
                }
                g_string_append (stringue, " ;");
        }
        if (stringue) {
                str = g_string_free_and_steal (stringue) ;
                stringue = NULL ;
        }
        return str ;
}


/*******************
 *public functions
 ******************/

/**
 * cr_statement_new_ruleset:
 *
 *@a_sel_list: the list of #CRSimpleSel (selectors)
 *the rule applies to.
 *@a_decl_list: the list of instances of #CRDeclaration
 *that composes the ruleset.
 *@a_media_types: a list of instances of GString that
 *describe the media list this ruleset applies to.
 *
 *Creates a new instance of #CRStatement of type
 *#CRRulSet.
 *
 *Returns the new instance of #CRStatement or NULL if something
 *went wrong.
 */
CRStatement *
cr_statement_new_ruleset (CRStyleSheet * a_sheet,
                          CRSelector * a_sel_list,
                          CRDeclaration * a_decl_list,
                          CRStatement * a_parent_media_rule)
{
        CRStatement *result = NULL;

        g_return_val_if_fail (a_sel_list, NULL);

        if (a_parent_media_rule) {
                g_return_val_if_fail
                        (a_parent_media_rule->type == AT_MEDIA_RULE_STMT,
                         NULL);
                g_return_val_if_fail (a_parent_media_rule->kind.media_rule,
                                      NULL);
        }

        result = g_try_malloc (sizeof (CRStatement));

        if (!result) {
                cr_utils_trace_info ("Out of memory");
                return NULL;
        }

        memset (result, 0, sizeof (CRStatement));
        result->type = RULESET_STMT;
        result->kind.ruleset = g_try_malloc (sizeof (CRRuleSet));

        if (!result->kind.ruleset) {
                cr_utils_trace_info ("Out of memory");
                g_free (result);
                return NULL;
        }

        memset (result->kind.ruleset, 0, sizeof (CRRuleSet));
        result->kind.ruleset->sel_list = a_sel_list;
        if (a_sel_list)
                cr_selector_ref (a_sel_list);
        result->kind.ruleset->decl_list = a_decl_list;

        if (a_parent_media_rule) {
                a_parent_media_rule->kind.media_rule->rulesets =
                        cr_statement_append
                        (a_parent_media_rule->kind.media_rule->rulesets,
                         result);
        }

        cr_statement_set_parent_sheet (result, a_sheet);

        return result;
}

/**
 * cr_statement_new_at_media_rule:
 *
 *@a_ruleset: the ruleset statements contained
 *in the \@media rule.
 *@a_media: the media string list. A list of GString pointers.
 *
 *Instantiates an instance of #CRStatement of type
 *AT_MEDIA_RULE_STMT (\@media ruleset).
 *
 */
CRStatement *
cr_statement_new_at_media_rule (CRStyleSheet * a_sheet,
                                CRStatement * a_rulesets, GList * a_media)
{
        CRStatement *result = NULL,
                *cur = NULL;

        if (a_rulesets)
                g_return_val_if_fail (a_rulesets->type == RULESET_STMT, NULL);

        result = g_try_malloc (sizeof (CRStatement));

        if (!result) {
                cr_utils_trace_info ("Out of memory");
                return NULL;
        }

        memset (result, 0, sizeof (CRStatement));
        result->type = AT_MEDIA_RULE_STMT;

        result->kind.media_rule = g_try_malloc (sizeof (CRAtMediaRule));
        if (!result->kind.media_rule) {
                cr_utils_trace_info ("Out of memory");
                g_free (result);
                return NULL;
        }
        memset (result->kind.media_rule, 0, sizeof (CRAtMediaRule));
        result->kind.media_rule->rulesets = a_rulesets;
        for (cur = a_rulesets; cur; cur = cur->next) {
                if (cur->type != RULESET_STMT || !cur->kind.ruleset) {
                        cr_utils_trace_info ("Bad parameter a_rulesets. "
                                             "It should be a list of "
                                             "correct ruleset statement only !");
                        goto error;
                }
        }

        result->kind.media_rule->media_list = a_media;
        if (a_sheet) {
                cr_statement_set_parent_sheet (result, a_sheet);
        }

        return result;

      error:
        return NULL;
}

/**
 * cr_statement_new_at_import_rule:
 *
 *@a_url: the url to connect to the get the file
 *to be imported.
 *@a_sheet: the imported parsed stylesheet.
 *
 *Creates a new instance of #CRStatment of type
 *#CRAtImportRule.
 *
 *Returns the newly built instance of #CRStatement.
 */
CRStatement *
cr_statement_new_at_import_rule (CRStyleSheet * a_container_sheet,
                                 CRString * a_url,
                                 GList * a_media_list,
                                 CRStyleSheet * a_imported_sheet)
{
        CRStatement *result = NULL;

        result = g_try_malloc (sizeof (CRStatement));

        if (!result) {
                cr_utils_trace_info ("Out of memory");
                return NULL;
        }

        memset (result, 0, sizeof (CRStatement));
        result->type = AT_IMPORT_RULE_STMT;

        result->kind.import_rule = g_try_malloc (sizeof (CRAtImportRule));

        if (!result->kind.import_rule) {
                cr_utils_trace_info ("Out of memory");
                g_free (result);
                return NULL;
        }

        memset (result->kind.import_rule, 0, sizeof (CRAtImportRule));
        result->kind.import_rule->url = a_url;
        result->kind.import_rule->media_list = a_media_list;
        result->kind.import_rule->sheet = a_imported_sheet;
        if (a_container_sheet)
                cr_statement_set_parent_sheet (result, a_container_sheet);

        return result;
}

/**
 * cr_statement_new_at_page_rule:
 *
 *@a_decl_list: a list of instances of #CRDeclarations
 *which is actually the list of declarations that applies to
 *this page rule.
 *@a_selector: the page rule selector.
 *
 *Creates a new instance of #CRStatement of type
 *#CRAtPageRule.
 *
 *Returns the newly built instance of #CRStatement or NULL
 *in case of error.
 */
CRStatement *
cr_statement_new_at_page_rule (CRStyleSheet * a_sheet,
                               CRDeclaration * a_decl_list,
                               CRString * a_name, CRString * a_pseudo)
{
        CRStatement *result = NULL;

        result = g_try_malloc (sizeof (CRStatement));

        if (!result) {
                cr_utils_trace_info ("Out of memory");
                return NULL;
        }

        memset (result, 0, sizeof (CRStatement));
        result->type = AT_PAGE_RULE_STMT;

        result->kind.page_rule = g_try_malloc (sizeof (CRAtPageRule));

        if (!result->kind.page_rule) {
                cr_utils_trace_info ("Out of memory");
                g_free (result);
                return NULL;
        }

        memset (result->kind.page_rule, 0, sizeof (CRAtPageRule));
        if (a_decl_list) {
                result->kind.page_rule->decl_list = a_decl_list;
                cr_declaration_ref (a_decl_list);
        }
        result->kind.page_rule->name = a_name;
        result->kind.page_rule->pseudo = a_pseudo;
        if (a_sheet)
                cr_statement_set_parent_sheet (result, a_sheet);

        return result;
}

/**
 * cr_statement_new_at_charset_rule:
 *
 *@a_charset: the string representing the charset.
 *Note that the newly built instance of #CRStatement becomes
 *the owner of a_charset. The caller must not free a_charset !!!.
 *
 *Creates a new instance of #CRStatement of type
 *#CRAtCharsetRule.
 *
 *Returns the newly built instance of #CRStatement or NULL
 *if an error arises.
 */
CRStatement *
cr_statement_new_at_charset_rule (CRStyleSheet * a_sheet,
                                  CRString * a_charset)
{
        CRStatement *result = NULL;

        g_return_val_if_fail (a_charset, NULL);

        result = g_try_malloc (sizeof (CRStatement));

        if (!result) {
                cr_utils_trace_info ("Out of memory");
                return NULL;
        }

        memset (result, 0, sizeof (CRStatement));
        result->type = AT_CHARSET_RULE_STMT;

        result->kind.charset_rule = g_try_malloc (sizeof (CRAtCharsetRule));

        if (!result->kind.charset_rule) {
                cr_utils_trace_info ("Out of memory");
                g_free (result);
                return NULL;
        }
        memset (result->kind.charset_rule, 0, sizeof (CRAtCharsetRule));
        result->kind.charset_rule->charset = a_charset;
        cr_statement_set_parent_sheet (result, a_sheet);

        return result;
}

/**
 * cr_statement_new_at_font_face_rule:
 *
 *@a_font_decls: a list of instances of #CRDeclaration. Each declaration
 *is actually a font declaration.
 *
 *Creates an instance of #CRStatement of type #CRAtFontFaceRule.
 *
 *Returns the newly built instance of #CRStatement.
 */
CRStatement *
cr_statement_new_at_font_face_rule (CRStyleSheet * a_sheet,
                                    CRDeclaration * a_font_decls)
{
        CRStatement *result = NULL;

        result = g_try_malloc (sizeof (CRStatement));

        if (!result) {
                cr_utils_trace_info ("Out of memory");
                return NULL;
        }
        memset (result, 0, sizeof (CRStatement));
        result->type = AT_FONT_FACE_RULE_STMT;

        result->kind.font_face_rule = g_try_malloc
                (sizeof (CRAtFontFaceRule));

        if (!result->kind.font_face_rule) {
                cr_utils_trace_info ("Out of memory");
                g_free (result);
                return NULL;
        }
        memset (result->kind.font_face_rule, 0, sizeof (CRAtFontFaceRule));

        result->kind.font_face_rule->decl_list = a_font_decls;
        if (a_sheet)
                cr_statement_set_parent_sheet (result, a_sheet);

        return result;
}

/**
 * cr_statement_set_parent_sheet:
 *
 *@a_this: the current instance of #CRStatement.
 *@a_sheet: the sheet that contains the current statement.
 *
 *Sets the container stylesheet.
 *
 *Returns CR_OK upon successful completion, an error code otherwise.
 */
enum CRStatus
cr_statement_set_parent_sheet (CRStatement * a_this, CRStyleSheet * a_sheet)
{
        g_return_val_if_fail (a_this, CR_BAD_PARAM_ERROR);
        a_this->parent_sheet = a_sheet;
        return CR_OK;
}

/**
 * cr_statement_append:
 *
 *@a_this: the current instance of the statement list.
 *@a_new: a_new the new instance of #CRStatement to append.
 *
 *Appends a new statement to the statement list.
 *
 *Returns the new list statement list, or NULL in cas of failure.
 */
CRStatement *
cr_statement_append (CRStatement * a_this, CRStatement * a_new)
{
        CRStatement *cur = NULL;

        g_return_val_if_fail (a_new, NULL);

        if (!a_this) {
                return a_new;
        }

        /*walk forward in the current list to find the tail list element */
        for (cur = a_this; cur && cur->next; cur = cur->next) ;

        cur->next = a_new;
        a_new->prev = cur;

        return a_this;
}

/**
 * cr_statement_to_string:
 *
 *@a_this: the current statement to serialize
 *@a_indent: the number of white space of indentation.
 *
 *Serializes a css statement into a string
 *
 *Returns the serialized statement. Must be freed by the caller
 *using g_free().
 */
gchar *
cr_statement_to_string (CRStatement const * a_this, gulong a_indent)
{
        gchar *str = NULL ;

        if (!a_this)
                return NULL;

        switch (a_this->type) {
        case RULESET_STMT:
                str = cr_statement_ruleset_to_string
                        (a_this, a_indent);
                break;

        case AT_FONT_FACE_RULE_STMT:
                str = cr_statement_font_face_rule_to_string
                        (a_this, a_indent) ;
                break;

        case AT_CHARSET_RULE_STMT:
                str = cr_statement_charset_to_string
                        (a_this, a_indent);
                break;

        case AT_PAGE_RULE_STMT:
                str = cr_statement_at_page_rule_to_string
                        (a_this, a_indent);
                break;

        case AT_MEDIA_RULE_STMT:
                str = cr_statement_media_rule_to_string
                        (a_this, a_indent);
                break;

        case AT_IMPORT_RULE_STMT:
                str = cr_statement_import_rule_to_string
                        (a_this, a_indent);
                break;

        default:
                cr_utils_trace_info ("Statement unrecognized");
                break;
        }
        return str ;
}

gchar*
cr_statement_list_to_string (CRStatement const *a_this, gulong a_indent)
{
        CRStatement const *cur_stmt = NULL ;
        GString *stringue = NULL ;
        gchar *str = NULL ;

        g_return_val_if_fail (a_this, NULL) ;

        stringue = g_string_new (NULL) ;
        if (!stringue) {
                cr_utils_trace_info ("Out of memory") ;
                return NULL ;
        }
        for (cur_stmt = a_this ; cur_stmt;
             cur_stmt = cur_stmt->next) {
                str = cr_statement_to_string (cur_stmt, a_indent) ;
                if (str) {
                        if (!cur_stmt->prev) {
                                g_string_append (stringue, str) ;
                        } else {
                                g_string_append_printf
                                        (stringue, "\n%s", str) ;
                        }
                        g_free (str) ;
                        str = NULL ;
                }
        }
        str = g_string_free_and_steal (stringue) ;
        return str ;
}

/**
 * cr_statement_destroy:
 *
 * @a_this: the current instance of #CRStatement.
 *
 *Destructor of #CRStatement.
 */
void
cr_statement_destroy (CRStatement * a_this)
{
        CRStatement *cur = NULL;

        g_return_if_fail (a_this);

        /*go get the tail of the list */
        for (cur = a_this; cur && cur->next; cur = cur->next) {
                cr_statement_clear (cur);
        }

        if (cur)
                cr_statement_clear (cur);

        if (cur->prev == NULL) {
                g_free (a_this);
                return;
        }

        /*walk backward and free next element */
        for (cur = cur->prev; cur && cur->prev; cur = cur->prev) {
                g_clear_pointer (&cur->next, g_free);
        }

        if (!cur)
                return;

        /*free the one remaining list */
        g_clear_pointer (&cur->next, g_free);

        g_free (cur);
        cur = NULL;
}
