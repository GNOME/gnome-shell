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
 * See COPYRIGHTS file for copyright information.
 */


#include <string.h>
#include "cr-declaration.h"
#include "cr-statement.h"
#include "cr-parser.h"

/**
 *@CRDeclaration:
 *
 *The definition of the #CRDeclaration class.
 */

/**
 * cr_declaration_new:
 * @a_statement: the statement this declaration belongs to. can be NULL.
 *@a_property: the property string of the declaration
 *@a_value: the value expression of the declaration.
 *Constructor of #CRDeclaration.
 *
 *Returns the newly built instance of #CRDeclaration, or NULL in
 *case of error.
 *
 *The returned CRDeclaration takes ownership of @a_property and @a_value.
 *(E.g. cr_declaration_destroy on this CRDeclaration will also free
 *@a_property and @a_value.)
 */
CRDeclaration *
cr_declaration_new (CRStatement * a_statement,
                    CRString * a_property, CRTerm * a_value)
{
        CRDeclaration *result = NULL;

        g_return_val_if_fail (a_property, NULL);

        if (a_statement)
                g_return_val_if_fail (a_statement
                                      && ((a_statement->type == RULESET_STMT)
                                          || (a_statement->type
                                              == AT_FONT_FACE_RULE_STMT)
                                          || (a_statement->type
                                              == AT_PAGE_RULE_STMT)), NULL);

        result = g_try_malloc (sizeof (CRDeclaration));
        if (!result) {
                cr_utils_trace_info ("Out of memory");
                return NULL;
        }
        memset (result, 0, sizeof (CRDeclaration));
        result->property = a_property;
        result->value = a_value;

        if (a_value) {
                cr_term_ref (a_value);
        }
        result->parent_statement = a_statement;
        return result;
}

/**
 * cr_declaration_parse_list_from_buf:
 *@a_str: the input buffer that contains the list of declaration to
 *parse.
 *
 *Parses a ';' separated list of properties declaration.
 *Returns the parsed list of declaration, NULL if parsing failed.
 */
CRDeclaration *
cr_declaration_parse_list_from_buf (const guchar * a_str)
{

        enum CRStatus status = CR_OK;
        CRTerm *value = NULL;
        CRString *property = NULL;
        CRDeclaration *result = NULL,
                *cur_decl = NULL;
        CRParser *parser = NULL;
        CRTknzr *tokenizer = NULL;
        gboolean important = FALSE;

        g_return_val_if_fail (a_str, NULL);

        parser = cr_parser_new_from_buf ((guchar*)a_str, strlen ((const char *) a_str), FALSE);
        g_return_val_if_fail (parser, NULL);
        status = cr_parser_get_tknzr (parser, &tokenizer);
        if (status != CR_OK || !tokenizer) {
                if (status == CR_OK)
                        status = CR_ERROR;
                goto cleanup;
        }
        status = cr_parser_try_to_skip_spaces_and_comments (parser);
        if (status != CR_OK)
                goto cleanup;

        status = cr_parser_parse_declaration (parser, &property,
                                              &value, &important);
        if (status != CR_OK || !property) {
                if (status != CR_OK)
                        status = CR_ERROR;
                goto cleanup;
        }
        result = cr_declaration_new (NULL, property, value);
        if (result) {
                property = NULL;
                value = NULL;
                result->important = important;
        }
        /*now, go parse the other declarations */
        for (;;) {
                guint32 c = 0;

                cr_parser_try_to_skip_spaces_and_comments (parser);
                status = cr_tknzr_peek_char (tokenizer, &c);
                if (status != CR_OK) {
                        if (status == CR_END_OF_INPUT_ERROR)
                                status = CR_OK;
                        goto cleanup;
                }
                if (c == ';') {
                        status = cr_tknzr_read_char (tokenizer, &c);
                } else {
                        break;
                }
                important = FALSE;
                cr_parser_try_to_skip_spaces_and_comments (parser);
                status = cr_parser_parse_declaration (parser, &property,
                                                      &value, &important);
                if (status != CR_OK || !property) {
                        if (status == CR_END_OF_INPUT_ERROR) {
                                status = CR_OK;
                        }
                        break;
                }
                cur_decl = cr_declaration_new (NULL, property, value);
                if (cur_decl) {
                        cur_decl->important = important;
                        result = cr_declaration_append (result, cur_decl);
                        property = NULL;
                        value = NULL;
                        cur_decl = NULL;
                } else {
                        break;
                }
        }

      cleanup:

        if (parser) {
                cr_parser_destroy (parser);
                parser = NULL;
        }

        if (property) {
                cr_string_destroy (property);
                property = NULL;
        }

        if (value) {
                cr_term_destroy (value);
                value = NULL;
        }

        if (status != CR_OK && result) {
                cr_declaration_destroy (result);
                result = NULL;
        }
        return result;
}

/**
 * cr_declaration_append:
 *@a_this: the current declaration list.
 *@a_new: the declaration to append.
 *
 *Appends a new declaration to the current declarations list.
 *Returns the declaration list with a_new appended to it, or NULL
 *in case of error.
 */
CRDeclaration *
cr_declaration_append (CRDeclaration * a_this, CRDeclaration * a_new)
{
        CRDeclaration *cur = NULL;

        g_return_val_if_fail (a_new, NULL);

        if (!a_this)
                return a_new;

        for (cur = a_this; cur && cur->next; cur = cur->next) ;

        cur->next = a_new;
        a_new->prev = cur;

        return a_this;
}


