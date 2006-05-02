#include "screen.h"

typedef struct MetaScreenInfo MetaScreenInfo;

MetaScreenInfo *meta_screen_info_new     (WsDisplay	 *display,
					  MetaScreen     *screen);
MetaScreenInfo *meta_screen_info_get_by_xwindow (Window xwindow);
void            meta_screen_info_destroy (MetaScreenInfo *scr_info);
void		meta_screen_info_redirect (MetaScreenInfo *info);
void		meta_screen_info_unredirect (MetaScreenInfo *info);
void		meta_screen_info_add_window (MetaScreenInfo *scr_info,
					     Window          xwindow);
void		meta_screen_info_remove_window (MetaScreenInfo *scr_info,
						Window	     xwindow);
void		meta_screen_info_restack	(MetaScreenInfo *scr_info,
						 Window		 window,
						 Window		 above_this);
void	        meta_screen_info_set_size       (MetaScreenInfo *info,
						 Window	         window,
						 gint		 x,
						 gint		 y,
						 gint		 width,
						 gint		 height);
void            meta_screen_info_raise_window (MetaScreenInfo  *scr_info,
					       Window           window);
void		meta_screen_info_queue_paint (MetaScreenInfo *info);
void		meta_screen_info_set_updates (MetaScreenInfo *info,
					      Window	      xwindow,
					      gboolean        updates);
void		meta_screen_info_set_patch (MetaScreenInfo *info,
					    Window	    xwindow,
					    CmPoint         points[4][4]);
void		meta_screen_info_unset_patch (MetaScreenInfo *info,
					      Window	      xwindow);
void            meta_screen_info_set_alpha (MetaScreenInfo *info,
				       Window	xwindow,
				       gdouble alpha);
void		meta_screen_info_get_real_size (MetaScreenInfo *info,
						Window xwindow,
						WsRectangle *size);
void		meta_screen_info_set_target_rect (MetaScreenInfo *info,
						  Window xwindow,
						  WsRectangle *rect);

void		meta_screen_info_set_explode (MetaScreenInfo *info,
					      Window xwindow,
					      gdouble level);
void		meta_screen_info_hide_window (MetaScreenInfo *info,
					      Window          xwindow);
void		meta_screen_info_unmap (MetaScreenInfo *info,
					Window		xwindow);
