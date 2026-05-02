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

#include "cr-utils.h"
#include "cr-string.h"

/**
 *@file:
 *Some misc utility functions used
 *in the libcroco.
 *Note that troughout this file I will
 *refer to the CSS SPECIFICATIONS DOCUMENTATION
 *written by the w3c guys. You can find that document
 *at http://www.w3.org/TR/REC-CSS2/ .
 */

/****************************
 *Encoding helpers
 ****************************/

/**
 *Reads a character from an utf8 buffer.
 *Actually decode the next character code (unicode character code)
 *and returns it.
 *@param a_in the starting address of the utf8 buffer.
 *@param a_in_len the length of the utf8 buffer.
 *@param a_out output parameter. The resulting read char.
 *@param a_consumed the number of the bytes consumed to
 *decode the returned character code.
 *@return CR_OK upon successful completion, an error code otherwise.
 */
enum CRStatus
cr_utils_read_char_from_utf8_buf (const guchar * a_in,
                                  gulong a_in_len,
                                  guint32 * a_out, gulong * a_consumed)
{
        gulong in_index = 0,
               nb_bytes_2_decode = 0;
        enum CRStatus status = CR_OK;

        /*
         *to store the final decoded
         *unicode char
         */
        guint32 c = 0;

        g_return_val_if_fail (a_in && a_out && a_out
                              && a_consumed, CR_BAD_PARAM_ERROR);

        if (a_in_len < 1) {
                status = CR_OK;
                goto end;
        }

        if (*a_in <= 0x7F) {
                /*
                 *7 bits long char
                 *encoded over 1 byte:
                 * 0xxx xxxx
                 */
                c = *a_in;
                nb_bytes_2_decode = 1;

        } else if ((*a_in & 0xE0) == 0xC0) {
                /*
                 *up to 11 bits long char.
                 *encoded over 2 bytes:
                 *110x xxxx  10xx xxxx
                 */
                c = *a_in & 0x1F;
                nb_bytes_2_decode = 2;

        } else if ((*a_in & 0xF0) == 0xE0) {
                /*
                 *up to 16 bit long char
                 *encoded over 3 bytes:
                 *1110 xxxx  10xx xxxx  10xx xxxx
                 */
                c = *a_in & 0x0F;
                nb_bytes_2_decode = 3;

        } else if ((*a_in & 0xF8) == 0xF0) {
                /*
                 *up to 21 bits long char
                 *encoded over 4 bytes:
                 *1111 0xxx  10xx xxxx  10xx xxxx  10xx xxxx
                 */
                c = *a_in & 0x7;
                nb_bytes_2_decode = 4;

        } else if ((*a_in & 0xFC) == 0xF8) {
                /*
                 *up to 26 bits long char
                 *encoded over 5 bytes.
                 *1111 10xx  10xx xxxx  10xx xxxx
                 *10xx xxxx  10xx xxxx
                 */
                c = *a_in & 3;
                nb_bytes_2_decode = 5;

        } else if ((*a_in & 0xFE) == 0xFC) {
                /*
                 *up to 31 bits long char
                 *encoded over 6 bytes:
                 *1111 110x  10xx xxxx  10xx xxxx
                 *10xx xxxx  10xx xxxx  10xx xxxx
                 */
                c = *a_in & 1;
                nb_bytes_2_decode = 6;

        } else {
                /*BAD ENCODING */
                goto end;
        }

        if (nb_bytes_2_decode > a_in_len) {
                status = CR_END_OF_INPUT_ERROR;
                goto end;
        }

        /*
         *Go and decode the remaining byte(s)
         *(if any) to get the current character.
         */
        for (in_index = 1; in_index < nb_bytes_2_decode; in_index++) {
                /*byte pattern must be: 10xx xxxx */
                if ((a_in[in_index] & 0xC0) != 0x80) {
                        goto end;
                }

                c = (c << 6) | (a_in[in_index] & 0x3F);
        }

        /*
         *The decoded ucs4 char is now
         *in c.
         */

    /************************
     *Some security tests
     ***********************/

        /*be sure c is a char */
        if (c == 0xFFFF || c == 0xFFFE)
                goto end;

        /*be sure c is inferior to the max ucs4 char value */
        if (c > 0x10FFFF)
                goto end;

        /*
         *c must be less than UTF16 "lower surrogate begin"
         *or higher than UTF16 "High surrogate end"
         */
        if (c >= 0xD800 && c <= 0xDFFF)
                goto end;

        /*Avoid characters that equals zero */
        if (c == 0)
                goto end;

        *a_out = c;

      end:
        *a_consumed = nb_bytes_2_decode;

        return status;
}

/*****************************************
 *CSS basic types identification utilities
 *****************************************/

/**
 *Returns TRUE if a_char is a white space as
 *defined in the css spec in chap 4.1.1.
 *
 *white-space ::= ' '| \t|\r|\n|\f
 *
 *@param a_char the character to test.
 *return TRUE if is a white space, false otherwise.
 */
gboolean
cr_utils_is_white_space (guint32 a_char)
{
        switch (a_char) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case '\f':
                return TRUE;
                break;
        default:
                return FALSE;
        }
}

/**
 *Returns true if the character is a nonascii
 *character (as defined in the css spec chap 4.1.1):
 *
 *nonascii ::= [^\0-\177]
 *
 *@param a_char the character to test.
 *@return TRUE if the character is a nonascii char,
 *FALSE otherwise.
 */
gboolean
cr_utils_is_nonascii (guint32 a_char)
{
        if (a_char <= 177) {
                return FALSE;
        }

        return TRUE;
}

void
cr_utils_dump_n_chars2 (guchar a_char, GString * a_string, glong a_nb)
{
        glong i = 0;

        g_return_if_fail (a_string);

        for (i = 0; i < a_nb; i++) {
                g_string_append_printf (a_string, "%c", a_char);
        }
}

/**
 *Duplicate a GList where the GList::data is a CRString.
 *@param a_list_of_strings the list to duplicate
 *@return the duplicated list, or NULL if something bad
 *happened.
 */
GList *
cr_utils_dup_glist_of_cr_string (GList const * a_list_of_strings)
{
        GList const *cur = NULL;
        GList *result = NULL;

        g_return_val_if_fail (a_list_of_strings, NULL);

        for (cur = a_list_of_strings; cur; cur = cur->next) {
                CRString *str = NULL;

                str = cr_string_dup ((CRString const *) cur->data) ;
                if (str)
                        result = g_list_append (result, str);
        }

        return result;
}
