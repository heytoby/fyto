#ifndef FYTO_IMAGE_FIT_H
#define FYTO_IMAGE_FIT_H
/* Geometry is based on the image, never on the orientation of a cached window. */
typedef struct fyto_image_fit_s {int image_width,image_height,window_width,window_height,sidebar;} fyto_image_fit_t;
static int fyto_image_fit(int area_width,int area_height,int image_width,int image_height,int chrome_height,int show_sidebar,int sidebar_limit,fyto_image_fit_t *out)
{
    int available_width,available_height,reserved,iw,ih,side;double scale;
    if(!out || area_width<=0 || area_height<=chrome_height || image_width<=0 || image_height<=0)return 0;
    if(sidebar_limit<0)sidebar_limit=0;
    reserved=show_sidebar?(sidebar_limit<area_width/2?sidebar_limit:area_width/2):0;
    available_width=area_width-reserved;available_height=area_height-chrome_height;
    scale=(double)available_width/image_width;
    if(scale>(double)available_height/image_height)scale=(double)available_height/image_height;
    if(scale>1.0)scale=1.0;
    iw=(int)(image_width*scale+0.5);ih=(int)(image_height*scale+0.5);
    if(iw<1)iw=1;if(ih<1)ih=1;
    side=show_sidebar?(sidebar_limit<iw?sidebar_limit:iw):0;
    out->image_width=iw;out->image_height=ih;out->sidebar=side;
    out->window_width=iw+side;out->window_height=ih+chrome_height;
    return 1;
}
static int fyto_near_edge(int a,int b,int tolerance){int d=a-b;return d>=-tolerance && d<=tolerance;}
/* Native Snap is authoritative. This fallback also recognizes conventional tiles
   created by desktop layout tools that do not set the native arranged flag. */
static int fyto_fixed_tile(int left,int top,int right,int bottom,int work_left,int work_top,int work_right,int work_bottom,int tolerance)
{
    int width=work_right-work_left,height=work_bottom-work_top,columns,rows,column,row,span;
    if(width<=0 || height<=0)return 0;
    for(columns=1;columns<=3;columns++)for(rows=1;rows<=2;rows++)for(span=1;span<=columns;span++){
        for(column=0;column+span<=columns;column++)for(row=0;row<rows;row++)
            if(fyto_near_edge(left,work_left+width*column/columns,tolerance) &&
               fyto_near_edge(right,work_left+width*(column+span)/columns,tolerance) &&
               fyto_near_edge(top,work_top+height*row/rows,tolerance) &&
               fyto_near_edge(bottom,work_top+height*(row+1)/rows,tolerance))return 1;
    }
    return 0;
}
#endif
