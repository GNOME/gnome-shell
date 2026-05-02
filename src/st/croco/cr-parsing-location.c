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
 * See the COPYRIGHTS file for copyright information.
 */

#include <string.h>
#include "cr-parsing-location.h"

/**
 *@CRParsingLocation:
 *
 *Definition of the #CRparsingLocation class.
 */

/**
 * cr_parsing_location_copy:
 *@a_to: the destination of the copy.
 *Must be allocated by the caller.
 *@a_from: the source of the copy.
 *
 *Copies an instance of CRParsingLocation into another one.
 *
 *Returns CR_OK upon successful completion, an error code
 *otherwise.
 */
enum CRStatus
cr_parsing_location_copy (CRParsingLocation *a_to,
			  CRParsingLocation const *a_from)
{
	g_return_val_if_fail (a_to && a_from, CR_BAD_PARAM_ERROR) ;

	memcpy (a_to, a_from, sizeof (CRParsingLocation)) ;
	return CR_OK ;
}
