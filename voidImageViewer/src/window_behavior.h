/* Empty-view sizing is independent of a monitor's portrait/landscape ratio. */
static void fyto_empty_window_rect(RECT *result,const RECT *work)
{
    int available_w=work->right-work->left,available_h=work->bottom-work->top;
    int width=min(MulDiv(960,os_logical_wide,96),available_w*9/10);
    int height;
    width=min(width,(available_h*4/5)*8/5);
    width=max(1,width);height=max(1,width*5/8);
    result->left=work->left+(available_w-width)/2;
    result->top=work->top+(available_h-height)/2;
    result->right=result->left+width;result->bottom=result->top+height;
}

static BOOL fyto_setup_launch(void)
{
    int count,i;BOOL setup=FALSE;
    wchar_t **args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!args)return FALSE;
    for(i=1;i<count;i++)if(!lstrcmpiW(args[i],L"/setup") || !lstrcmpiW(args[i],L"-setup")){setup=TRUE;break;}
    LocalFree(args);return setup;
}

static BYTE fyto_window_drag_pending,fyto_window_drag_from_image;
static POINT fyto_window_drag_start;
static RECT fyto_window_drag_origin;

static void fyto_restore_for_drag(HWND hwnd,POINT cursor)
{
    RECT normal;MONITORINFO monitor;HMONITOR target;
    int width,height,x,y,offset_y;double horizontal,vertical;
    BYTE was_moving=_viv_in_sizemove;
    horizontal=(double)(fyto_window_drag_start.x-fyto_window_drag_origin.left)/
        max(1,fyto_window_drag_origin.right-fyto_window_drag_origin.left);
    horizontal=max(0.0,min(1.0,horizontal));
    vertical=(double)(fyto_window_drag_start.y-fyto_window_drag_origin.top)/
        max(1,fyto_window_drag_origin.bottom-fyto_window_drag_origin.top);
    vertical=max(0.0,min(1.0,vertical));
    offset_y=max(4,min(MulDiv(24,os_logical_high,96),fyto_window_drag_start.y-fyto_window_drag_origin.top));
    target=MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST);
    monitor.cbSize=sizeof(monitor);if(!GetMonitorInfo(target,&monitor))return;
    _viv_user_sized_window=1;_viv_manual_view_wide=_viv_manual_view_high=0;
    _viv_in_sizemove=1;
    // A drag restores an ordinary window even if fullscreen began maximized.
    if(_viv_is_fullscreen){
        _viv_fullscreen_is_maxed=0;
        _viv_toggle_fullscreen();
        if(IsZoomed(hwnd))ShowWindow(hwnd,SW_RESTORE);
        GetWindowRect(hwnd,&normal);
    }else{
        WINDOWPLACEMENT placement;placement.length=sizeof(placement);
        GetWindowRect(hwnd,&normal);
        if(IsZoomed(hwnd)){
            ShowWindow(hwnd,SW_RESTORE);GetWindowRect(hwnd,&normal);
        }else{
            if(_viv_fit_last_valid)normal=_viv_fit_last_rect;
            else if(GetWindowPlacement(hwnd,&placement))normal=placement.rcNormalPosition;
            ShowWindow(hwnd,SW_RESTORE);
        }
    }
    width=min(normal.right-normal.left,(monitor.rcWork.right-monitor.rcWork.left)*9/10);
    height=min(normal.bottom-normal.top,(monitor.rcWork.bottom-monitor.rcWork.top)*9/10);
    if(width<=0 || height<=0){fyto_empty_window_rect(&normal,&monitor.rcWork);width=normal.right-normal.left;height=normal.bottom-normal.top;}
    if(fyto_window_drag_from_image)offset_y=(int)(height*vertical);
    x=cursor.x-(int)(width*horizontal);y=cursor.y-offset_y;
    x=max(monitor.rcWork.left,min(x,monitor.rcWork.right-width));
    y=max(monitor.rcWork.top,min(y,monitor.rcWork.bottom-height));
    SetWindowPos(hwnd,NULL,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    _viv_fit_box_wide=width;_viv_fit_box_high=height;
    _viv_fit_monitor=target;_viv_fit_monitor_work=monitor.rcWork;
    GetWindowRect(hwnd,&_viv_fit_last_rect);_viv_fit_last_valid=1;
    _viv_in_sizemove=was_moving;
}

