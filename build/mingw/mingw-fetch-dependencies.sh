#!/bin/bash

# This script will download and setup a cross compilation environment
# for targetting Win32 from Linux. It can also be used to build on
# Windows under the MSYS/MinGW environment. It will use the GTK
# binaries from Tor Lillqvist.

TOR_URL="http://ftp.gnome.org/pub/gnome/binaries/win32";

TOR_BINARIES=( \
    pango/1.28/pango{-dev,}_1.28.0-1_win32.zip);

TOR_DEP_URL="http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies";

ZLIB_VERSION=1.2.4-2
FFI_VERSION=3.0.6
GLIB_VERSION=2.34.3
GLIB_MINOR_VERSION="${GLIB_VERSION%.*}"

TOR_DEPS=( \
    cairo{-dev,}_1.10.0-2_win32.zip \
    gettext-runtime-{dev-,}0.17-1.zip \
    fontconfig{-dev,}_2.8.0-2_win32.zip \
    freetype{-dev,}_2.3.12-1_win32.zip \
    expat_2.0.1-1_win32.zip \
    libpng{-dev,}_1.4.0-1_win32.zip \
    zlib{-dev,}_${ZLIB_VERSION}_win32.zip \
    libffi{-dev,}_${FFI_VERSION}-1_win32.zip \
    gettext-runtime{-dev,}_0.18.1.1-2_win32.zip );

GNOME_SOURCES_URL="http://ftp.gnome.org/pub/GNOME/sources/"
SOURCES_DEPS=(\
    glib/${GLIB_MINOR_VERSION}/glib-${GLIB_VERSION}.tar.xz );

GL_HEADER_URLS=( \
    http://cgit.freedesktop.org/mesa/mesa/plain/include/GL/gl.h \
    http://www.opengl.org/registry/api/glext.h );

GL_HEADERS=( gl.h glext.h );

CONFIG_GUESS_URL="http://git.savannah.gnu.org/gitweb/?p=automake.git;a=blob_plain;f=lib/config.guess"

function download_file ()
{
    local url="$1"; shift;
    local filename="$1"; shift;

    if test -f "$DOWNLOAD_DIR/$filename"; then
        echo "Skipping download of $filename because the file already exists";
        return 0;
    fi;

    case "$DOWNLOAD_PROG" in
	curl)
	    curl -L -o "$DOWNLOAD_DIR/$filename" "$url";
	    ;;
	*)
	    $DOWNLOAD_PROG -O "$DOWNLOAD_DIR/$filename" "$url";
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

function do_untar_source ()
{
    do_untar_source_d "$BUILD_DIR" "$@";
}

function do_untar_source_d ()
{
    local exdir="$1"; shift;
    local tarfile="$1"; shift;

    tar -C "$exdir" -axvf "$tarfile" "$@";

    if [ "$?" -ne 0 ]; then
	echo "Failed to extract $tarfile";
	exit 1;
    fi;
}

function add_env ()
{
    echo "export $1=\"$2\"" >> $env_file;
}

function find_compiler ()
{
    local gccbin fullpath;

    if [ -z "$MINGW_TOOL_PREFIX" ]; then
	for gccbin in i{3,4,5,6}86{-pc,}-mingw32{,msvc}-gcc; do
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

    TARGET="${MINGW_TOOL_PREFIX##*/}";
    TARGET="${TARGET%%-}";

    echo "Using compiler ${MINGW_TOOL_PREFIX}gcc and target $TARGET";
}

function generate_pc_file ()
{
    local pcfile="$1"; shift;
    local libs="$1"; shift;
    local version="$1"; shift;
    local include="$1"; shift;
    local bn=`basename "$pcfile"`;

    if test -z "$include"; then
        include="\${prefix}/include";
    fi;

    if ! test -f "$pcfile"; then
        cat > "$pcfile" <<EOF
prefix=$ROOT_DIR
exec_prefix=\${prefix}
libdir=\${prefix}/lib
sharedlibdir=\${libdir}
includedir=$include

Name: $bn
Description: $bn

Requires:
Libs: -L\${libdir} $libs
Cflags: -I\${includedir}
Version: $version
EOF
    fi
}

