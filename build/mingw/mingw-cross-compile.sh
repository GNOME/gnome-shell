#!/bin/bash

# This script will download and setup a cross compilation environment
# for targetting Win32 from Linux. It will use the GTK binaries from
# Tor Lillqvist.

TOR_URL="http://ftp.gnome.org/pub/gnome/binaries/win32";

TOR_BINARIES=( \
    glib/2.18/glib{-dev,}_2.18.2-1_win32.zip \
    gtk+/2.14/gtk+{-dev,}_2.14.4-2_win32.zip \
    pango/1.22/pango{-dev,}_1.22.0-1_win32.zip \
    atk/1.24/atk{-dev,}_1.24.0-1_win32.zip );

TOR_DEP_URL="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies";

TOR_DEPS=( \
    cairo-{dev-,}1.6.4-2.zip \
    gettext-runtime-{dev-,}0.17-1.zip \
    fontconfig-{dev-,}2.4.2-tml-20071015.zip \
    freetype-{dev-,}2.3.6.zip \
    expat-2.0.0.zip );

#SF_URL="http://kent.dl.sourceforge.net/sourceforge";
#SF_URL="http://surfnet.dl.sourceforge.net/sourceforge";
SF_URL="http://mesh.dl.sourceforge.net/sourceforge";

OTHER_DEPS=( \
    "http://www.gimp.org/~tml/gimp/win32/libiconv-1.9.1.bin.woe32.zip" \
    "${SF_URL}/libpng/zlib123-dll.zip" \
    "http://www.libsdl.org/release/SDL-devel-1.2.13-mingw32.tar.gz" \
    "${SF_URL}/mesa3d/MesaLib-7.2.tar.bz2" );

GNUWIN32_URL="${SF_URL}/gnuwin32";

GNUWIN32_DEPS=( \
    libpng-1.2.33-{bin,lib}.zip \
    jpeg-6b-4-{bin,lib}.zip \
    tiff-3.8.2-1-{bin,lib}.zip );

CLUTTER_SVN="http://svn.o-hand.com/repos/clutter/trunk";

function download_file ()
{
    local url="$1"; shift;
    local filename="$1"; shift;

    case "$DOWNLOAD_PROG" in
	curl)
	    curl -C - -o "$DOWNLOAD_DIR/$filename" "$url";
	    ;;
	*)
	    $DOWNLOAD_PROG -O "$DOWNLOAD_DIR/$filename" -c "$url";
	    ;;
    esac;

    if [ $? -ne 0 ]; then
	echo "Downloading ${url} failed.";
	exit 1;
    fi;
}

function guess_dir ()
{
    local var="$1"; shift;
    local suffix="$1"; shift;
    local msg="$1"; shift;
    local prompt="$1"; shift;
    local dir="${!var}";

    if [ -z "$dir" ]; then
	echo "Please enter ${msg}.";
	dir="$PWD/$suffix";
	read -r -p "$prompt [$dir] ";
	if [ -n "$REPLY" ]; then
	    dir="$REPLY";
	fi;
    fi;

    eval $var="\"$dir\"";

    if [ ! -d "$dir" ]; then
	if ! mkdir -p "$dir"; then
	    echo "Error making directory $dir";
	    exit 1;
	fi;
    fi;
}

function y_or_n ()
{
    local prompt="$1"; shift;

    while true; do
	read -p "${prompt} [y/n] " -n 1;
	echo;
	case "$REPLY" in
	    y) return 0 ;;
	    n) return 1 ;;
	    *) echo "Please press y or n" ;;
	esac;
    done;
}

function do_unzip ()
{
    do_unzip_d "$ROOT_DIR" "$@";
}

function do_unzip_d ()
{
    local exdir="$1"; shift;
    local zipfile="$1"; shift;

    unzip -o -q -d "$exdir" "$zipfile" "$@";

    if [ "$?" -ne 0 ]; then
	echo "Failed to extract $zipfile";
	exit 1;
    fi;
}

