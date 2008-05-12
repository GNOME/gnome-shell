/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Johan Bilien  <johan.bilien@nokia.com>
 *
 * Copyright (C) 2007 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-x11-texture-pixmap
 * @short_description: A texture which displays the content of an X Pixmap.
 *
 * #ClutterX11TexturePixmap is a class for displaying the content of an
 * X Pixmap as a ClutterActor. Used together with the X Composite extension,
 * it allows to display the content of X Windows inside Clutter.
 *
 * The class uses the GLX_EXT_texture_from_pixmap OpenGL extension
 * (http://people.freedesktop.org/~davidr/GLX_EXT_texture_from_pixmap.txt)
 * if available
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../clutter-marshal.h"
#include "clutter-x11-texture-pixmap.h"
#include "clutter-x11.h"
#include "clutter-backend-x11.h"

#include "cogl/cogl.h"

/* FIXME: Check exts exist in autogen */
#include <X11/extensions/Xdamage.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <X11/extensions/XShm.h>

enum
{
  PROP_PIXMAP = 1,
  PROP_PIXMAP_WIDTH,
  PROP_PIXMAP_HEIGHT,
  PROP_DEPTH,
  PROP_AUTO
};

enum
{
  UPDATE_AREA,
  /* FIXME: Pixmap lost signal? */
  LAST_SIGNAL
};

static ClutterX11FilterReturn 
on_x_event_filter (XEvent *xev, ClutterEvent *cev, gpointer data);

static void
clutter_x11_texture_pixmap_update_area_real (ClutterX11TexturePixmap *texture,
                                             gint                     x,
                                             gint                     y,
                                             gint                     width,
                                             gint                     height);

static guint signals[LAST_SIGNAL] = { 0, };

struct _ClutterX11TexturePixmapPrivate
{
  Pixmap        pixmap;
  guint         pixmap_width, pixmap_height;
  guint         depth;

  XImage       *image;
  XShmSegmentInfo shminfo;

  gboolean      automatic_updates;     
  Damage        damage;

  gboolean	have_shm;
};

static int _damage_event_base = 0;

/* FIXME: Ultimatly with current cogl we should subclass clutter actor */
G_DEFINE_TYPE (ClutterX11TexturePixmap, \
               clutter_x11_texture_pixmap, \
               CLUTTER_TYPE_TEXTURE);

static gboolean
check_extensions (ClutterX11TexturePixmap *texture)
{
  int                             damage_error;
  ClutterX11TexturePixmapPrivate *priv;
  Display                        *dpy;

  priv = texture->priv;

  if (_damage_event_base)
    return TRUE;

  dpy = clutter_x11_get_default_display();

  if (!XDamageQueryExtension (dpy,
                              &_damage_event_base, &damage_error))
    {
      g_warning ("No Damage extension\n");
      return FALSE;
    }

  return TRUE;
}

static void
free_shm_resources (ClutterX11TexturePixmap *texture)
{
  ClutterX11TexturePixmapPrivate *priv;

  priv = texture->priv;

  if (priv->shminfo.shmid != -1)
    {
      XShmDetach(clutter_x11_get_default_display(),
		 &priv->shminfo);
      shmdt(priv->shminfo.shmaddr);
      shmctl(priv->shminfo.shmid, IPC_RMID, 0);
      priv->shminfo.shmid = -1;
    }
}

/* Tries to allocate enough shared mem to handle a full size
 * update size of the X Pixmap. */
static gboolean
try_alloc_shm (ClutterX11TexturePixmap *texture)
{
  ClutterX11TexturePixmapPrivate  *priv;
  XImage			  *dummy_image;
  Display			  *dpy;

  priv = texture->priv;
  dpy  = clutter_x11_get_default_display();

  g_return_val_if_fail (priv->pixmap, FALSE);

  if (!XShmQueryExtension(dpy) || g_getenv("CLUTTER_X11_NO_SHM"))
    {
      priv->have_shm = FALSE;
      return FALSE;
    }

  clutter_x11_trap_x_errors ();

  /* We are creating a dummy_image so we can have Xlib calculate
   * image->bytes_per_line - including any magic padding it may
   * want - for the largest possible ximage we might need to use
   * when handling updates to the texture.
   *
   * Note: we pass a NULL shminfo here, but that has no bearing
   * on the setup of the XImage, except that ximage->obdata will
   * == NULL.
   */
  dummy_image =
    XShmCreateImage(dpy,
		    DefaultVisual(dpy,
				  clutter_x11_get_default_screen()),
		    priv->depth,
		    ZPixmap,
		    NULL,
		    NULL, /* shminfo, */
		    priv->pixmap_width,
		    priv->pixmap_height);
  if (!dummy_image)
    goto failed_image_create;

  priv->shminfo.shmid = shmget (IPC_PRIVATE,
				dummy_image->bytes_per_line
				* dummy_image->height,
				IPC_CREAT|0777);
  if (priv->shminfo.shmid == -1)
    goto failed_shmget;

  priv->shminfo.shmaddr =
    shmat (priv->shminfo.shmid, 0, 0);
  if (priv->shminfo.shmaddr == (void *)-1)
    goto failed_shmat;

  priv->shminfo.readOnly = False;

  if (XShmAttach(dpy, &priv->shminfo) == 0)
    goto failed_xshmattach;

  if (clutter_x11_untrap_x_errors ())
    g_warning ("X Error: Failed to setup XShm.");

  priv->have_shm = TRUE;
  return TRUE;

failed_xshmattach:
  g_warning ("XShmAttach failed");
  shmdt(priv->shminfo.shmaddr);
failed_shmat:
  g_warning ("shmat failed");
  shmctl(priv->shminfo.shmid, IPC_RMID, 0);
failed_shmget:
  g_warning ("shmget failed");
  XDestroyImage(dummy_image);
failed_image_create:

  if (clutter_x11_untrap_x_errors ())
    g_warning ("X Error: Failed to setup XShm.");

  priv->have_shm = FALSE;
  return FALSE;
}

static ClutterX11FilterReturn 
on_x_event_filter (XEvent *xev, ClutterEvent *cev, gpointer data)
{
  ClutterX11TexturePixmap        *texture;
  ClutterX11TexturePixmapPrivate *priv;
  Display                        *dpy;

  texture = CLUTTER_X11_TEXTURE_PIXMAP (data);
  
  g_return_val_if_fail (CLUTTER_X11_IS_TEXTURE_PIXMAP (texture), \
                        CLUTTER_X11_FILTER_CONTINUE);

  dpy = clutter_x11_get_default_display();
  priv = texture->priv;
  
  if (xev->type == _damage_event_base + XDamageNotify)
    {
      XserverRegion  parts;
      gint           i, r_count;
      XRectangle    *r_damage;
      XRectangle     r_bounds;
      XDamageNotifyEvent *dev = (XDamageNotifyEvent*)xev;
      
      if (dev->drawable != priv->pixmap)
        return CLUTTER_X11_FILTER_CONTINUE;


      clutter_x11_trap_x_errors ();
      /*
       * Retrieve the damaged region and break it down into individual
       * rectangles so we do not have to update the whole shebang.
       */
      parts = XFixesCreateRegion (dpy, 0, 0);
      XDamageSubtract (dpy, priv->damage, None, parts);

      r_damage = XFixesFetchRegionAndBounds (dpy, 
                                             parts,
                                             &r_count,
                                             &r_bounds);

      clutter_x11_untrap_x_errors ();

      if (r_damage)
        {
          for (i = 0; i < r_count; ++i)
            clutter_x11_texture_pixmap_update_area (texture,
                                                    r_damage[i].x,
                                                    r_damage[i].y,
                                                    r_damage[i].width,
                                                    r_damage[i].height);
          XFree (r_damage);
        }

      XFixesDestroyRegion (dpy, parts);
    }

  return  CLUTTER_X11_FILTER_CONTINUE;
}


static void
free_damage_resources (ClutterX11TexturePixmap *texture)
{
  ClutterX11TexturePixmapPrivate *priv;
  Display                        *dpy;

  priv = texture->priv;
  dpy = clutter_x11_get_default_display();

  if (priv->damage)
    {
      clutter_x11_trap_x_errors ();
      XDamageDestroy (dpy, priv->damage);
      XSync (dpy, FALSE);
      clutter_x11_untrap_x_errors ();
      priv->damage = None;
    }

  clutter_x11_remove_filter (on_x_event_filter, (gpointer)texture);
}


static void
clutter_x11_texture_pixmap_init (ClutterX11TexturePixmap *self)
{
  self->priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   CLUTTER_X11_TYPE_TEXTURE_PIXMAP,
                                   ClutterX11TexturePixmapPrivate);

  if (!check_extensions (self))
    {
      /* FIMXE: means display lacks needed extensions for at least auto. 
       *        - a _can_autoupdate() method ?
      */
    }

  self->priv->image = NULL;
  self->priv->automatic_updates = FALSE;
  self->priv->damage = None;
  self->priv->pixmap = None;
  self->priv->shminfo.shmid = -1;
}

