#include <stdio.h>
#include <stdlib.h>

#include <string.h> /* For memset() */
#include <unistd.h> /* For read() */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/mman.h> /* For mmap()/munmap() */
#include <sys/types.h>


#include <png.h>
#include <jpeglib.h>

#include "pixbuf.h"
#include "util.h"

static int*
load_png_file( const char *file, 
	       int        *width, 
	       int        *height) 
{
  FILE *fd;
  /* GLubyte *data; */
  int  *data;
  unsigned char header[8];
  int          bit_depth, color_type;
  png_uint_32  png_width, png_height, i, rowbytes;
  png_structp  png_ptr;
  png_infop    info_ptr;
  png_bytep   *row_pointers;

  if ((fd = fopen( file, "rb" )) == NULL) return NULL;

  /* check header etc */

  fread(header, 1, 8, fd);

  if (!png_check_sig(header, 8)) 
    {
      fclose(fd);
      return NULL;
    }

  png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr) 
    {
      fclose(fd);
      return NULL;
    }

  info_ptr = png_create_info_struct(png_ptr);

  if (!info_ptr) 
    {
      png_destroy_read_struct( &png_ptr, (png_infopp)NULL, (png_infopp)NULL);
      fclose(fd);
      return NULL;
    }

  if (setjmp( png_ptr->jmpbuf ) ) 
    {
      png_destroy_read_struct( &png_ptr, &info_ptr, NULL);
      fclose(fd);
      return NULL;
    }

  png_init_io( png_ptr, fd );

  png_set_sig_bytes( png_ptr, 8);
  png_read_info( png_ptr, info_ptr);

  png_get_IHDR( png_ptr, info_ptr, 
		&png_width, &png_height, &bit_depth, 
		&color_type, NULL, NULL, NULL);

  *width =  (int) png_width;
  *height = (int) png_height;

  /* Tranform to req 8888 */

  if (bit_depth == 16 ) png_set_strip_16(png_ptr);      /* 16 -> 8 */

  if (bit_depth < 8)   png_set_packing(png_ptr);        /* 1,2,4 -> 8 */

  if (( color_type == PNG_COLOR_TYPE_GRAY ) ||
            ( color_type == PNG_COLOR_TYPE_GRAY_ALPHA ))
    png_set_gray_to_rgb(png_ptr);

  if (( color_type == PNG_COLOR_TYPE_GRAY ) ||
      ( color_type == PNG_COLOR_TYPE_RGB ))
    png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER); /* req 1.2.7 */

  if (( color_type == PNG_COLOR_TYPE_PALETTE )||
      ( png_get_valid( png_ptr, info_ptr, PNG_INFO_tRNS )))
    png_set_expand(png_ptr);

  png_read_update_info( png_ptr, info_ptr);
 
  /* Now load the actual data */

  rowbytes = png_get_rowbytes( png_ptr, info_ptr);

  data = (int *) malloc( (rowbytes*(*height + 1)));

  row_pointers = (png_bytep *) malloc( (*height)*sizeof(png_bytep));

  if (( data == NULL ) || ( row_pointers == NULL )) 
    {
      png_destroy_read_struct( &png_ptr, &info_ptr, NULL);
      if (data) free(data);
      if (row_pointers) free(row_pointers);
      return NULL;
    }

  for ( i = 0;  i < *height; i++ )
    row_pointers[i] = (png_bytep) data + i*rowbytes;

  png_read_image( png_ptr, row_pointers );
  png_read_end( png_ptr, NULL);

  free(row_pointers);
  png_destroy_read_struct( &png_ptr, &info_ptr, NULL);
  fclose(fd);

  return data;
}

struct local_error_mgr 
{
  struct jpeg_error_mgr pub;	/* "public" fields */
  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct local_error_mgr * local_error_ptr;

static void
_jpeg_error_exit (j_common_ptr cinfo)
{
  local_error_ptr err = (local_error_ptr) cinfo->err;
  (*cinfo->err->output_message) (cinfo);
  longjmp(err->setjmp_buffer, 1);
}

static int* 
load_jpg_file( const char *file, 
	       int        *width, 
	       int        *height)
{
  struct jpeg_decompress_struct cinfo;
  struct local_error_mgr jerr;
  FILE    *infile;		/* source file */
  JSAMPLE *buffer;		/* Output row buffer */
  int      row_stride;		/* physical row width in output buffer */
 
  int     *data = NULL, *d = NULL;

  if ((infile = fopen(file, "rb")) == NULL) 
    return NULL;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = _jpeg_error_exit;

  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return NULL;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);
  jpeg_read_header(&cinfo, TRUE);

