#!/bin/sh

# The ClutterFixed type and macros now use floats, but we are keeping the
# CoglFixed type + macros using fixed point so now we convert all uses of
# the Cogl fixed point macros within Clutter proper to use the ClutterFixed
# macros instead.
find ./clutter -maxdepth 1 -iname '*.c' -exec sed -i 's/COGL_FIXED_MUL/CLUTTER_FIXED_MUL/g' {} \;
find ./clutter -maxdepth 1 -iname '*.c' -exec sed -i 's/COGL_FIXED_DIV/CLUTTER_FIXED_DIV/g' {} \;
find ./clutter -maxdepth 1 -iname '*.c' -exec sed -i 's/COGL_FIXED_FAST_MUL/CLUTTER_FIXED_MUL/g' {} \;
find ./clutter -maxdepth 1 -iname '*.c' -exec sed -i 's/COGL_FIXED_FAST_DIV/CLUTTER_FIXED_DIV/g' {} \;
find ./clutter -maxdepth 1 -iname '*.c' -exec sed -i 's/COGL_FIXED_FROM_FLOAT/CLUTTER_FLOAT_TO_FIXED/g' {} \;
find ./clutter -maxdepth 1 -iname '*.c' -exec sed -i 's/COGL_FIXED_TO_FLOAT/CLUTTER_FIXED_TO_FLOAT/g' {} \;
find ./clutter -maxdepth 1 -iname '*.c' -exec sed -i 's/COGL_FIXED_TO_DOUBLE/CLUTTER_FIXED_TO_DOUBLE/g' {} \;
find ./clutter -maxdepth 1 -iname '*.c' -exec sed -i 's/COGL_FIXED_PI/CFX_PI/g' {} \;

# All remaining uses of the Cogl fixed point API now get expanded out to simply
# use float calculations... (we will restore the cogl-fixed code itself later)

# XXX: This assumes that no nested function - with multiple arguments - is ever
# found as the RHS argument to COGL_FIXED_MUL. This is because we simply replace
# the last ',' with the * operator. If you want to double check that's still true:
# $ grep -r --include=*.c COGL_FIXED_MUL *|less
find ./clutter -iname '*.[ch]' -exec sed -i -r 's/COGL_FIXED_MUL (.*),/\1 */g' {} \;
# XXX: We use the same assumption here...
find ./clutter -iname '*.[ch]' -exec sed -i -r 's|COGL_FIXED_FAST_DIV (.*),|\1 /|g' {} \;
# XXX: And again here. (Note in this case there were examples of COGL_FIXED_MUL
# being used as the RHS argument, but since we have already replaced instances
# of COGL_FIXED_MUL, that works out ok.
find ./clutter -iname '*.[ch]' -exec sed -i -r 's|COGL_FIXED_DIV (.*),|\1 /|g' {} \;

# A fix due to the assumptions used above
sed -i 's/#define DET2X(a,b,c,d).*/#define DET2X(a,b,c,d)   ((a * d) - (b * c))/g' ./clutter/clutter-actor.c

find ./clutter/cogl/gles -iname '*.[ch]' -exec sed -i 's/GLfixed/GLfloat/g' {} \;

#we get some redundant brackets like this, but C's automatic type promotion
#works out fine for most cases...
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_TO_INT//g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_FROM_INT /(float)/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_FROM_INT/(float)/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_TO_FLOAT//g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_FROM_FLOAT//g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_TO_DOUBLE /(double)/g' {} \;

find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_FLOOR/floorf/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_CEIL/ceilf/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_360/360.0/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_240/240.0/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_255/255.0/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_180/180.0/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_120/120.0/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_60/60.0/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_1/1.0/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_0_5/0.5/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/COGL_FIXED_PI/G_PI/g' {} \;

find ./clutter -iname '*.[ch]' -exec sed -i -r 's/COGL_ANGLE_FROM_DEG \((.*)\),/\1,/g' {} \;

find ./clutter -iname '*.[ch]' -exec perl -p -i -e "s|cogl_angle_cos \((.*?)\)|cosf (\1 * (G_PI/180.0))|;" {} \;
find ./clutter -iname '*.[ch]' -exec perl -p -i -e "s|cogl_angle_sin \((.*?)\)|sinf (\1 * (G_PI/180.0))|;" {} \;
find ./clutter -iname '*.[ch]' -exec perl -p -i -e "s|cogl_angle_tan \((.*?)\)|tanf (\1 * (G_PI/180.0))|;" {} \;

#XXX: NB: cogl_fixed_div must be done before mul since there is a case were they
#are nested which would otherwise break the assumption used here that the last
#coma of the line can simply be replaced with the corresponding operator
find ./clutter -iname '*.[ch]' -exec sed -i -r 's|cogl_fixed_div (.*),|\1 /|g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i -r 's|cogl_fixed_mul (.*),|\1 *|g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/cogl_fixed_pow2/pow2f/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/cogl_fixed_pow/powf/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/cogl_fixed_log2/log2f/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/cogl_fixed_sqrt/sqrtf/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/cogl_fixed_cos/cosf/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/cogl_fixed_sin/sinf/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/cogl_fixed_atan2/atan2f/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/cogl_fixed_atan/atanf/g' {} \;
find ./clutter -iname '*.[ch]' -exec sed -i 's/cogl_fixed_tan/tanf/g' {} \;

#TODO: fixup gles/cogl.c set_clip_plane

