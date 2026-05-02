/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 * This file is part of The Croco Library
 *
 * Copyright (C) 2002-2004 Dodji Seketeli
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
 */

#include "string.h"
#include "cr-stylesheet.h"

/**
 *@file
 *The definition of the #CRStyleSheet class
 */

/**
 *Constructor of the #CRStyleSheet class.
 *@param the initial list of css statements.
 *@return the newly built css2 stylesheet, or NULL in case of error.
 */
CRStyleSheet *
cr_stylesheet_new (CRStatement * a_stmts)
{
        CRStyleSheet *result;

        result = g_try_malloc (sizeof (CRStyleSheet));
        if (!result) {
                cr_utils_trace_info ("Out of memory");
                return NULL;
        }

        memset (result, 0, sizeof (CRStyleSheet));

        if (a_stmts)
                result->statements = a_stmts;

        return result;
}

void
cr_stylesheet_ref (CRStyleSheet * a_this)
{
        g_return_if_fail (a_this);

        a_this->ref_count++;
}

gboolean
cr_stylesheet_unref (CRStyleSheet * a_this)
{
        g_return_val_if_fail (a_this, FALSE);

        if (a_this->ref_count)
                a_this->ref_count--;

        if (!a_this->ref_count) {
                cr_stylesheet_destroy (a_this);
                return TRUE;
        }

        return FALSE;
}

/**
 *Destructor of the #CRStyleSheet class.
 *@param a_this the current instance of the #CRStyleSheet class.
 */
void
cr_stylesheet_destroy (CRStyleSheet * a_this)
{
        g_return_if_fail (a_this);

        if (a_this->statements) {
                cr_statement_destroy (a_this->statements);
                a_this->statements = NULL;
        }
        g_free (a_this);
}