  cinfo.do_fancy_upsampling = FALSE;
  cinfo.do_block_smoothing  = FALSE;
  cinfo.out_color_space     = JCS_RGB;
  cinfo.scale_num           = 1;

  jpeg_start_decompress(&cinfo);

  if( cinfo.output_components != 3 ) 
    {
      /*
      fprintf( stderr, "mbpixbuf: jpegs with %d channles not supported\n", 
	       cinfo.output_components );
      */
      jpeg_finish_decompress(&cinfo);
      jpeg_destroy_decompress(&cinfo);
      return NULL;
    }

  *width     = cinfo.output_width;
  *height    = cinfo.output_height;
 
  d = data = malloc(*width * *height * 4 );

  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = malloc( sizeof(JSAMPLE)*row_stride );

  while (cinfo.output_scanline < cinfo.output_height) 
    {
      int off = 0;

      jpeg_read_scanlines(&cinfo, &buffer, 1);

      while (off < row_stride)
	{
	  /* XXX Endianess */
	  *d++ = 
	    (buffer[off]   << 24) |  /* RGBA */
	    (buffer[off+1] << 16) |
  	    (buffer[off+2] << 8)  |
	    (0xff << 0); 
	  off += 3;
	}
    }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(infile);

  if (buffer) free(buffer);

  return data;
}

/* X pcx code, based on usplash code by paul coden */
/* http://courses.ece.uiuc.edu/ece390/books/labmanual/graphics-pcx.html */

typedef struct 
{
  unsigned char manufacturer;
  unsigned char version;
  unsigned char encoding;
  unsigned char bits_per_pixel;
  unsigned short xmin;
  unsigned short ymin;
  unsigned short xmax;
  unsigned short ymax;
  unsigned short xdpi;
  unsigned short ydpi;
  unsigned char colourmap[48];
  unsigned char reserved;
  unsigned char planes;
  unsigned short scanline_length;
  unsigned short palette_info;
  unsigned short xsize;
  unsigned short ysize;
  unsigned char fill[54];
  unsigned char data[0];
} pcx;

enum 
{
  PCX_ZSOFT = 10,
  PCX_RLE = 1,
  PCX_WITH_PALETTE = 2,
  PCX_COLOUR_MAP_LENGTH = 769
};


/*
** Reads the first 128 bytes of a PCX headers, from an file
** descriptor, into memory.
** RETURN zero on success.
*/
int 
pcx_read_header(pcx *header, int fd)
{
  if(!lseek(fd, 0, SEEK_SET))
    if(read(fd, header, sizeof(pcx)) == sizeof(pcx))
      return 0;
  return -1;
}

/*
** Does the file descriptor point to a PCX file, which is of a
** suitable colour-depth (8-bit) for us to use?
** RETURN zero on success.
*/
static int 
pcx_is_suitable(int fd)
{
  pcx header;
  if(!pcx_read_header(&header, fd))
    if(header.manufacturer == PCX_ZSOFT 
       /* && header.version >= PCX_WITH_PALETTE && */
       && header.encoding == PCX_RLE 
       && header.planes == 3 		/* 24bpp */
       && header.bits_per_pixel == 8 ) /* why not 24 from gimp */
      return 0;
  
  return -1;
}

