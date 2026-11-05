#include "hover_animation.h"
#define FYTO_HOVER_FADE_TIMER 0x7f32
static BYTE fyto_image_hover,fyto_play_pressed;
static fyto_hover_fade_t fyto_hover_fade;
static BOOL fyto_visible_image_rect(RECT *result)
{
    RECT viewport,image;int w,h,rw,rh,rx,ry;
    if(!_viv_frame_count || !_viv_dst_zoom_values)return FALSE;
    _viv_viewport_rect(&viewport);w=viewport.right-viewport.left;h=viewport.bottom-viewport.top;
    _viv_get_render_size(&rw,&rh);
    rw=(int)(rw*_viv_dst_zoom_values[_viv_dst_zoom_x_pos]);
    rh=(int)(rh*_viv_dst_zoom_values[_viv_dst_zoom_y_pos]);
    if(w<=0 || h<=0 || rw<=0 || rh<=0)return FALSE;
    rx=((_viv_dst_pos_x-250)*(w*2))/1000-rw/2-_viv_view_x;
    ry=((_viv_dst_pos_y-250)*(h*2))/1000-rh/2-_viv_view_y;
    SetRect(&image,rx,ry,rx+rw,ry+rh);
    return IntersectRect(result,&image,&viewport) && !IsRectEmpty(result);
}
static void fyto_hover_invalidate(HWND hwnd)
{
    RECT image,canvas,r;fyto_hover_layout_t layout;int i;
    if(!fyto_visible_image_rect(&image))return;
    _viv_viewport_rect(&canvas);fyto_hover_layout(&image,&canvas,os_logical_wide,os_logical_high,&layout);
    for(i=0;i<3;i++){
        r=i==0?layout.filename:i==1?layout.resolution:layout.play;
        if(!IsRectEmpty(&r)){int margin=fyto_hover_shadow_radius(os_logical_high);InflateRect(&r,margin,margin);InvalidateRect(hwnd,&r,FALSE);}
    }
}
static void fyto_hover_target(HWND hwnd,BYTE hover)
{
    BYTE target=hover?255:0;
    if(hover==fyto_image_hover && fyto_hover_fade.to==target)return;
    fyto_image_hover=hover;
    fyto_hover_fade_to(&fyto_hover_fade,target,GetTickCount());
    if(fyto_hover_fade.from!=target){
        if(!SetTimer(hwnd,FYTO_HOVER_FADE_TIMER,16,NULL))fyto_hover_fade.duration=0;
    }else KillTimer(hwnd,FYTO_HOVER_FADE_TIMER);
    fyto_hover_invalidate(hwnd);
}
static BOOL fyto_hover_play_hit(POINT point)
{
    RECT image,canvas;fyto_hover_layout_t layout;
    if(!fyto_visible_image_rect(&image) || !PtInRect(&image,point))return FALSE;
    _viv_viewport_rect(&canvas);fyto_hover_layout(&image,&canvas,os_logical_wide,os_logical_high,&layout);
    return PtInRect(&layout.play,point);
}
static void fyto_update_image_hover(POINT point)
{
    RECT image;BYTE hover=fyto_visible_image_rect(&image) && PtInRect(&image,point);
    fyto_hover_target(_viv_hwnd,hover);
}
static BOOL fyto_hover_input(HWND hwnd,UINT message,WPARAM buttons,LPARAM position)
{
    POINT point;
    if(message==WM_TIMER && buttons==FYTO_HOVER_FADE_TIMER){
        if(!_viv_frame_count || _viv_in_sizemove)fyto_hover_target(hwnd,0);
        fyto_hover_invalidate(hwnd);
        if(fyto_hover_fade_alpha(&fyto_hover_fade,GetTickCount())==fyto_hover_fade.to)
            KillTimer(hwnd,FYTO_HOVER_FADE_TIMER);
        return TRUE;
    }
    point.x=GET_X_LPARAM(position);point.y=GET_Y_LPARAM(position);
    if(message==WM_MOUSEMOVE)fyto_update_image_hover(point);
    if(message==WM_MOUSELEAVE)fyto_hover_target(hwnd,0);
    if(message==WM_LBUTTONDOWN && fyto_hover_play_hit(point)){
        fyto_play_pressed=1;SetCapture(hwnd);return TRUE;
    }
    if(message==WM_LBUTTONUP && fyto_play_pressed){
        BOOL activate=fyto_hover_play_hit(point);
        fyto_play_pressed=0;ReleaseCapture();
        if(activate)PostMessage(hwnd,WM_COMMAND,VIV_ID_SLIDESHOW_PAUSE,0);
        fyto_hover_invalidate(hwnd);return TRUE;
    }
    if((message==WM_CAPTURECHANGED || message==WM_CANCELMODE) && fyto_play_pressed){
        fyto_play_pressed=0;fyto_hover_invalidate(hwnd);
    }
    if(message==WM_SETCURSOR && LOWORD(position)==HTCLIENT){
        GetCursorPos(&point);ScreenToClient(hwnd,&point);
        if(fyto_hover_play_hit(point)){SetCursor(LoadCursor(NULL,IDC_HAND));return TRUE;}
    }
    return FALSE;
}
static void fyto_paint_image_hover(HDC dc)
{
    RECT image,canvas;POINT point;BYTE visible,alpha;
    if(!fyto_visible_image_rect(&image) || _viv_in_sizemove){fyto_hover_target(_viv_hwnd,0);return;}
    GetCursorPos(&point);visible=WindowFromPoint(point)==_viv_hwnd;
    ScreenToClient(_viv_hwnd,&point);fyto_hover_target(_viv_hwnd,visible && PtInRect(&image,point));
    alpha=fyto_hover_fade_alpha(&fyto_hover_fade,GetTickCount());
    if(!alpha && !fyto_image_hover)return;
    _viv_viewport_rect(&canvas);
    fyto_draw_hover(dc,&image,&canvas,_viv_current_fd?string_get_filename_part(_viv_current_fd->cFileName):L"",
        _viv_image_wide,_viv_image_high,!config_show_caption,_viv_is_slideshow,alpha,fyto_image_hover,os_logical_wide,os_logical_high);
}
static void fyto_hover_stop(void)
{
    KillTimer(_viv_hwnd,FYTO_HOVER_FADE_TIMER);ZeroMemory(&fyto_hover_fade,sizeof(fyto_hover_fade));
    fyto_image_hover=fyto_play_pressed=0;
}
static BOOL fyto_image_body_drag(HWND hwnd,WPARAM buttons,LPARAM position)
{
    POINT client,screen;RECT viewport;int rw,rh;
    if(!_viv_frame_count || (buttons&(MK_CONTROL|MK_SHIFT)))return FALSE;
    if(config_left_click_action!=0 && config_left_click_action!=6)return FALSE;
    client.x=GET_X_LPARAM(position);client.y=GET_Y_LPARAM(position);
    if(config_show_caption && !_viv_is_fullscreen &&
        !(client.y>=0 && client.y<MulDiv(36,os_logical_high,96) && fyto_window_needs_drag_restore(hwnd)))return FALSE;
    if(fyto_hover_play_hit(client))return FALSE;
    _viv_viewport_rect(&viewport);_viv_get_render_size(&rw,&rh);
    rw=(int)(rw*_viv_dst_zoom_values[_viv_dst_zoom_x_pos]);
    rh=(int)(rh*_viv_dst_zoom_values[_viv_dst_zoom_y_pos]);
    if((rw>viewport.right-viewport.left || rh>viewport.bottom-viewport.top) &&
        !(client.y>=viewport.top && client.y<viewport.top+MulDiv(36,os_logical_high,96) &&
          fyto_window_needs_drag_restore(hwnd)))return FALSE;
    screen=client;ClientToScreen(hwnd,&screen);
    fyto_window_drag_from_image=1;
    if(!fyto_window_drag(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(screen.x,screen.y))){
        fyto_window_drag_from_image=0;
        DefWindowProc(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(screen.x,screen.y));
    }
    return TRUE;
}

