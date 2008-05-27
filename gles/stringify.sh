#! /bin/sh

output_copyright ()
{
    cat <<EOF > "$1";
/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
    bfname="${2%.glsl}";
    bname=`basename "$bfname"`;
    varname=`echo -n "${bname}" | tr -c a-z _`;
    guardname=`echo -n "${varname}" | tr a-z A-Z`;
    guardname="__${guardname}_H";
    headername="${bfname}.h";

    output_copyright "${headername}";
    echo >> "${headername}";
    echo "#ifndef ${guardname}" >> "${headername}";
    echo "#define ${guardname}" >> "${headername}";
    echo >> "${headername}";
    echo "extern const char ${varname}[];" >> "${headername}";
    echo >> "${headername}";
    echo "#endif /* ${guardname} */" >> "${headername}";

else

    bfname="${1%.glsl}";
    bname=`basename "${bfname}"`;
    cname="${bfname}.c";
    varname=`echo -n "${bname}" | tr -c a-z _`;

    output_copyright "${cname}";
    echo >> "${cname}";
    echo "const char ${varname}[] =" >> "${cname}";
    sed -e 's/"/\\"/' -e 's/^/  \"/' -e 's/$/\\n"/' \
	< "$1" >> "${cname}";
    echo "  ;" >> "${cname}";

fi;