/*
** Takes a raw PCX RLE stream and decompresses it into the destination
** buffer, which must be big enough!
** RETURN zero on success
**
** PCX images are RLE (Run Length Encoded as follows:
**  if(top two bits are set)  // >= 0xc0
**    use bottom six bit (& 0x3f) as RLE count for next byte;
**  else // < 0xc0
**    copy one byte normally;
*/
static void 
pcx_raw_decode24(int           *dest, 
		 unsigned char *src, 
		 int            width, 
		 int            height)
{
  int x, y, i, count;
  int *d = dest;
  unsigned char *p;

  memset(dest, 0xff, height * width * 4);

  for(y = 0; y < height; y++)
    {
      d = dest + (y * width);
      /* RGB */
      for(x = 0; x < width;)
	if(*src < 0xc0)
	  {
	    x++;
	    p = (unsigned char *)d++;
	    *p = *src++;
	  }
	else
	  {
	    count = *src++ & 0x3f;
	    for (i=0; i<count; i++)
	      {
		p = (unsigned char *)d++;
		*p = *src++;
		x += count;
	      }
	  }

      d = dest + (y * width);

      /* RGB */
      for(x = 0; x < width;)
	if(*src < 0xc0)
	  {
	    x++;
	    p = (unsigned char *)d++; 
	    *(p+1) = *src++;
	  }
	else
	  {
	    count = *src++ & 0x3f;
	    for (i=0; i<count; i++)
	      {
		p = (unsigned char *)d++;
		*(p+1) = *src++;
	      }
	    x += count;
	  }

      d = dest + (y * width);

      /* RGB */
      for(x = 0; x < width;)
	if(*src < 0xc0)
	  {
	    x++;
	    p = (unsigned char *)d++; 
	    *(p+2) = *src++ ;
 	  }
	else
	  {
	    count = *src++ & 0x3f;
	    for (i=0; i<count; i++)
	      {
		p = (unsigned char *)d++;
		*(p+2) = *src++;
	      }
	    x += count;
	  }

    }
}

int* 
load_pcx_file (const char *filename,
	       int        *width,
	       int        *height)
{
  int   *data;
  int    fd, file_length;
  pcx   *header;
  struct stat st;

	/* Open file */
  if((fd = open(filename, O_RDONLY)) < 0)
    return NULL;
	
  /* Test file */
  if(pcx_is_suitable(fd))
    return NULL;
	
  /* Get file size */
  if (fstat(fd, &st))
    return NULL;

  file_length = st.st_size;
	
  /* mmap the pcx file into our header */
  header = mmap(NULL, file_length, PROT_READ, MAP_SHARED, fd, 0);
  if (header == MAP_FAILED)
      return NULL;
	
  /* Get the width and height of the image */
  *width = header->xmax - header->xmin + 1;
  *height = header->ymax - header->ymin + 1;

  /* Allocate enough room for the data and colourmap*/
  data = malloc(*width * *height * 4);

  if (!data) 
    {
      munmap(header, file_length);
      return NULL;
    }
	
  /* Decode the data */
  pcx_raw_decode24(data, header->data, *width, *height);

  /* Clean up */
  munmap(header, file_length);
  
  close(fd);

  return data;
}


/* -------------------------------------------------------------------- */

Pixbuf*
pixbuf_new(int width, int height)
{
  Pixbuf *pixb;

  pixb = util_malloc0(sizeof(Pixbuf));

  pixb->width           = width; 
  pixb->height          = height; 
  pixb->bytes_per_pixel = 4; 
  pixb->channels        = 4;
  pixb->bytes_per_line  = pixb->bytes_per_pixel * pixb->width;
  pixb->data            = malloc(pixb->bytes_per_line * pixb->height);

  memset(pixb->data, 0, pixb->bytes_per_line * pixb->height);

  return pixb;
}

void
pixbuf_unref(Pixbuf *pixb)
{
  pixb->refcnt--;

  if (pixb->refcnt < 0)
    {
      free(pixb->data);
      free(pixb);
    }
}

void
pixbuf_ref(Pixbuf *pixb)
{
  pixb->refcnt++;
}