static void
clutter_x11_texture_pixmap_dispose (GObject *object)
{
  ClutterX11TexturePixmap *texture = CLUTTER_X11_TEXTURE_PIXMAP (object);
  ClutterX11TexturePixmapPrivate *priv = texture->priv;

  free_damage_resources (texture);

  if (priv->image)
    {
      XDestroyImage (priv->image);
      priv->image = NULL;
    }

  free_shm_resources (texture);

  G_OBJECT_CLASS (clutter_x11_texture_pixmap_parent_class)->dispose (object);
}

static void
clutter_x11_texture_pixmap_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ClutterX11TexturePixmap  *texture = CLUTTER_X11_TEXTURE_PIXMAP (object);

  switch (prop_id)
    {
    case PROP_PIXMAP:
      clutter_x11_texture_pixmap_set_pixmap (texture,
                                             g_value_get_uint (value));
      break;
    case PROP_AUTO:
      clutter_x11_texture_pixmap_set_automatic (texture,
                                                g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_x11_texture_pixmap_get_property (GObject      *object,
                                         guint         prop_id,
                                         GValue       *value,
                                         GParamSpec   *pspec)
{
  ClutterX11TexturePixmap *texture = CLUTTER_X11_TEXTURE_PIXMAP (object);
  ClutterX11TexturePixmapPrivate *priv = texture->priv;

  switch (prop_id)
    {
    case PROP_PIXMAP:
      g_value_set_uint (value, priv->pixmap);
      break;
    case PROP_PIXMAP_WIDTH:
      g_value_set_uint (value, priv->pixmap_width);
      break;
    case PROP_PIXMAP_HEIGHT:
      g_value_set_uint (value, priv->pixmap_height);
      break;
    case PROP_DEPTH:
      g_value_set_uint (value, priv->depth);
      break;
    case PROP_AUTO:
      g_value_set_boolean (value, priv->automatic_updates);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_x11_texture_pixmap_realize (ClutterActor *actor)
{
  ClutterX11TexturePixmap        *texture = CLUTTER_X11_TEXTURE_PIXMAP (actor);
  ClutterX11TexturePixmapPrivate *priv = texture->priv;

  CLUTTER_ACTOR_CLASS (clutter_x11_texture_pixmap_parent_class)->
      realize (actor);

  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);

  clutter_x11_texture_pixmap_update_area_real (texture,
					       0, 0,
					       priv->pixmap_width,
					       priv->pixmap_height);
}

static void
clutter_x11_texture_pixmap_class_init (ClutterX11TexturePixmapClass *klass)
{
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec        *pspec;
  ClutterBackend    *default_backend;

  g_type_class_add_private (klass, sizeof (ClutterX11TexturePixmapPrivate));

  object_class->dispose      = clutter_x11_texture_pixmap_dispose;
  object_class->set_property = clutter_x11_texture_pixmap_set_property;
  object_class->get_property = clutter_x11_texture_pixmap_get_property;

  actor_class->realize       = clutter_x11_texture_pixmap_realize;

  klass->update_area         = clutter_x11_texture_pixmap_update_area_real;

  pspec = g_param_spec_uint ("pixmap",
                             "Pixmap",
                             "The X11 Pixmap to be bound",
                             0, G_MAXINT,
                             None,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_PIXMAP, pspec);

  pspec = g_param_spec_uint ("pixmap-width",
                             "Pixmap width",
                             "The width of the "
                             "pixmap bound to this texture",
                             0, G_MAXUINT,
                             0,
                             G_PARAM_READABLE);

  g_object_class_install_property (object_class, PROP_PIXMAP_WIDTH, pspec);

  pspec = g_param_spec_uint ("pixmap-height",
                             "Pixmap height",
                             "The height of the "
                             "pixmap bound to this texture",
                             0, G_MAXUINT,
                             0,
                             G_PARAM_READABLE);

  g_object_class_install_property (object_class, PROP_PIXMAP_HEIGHT, pspec);

  pspec = g_param_spec_uint ("pixmap-depth",
                             "Pixmap Depth",
                             "The depth (in number of bits) of the "
                             "pixmap bound to this texture",
                             0, G_MAXUINT,
                             0,
                             G_PARAM_READABLE);

  g_object_class_install_property (object_class, PROP_DEPTH, pspec);

  pspec = g_param_spec_boolean ("automatic-updates",
                                "Automatic Updates",
                                "If the texture should be kept in "
                                "sync with any pixmap changes.",
                                FALSE,
                                G_PARAM_READWRITE);

  g_object_class_install_property (object_class, PROP_AUTO, pspec);


  /**
   * ClutterX11TexturePixmap::update-area:
   * @texture: the object which received the signal
   *
   * The ::hide signal is emitted to ask the texture to update its
   * content from its source pixmap.
   *
   * Since: 0.8
   */
  signals[UPDATE_AREA] =
      g_signal_new ("update-area",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (ClutterX11TexturePixmapClass, \
                                     update_area),
                    NULL, NULL,
                    clutter_marshal_VOID__INT_INT_INT_INT,
                    G_TYPE_NONE, 4,
                    G_TYPE_INT,
                    G_TYPE_INT,
                    G_TYPE_INT,
                    G_TYPE_INT);

  default_backend = clutter_get_default_backend ();

  if (!CLUTTER_IS_BACKEND_X11 (default_backend))
    {
      g_critical ("ClutterX11TexturePixmap instanciated with a "
                  "non-X11 backend");
      return;
    }
}

static void
clutter_x11_texture_pixmap_update_area_real (ClutterX11TexturePixmap *texture,
                                             gint                     x,
                                             gint                     y,
                                             gint                     width,
                                             gint                     height)
{
  ClutterX11TexturePixmapPrivate       *priv;
  Display                              *dpy;
  XImage                               *image;
  char				       *first_pixel;
  GError                               *error = NULL;
  guint                                 bytes_per_line;
  char				       *data;
  gboolean                              data_allocated = FALSE;
  int                                   err_code;

  if (!CLUTTER_ACTOR_IS_REALIZED (texture))
    return;

  priv = texture->priv;
  dpy  = clutter_x11_get_default_display();

  if (!priv->pixmap)
    return;

  clutter_x11_trap_x_errors ();

  if (priv->have_shm)
    {
      image =
	XShmCreateImage(dpy,
			DefaultVisual(dpy,
				      clutter_x11_get_default_screen()),
			priv->depth,
			ZPixmap,
			NULL,
			&priv->shminfo,
			width,
			height);
      image->data = priv->shminfo.shmaddr;

      XShmGetImage (dpy, priv->pixmap, image, x, y, AllPlanes);
      first_pixel = image->data;
    }
  else
    {
      if (!priv->image)
	{
          priv->image = XGetImage (dpy,
                                   priv->pixmap,
                                   0, 0,
                                   priv->pixmap_width, priv->pixmap_height,
                                   AllPlanes,
                                   ZPixmap);
	  first_pixel  = priv->image->data + priv->image->bytes_per_line * y
			  + x * priv->image->bits_per_pixel/8;
	}
      else
	{
          XGetSubImage (dpy,
                        priv->pixmap,
                        x, y,
                        width, height,
                        AllPlanes,
                        ZPixmap,
                        priv->image,
                        x, y);
	  first_pixel  = priv->image->data + priv->image->bytes_per_line * y
            + x * priv->image->bits_per_pixel/8;
	}
      image = priv->image;
    }

  XSync (dpy, FALSE);

  if ((err_code = clutter_x11_untrap_x_errors ()))
    {
      g_warning ("Failed to get XImage of pixmap: %lx, removing.",
                 priv->pixmap);
      /* safe to assume pixmap has gone away? - therefor reset */
      clutter_x11_texture_pixmap_set_pixmap (texture, None);
      return;
    }

  if (priv->depth == 24)
    {
      guint xpos, ypos;

      for (ypos=0; ypos<height; ypos++)
	for (xpos=0; xpos<width; xpos++)
            {
	    char *p = first_pixel + image->bytes_per_line*ypos
			  + xpos * 4;
	    p[3] = 0xFF;
        }

      data = first_pixel;
      bytes_per_line = image->bytes_per_line;
    }
  else if (priv->depth == 16)
    {
      guint xpos, ypos;
      data = g_malloc (height * width * 4);
      data_allocated = TRUE;
      bytes_per_line = width * 4;

      for (ypos=0; ypos<height; ypos++)
	for (xpos=0; xpos<width; xpos++)
            {
	    char *src_p = first_pixel + image->bytes_per_line * ypos
			    + xpos * 2;
	    guint16 *src_pixel = (guint16 *)src_p;
	    char *dst_p = data + bytes_per_line * ypos + xpos * 4;
	    guint32 *dst_pixel = (guint32 *)dst_p;

	    *dst_pixel =
	      ((((*src_pixel << 3) & 0xf8) | ((*src_pixel >> 2) & 0x7)) | \
	      (((*src_pixel << 5) & 0xfc00) | ((*src_pixel >> 1) & 0x300)) | \
	      (((*src_pixel << 8) & 0xf80000) | ((*src_pixel << 3) & 0x70000)))
	      | 0xff000000;
	  }
    }
  else if (priv->depth == 32)
    {
      bytes_per_line = image->bytes_per_line;
      data = first_pixel;
    }
  else
    return;

  /* For debugging purposes, un comment to simply generate dummy
   * pixmap data. (A Green background and Blue cross) */
#if 0
  {
    guint xpos, ypos;

    if (data_allocated)
      g_free (data);
    data_allocated = TRUE;
    data = g_malloc (width*height*4);
    bytes_per_line = width *4;

    for (ypos=0; ypos<height; ypos++)
      for (xpos=0; xpos<width; xpos++)
	{
	  char *p = data + width*4*ypos + xpos * 4;
	  guint32 *pixel = (guint32 *)p;
	  if ((xpos > width/2 && xpos <= (width/2) + width/4)
	      || (ypos > height/2 && ypos <= (height/2) + height/4))
	    *pixel=0xff0000ff;
	  else
	    *pixel=0xff00ff00;
	}
  }
#endif

  if (x != 0 || y != 0 ||
      width != priv->pixmap_width || height != priv->pixmap_height)
    clutter_texture_set_area_from_rgb_data  (CLUTTER_TEXTURE (texture),
					     (guint8 *)data,
					     TRUE,
					     x, y,
					     width, height,
					     bytes_per_line,
					     4,
					     CLUTTER_TEXTURE_RGB_FLAG_BGR,
					     &error);
  else
    clutter_texture_set_from_rgb_data  (CLUTTER_TEXTURE (texture),
					(guint8 *)data,
					TRUE,
					width, height,
					bytes_per_line,
					4,
					CLUTTER_TEXTURE_RGB_FLAG_BGR,
					&error);



  if (error)
    {
      g_warning ("Error when uploading from pixbuf: %s",
                 error->message);
      g_error_free (error);
    }

  if (data_allocated)
    g_free (data);

  if (priv->have_shm)
    XFree (image);
}

/**
 * clutter_x11_texture_pixmap_new:
 *
 * Return value: A new #ClutterX11TexturePixmap
 *
 * Since: 0.8
 **/
ClutterActor *
clutter_x11_texture_pixmap_new (void)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_X11_TYPE_TEXTURE_PIXMAP, NULL);

  return actor;
}

/**
 * clutter_x11_texture_pixmap_new_with_pixmap:
 * @pixmap: the X Pixmap to which this texture should be bound
 * @width: the width of the X pixmap
 * @height: the height of the X pixmap
 * @depth: the depth of the X pixmap
 *
 * Return value: A new #ClutterX11TexturePixmap bound to the given X Pixmap
 *
 * Since 0.8
 **/
ClutterActor *
clutter_x11_texture_pixmap_new_with_pixmap (Pixmap pixmap)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_X11_TYPE_TEXTURE_PIXMAP,
			"pixmap", pixmap,
			NULL);

  return actor;
}

