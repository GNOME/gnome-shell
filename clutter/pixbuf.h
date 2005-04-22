#ifndef _HAVE_PIXBUF_H
#define _HAVE_PIXBUF_H

typedef struct Pixbuf Pixbuf;
typedef struct PixbufPixel PixbufPixel;

typedef enum PixbufFormat 
{
  PB_FMT_RGBA 
} 
PixbufFormat;

struct PixbufPixel
{
  unsigned char r,g,b,a;
};

struct Pixbuf 
{
  int *data;
  int  bytes_per_pixel;         /* bits per pixel = bpp << 3 */
  int  channels;                /* 4 with alpha */
  int  width, height;
  int  bytes_per_line;          /* ( width * bpp ) */

  int refcnt;			/* starts at 0  */

  void *meta; 			/* for jpeg meta text ? to a hash ? */

  /* Possibles */

  int rmask, gmask, bmask, amask;  /* Masks - good for packed formats > */
  int has_alpha;                   /* Rather than channels ? */


  /* PixbufFormat  format; like GL format	*/
  
};

Pixbuf*
pixbuf_new_from_file(const char *filename);

Pixbuf*
pixbuf_new(int width, int height);

void
pixbuf_unref(Pixbuf *pixb);

void
pixbuf_ref(Pixbuf *pixb);

void
pixbuf_set_pixel(Pixbuf *pixb, int x, int y, PixbufPixel *p);

void
pixbuf_get_pixel(Pixbuf *pixbuf, int x, int y, PixbufPixel *p);

void
pixel_set_vals(PixbufPixel        *p, 
	       const unsigned char r,
	       const unsigned char g,
	       const unsigned char b,
	       const unsigned char a);

void
pixbuf_copy(Pixbuf *src_pixb,
	    Pixbuf *dst_pixb,
	    int     srcx, 
	    int     srcy, 
	    int     srcw, 
	    int     srch,
	    int     dstx, 
	    int     dsty);

void
pixbuf_fill_rect(Pixbuf      *pixb,
		 int          x,
		 int          y,
		 int          width,
		 int          height,
		 PixbufPixel *p);


Pixbuf*
pixbuf_scale_down(Pixbuf *pixb,
		  int     new_width, 
		  int     new_height);

Pixbuf*
pixbuf_clone(Pixbuf *pixb);

Pixbuf*
pixbuf_convolve(Pixbuf *pixb, 
		int    *kernel, 
		int     kernel_size, 
		int     kernel_divisor) ;

Pixbuf*
pixbuf_blur(Pixbuf *pixb);

Pixbuf*
pixbuf_sharpen(Pixbuf *pixb);


#endif