Pixbuf*
pixbuf_new_from_file(const char *filename)
{
  Pixbuf *pixb;

  pixb = util_malloc0(sizeof(Pixbuf));

  if (!strcasecmp(&filename[strlen(filename)-4], ".png"))
    pixb->data =load_png_file(filename, &pixb->width, &pixb->height); 
  else if (!strcasecmp(&filename[strlen(filename)-4], ".jpg")
	   || !strcasecmp(&filename[strlen(filename)-5], ".jpeg"))
    pixb->data = load_jpg_file( filename, &pixb->width, &pixb->height); 
  else if (!strcasecmp(&filename[strlen(filename)-4], ".pcx"))
    pixb->data = load_pcx_file( filename, &pixb->width, &pixb->height); 

  if (pixb->data == NULL)
    {
      free (pixb);
      return NULL;
    }

  pixb->bytes_per_pixel = 4; 
  pixb->channels        = 4;
  pixb->bytes_per_line  = pixb->bytes_per_pixel * pixb->width;
		
  return pixb;
}

void
pixbuf_set_pixel(Pixbuf *pixb, int x, int y, PixbufPixel *p)
{
  int *offset = pixb->data + ( y * pixb->width) + x;

  /* ARGB_32 MSB */

  // *offset = (p->r << 0) | (p->g << 8) | (p->b << 16) | (p->a << 24);
  *offset = (p->r << 24) | (p->g << 16) | (p->b << 8) | (p->a);
}

void
pixbuf_get_pixel(Pixbuf *pixb, int x, int y, PixbufPixel *p)
{
  int *offset = pixb->data + ( y * pixb->width) + x;

  /* ARGB_32 MSB */

  p->r = (*offset >> 24) & 0xff;
  p->g = (*offset >> 16) & 0xff;
  p->b = (*offset >> 8) & 0xff;
  p->a =  *offset & 0xff;

}

void 				/* XXX could be DEFINE */
pixel_set_vals(PixbufPixel        *p, 
	       const unsigned char r,
	       const unsigned char g,
	       const unsigned char b,
	       const unsigned char a)
{
  p->r = r; p->g = g; p->b = b; p->a = a;
}

void
pixbuf_copy(Pixbuf *src_pixb,
	    Pixbuf *dst_pixb,
	    int     srcx, 
	    int     srcy, 
	    int     srcw, 
	    int     srch,
	    int     dstx, 
	    int     dsty)
{
  int j, *sp, *dp;
  
  sp = src_pixb->data + (srcy * src_pixb->width) + srcx;
  dp = dst_pixb->data + (dsty * dst_pixb->width) + dstx;

  /* basic source clipping - needed by texture tiling code */

  if (srcx + srcw > src_pixb->width)
    srcw = src_pixb->width - srcx;

  if (srcy + srch > src_pixb->height)
    srch = src_pixb->height - srcy;

  while (srch--)
    {
      j = srcw;
      while (j--)
	*dp++ = *sp++;
      dp += (dst_pixb->width - srcw);
      sp += (src_pixb->width - srcw);
    }
}

void
pixbuf_fill_rect(Pixbuf      *pixb,
		 int          x,
		 int          y,
		 int          width,
		 int          height,
		 PixbufPixel *p)
{
  int i, j;

  if (width  < 0) width  = pixb->width;
  if (height < 0) height = pixb->height;

  for (i = x; i<width; i++)
    for (j =y; j<height; j++)
	pixbuf_set_pixel(pixb, i, j, p);
}