function find_compiler ()
{
    local gccbin fullpath;

    if [ -z "$MINGW_TOOL_PREFIX" ]; then
	for gccbin in i{3,4,5,6}86-mingw32{,msvc}-gcc; do
	    fullpath="`which $gccbin 2>/dev/null`";
	    if [ "$?" -eq 0 ]; then
		MINGW_TOOL_PREFIX="${fullpath%%gcc}";
		break;
	    fi;
	done;
	if [ -z "$MINGW_TOOL_PREFIX" ]; then
	    echo;
	    echo "No suitable cross compiler was found.";
	    echo;
	    echo "If you already have a compiler installed,";
	    echo "please set the MINGW_TOOL_PREFIX variable";
	    echo "to point to its location without the";
	    echo "gcc suffix (eg: \"/usr/bin/i386-mingw32-\").";
	    echo;
	    echo "If you are using Ubuntu, you can install a";
	    echo "compiler by typing:";
	    echo;
	    echo " sudo apt-get install mingw32";
	    echo;
	    echo "Otherwise you can try following the instructions here:";
	    echo;
	    echo " http://www.libsdl.org/extras/win32/cross/README.txt";

	    exit 1;
	fi;
    fi;

    export ADDR2LINE="${MINGW_TOOL_PREFIX}addr2line"
    export AS="${MINGW_TOOL_PREFIX}as"
    export CC="${MINGW_TOOL_PREFIX}gcc"
    export CPP="${MINGW_TOOL_PREFIX}cpp"
    export CPPFILT="${MINGW_TOOL_PREFIX}c++filt"
    export CXX="${MINGW_TOOL_PREFIX}g++"
    export DLLTOOL="${MINGW_TOOL_PREFIX}dlltool"
    export DLLWRAP="${MINGW_TOOL_PREFIX}dllwrap"
    export GCOV="${MINGW_TOOL_PREFIX}gcov"
    export LD="${MINGW_TOOL_PREFIX}ld"
    export NM="${MINGW_TOOL_PREFIX}nm"
    export OBJCOPY="${MINGW_TOOL_PREFIX}objcopy"
    export OBJDUMP="${MINGW_TOOL_PREFIX}objdump"
    export READELF="${MINGW_TOOL_PREFIX}readelf"
    export SIZE="${MINGW_TOOL_PREFIX}size"
    export STRINGS="${MINGW_TOOL_PREFIX}strings"
    export WINDRES="${MINGW_TOOL_PREFIX}windres"
    export AR="${MINGW_TOOL_PREFIX}ar"
    export RANLIB="${MINGW_TOOL_PREFIX}ranlib"
    export STRIP="${MINGW_TOOL_PREFIX}strip"

    TARGET="${MINGW_TOOL_PREFIX##*/}";
    TARGET="${TARGET%%-}";

    echo "Using compiler $CC and target $TARGET";
}

# If a download directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir DOWNLOAD_DIR "downloads" \
    "the directory to download to" "Download directory";

# Try to guess a download program if none has been specified
if [ -z "$DOWNLOAD_PROG" ]; then
    # If no download program has been specified then check if wget or
    # curl exists
    #wget first, because my curl can't download libsdl...
    for x in wget curl; do
	if [ "`type -t $x`" != "" ]; then
	    DOWNLOAD_PROG="$x";
	    break;
	fi;
    done;

    if [ -z "$DOWNLOAD_PROG" ]; then
	echo "No DOWNLOAD_PROG was set and neither wget nor curl is ";
	echo "available.";
	exit 1;
    fi;
fi;

# If a download directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir ROOT_DIR "clutter-cross" \
    "the root prefix for the build environment" "Root dir";
SLASH_SCRIPT='s/\//\\\//g';
quoted_root_dir=`echo "$ROOT_DIR" | sed "$SLASH_SCRIPT" `;

##
# Download files
##

for bin in "${TOR_BINARIES[@]}"; do
    bn="${bin##*/}";
    download_file "$TOR_URL/$bin" "$bn"
done;

for dep in "${TOR_DEPS[@]}"; do
    download_file "$TOR_DEP_URL/$dep" "$dep";
done;

for dep in "${OTHER_DEPS[@]}"; do
    bn="${dep##*/}";
    download_file "$dep" "$bn";
done;

for dep in "${GNUWIN32_DEPS[@]}"; do
    download_file "$GNUWIN32_URL/$dep" "$dep";
done;

##
# Extract files
##

for bin in "${TOR_BINARIES[@]}"; do
    echo "Extracting $bin...";
    bn="${bin##*/}";
    do_unzip "$DOWNLOAD_DIR/$bn";
done;

for dep in "${TOR_DEPS[@]}"; do
    echo "Extracting $dep...";
    do_unzip "$DOWNLOAD_DIR/$dep";
done;

for dep in "${GNUWIN32_DEPS[@]}"; do
    echo "Extracting $dep...";
    do_unzip "$DOWNLOAD_DIR/$dep";
done;

echo "Extracting libiconv...";
do_unzip "$DOWNLOAD_DIR/libiconv-1.9.1.bin.woe32.zip";
echo "Extracting zlib...";
do_unzip "$DOWNLOAD_DIR/zlib123-dll.zip" "zlib1.dll";

if ! mv "$ROOT_DIR/zlib1.dll" "$ROOT_DIR/bin/"; then
    echo "Failed to mv zlib1.dll";
    exit 1;
fi;

echo "Extracting SDL...";
if ! tar -C "$ROOT_DIR" \
    -zxf "$DOWNLOAD_DIR/SDL-devel-1.2.13-mingw32.tar.gz"; then
    echo "Failed to extract SDL";
    exit 1;
