#! /bin/sh

# Cogl
#
# An object oriented GL/GLES Abstraction/Utility Layer
#
# Copyright (C) 2008,2009 Intel Corporation.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.


output_copyright ()
{
    cat <<EOF
/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
EOF
}

# If two arguments are given then generate the header file instead
if test "$#" = 2; then
    bfname="${2%.glsl}"
    bname=`basename "$bfname"`
    varname=`echo -n "${bname}" | tr -c a-z _`
    guardname=`echo -n "${varname}" | tr a-z A-Z`
    guardname="__${guardname}_H"

    output_copyright
    echo
    echo "#ifndef ${guardname}"
    echo "#define ${guardname}"
    echo

    sed -n \
     -e 's/^ *\/\*\*\* \([a-zA-Z0-9_]*\) \*\*\*\//extern const char \1[];/p' \
     < "$2"

    echo
    echo "#endif /* ${guardname} */"

else

    bfname="${1%.glsl}";
    bname=`basename "${bfname}"`;
    varname=`echo -n "${bname}" | tr -c a-z _`;

    output_copyright
    echo
    sed -n \
	-e h \
	-e 's/^ *\/\*\*\* \([a-zA-Z0-9_]*\) \*\*\*\//  ;\nconst char \1[] =/' \
	-e 't got' \
	-e g \
	-e 's/"/\\"/' \
	-e 's/^/  "/' \
	-e 's/$/\\n"/' \
	-e ': got' \
	-e p \
	< "$1"
    echo "  ;"
fi