Pixbuf *
pixbuf_scale_down(Pixbuf *pixb,
		  int     new_width, 
		  int     new_height)
{
  Pixbuf        *pixb_scaled;
  int *xsample, *ysample, *dest, *src, *srcy;
  int i, x, y,  r, g, b, a, nb_samples, xrange, yrange, rx, ry;

  if ( new_width > pixb->width || new_height > pixb->height) 
    return NULL;

  pixb_scaled = pixbuf_new(new_width, new_height);

  xsample = malloc( (new_width+1) * sizeof(int));
  ysample = malloc( (new_height+1) * sizeof(int));

  for ( i = 0; i <= new_width; i++ )
    xsample[i] = i * pixb->width / new_width;

  for ( i = 0; i <= new_height; i++ )
    ysample[i] = i * pixb->height / new_height * pixb->width;

  dest = pixb_scaled->data;

  /* scan output image */
  for ( y = 0; y < new_height; y++ ) 
    {
      yrange = ( ysample[y+1] - ysample[y] ) / pixb->width;
      for ( x = 0; x < new_width; x++) 
	{
	  xrange = xsample[x+1] - xsample[x];
	  srcy   = pixb->data + ( ysample[y] + xsample[x] );
	  
	  /* average R,G,B,A values on sub-rectangle of source image */
	  nb_samples = xrange * yrange;

	  if ( nb_samples > 1 ) 
	    {
	      r = 0; g = 0; b = 0; a = 0;
	      for ( ry = 0; ry < yrange; ry++ ) 
		{
		  src = srcy;
		  for ( rx = 0; rx < xrange; rx++ ) 
		    {
		      /* average R,G,B,A values */
		      r +=  *src & 0xff;
		      g += ((*src) >> 8) & 0xff;
		      b += ((*src) >> 16) & 0xff;
		      a += ((*src) >> 24) & 0xff;

		      src++;
		    }

		  srcy += pixb->width;
		}

	      *dest++ = 
		((unsigned char)(r/nb_samples) << 0)  | 
		((unsigned char)(g/nb_samples) << 8)  | 
		((unsigned char)(b/nb_samples) << 16) | 
		((unsigned char)(a/nb_samples) << 24);
	    }
	  else 
	    {
	      *dest++ = *srcy++;
	    }
	}
    }

  /* cleanup */
  free( xsample );
  free( ysample );

  return pixb_scaled;
}