/**
 * cr_declaration_to_string:
 *@a_this: the current instance of #CRDeclaration.
 *@a_indent: the number of indentation white char
 *to put before the actual serialisation.
 *
 *Serializes the declaration into a string
 *Returns the serialized form the declaration. The caller must
 *free the string using g_free().
 */
gchar *
cr_declaration_to_string (CRDeclaration const * a_this, gulong a_indent)
{
        GString *stringue = NULL;

        gchar *str = NULL,
                *result = NULL;

        g_return_val_if_fail (a_this, NULL);

	stringue = g_string_new (NULL);

	if (a_this->property
	    && a_this->property->stryng
	    && a_this->property->stryng->str) {
		str = g_strndup (a_this->property->stryng->str,
				 a_this->property->stryng->len);
		if (str) {
			cr_utils_dump_n_chars2 (' ', stringue,
						a_indent);
			g_string_append (stringue, str);
			g_free (str);
			str = NULL;
		} else
                        goto error;

                if (a_this->value) {
                        guchar *value_str = NULL;

                        value_str = cr_term_to_string (a_this->value);
                        if (value_str) {
                                g_string_append_printf (stringue, " : %s",
                                                        value_str);
                                g_free (value_str);
                        } else
                                goto error;
                }
                if (a_this->important == TRUE) {
                        g_string_append_printf (stringue, " %s",
                                                "!important");
                }
        }
        if (stringue && stringue->str) {
                result = g_string_free_and_steal (stringue);
        }
        return result;

      error:
        if (stringue) {
                g_string_free (stringue, TRUE);
                stringue = NULL;
        }
        g_clear_pointer (&str, g_free);

        return result;
}

/**
 * cr_declaration_list_to_string2:
 *@a_this: the current instance of #CRDeclaration.
 *@a_indent: the number of indentation white char
 *@a_one_decl_per_line: whether to output one doc per line or not.
 *to put before the actual serialisation.
 *
 *Serializes the declaration list into a string
 *Returns the serialized form the declararation.
 */
guchar *
cr_declaration_list_to_string2 (CRDeclaration const * a_this,
                                gulong a_indent, gboolean a_one_decl_per_line)
{
        CRDeclaration const *cur = NULL;
        GString *stringue = NULL;
        guchar *str = NULL,
                *result = NULL;

        g_return_val_if_fail (a_this, NULL);

        stringue = g_string_new (NULL);

        for (cur = a_this; cur; cur = cur->next) {
                str = (guchar *) cr_declaration_to_string (cur, a_indent);
                if (str) {
                        if (a_one_decl_per_line == TRUE) {
                                if (cur->next)
                                        g_string_append_printf (stringue,
                                                                "%s;\n", str);
                                else
                                        g_string_append (stringue,
                                                         (const gchar *) str);
                        } else {
                                if (cur->next)
                                        g_string_append_printf (stringue,
                                                                "%s;", str);
                                else
                                        g_string_append (stringue,
                                                         (const gchar *) str);
                        }
                        g_free (str);
                } else
                        break;
        }
        if (stringue && stringue->str) {
                result = (guchar *) g_string_free_and_steal (stringue);
        }

        return result;
}

/**
 * cr_declaration_ref:
 *@a_this: the current instance of #CRDeclaration.
 *
 *Increases the ref count of the current instance of #CRDeclaration.
 */
void
cr_declaration_ref (CRDeclaration * a_this)
{
        g_return_if_fail (a_this);

        a_this->ref_count++;
}

/**
 * cr_declaration_unref:
 *@a_this: the current instance of #CRDeclaration.
 *
 *Decrements the ref count of the current instance of #CRDeclaration.
 *If the ref count reaches zero, the current instance of #CRDeclaration
 *if destroyed.
 *Returns TRUE if @a_this was destroyed (ref count reached zero),
 *FALSE otherwise.
 */
gboolean
cr_declaration_unref (CRDeclaration * a_this)
{
        g_return_val_if_fail (a_this, FALSE);

        if (a_this->ref_count) {
                a_this->ref_count--;
        }

        if (a_this->ref_count == 0) {
                cr_declaration_destroy (a_this);
                return TRUE;
        }
        return FALSE;
}

/**
 * cr_declaration_destroy:
 *@a_this: the current instance of #CRDeclaration.
 *
 *Destructor of the declaration list.
 */
void
cr_declaration_destroy (CRDeclaration * a_this)
{
        CRDeclaration *cur = NULL;

        g_return_if_fail (a_this);

        /*
         * Go to the last element of the list.
         */
        for (cur = a_this; cur->next; cur = cur->next)
                g_assert (cur->next->prev == cur);

        /*
         * Walk backward the list and free each "next" element.
         * Meanwhile, free each property/value pair contained in the list.
         */
        for (; cur; cur = cur->prev) {
                g_free (cur->next);
                cur->next = NULL;

                if (cur->property) {
                        cr_string_destroy (cur->property);
                        cur->property = NULL;
                }

                if (cur->value) {
                        cr_term_destroy (cur->value);
                        cur->value = NULL;
                }
        }

        g_free (a_this);
}
