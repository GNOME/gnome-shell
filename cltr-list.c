#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <X11/Xlib.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include "pixbuf.h"
#include "fonts.h"

#define TEX_W 1024   /* must be > than frame_w & power of 2 */
#define TEX_H 1024
/* 
 *
 */

#define WINW 640
#define WINH 480

#define NBOXITEMS 10

#define NUMRECTS 4
#define MAXSCALE 2
#define MAXDIST (WINH)
#define MAXH    (WINH/NUMRECTS) 
#define MAXW    (WINW - 20)

#ifndef ABS
#define ABS(a) ((a > 0) ? (a) : -1 * (a))
#endif

typedef struct TableWidget TableWidget;

typedef struct TableWidgetCell TableWidgetCell;

int ScrollDir = 1;

struct TableWidget
{
  int x,y,width,height;
  
  TableWidgetCell *cells, *active_cell;
  
  int active_cell_y;


};

struct TableWidgetCell
{
  XRectangle rect;
  TableWidgetCell   *next, *prev;

};


Display       *xdpy;
Window         xwin;
XEvent         xevent;         
GC             xgc;
Pixbuf        *pix, *pix_orig;

float
distfunc(TableWidget *table, int d)
{
/*   printf("returning %f\n", (exp((float)d/MAXDIST)/exp(1.0)));   */
  int maxdist = table->height;

  d = (maxdist-ABS(d)) ;
  return ( exp( (float)d/maxdist * 2.0 ) / exp(2.0) );
}

TableWidgetCell*
table_cell_new()
{
  TableWidgetCell *cell = NULL;

  cell = malloc(sizeof(TableWidgetCell));
  memset(cell, 0, sizeof(TableWidgetCell));

  return cell;
}

TableWidget*
table_new(int n_items)
{
  TableWidget     *table = NULL;
  TableWidgetCell *last = NULL, *cell = NULL;
  int              i;

  table = malloc(sizeof(TableWidget));
  memset(table, 0, sizeof(TableWidget));

  table->width = WINW;
  table->height = WINH;

  table->active_cell_y = 100;

  for (i=0; i<n_items; i++)
    {
      cell = table_cell_new();
      if (last)
	{
	  last->next = cell;
	  cell->prev = last;
	}
      else
	table->cells = table->active_cell = cell;
  
      last = cell;
    }

  table->cells->rect.y = table->active_cell_y;

  return table;
}

void
table_redraw(TableWidget *table)
{
  TableWidgetCell *cur = table->cells;
  int             last = table->cells->rect.y;

  glClearColor( 0.0, 0.0, 0.0, 1.0);
  glClear    (GL_COLOR_BUFFER_BIT);

  while (cur)
    {
      cur->rect.y = last;
      
      if (cur->rect.y+cur->rect.height >= 0)
	{
	  cur->rect.width  = MAXW * distfunc(table, cur->rect.y - table->active_cell_y);
	  cur->rect.height = MAXH * distfunc(table, cur->rect.y - table->active_cell_y);

	  cur->rect.x = (WINW - cur->rect.width)/6; 
	}
      
      last = cur->rect.y + cur->rect.height;
      
      if (last > 0 && cur->rect.y < WINH) /* crappy clip */
	{
	  float tx = 1.0, ty = 1.0, sx, sy;
	  int x1 = cur->rect.x, x2 = cur->rect.x + cur->rect.width;
	  int y1 = cur->rect.y, y2 = cur->rect.y + cur->rect.height;

	  tx = (float) pix->width  / TEX_W;
	  ty = (float) pix->height / TEX_H;


	  glBegin (GL_QUADS);
	  glTexCoord2f (tx, ty);   glVertex2i   (x2, y2);
	  glTexCoord2f (0,  ty);   glVertex2i   (x1, y2);
	  glTexCoord2f (0,  0);    glVertex2i   (x1, y1);
	  glTexCoord2f (tx, 0);    glVertex2i   (x2, y1);
	  glEnd ();	

	  /* draw with X primitives
	  XDrawRectangle(xdpy, xwin, xgc, 
			 cur->rect.x, cur->rect.y,
			 cur->rect.width, cur->rect.height);

	  XDrawLine(xdpy, xwin, xgc,
		    cur->rect.x, cur->rect.y,
		    cur->rect.x + cur->rect.width, 
		    cur->rect.y + cur->rect.height);
	  */
	}
      
      cur = cur->next;
    }

  glXSwapBuffers(xdpy, xwin);  

}