#if 0

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%     C o n v o l v e I m a g e                                               %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  Method ConvolveImage applies a general image convolution kernel to an
%  image returns the results.  ConvolveImage allocates the memory necessary for
%  the new Image structure and returns a pointer to the new image.
%
%  The format of the ConvolveImage method is:
%
%      Image *ConvolveImage(Image *image,const unsigned int order,
%        const double *kernel,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o convolve_image: Method ConvolveImage returns a pointer to the image
%      after it is convolved.  A null image is returned if there is a memory
%      shortage.
%
%    o image: The address of a structure of type Image;  returned from
%      ReadImage.
%
%    o order:  The number of columns and rows in the filter kernel.
%
%    o kernel:  An array of double representing the convolution kernel.
%
%    o exception: return any errors or warnings in this structure.
%
%
*/
MagickExport Image *ConvolveImage(Image *image,
				  const unsigned int order,
				  const double *kernel,
				  ExceptionInfo *exception)
{
#define ConvolveImageText  "  Convolving image...  "
#define Cx(x) \
  (x) < 0 ? (x)+image->columns : (x) >= image->columns ? (x)-image->columns : x
#define Cy(y) \
  (y) < 0 ? (y)+image->rows : (y) >= image->rows ? (y)-image->rows : y

  double
    blue,
    green,
    normalize,
    opacity,
    red;

  Image
    *convolve_image;

  int
    i,
    width,
    y;

  PixelPacket
    *p,
    pixel;

  register const double
    *k;

  register int
    u,
    v,
    x;

  register PixelPacket
    *q,
    *s;

  /*
    Initialize convolved image attributes.
  */
  assert(image != (Image *) NULL);
  assert(image->signature == MagickSignature);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickSignature);
  width=order;
  if ((width % 2) == 0)
    ThrowImageException(OptionWarning,"Unable to convolve image",
			"kernel width must be an odd number");
  if ((image->columns < width) || (image->rows < width))
    ThrowImageException(OptionWarning,"Unable to convolve image",
			"image smaller than kernel width");
  convolve_image=CloneImage(image,image->columns,image->rows,False,exception);
  if (convolve_image == (Image *) NULL)
    return((Image *) NULL);
  convolve_image->storage_class=DirectClass;
  /*
    Convolve image.
  */
  normalize=0.0;
  for (i=0; i < (width*width); i++)
    normalize+=kernel[i];
  for (y=0; y < (int) convolve_image->rows; y++)
    {
      p=(PixelPacket *) NULL;
      q=SetImagePixels(convolve_image,0,y,convolve_image->columns,1);
      if (q == (PixelPacket *) NULL)
	break;
      for (x=0; x < (int) convolve_image->columns; x++)
	{
	  red=0.0;
	  green=0.0;
	  blue=0.0;
	  opacity=0.0;
	  k=kernel;
	  if ((x < (width/2)) || (x >= (int) (image->columns-width/2)) ||
	      (y < (width/2)) || (y >= (int) (image->rows-width/2)))
	    {
	      for (v=(-width/2); v <= (width/2); v++)
		{
		  for (u=(-width/2); u <= (width/2); u++)
		    {
		      pixel=GetOnePixel(image,Cx(x+u),Cy(y+v));
		      red+=(*k)*pixel.red;
		      green+=(*k)*pixel.green;
		      blue+=(*k)*pixel.blue;
		      opacity+=(*k)*pixel.opacity;
		      k++;
		    }
		}
	    }
	  else
	    {
	      if (p == (PixelPacket *) NULL)
		{
		  p=GetImagePixels(image,0,y-width/2,image->columns,width);
		  if (p == (PixelPacket *) NULL)
		    break;
		}
	      s=p+x;
	      for (v=(-width/2); v <= (width/2); v++)
		{
		  for (u=(-width/2); u <= (width/2); u++)
		    {
		      red+=(*k)*s[u].red;
		      green+=(*k)*s[u].green;
		      blue+=(*k)*s[u].blue;
		      opacity+=(*k)*s[u].opacity;
		      k++;
		    }
		  s+=image->columns;
		}
	    }
	  if ((normalize != 0.0) && (normalize != 1.0))
	    {
	      red/=normalize;
	      green/=normalize;
	      blue/=normalize;
	      opacity/=normalize;
	    }
	  q->red=(Quantum) ((red < 0) ? 0 : (red > MaxRGB) ? MaxRGB : red+0.5);
	  q->green=(Quantum)
	    ((green < 0) ? 0 : (green > MaxRGB) ? MaxRGB : green+0.5);
	  q->blue=(Quantum) ((blue < 0) ? 0 : (blue > MaxRGB) ? MaxRGB : blue+0.5);
	  q->opacity=(Quantum)
	    ((opacity < 0) ? 0 : (opacity > MaxRGB) ? MaxRGB : opacity+0.5);
	  q++;
	}
      if (!SyncImagePixels(convolve_image))
	break;
      if (QuantumTick(y,convolve_image->rows))
	MagickMonitor(ConvolveImageText,y,convolve_image->rows);
    }
  return(convolve_image);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%     G a u s s i a n B l u r I m a g e                                       %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  Method GaussianBlurImage creates a new image that is a copy of an existing
%  one with the pixels blur.  It allocates the memory necessary for the
%  new Image structure and returns a pointer to the new image.
%
%  The format of the BlurImage method is:
%
%      Image *GaussianBlurImage(Image *image,const double radius,
%        const double sigma,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o blur_image: Method GaussianBlurImage returns a pointer to the image
%      after it is blur.  A null image is returned if there is a memory
%      shortage.
%
%    o radius: the radius of the Gaussian, in pixels, not counting the center
%      pixel.
%
%    o sigma: the standard deviation of the Gaussian, in pixels.
%
%    o exception: return any errors or warnings in this structure.
%
%
*/
MagickExport Image *GaussianBlurImage(Image *image,const double radius,
				      const double sigma,ExceptionInfo *exception)
{
  double
    *kernel;

  Image
    *blur_image;

  int
    width;

  register int
    i,
    u,
    v;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickSignature);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickSignature);
  width=GetOptimalKernelWidth2D(radius,sigma);
  if ((image->columns < width) || (image->rows < width))
    ThrowImageException(OptionWarning,"Unable to Gaussian blur image",
			"image is smaller than radius");
  kernel=(double *) AcquireMemory(width*width*sizeof(double));
  if (kernel == (double *) NULL)
    ThrowImageException(ResourceLimitWarning,"Unable to Gaussian blur image",
			"Memory allocation failed");
  i=0;
  for (v=(-width/2); v <= (width/2); v++)
    {
      for (u=(-width/2); u <= (width/2); u++)
	{
	  kernel[i]=exp((double) -(u*u+v*v)/(sigma*sigma));
	  i++;
	}
    }
  blur_image=ConvolveImage(image,width,kernel,exception);
  LiberateMemory((void **) &kernel);
  return(blur_image);
}

#endif
