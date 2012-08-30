#! /bin/sh

has_x11_backend=no
has_gdk_backend=no
has_wayland_backend=no
for backend in ${CLUTTER_BACKENDS}; do
        case "$backend" in
                x11) has_x11_backend=yes ;;
                gdk) has_gdk_backend=yes ;;
                wayland) has_wayland_backend=yes ;;
        esac
done

cppargs="-DG_OS_UNIX"
if [ $has_x11_backend = "yes" ]; then
        cppargs="$cppargs -DCLUTTER_WINDOWING_X11 -DCLUTTER_WINDOWING_GLX"
fi

if [ $has_gdk_backend = "yes" ]; then
        cppargs="$cppargs -DCLUTTER_WINDOWING_GDK"
fi

if [ $has_wayland_backend = "yes" ]; then
        cppargs="$cppargs -DCLUTTER_WINDOWING_WAYLAND"
fi

cpp -P ${cppargs} ${srcdir:-.}/clutter.symbols | sed -e '/^$/d' -e 's/ G_GNUC.*$//' -e 's/ PRIVATE//' -e 's/ DATA//' | sort > expected-abi

nm -D -g --defined-only .libs/libclutter-1.0.so | cut -d ' ' -f 3 | egrep -v '^(__bss_start|_edata|_end)' | sort > actual-abi
diff -u expected-abi actual-abi && rm -f expected-abi actual-abi
