#include "cltr-glu.h"
#include "cltr-private.h"

/* Clutter GL Utility routines */

#define PI 3.1415926535897932384626433832795

void
cltr_glu_set_color(PixbufPixel *p)
{
  glColor4ub(p->r, p->b, p->g, p->a); 
}

#if 0
void 
DrawRoundedSquare(float LineWidth, 
		  float x, 
		  float y, 
		  float radius, 
		  float width, 
		  float height, 
		  float r, 
		  float b, 
		  float g) 
{  
  double ang=0; 

  glColor3f(r, b, g); 
  glLineWidth(LineWidth); 

  glBegin(GL_LINES); 
  glVertex2f(x, y + radius); 
  glVertex2f(x, y + height - radius); /* Left Line */

  glVertex2f(x + radius, y); 
  glVertex2f(x + width - radius, y); /* Top Line */

  glVertex2f(x + width, y + radius); 
  glVertex2f(x + width, y + height - radius); /* Right Line */

  glVertex2f(x + radius, y + height); 
  glVertex2f(x + width - radius, y + height);//Bottom Line 
  glEnd(); 

  float cX= x+radius, cY = y+radius; 
  glBegin(GL_LINE_STRIP); 
  for(ang = PI; ang <= (1.5*PI); ang = ang + 0.05) 
    { 
      glVertex2d(radius* cos(ang) + cX, radius * sin(ang) + cY); //Top Left 
    }  
  cX = x+width-radius; 
  glEnd(); 
  glBegin(GL_LINE_STRIP); 
  for(ang = (1.5*PI); ang <= (2 * PI); ang = ang + 0.05) 
    { 
      glVertex2d(radius* cos(ang) + cX, radius * sin(ang) + cY); //Top Right 
    } 
  glEnd(); 
  glBegin(GL_LINE_STRIP); 
  cY = y+height-radius; 
  for(ang = 0; ang <= (0.5*PI); ang = ang + 0.05) 
    { 
      glVertex2d(radius* cos(ang) + cX, radius * sin(ang) + cY); //Bottom Right 
    } 
  glEnd(); 
  glBegin(GL_LINE_STRIP); 
  cX = x+radius; 
  for(ang = (0.5*PI); ang <= PI; ang = ang + 0.05) 
    { 
      glVertex2d(radius* cos(ang) + cX, radius * sin(ang) + cY);//Bottom Left 
    } 
  glEnd(); 
} 
#endif

void 
cltr_glu_rounded_rect(int        x1, 
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