/**
 * clutter_x11_texture_pixmap_set_pixmap:
 * @texture: the texture to bind
 * @pixmap: the X Pixmap to which the texture should be bound
 * @width: the Pixmap width
 * @height: the Pixmap height
 * @depth: the Pixmap depth, in number of bits
 *
 * Sets the X Pixmap to which the texture should be bound.
 *
 * Since: 0.8
 **/
void
clutter_x11_texture_pixmap_set_pixmap (ClutterX11TexturePixmap *texture,
                                       Pixmap                   pixmap)
{
  Window       root;
  int          x, y; 
  unsigned int width, height, border_width, depth;
  Status       status = 0;
  gboolean     new_pixmap = FALSE, new_pixmap_width = FALSE;
  gboolean     new_pixmap_height = FALSE, new_pixmap_depth = FALSE;

  ClutterX11TexturePixmapPrivate *priv;

  g_return_if_fail (CLUTTER_X11_IS_TEXTURE_PIXMAP (texture));

  priv = texture->priv;

  clutter_x11_trap_x_errors ();

  status = XGetGeometry (clutter_x11_get_default_display(),
                         (Drawable)pixmap, 
                         &root,
                         &x, 
                         &y, 
                         &width,
                         &height, 
                         &border_width,
                         &depth);
  
  if (clutter_x11_untrap_x_errors () || status == 0)
    {
      if (pixmap != None)
        g_warning ("Unable to query pixmap: %lx\n", pixmap);
      pixmap = None;
      width = height = depth = 0; 
    }

  if (priv->image)
    {
      XDestroyImage (priv->image);
      priv->image = NULL;
    }
  
  if (priv->pixmap != pixmap)
    {
      priv->pixmap = pixmap;
      new_pixmap = TRUE;
    }

  if (priv->pixmap_width != width)
    {
      priv->pixmap_width = width;
      new_pixmap_width = TRUE;
    }

  if (priv->pixmap_height != height)
    {
      priv->pixmap_height = height;
      new_pixmap_height = TRUE;
    }

  if (priv->depth != depth)
    {
      priv->depth = depth;
      new_pixmap_depth = TRUE;
    }

  /* NB: We defer sending the signals until updating all the
   * above members so the values are all available to the
   * signal handlers. */
  g_object_ref (texture);
  if (new_pixmap)
    g_object_notify (G_OBJECT (texture), "pixmap");
  if (new_pixmap_width)
    g_object_notify (G_OBJECT (texture), "pixmap-width");
  if (new_pixmap_height)
    g_object_notify (G_OBJECT (texture), "pixmap-height");
  if (new_pixmap_depth)
    g_object_notify (G_OBJECT (texture), "pixmap-depth");
  g_object_unref (texture);

  free_shm_resources (texture);
  if (pixmap != None)
    try_alloc_shm (texture);

  if (priv->depth != 0 &&
      priv->pixmap != None &&
      priv->pixmap_width != 0 &&
      priv->pixmap_height != 0)
    {
      if (CLUTTER_ACTOR_IS_REALIZED (texture))
        clutter_x11_texture_pixmap_update_area (texture,
                                                0, 0,
                                                priv->pixmap_width,
                                                priv->pixmap_height);
    }
}