static void fyto_restore_for_resize(HWND hwnd,int hit,POINT cursor)
{
    RECT normal;MONITORINFO monitor;WINDOWPLACEMENT placement;
    int width,height,x,y,left,top,right,bottom;BYTE was_moving=_viv_in_sizemove;
    monitor.cbSize=sizeof(monitor);
    if(!GetMonitorInfo(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&monitor))return;
    _viv_user_sized_window=1;_viv_manual_view_wide=_viv_manual_view_high=0;_viv_in_sizemove=1;
    if(_viv_is_fullscreen){
        _viv_fullscreen_is_maxed=0;_viv_toggle_fullscreen();
        if(IsZoomed(hwnd))ShowWindow(hwnd,SW_RESTORE);
        GetWindowRect(hwnd,&normal);
    }else if(IsZoomed(hwnd) || IsIconic(hwnd)){
        ShowWindow(hwnd,SW_RESTORE);GetWindowRect(hwnd,&normal);
    }else{
        GetWindowRect(hwnd,&normal);placement.length=sizeof(placement);
        if(_viv_fit_last_valid)normal=_viv_fit_last_rect;
        else if(GetWindowPlacement(hwnd,&placement))normal=placement.rcNormalPosition;
    }
    width=max(64,min(normal.right-normal.left,monitor.rcWork.right-monitor.rcWork.left));
    height=max(64,min(normal.bottom-normal.top,monitor.rcWork.bottom-monitor.rcWork.top));
    left=hit==HTLEFT || hit==HTTOPLEFT || hit==HTBOTTOMLEFT;
    right=hit==HTRIGHT || hit==HTTOPRIGHT || hit==HTBOTTOMRIGHT;
    top=hit==HTTOP || hit==HTTOPLEFT || hit==HTTOPRIGHT;
    bottom=hit==HTBOTTOM || hit==HTBOTTOMLEFT || hit==HTBOTTOMRIGHT;
    x=left?cursor.x:right?cursor.x-width:normal.left;
    y=top?cursor.y:bottom?cursor.y-height:normal.top;
    x=max(monitor.rcWork.left,min(x,monitor.rcWork.right-width));
    y=max(monitor.rcWork.top,min(y,monitor.rcWork.bottom-height));
    SetWindowPos(hwnd,NULL,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    _viv_fit_box_wide=width;_viv_fit_box_high=height;
    _viv_fit_monitor=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);_viv_fit_monitor_work=monitor.rcWork;
    GetWindowRect(hwnd,&_viv_fit_last_rect);_viv_fit_last_valid=1;
    _viv_in_sizemove=was_moving;
}

static BOOL fyto_window_drag(HWND hwnd,UINT message,WPARAM buttons,LPARAM position)
{
    if(message==WM_NCLBUTTONDOWN && fyto_is_resize_hit((int)buttons)){
        POINT cursor;cursor.x=GET_X_LPARAM(position);cursor.y=GET_Y_LPARAM(position);
        _viv_user_sized_window=1;_viv_manual_view_wide=_viv_manual_view_high=0;
        if(_viv_is_fullscreen || IsZoomed(hwnd) || _viv_window_arranged(hwnd))
            fyto_restore_for_resize(hwnd,(int)buttons,cursor);
        return FALSE;
    }
    if(message==WM_NCLBUTTONDOWN && buttons==HTCAPTION && (_viv_is_fullscreen || IsZoomed(hwnd) || _viv_window_arranged(hwnd))){
        fyto_window_drag_start.x=GET_X_LPARAM(position);fyto_window_drag_start.y=GET_Y_LPARAM(position);
        GetWindowRect(hwnd,&fyto_window_drag_origin);
        fyto_window_drag_pending=1;SetCapture(hwnd);return TRUE;
    }
    if(!fyto_window_drag_pending)return FALSE;
    if(message==WM_CAPTURECHANGED){fyto_window_drag_pending=fyto_window_drag_from_image=0;return TRUE;}
    if(message==WM_LBUTTONUP || message==WM_NCLBUTTONUP || message==WM_CANCELMODE){
        fyto_window_drag_pending=fyto_window_drag_from_image=0;ReleaseCapture();return TRUE;
    }
    if(message==WM_MOUSEMOVE){
        POINT cursor;int dx,dy;
        if(!(buttons&MK_LBUTTON)){fyto_window_drag_pending=fyto_window_drag_from_image=0;ReleaseCapture();return TRUE;}
        cursor.x=GET_X_LPARAM(position);cursor.y=GET_Y_LPARAM(position);ClientToScreen(hwnd,&cursor);
        dx=abs(cursor.x-fyto_window_drag_start.x);dy=abs(cursor.y-fyto_window_drag_start.y);
        if(dx<GetSystemMetrics(SM_CXDRAG) && dy<GetSystemMetrics(SM_CYDRAG))return TRUE;
        fyto_window_drag_pending=0;ReleaseCapture();
        fyto_restore_for_drag(hwnd,cursor);
        fyto_window_drag_from_image=0;
        // Continue with Windows' native move loop, including edge Snap and
        // maximized/snapped-window restore behavior. No custom moving loop.
        DefWindowProc(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(cursor.x,cursor.y));
        return TRUE;
    }
    return FALSE;
}