void
table_scroll_down(TableWidget *table)
{
  TableWidgetCell *next_active =  table->active_cell->next;

  if (!next_active)
    {
      ScrollDir = 0;
      return;
    }

  while (next_active->rect.y > table->active_cell_y)
    {
      table->cells->rect.y--;
      table_redraw(table);
    }

  table->active_cell = next_active;
}

void
table_scroll_up(TableWidget *table)
{
  TableWidgetCell *next_active =  table->active_cell->prev;

  if (!next_active)
    return;

  while (next_active->rect.y < table->active_cell_y)
    {
      table->cells->rect.y++;
      table_redraw(table);
    }

  table->active_cell = next_active;
}


int
main(int argc, char **argv)
{
  TableWidget *table;
  int       i, j, last, offset=0;
  XGCValues gcvals;
  ClutterFont *font = NULL;

  /* GL */
  GLXContext		context;	/* OpenGL context */
  GLubyte              *texture_data = NULL;
  XVisualInfo	       *vinfo;
  static int 		attributes[] =
			{
			  GLX_RGBA, 
			  GLX_DOUBLEBUFFER,
			  GLX_RED_SIZE, 1,
			  GLX_GREEN_SIZE, 1,
			  GLX_BLUE_SIZE, 1,
			  0
			};

  

  if ((xdpy = XOpenDisplay(getenv("DISPLAY"))) == NULL)
    {
      fprintf(stderr, "%s: Cant open display\n", argv[0]);
      exit(-1);
    }
  

  if ((vinfo = glXChooseVisual(xdpy, DefaultScreen(xdpy), attributes)) == NULL)
    {
      fprintf(stderr, "Unable to find visual\n");
      exit(-1);
    }

  xwin = XCreateSimpleWindow(xdpy,
			     RootWindow(xdpy, DefaultScreen(xdpy)),
			     0, 0,
			     WINW, WINH,
			     0, 0, WhitePixel(xdpy, DefaultScreen(xdpy)));

  gcvals.foreground = BlackPixel(xdpy, DefaultScreen(xdpy));
  gcvals.background = WhitePixel(xdpy, DefaultScreen(xdpy));
  gcvals.line_width = 1;

  xgc = XCreateGC(xdpy, RootWindow(xdpy, DefaultScreen(xdpy)), 
		  GCForeground|GCBackground|GCLineWidth, 
		  &gcvals);

  context = glXCreateContext(xdpy, vinfo, 0, True);
  glXMakeCurrent(xdpy, xwin, context);

  glViewport (0, 0, WINW, WINH);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  glClearColor (0, 0, 0, 0);
  glClearDepth (1.0f);

  glDisable    (GL_DEPTH_TEST);
  glDepthMask  (GL_FALSE);
  glDisable    (GL_CULL_FACE);
  glShadeModel (GL_FLAT);
  glHint       (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  // glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, WINW, WINH, 0, -1, 1);

  glEnable        (GL_TEXTURE_2D);
  glTexParameteri (GL_TEXTURE_2D,  GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D,  GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexEnvi       (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,   GL_REPLACE);

  texture_data = malloc (TEX_W * TEX_H * 4);

  for (i=0; i < (TEX_W * TEX_H * 4); i++)
    texture_data[i] = rand()%255;

  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
		TEX_W, TEX_H,
		0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);

  pix_orig = pixbuf_new_from_file(argv[1]);

  if (!pix_orig)
    {
      fprintf(stderr, "image load failed\n");
      exit(-1);
    }

  pix = pixbuf_scale_down(pix_orig, 100, 100);

  /*
  font = font_new("Sans Bold 48");

  font_draw(font, pix, "Hello World\nlove matmoo", 0, 0);
  */

  glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
		   (GLsizei)pix->width,
		   (GLsizei)pix->height,
		   GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
		   pix->data);

  table = table_new(NBOXITEMS);

  table_redraw(table);

  XMapWindow(xdpy, xwin);

  for (;;) 
    {
      XEvent ev;
      // XNextEvent(xdpy, &ev);
      // XClearWindow(xdpy, xwin);
      table_redraw(table);
      ScrollDir ? table_scroll_down(table) : table_scroll_up(table);
      
      // scroll_to_next();
      XFlush(xdpy);
      sleep(1);
    }

}
