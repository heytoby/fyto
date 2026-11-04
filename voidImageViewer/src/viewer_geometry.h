#ifndef FYTO_VIEWER_GEOMETRY_H
#define FYTO_VIEWER_GEOMETRY_H
#include "image_fit.h"
#define FYTO_DEFAULT_LONG_EDGE 1980
/* Use one screen limit for both orientations so portrait/landscape stay equal. */
static int fyto_long_edge_fit(int area_w,int area_h,int image_w,int image_h,
    int chrome_h,int show_sidebar,int sidebar_limit,int requested_edge,fyto_image_fit_t *out)
{
    int side_limit,available_w,available_h,long_edge,iw,ih,side;
    double scale;
    if(!out || area_w<=0 || area_h<=chrome_h || image_w<=0 || image_h<=0 || requested_edge<=0)return 0;
    side_limit=show_sidebar?(sidebar_limit<area_w/2?sidebar_limit:area_w/2):0;
    if(side_limit<0)side_limit=0;
    available_w=area_w-side_limit;available_h=area_h-chrome_h;
    long_edge=image_w>image_h?image_w:image_h;
    scale=(double)min(requested_edge,min(available_w,available_h))/long_edge;
    iw=(int)(image_w*scale+0.5);ih=(int)(image_h*scale+0.5);
    if(iw<1)iw=1;if(ih<1)ih=1;
    if(iw>available_w)iw=available_w;if(ih>available_h)ih=available_h;
    side=show_sidebar?(sidebar_limit<iw?sidebar_limit:iw):0;
    if(side<0)side=0;
    out->image_width=iw;out->image_height=ih;out->sidebar=side;
    out->window_width=iw+side;out->window_height=ih+chrome_h;
    return 1;
}
static int fyto_default_display_fit(int area_w,int area_h,int image_w,int image_h,
    int chrome_h,int show_sidebar,int sidebar_limit,fyto_image_fit_t *out)
{
    return fyto_long_edge_fit(area_w,area_h,image_w,image_h,chrome_h,show_sidebar,sidebar_limit,FYTO_DEFAULT_LONG_EDGE,out);
}
static int fyto_resize_hit(const RECT *r,POINT p,int edge,int corner)
{
    if(p.x<r->left || p.x>=r->right || p.y<r->top || p.y>=r->bottom)return HTNOWHERE;
    if(p.y<r->top+corner){
        if(p.x<r->left+corner)return HTTOPLEFT;
        if(p.x>=r->right-corner)return HTTOPRIGHT;
    }
    if(p.y>=r->bottom-corner){
        if(p.x<r->left+corner)return HTBOTTOMLEFT;
        if(p.x>=r->right-corner)return HTBOTTOMRIGHT;
    }
    if(p.y<r->top+edge)return HTTOP;
    if(p.y>=r->bottom-edge)return HTBOTTOM;
    if(p.x<r->left+edge)return HTLEFT;
    if(p.x>=r->right-edge)return HTRIGHT;
    return HTCLIENT;
}
static int fyto_is_resize_hit(int hit)
{
    return hit==HTLEFT || hit==HTRIGHT || hit==HTTOP || hit==HTTOPLEFT ||
        hit==HTTOPRIGHT || hit==HTBOTTOM || hit==HTBOTTOMLEFT || hit==HTBOTTOMRIGHT;
}
#endif

