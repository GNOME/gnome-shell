#include "cltr-glu.h"
#include "cltr-private.h"

/* Clutter GL Utility routines */

#define PI 3.1415926535897932384626433832795

void
cltr_glu_set_color(PixbufPixel *p)
{
  glColor4ub(p->r, p->b, p->g, p->a); 
}

void
cltr_glu_rounded_rect(int        x1, 
		      int        y1, 
		      int        x2, 
		      int        y2,
		      int        line_width,
		      int        radius, 
		      PixbufPixel *col)
{  
  double ang = 0;
  int    width = x2-x1, height = y2-y1; 
  float  cX = x1+radius, cY = y1+radius; 

  if (col)
    cltr_glu_set_color(col);

  glLineWidth(line_width); 

  glBegin(GL_LINES); 
  glVertex2f(x1, y1 + radius); 
  glVertex2f(x1, y1 + height - radius); /* Left Line */

  glVertex2f(x1 + radius, y1); 
  glVertex2f(x1 + width - radius, y1);  /* Top Line */

  glVertex2f(x1 + width, y1 + radius); 
  glVertex2f(x1 + width, y1 + height - radius); /* Right Line */

  glVertex2f(x1 + radius, y1 + height); 
  glVertex2f(x1 + width - radius, y1 + height); /* Bottom Line */
  glEnd(); 

  /* corners */

  glBegin(GL_LINE_STRIP); 

  /* Top Left  */
  for(ang = PI; ang <= (1.5*PI); ang = ang + 0.05) 
    { 
      glVertex2d(radius* cos(ang) + cX, radius * sin(ang) + cY);
    }  

  glEnd(); 

  /* Top Right */

  cX = x1 + width-radius; 

  glBegin(GL_LINE_STRIP); 

  for(ang = (1.5*PI); ang <= (2 * PI); ang = ang + 0.05) 
    { 
      glVertex2d(radius* cos(ang) + cX, radius * sin(ang) + cY);
    } 

  glEnd(); 

  glBegin(GL_LINE_STRIP); 

  cY = y1 + height-radius; 

  /* Bottom Right */

  for(ang = 0; ang <= (0.5*PI); ang = ang + 0.05) 
    { 
      glVertex2d(radius* cos(ang) + cX, radius * sin(ang) + cY);
    } 

  glEnd(); 

  glBegin(GL_LINE_STRIP); 

  cX = x1 + radius; 

  /* Bottom Left */

  for(ang = (0.5*PI); ang <= PI; ang = ang + 0.05) 
    { 
      glVertex2d(radius* cos(ang) + cX, radius * sin(ang) + cY);

    } 
  glEnd(); 

} 

void 
cltr_glu_rounded_rect_filled(int        x1, 
			     int        y1, 
			     int        x2, 
			     int        y2,
			     int        radius, 
			     PixbufPixel *col)
{  
  double i   = 0; 
  double gap = 0.05; 
  float  cX  = x1 + radius, cY = y1 + radius; 

  if (col)
    cltr_glu_set_color(col);

  glBegin(GL_POLYGON); 

 /* Left Line */
  glVertex2f(x1, y2 - radius);
  glVertex2f(x1, y1 + radius); 

 /* Top Left */
  for(i = PI; i <= (1.5*PI); i += gap) 
    glVertex2d(radius* cos(i) + cX, radius * sin(i) + cY);

 /* Top Line */
  glVertex2f(x1 + radius, y1); 
  glVertex2f(x2 - radius, y1);

  cX = x2 - radius; 

  /* Top Right */
  for(i = (1.5*PI); i <= (2 * PI); i += gap) 
    glVertex2d(radius* cos(i) + cX, radius * sin(i) + cY);

  glVertex2f(x2, y1 + radius); 

  /* Right Line */
  glVertex2f(x2, y2 - radius);

  cY = y2 - radius; 

  /* Bottom Right */
  for(i = 0; i <= (0.5*PI); i+=gap) 
    glVertex2d(radius* cos(i) + cX, radius * sin(i) + cY);

  /* Bottom Line */
  glVertex2f(x1 + radius, y2); 
  glVertex2f(x2 - radius, y2);

  /* Bottom Left */
  cX = x1 + radius; 
  for(i = (0.5*PI); i <= PI; i += gap) 
    glVertex2d(radius* cos(i) + cX, radius * sin(i) + cY);

  glEnd(); 
} 

