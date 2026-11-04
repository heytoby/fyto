/* External image launches always start on Windows' primary display. */
static HMONITOR fyto_primary_monitor_info(MONITORINFO *info)
{
    POINT origin={0,0};
    HMONITOR monitor=MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY);
    info->cbSize=sizeof(*info);
    return monitor && GetMonitorInfo(monitor,info) ? monitor : NULL;
}

static void fyto_primary_work_rect(RECT *work)
{
    MONITORINFO info;
    if(fyto_primary_monitor_info(&info))*work=info.rcWork;
    else if(!SystemParametersInfo(SPI_GETWORKAREA,0,work,0))
        SetRect(work,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
}

static void fyto_center_window_in_work(RECT *window,const RECT *work)
{
    int available_w=max(1,work->right-work->left),available_h=max(1,work->bottom-work->top);
    int width=window->right-window->left,height=window->bottom-window->top;
    if(width<=0)width=available_w*3/4;
    if(height<=0)height=available_h*3/4;
    width=max(1,min(width,available_w));height=max(1,min(height,available_h));
    window->left=work->left+(available_w-width)/2;
    window->top=work->top+(available_h-height)/2;
    window->right=window->left+width;window->bottom=window->top+height;
}

static void fyto_open_image_on_primary(HWND hwnd)
{
    MONITORINFO info;
    HMONITOR primary;
    RECT normal,target;
    BYTE was_moving;
    if(!hwnd)return;
    primary=fyto_primary_monitor_info(&info);
    if(!primary || MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST)==primary)return;

    /* Suppress a fit to the previous screen while restoring a maximized window. */
    was_moving=_viv_in_sizemove;_viv_in_sizemove=1;
    if(!_viv_is_fullscreen && (IsZoomed(hwnd) || IsIconic(hwnd)))ShowWindow(hwnd,SW_RESTORE);
    if(_viv_is_fullscreen)normal=_viv_fullscreen_rect;
    else if(!GetWindowRect(hwnd,&normal)){_viv_in_sizemove=was_moving;return;}
    fyto_center_window_in_work(&normal,&info.rcWork);

    if(_viv_is_fullscreen){
        /* Keep both fullscreen bounds and the future restored window on primary. */
        _viv_fullscreen_rect=normal;target=info.rcMonitor;
    }else target=normal;
    _viv_fit_monitor=primary;_viv_fit_monitor_work=info.rcWork;
    _viv_fit_box_wide=info.rcWork.right-info.rcWork.left;
    _viv_fit_box_high=info.rcWork.bottom-info.rcWork.top;
    _viv_fit_last_valid=0;
    SetWindowPos(hwnd,NULL,target.left,target.top,target.right-target.left,target.bottom-target.top,
        SWP_NOZORDER|SWP_NOACTIVATE);
    _viv_in_sizemove=was_moving;
}

