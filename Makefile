LIBS=-lpng -lGL -ljpeg -L/usr/X11R6/lib -lX11 `pkg-config --libs pangoft2 pango glib-2.0`
CFLAGS=`pkg-config --cflags pangoft2 pango glib-2.0`

.c.o:
	$(CC) -g -Wall $(CFLAGS) $(INCS) -c $*.c

OBJS=cltr.o pixbuf.o util.o fonts.o cltr-photo-grid.o

clutter: $(OBJS)
	$(CC) -g -Wall -o $@ $(OBJS) $(LIBS)

$(OBJS): pixbuf.h util.h fonts.h cltr.h cltr-photo-grid.h

clean:
	rm -fr *.o clutter test