cat clutter/cogl/common/cogl-primitives.c| \
    grep -v '#define CFX_MUL2'| \
    grep -v '#undef CFX_MUL2'| \
    grep -v '#define CFX_MUL3'| \
    grep -v '#undef CFX_MUL3'| \
    grep -v '#define CFX_SQ'| \
    grep -v '#undef CFX_SQ'| \
    sed -r 's/CFX_MUL2 \((.{7})\)/(\1 * 2)/g' | \
    sed -r 's/CFX_MUL3 \((.{7})\)/(\1 * 3)/g' | \
    sed -r 's/CFX_SQ \((.{7})\)/(\1 * \1)/g' \
    >./tmp
mv ./tmp clutter/cogl/common/cogl-primitives.c

#this has too many false positives...
#find ./clutter -iname '*.[ch]' -exec sed -i 's|>> 1|/ 2|g' {} \;
#find ./clutter -iname '*.[ch]' -exec sed -i 's|<< 1|* 2|g' {} \;

sed -i 's|>> 1|/ 2|g' ./clutter/cogl/common/cogl-primitives.c
sed -i 's|<< 1|* 2|g' ./clutter/cogl/common/cogl-primitives.c
#find ./clutter -iname '*.[ch]' -exec sed -i 's|<< 1|* 2|g' {} \;


find ./clutter -iname '*.[ch]' -exec sed -i 's/CoglFixed/float/g' {} \;
#XXX: This might need changing later...
find ./clutter -iname '*.[ch]' -exec sed -i 's/CoglFixedVec2/CoglVec2/g' {} \;
sed -i 's/CoglFixed/float/g' ./clutter/cogl/cogl.h.in

# maintain the existing CoglFixed code as utility code for applications:
sed -i 's/float:/CoglFixed:/g' clutter/cogl/cogl-types.h
sed -i 's/gint32 float/gint32 CoglFixed/g' clutter/cogl/cogl-types.h
git-checkout clutter/cogl/cogl-fixed.h clutter/cogl/common/cogl-fixed.c

find ./clutter -iname '*.[ch]' -exec sed -i 's/CoglAngle/float/g' {} \;

# maintain the existing CoglAngle code as utility code for applications:
sed -i 's/float:/CoglAngle:/g' clutter/cogl/cogl-types.h
sed -i 's/gint32 float/gint32 CoglAngle/g' clutter/cogl/cogl-types.h
git-checkout clutter/cogl/cogl-fixed.h clutter/cogl/common/cogl-fixed.c

find ./clutter -iname '*.[ch]' ! -iname 'clutter-fixed.h' -exec sed -i 's/ClutterAngle/float/g' {} \;

echo "Cogl API to remove/replace with float versions:"
find ./clutter/ -iname '*.c' -exec grep '^cogl_[a-zA-Z_]*x ' {} \; | cut -d' ' -f1|grep -v 'box$'|grep -v 'matrix$'
echo "Clutter API to remove/replace with float versions:"
find ./clutter/ -iname '*.c' -exec grep '^clutter_[a-zA-Z_]*x ' {} \; | cut -d' ' -f1|grep -v 'box$'|grep -v 'matrix$'|grep -v '_x$'

#
# Now the last mile is dealt with manually with a bunch of patches...
#

git-commit -a -m "[By fixed-to-float.sh] Fixed to Float automatic changes" --no-verify

patch -p1<fixed-to-float-patches/gl-cogl-texture.c.0.patch
patch -p1<fixed-to-float-patches/mtx_transform.0.patch
patch -p1<fixed-to-float-patches/clutter-actor.c.0.patch
patch -p1<fixed-to-float-patches/clutter-alpha.c.0.patch
patch -p1<fixed-to-float-patches/clutter-alpha.h.0.patch
patch -p1<fixed-to-float-patches/clutter-behaviour-ellipse.c.0.patch
patch -p1<fixed-to-float-patches/clutter-bezier.c.0.patch
patch -p1<fixed-to-float-patches/clutter-path.c.0.patch
patch -p1<fixed-to-float-patches/cogl-fixed.h.0.patch
patch -p1<fixed-to-float-patches/cogl-fixed.c.0.patch
patch -p1<fixed-to-float-patches/test-cogl-tex-tile.c.0.patch
patch -p1<fixed-to-float-patches/clutter-texture.c.0.patch
patch -p1<fixed-to-float-patches/clutter-fixed.c.0.patch
patch -p1<fixed-to-float-patches/gl-cogl.c
patch -p1<fixed-to-float-patches/cogl-pango-render.c.0.patch
patch -p1<fixed-to-float-patches/cogl-primitives.c.0.patch
patch -p1<fixed-to-float-patches/gl-cogl-primitives.c.0.patch
patch -p1<fixed-to-float-patches/gles-cogl.c.0.patch
patch -p1<fixed-to-float-patches/gles-cogl-gles2-wrapper.h.0.patch
patch -p1<fixed-to-float-patches/gles-cogl-primitives.c.0.patch
patch -p1<fixed-to-float-patches/gles-cogl-texture.c.0.patch

#XXX: COGL_PANGO_UNIT_TO_FIXED

git-commit -a -m "[By fixed-to-float.sh] Fixed to Float patches" --no-verify

# The fixes in these files are entirely handcoded, so to avoid clashes with the
# automatic stuff above the patches below are based against the pristine
# versions, and we don't want to commit any of the automatic changes here.
git-checkout HEAD~2 clutter/clutter-fixed.h
git-checkout HEAD~2 clutter/clutter-units.h

patch -p1<fixed-to-float-patches/clutter-fixed.h.0.patch
patch -p1<fixed-to-float-patches/clutter-units.h.0.patch

git-commit -a -m "[By fixed-to-float.sh] clutter-fixed.h and clutter-units.h changes" --no-verify

