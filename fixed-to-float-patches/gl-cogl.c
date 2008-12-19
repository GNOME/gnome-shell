diff --git a/clutter/cogl/gl/cogl.c b/clutter/cogl/gl/cogl.c
index 7b61b63..dcded98 100644
--- a/clutter/cogl/gl/cogl.c
+++ b/clutter/cogl/gl/cogl.c
@@ -211,17 +211,17 @@ cogl_pop_matrix (void)
 void
 cogl_scale (float x, float y)
 {
-  glScaled ((double)(x),
-	    (double)(y),
+  glScalef ((float)(x),
+	    (float)(y),
 	    1.0);
 }
 
 void
 cogl_translatex (float x, float y, float z)
 {
-  glTranslated ((double)(x),
-		(double)(y),
-		(double)(z));
+  glTranslatef ((float)(x),
+		(float)(y),
+		(float)(z));
 }
 
 void
@@ -233,10 +233,10 @@ cogl_translate (gint x, gint y, gint z)
 void
 cogl_rotatex (float angle, gint x, gint y, gint z)
 {
-  glRotated ((double)(angle),
-	     (double)(x),
-	     (double)(y),
-	     (double)(z));
+  glRotatef ((float)(angle),
+	     (float)(x),
+	     (float)(y),
+	     (float)(z));
 }
 
 void
@@ -645,17 +645,13 @@ cogl_perspective (float fovy,
    * 2) When working with small numbers, we are loosing significant
    * precision
    */
-  ymax =
-    (zNear *
-                    (sinf (fovy_rad_half) /
-                                         cosf (fovy_rad_half)));
-
+  ymax = (zNear * (sinf (fovy_rad_half) / cosf (fovy_rad_half)));
   xmax = (ymax * aspect);
 
   x = (zNear / xmax);
   y = (zNear / ymax);
   c = (-(zFar + zNear) / ( zFar - zNear));
-  d = cogl_fixed_mul_div (-(2 * zFar), zNear, (zFar - zNear));
+  d = (-(2 * zFar) * zNear) / (zFar - zNear);
 
 #define M(row,col)  m[col*4+row]
   M(0,0) =  (x);
@@ -696,12 +692,12 @@ cogl_frustum (float        left,
   GE( glMatrixMode (GL_PROJECTION) );
   GE( glLoadIdentity () );
 
-  GE( glFrustum ((double)(left),
-		 (double)(right),
-		 (double)(bottom),
-		 (double)(top),
-		 (double)(z_near),
-		 (double)(z_far)) );
+  GE( glFrustum ((GLdouble)(left),
+		 (GLdouble)(right),
+		 (GLdouble)(bottom),
+		 (GLdouble)(top),
+		 (GLdouble)(z_near),
+		 (GLdouble)(z_far)) );
 
   GE( glMatrixMode (GL_MODELVIEW) );
 
@@ -773,9 +769,7 @@ cogl_setup_viewport (guint        width,
   {
     float fovy_rad = (fovy * G_PI) / 180;
 
-    z_camera =
-       ((sinf (fovy_rad) /
-				           cosf (fovy_rad)) >> 1);
+    z_camera = ((sinf (fovy_rad) / cosf (fovy_rad)) / 2);
   }
 
   GE( glTranslatef (-0.5f, -0.5f, -z_camera) );