/**
 * clutter_x11_texture_pixmap_update_area:
 * @texture: The texture whose content shall be updated.
 * @x: the X coordinate of the area to update
 * @y: the Y coordinate of the area to update
 * @width: the width of the area to update
 * @height: the height of the area to update
 *
 * Performs the actual binding of texture to the current content of
 * the pixmap. Can be called to update the texture if the pixmap
 * content has changed.
 *
 * Since: 0.8
 **/
void
clutter_x11_texture_pixmap_update_area (ClutterX11TexturePixmap *texture,
                                        gint                     x,
                                        gint                     y,
                                        gint                     width,
                                        gint                     height)
{
  g_return_if_fail (CLUTTER_X11_IS_TEXTURE_PIXMAP (texture));

  g_signal_emit (texture, signals[UPDATE_AREA], 0, x, y, width, height);
}

/* FIXME: to implement */
void
clutter_x11_texture_pixmap_set_from_window (ClutterX11TexturePixmap *texture,
                                            Window                   win,
                                            gboolean                 reflect)
{
  ClutterX11TexturePixmapPrivate *priv;

  g_return_if_fail (CLUTTER_X11_IS_TEXTURE_PIXMAP (texture));

  /* This would mainly be used for compositing type situations 
   * with named pixmap (cannot be regular pixmap) and setting up  
   * actual window redirection.
   *
   * It also seems to can pass a window to texture_pixmap and it
   * it works like redirectwindow automatic. 
   *
   * Note windows do however change size, whilst pixmaps do not. 
  */

  priv = texture->priv;

  /*
  priv->window_pixmap = XCompositeNameWindowPixmap (dpy, win);

  XCompositeRedirectWindow(clutter_x11_get_default_display(),
                           win_remote,
                           CompositeRedirectAutomatic);
  */
}



/* FIXME: Below will change, just proof of concept atm - it will not work
 *        100% for named pixmaps. 
*/
void
clutter_x11_texture_pixmap_set_automatic (ClutterX11TexturePixmap *texture,
                                          gboolean                 setting)
{
  ClutterX11TexturePixmapPrivate *priv;
  Display                        *dpy;

  g_return_if_fail (CLUTTER_X11_IS_TEXTURE_PIXMAP (texture));

  priv = texture->priv;

  if (setting == priv->automatic_updates)
    return;

  dpy = clutter_x11_get_default_display();

  if (setting == TRUE)
    {
      clutter_x11_add_filter (on_x_event_filter, (gpointer)texture);

      clutter_x11_trap_x_errors ();
          
      /* NOTE: Appears this will not work for a named pixmap ? */
      priv->damage = XDamageCreate (dpy,
                                    priv->pixmap,
                                    XDamageReportNonEmpty);

      XSync (dpy, FALSE);
      clutter_x11_untrap_x_errors ();
    }
  else
    free_damage_resources (texture);

  priv->automatic_updates = setting;

}
