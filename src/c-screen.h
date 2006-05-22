#include "screen.h"
#include "c-window.h"

typedef struct MetaCompScreen MetaCompScreen;

MetaCompScreen *meta_comp_screen_new             (WsDisplay      *display,
						  MetaScreen     *screen);
MetaCompScreen *meta_comp_screen_get_by_xwindow  (Window          xwindow);
void            meta_comp_screen_destroy         (MetaCompScreen *scr_info);
void            meta_comp_screen_redirect        (MetaCompScreen *info);
void            meta_comp_screen_unredirect      (MetaCompScreen *info);
void            meta_comp_screen_add_window      (MetaCompScreen *scr_info,
						  Window          xwindow);
void            meta_comp_screen_remove_window   (MetaCompScreen *scr_info,
						  Window          xwindow);
void            meta_comp_screen_restack         (MetaCompScreen *scr_info,
						  Window          window,
						  Window          above_this);
void            meta_comp_screen_set_size        (MetaCompScreen *info,
						  Window          window,
						  gint            x,
						  gint            y,
						  gint            width,
						  gint            height);
void            meta_comp_screen_raise_window    (MetaCompScreen *scr_info,
						  Window          window);
void            meta_comp_screen_queue_paint     (MetaCompScreen *info);
void            meta_comp_screen_set_updates     (MetaCompScreen *info,
						  Window          xwindow,
						  gboolean        updates);
void            meta_comp_screen_set_patch       (MetaCompScreen *info,
						  Window          xwindow,
						  CmPoint         points[4][4]);
void            meta_comp_screen_unset_patch     (MetaCompScreen *info,
						  Window          xwindow);
void            meta_comp_screen_set_alpha       (MetaCompScreen *info,
						  Window          xwindow,
						  gdouble         alpha);
void            meta_comp_screen_get_real_size   (MetaCompScreen *info,
						  Window          xwindow,
						  WsRectangle    *size);
void            meta_comp_screen_set_target_rect (MetaCompScreen *info,
						  Window          xwindow,
						  WsRectangle    *rect);
void            meta_comp_screen_set_explode     (MetaCompScreen *info,
						  Window          xwindow,
						  gdouble         level);
void            meta_comp_screen_hide_window     (MetaCompScreen *info,
						  Window          xwindow);
void            meta_comp_screen_unmap           (MetaCompScreen *info,
						  Window          xwindow);
MetaCompWindow *meta_comp_screen_lookup_window   (MetaCompScreen *info,
						  Window          xwindow);
