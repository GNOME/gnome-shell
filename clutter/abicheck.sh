#! /bin/sh

cpp -P \
        -DG_OS_UNIX \
        -DCLUTTER_WINDOWING_X11 \
        -DCLUTTER_WINDOWING_GLX \
        -DCLUTTER_WINDOWING_GDK \
        ${srcdir:-.}/clutter.symbols | sed -e '/^$/d' -e 's/ G_GNUC.*$//' -e 's/ PRIVATE//' -e 's/ DATA//' | sort > expected-abi

nm -D -g --defined-only .libs/libclutter-1.0.so | cut -d ' ' -f 3 | egrep -v '^(__bss_start|_edata|_end)' | sort > actual-abi
diff -u expected-abi actual-abi && rm -f expected-abi actual-abi