function do_cross_compile ()
{
    local dep="$1"; shift;
    local builddir="$BUILD_DIR/$dep";

    cd "$builddir"
    ./configure --prefix="$ROOT_DIR" \
        --host="$TARGET" \
        --target="$TARGET" \
        --build="`./config.guess`" \
        CFLAGS="-mms-bitfields -I${ROOT_DIR}/include" \
        LDFLAGS="-L${ROOT_DIR}/lib" \
        PKG_CONFIG="$RUN_PKG_CONFIG" \
        "$@";

    if [ "$?" -ne 0 ]; then
	echo "Failed to configure $dep";
	exit 1;
    fi;

    make all install

    if [ "$?" -ne 0 ]; then
	echo "Failed to build $dep";
	exit 1;
    fi;
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
guess_dir ROOT_DIR "cogl-cross" \
    "the root prefix for the build environment" "Root dir";
SLASH_SCRIPT='s/\//\\\//g';
quoted_root_dir=`echo "$ROOT_DIR" | sed "$SLASH_SCRIPT" `;

# If a build directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir BUILD_DIR "build" \
    "the directory to build source dependencies in" "Build directory";

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

for dep in "${GL_HEADER_URLS[@]}"; do
    bn="${dep##*/}";
    download_file "$dep" "$bn";
done;

for dep in "${SOURCES_DEPS[@]}"; do
    src="${dep##*/}";
    download_file "$GNOME_SOURCES_URL/$dep" "$src";
done;

download_file "$CONFIG_GUESS_URL" "config.guess";

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

for src in "${SOURCES_DEPS[@]}"; do
    echo "Extracting $src...";
    src="${src##*/}";
    do_untar_source "$DOWNLOAD_DIR/$src";
done;

echo "Fixing pkgconfig files...";
for x in "$ROOT_DIR/lib/pkgconfig/"*.pc; do
    sed "s/^prefix=.*\$/prefix=${quoted_root_dir}/" \
	< "$x" > "$x.tmp";
    mv "$x.tmp" "$x";
done;

# The Pango FT pc file hardcodes the include path for freetype, so it
# needs to be fixed separately
sed -e 's/^Cflags:.*$/Cflags: -I${includedir}\/pango-1.0 -I${includedir}\/freetype2/' \
    -e 's/^\(Libs:.*\)$/\1 -lfreetype -lfontconfig/' \
    < "$ROOT_DIR/lib/pkgconfig/pangoft2.pc" \
    > "$ROOT_DIR/lib/pkgconfig/pangoft2.pc.tmp";
mv "$ROOT_DIR/lib/pkgconfig/pangoft2.pc"{.tmp,};

echo "Copying GL headers...";
if ! ( test -d "$ROOT_DIR/include/GL" || \
    mkdir "$ROOT_DIR/include/GL" ); then
    echo "Failed to create GL header directory";
    exit 1;
fi;
for header in "${GL_HEADERS[@]}"; do
    if ! cp "$DOWNLOAD_DIR/$header" "$ROOT_DIR/include/GL/"; then
        echo "Failed to copy $header";
        exit 1;
    fi;
done;

# We need pkg-config files for zlib and ffi to build glib. The
# prepackaged binaries from tml doesn't seem to include them so we'll
# just generate it manually.
generate_pc_file "$ROOT_DIR/lib/pkgconfig/zlib.pc" "-lz" "$ZLIB_VERSION"
generate_pc_file "$ROOT_DIR/lib/pkgconfig/libffi.pc" "-lffi" "$FFI_VERSION" \
    "${ROOT_DIR}/lib/libffi-${FFI_VERSION}/include"

RUN_PKG_CONFIG="$BUILD_DIR/run-pkg-config.sh";

echo "Generating $BUILD_DIR/run-pkg-config.sh";

cat > "$RUN_PKG_CONFIG" <<EOF
# This is a wrapper script for pkg-config that overrides the
# PKG_CONFIG_LIBDIR variable so that it won't pick up the local system
# .pc files.

# The MinGW compiler on Fedora tries to do a similar thing except that
# it also unsets PKG_CONFIG_PATH. This breaks any attempts to add a
# local search path so we need to avoid using that script.

export PKG_CONFIG_LIBDIR="$ROOT_DIR/lib/pkgconfig"

exec pkg-config "\$@"
EOF

chmod a+x "$RUN_PKG_CONFIG";

##
# Build environment
##

find_compiler;

build_config=`bash $DOWNLOAD_DIR/config.guess`;

##
# Build source dependencies
##

do_cross_compile "glib-${GLIB_VERSION}" --disable-modular-tests

echo
echo "Done!"
echo
echo "You should now have everything you need to cross compile Cogl"
echo
echo "To get started, you should be able to configure and build from"
echo "the top of your cogl source directory as follows:"
echo
echo "./configure --host=\"$TARGET\" --target=\"$TARGET\" --build=\"$build_config\" --enable-wgl CFLAGS=\"-mms-bitfields -I$ROOT_DIR/include\" --enable-deprecated PKG_CONFIG=\"$RUN_PKG_CONFIG\"" PKG_CONFIG_PATH=
echo "make"
echo
echo "Note: the explicit --build option is often necessary to ensure autoconf"
echo "realizes you are cross-compiling."