fi;
for x in bin docs include lib man share; do
    if ! cp -pR "$ROOT_DIR/SDL-1.2.13/$x" "$ROOT_DIR/"; then
	echo "Failed to copy SDL files";
	exit 1;
    fi;
done;
rm -fr "$ROOT_DIR/SDL-1.2.13";
export SDL_CONFIG="$ROOT_DIR/bin/sdl-config";

echo "Fixing SDL libtool files...";
sed "s/^libdir=.*\$/libdir='${quoted_root_dir}\/lib'/" \
    < "$ROOT_DIR/lib/libSDL.la" > "$ROOT_DIR/lib/libSDL.la.tmp";
mv "$ROOT_DIR/lib/libSDL.la.tmp" "$ROOT_DIR/lib/libSDL.la";

echo "Fixing pkgconfig files...";
for x in "$ROOT_DIR/lib/pkgconfig/"*.pc "$ROOT_DIR/bin/sdl-config"; do
    sed "s/^prefix=.*\$/prefix=${quoted_root_dir}/" \
	< "$x" > "$x.tmp";
    mv "$x.tmp" "$x";
done;

chmod +x "$ROOT_DIR/bin/sdl-config";

# The Pango FT pc file hardcodes the include path for freetype, so it
# needs to be fixed separately
sed -e 's/^Cflags:.*$/Cflags: -I${includedir}\/pango-1.0 -I${includedir}\/freetype2/' \
    -e 's/^\(Libs:.*\)$/\1 -lfreetype -lfontconfig/' \
    < "$ROOT_DIR/lib/pkgconfig/pangoft2.pc" \
    > "$ROOT_DIR/lib/pkgconfig/pangoft2.pc.tmp";
mv "$ROOT_DIR/lib/pkgconfig/pangoft2.pc"{.tmp,};

echo "Extracting Mesa headers...";
if ! tar -C "$DOWNLOAD_DIR" \
    -jxf "$DOWNLOAD_DIR/MesaLib-7.2.tar.bz2" \
    Mesa-7.2/include; then
    echo "Failed to extract Mesa headers";
    exit 1;
fi;
cp -R "$DOWNLOAD_DIR/Mesa-7.2/include/"* "$ROOT_DIR/include";

##
# Build
##

export PKG_CONFIG_PATH="$ROOT_DIR/lib/pkgconfig:$PKG_CONFIG_PATH";

export LDFLAGS="-L$ROOT_DIR/lib -mno-cygwin $LDFLAGS"
export CPPFLAGS="-I$ROOT_DIR/include $CPPFLAGS"
export CFLAGS="-I$ROOT_DIR/include -mno-cygwin -mms-bitfields -march=i686 ${CFLAGS:-"-g"}"
export CXXFLAGS="-I$ROOT_DIR/include -mno-cygwin -mms-bitfields -march=i686 ${CFLAGS:-"-g"}"

if y_or_n "Do you want to checkout and build Clutter?"; then
    find_compiler;

    guess_dir CLUTTER_BUILD_DIR "clutter" \
	"the build directory for clutter" "Build dir";
    svn checkout "$CLUTTER_SVN/clutter" $CLUTTER_BUILD_DIR;
    if [ "$?" -ne 0 ]; then
	echo "svn failed";
	exit 1;
    fi;
    ( cd "$CLUTTER_BUILD_DIR" && ./autogen.sh --prefix="$ROOT_DIR" \
	--host="$TARGET" --target="$TARGET" --with-flavour=win32 );
    if [ "$?" -ne 0 ]; then
	echo "autogen failed";
	exit 1;
    fi;
    ( cd "$CLUTTER_BUILD_DIR" && make all install );
    if [ "$?" -ne 0 ]; then
	echo "make failed";
	exit 1;
    fi;
fi;

if y_or_n "Do you want to checkout and build Clutter-Cairo?"; then
    find_compiler;

    guess_dir CLUTTER_CAIRO_BUILD_DIR "clutter-cairo" \
	"the build directory for clutter-cairo" "Build dir";
    svn checkout "$CLUTTER_SVN/clutter-cairo" $CLUTTER_CAIRO_BUILD_DIR;
    if [ "$?" -ne 0 ]; then
	echo "svn failed";
	exit 1;
    fi;
    ( cd "$CLUTTER_CAIRO_BUILD_DIR" && ./autogen.sh --prefix="$ROOT_DIR" \
	--host="$TARGET" --target="$TARGET" );
    if [ "$?" -ne 0 ]; then
	echo "autogen failed";
	exit 1;
    fi;
    ( cd "$CLUTTER_CAIRO_BUILD_DIR" && make all install );
    if [ "$?" -ne 0 ]; then
	echo "make failed";
	exit 1;
    fi;
fi;
