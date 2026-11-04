// About page uses the unchanged, user-supplied introduction image.
static IStream *fyto_about_stream;
static void *fyto_about_image;
static UINT fyto_about_width,fyto_about_height;
static void fyto_about_release(void)
{
    if(fyto_about_image && os_GdipDisposeImage)os_GdipDisposeImage(fyto_about_image);
    fyto_about_image=NULL;
    if(fyto_about_stream)fyto_about_stream->lpVtbl->Release(fyto_about_stream);
    fyto_about_stream=NULL;fyto_about_width=fyto_about_height=0;
}
static void fyto_about_load(void)
{
    HRSRC res;HGLOBAL data;DWORD size;fyto_about_release();
    if(!os_GdipLoadImageFromStream || !os_GdipGetImageWidth || !os_GdipGetImageHeight)return;
    res=FindResourceW(os_hinstance,MAKEINTRESOURCEW(60010),RT_RCDATA);if(!res)return;
    data=LoadResource(os_hinstance,res);size=SizeofResource(os_hinstance,res);if(!data || !size)return;
    fyto_about_stream=SHCreateMemStream((const BYTE *)LockResource(data),size);if(!fyto_about_stream)return;
    if(os_GdipLoadImageFromStream(fyto_about_stream,&fyto_about_image)!=0){fyto_about_release();return;}
    os_GdipGetImageWidth(fyto_about_image,&fyto_about_width);os_GdipGetImageHeight(fyto_about_image,&fyto_about_height);
    if(!fyto_about_width || !fyto_about_height)fyto_about_release();
}
static void fyto_about_layout(HWND hwnd)
{
    MONITORINFO mi;RECT frame;int dpi=os_logical_high,pad=MulDiv(16,dpi,96),line=MulDiv(20,dpi,96);
    int width,height,iw,ih,y,bh=MulDiv(28,dpi,96),bw=MulDiv(94,dpi,96),maxh,credits=4*line+bh+5*pad;
    mi.cbSize=sizeof(mi);if(!GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi))return;
    maxh=(mi.rcWork.bottom-mi.rcWork.top)*9/10;
    width=min(MulDiv(512,dpi,96),(mi.rcWork.right-mi.rcWork.left)*9/10);iw=max(1,width-2*pad);
    ih=fyto_about_width?MulDiv(iw,fyto_about_height,fyto_about_width):MulDiv(iw,1535,2048);
    if(ih+credits>maxh){ih=max(1,maxh-credits);iw=fyto_about_height?MulDiv(ih,fyto_about_width,fyto_about_height):MulDiv(ih,2048,1535);width=iw+2*pad;}
    y=pad+ih+pad;height=y+4*line+pad+bh+pad;SetRect(&frame,0,0,width,height);
    AdjustWindowRectEx(&frame,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));
    SetWindowPos(hwnd,NULL,0,0,frame.right-frame.left,frame.bottom-frame.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(hwnd,60010),NULL,pad,pad,iw,ih,SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(hwnd,IDC_ABOUTVERSION),NULL,pad,y,iw,line,SWP_NOZORDER|SWP_NOACTIVATE);y+=line;
    SetWindowPos(GetDlgItem(hwnd,IDC_ABOUTVOIDIMAGEVIEWER),NULL,pad,y,iw,line,SWP_NOZORDER|SWP_NOACTIVATE);y+=line;
    SetWindowPos(GetDlgItem(hwnd,IDC_ABOUTCOPYRIGHT),NULL,pad,y,iw,line,SWP_NOZORDER|SWP_NOACTIVATE);y+=line;
    SetWindowPos(GetDlgItem(hwnd,IDC_ABOUTWEBSITE),NULL,pad,y,iw,line,SWP_NOZORDER|SWP_NOACTIVATE);y+=line+pad;
    SetWindowPos(GetDlgItem(hwnd,IDC_ABOUTEMAIL),NULL,pad,y,2*bw,bh,SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(hwnd,IDOK),NULL,width-pad-bw,y,bw,bh,SWP_NOZORDER|SWP_NOACTIVATE);os_center_dialog(hwnd);
}
static INT_PTR CALLBACK fyto_licenses_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    if(msg==WM_INITDIALOG){
        HRSRC res=FindResourceW(os_hinstance,MAKEINTRESOURCEW(60012),RT_RCDATA);
        if(res){DWORD n=SizeofResource(os_hinstance,res);HGLOBAL data=LoadResource(os_hinstance,res);char *text=(char *)HeapAlloc(GetProcessHeap(),0,n*2+1);
            if(data && text){const char *src=(const char *)LockResource(data);DWORD i,j=0;for(i=0;i<n;i++){if(src[i]=='\n' && (i==0 || src[i-1]!='\r'))text[j++]='\r';text[j++]=src[i];}text[j]=0;SetDlgItemTextA(hwnd,60012,text);}if(text)HeapFree(GetProcessHeap(),0,text);
        }
        SetWindowTextW(hwnd,config_language?L"fyto · 开源许可与第三方声明":L"fyto · Open-source licenses");os_center_dialog(hwnd);return TRUE;
    }
    if(msg==WM_CLOSE || (msg==WM_COMMAND && (LOWORD(wp)==IDOK || LOWORD(wp)==IDCANCEL))){EndDialog(hwnd,0);return TRUE;}
    return FALSE;
}
static INT_PTR CALLBACK _viv_about_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    switch(msg){
        case WM_INITDIALOG:{wchar_t version[96];l10n_dialog(hwnd);
            SetWindowTextW(hwnd,config_language?L"关于 fyto":L"About fyto");
            string_printf(version,"fyto %d.%d.%d.%d%s %s",VERSION_MAJOR,VERSION_MINOR,VERSION_REVISION,VERSION_BUILD,VERSION_TYPE,VERSION_TARGET_MACHINE);
            SetDlgItemTextW(hwnd,IDC_ABOUTVERSION,version);
            SetDlgItemTextW(hwnd,IDC_ABOUTVOIDIMAGEVIEWER,config_language?L"基于 void Image Viewer 开发 · MIT 许可证":L"Based on void Image Viewer · MIT License");
            SetDlgItemTextW(hwnd,IDC_ABOUTCOPYRIGHT,L"Copyright 2025 voidtools / David Carpenter");
            SetDlgItemTextW(hwnd,IDC_ABOUTWEBSITE,L"github.com/voidtools/voidImageViewer");
            SetDlgItemTextW(hwnd,IDC_ABOUTEMAIL,config_language?L"开源许可与第三方声明":L"Open-source licenses");
            SetDlgItemTextW(hwnd,IDOK,config_language?L"确定":L"OK");fyto_about_load();fyto_about_layout(hwnd);return TRUE;
        }
        case WM_DRAWITEM:{DRAWITEMSTRUCT *item=(DRAWITEMSTRUCT *)lp;void *graphics=NULL;if(item->CtlID!=60010)break;
            FillRect(item->hDC,&item->rcItem,(HBRUSH)GetStockObject(WHITE_BRUSH));
            if(fyto_about_image && os_GdipCreateFromHDC && os_GdipDrawImageRectI && os_GdipDeleteGraphics && os_GdipCreateFromHDC(item->hDC,&graphics)==0){
                int w=item->rcItem.right-item->rcItem.left,h=item->rcItem.bottom-item->rcItem.top,dw=w,dh=MulDiv(w,fyto_about_height,fyto_about_width);
                if(dh>h){dh=h;dw=MulDiv(h,fyto_about_width,fyto_about_height);}
                if(os_GdipSetInterpolationMode)os_GdipSetInterpolationMode(graphics,7);if(os_GdipSetPixelOffsetMode)os_GdipSetPixelOffsetMode(graphics,4);
                os_GdipDrawImageRectI(graphics,fyto_about_image,item->rcItem.left+(w-dw)/2,item->rcItem.top+(h-dh)/2,dw,dh);os_GdipDeleteGraphics(graphics);
            }return TRUE;
        }
        case WM_CTLCOLORSTATIC:SetTextColor((HDC)wp,RGB(32,32,32));SetBkColor((HDC)wp,RGB(255,255,255));return (INT_PTR)GetStockObject(WHITE_BRUSH);
        case WM_PAINT:{PAINTSTRUCT ps;RECT r;BeginPaint(hwnd,&ps);GetClientRect(hwnd,&r);FillRect(ps.hdc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));EndPaint(hwnd,&ps);return TRUE;}
        case WM_COMMAND:switch(LOWORD(wp)){
            case IDC_ABOUTEMAIL:DialogBoxParamW(os_hinstance,MAKEINTRESOURCEW(60011),hwnd,fyto_licenses_proc,0);return TRUE;
            case IDOK:case IDCANCEL:EndDialog(hwnd,0);return TRUE;
        }break;
        case WM_CLOSE:EndDialog(hwnd,0);return TRUE;
        case WM_DESTROY:fyto_about_release();return TRUE;
    }return FALSE;
}
