#include <wchar.h>
#include "caption_metrics.h"
#include "pin_icon.h"
// One native non-client row for menus, filename and window controls.
static void title_label(int index,wchar_t *out,int cap){wchar_t input[256];int i,j=0,n;GetMenuStringW(_viv_hmenu,index,input,256,MF_BYPOSITION);for(i=0;input[i] && j<cap-1;i++)if(input[i]!=L'&')out[j++]=input[i];out[j]=0;n=(int)wcslen(out);if(config_language && n>=3 && out[n-3]==L'(' && out[n-1]==L')')out[n-3]=0;}
static HFONT title_font,title_menu_font;
static int title_font_dpi;
static BOOL title_deferred, title_dirty;
static fyto_paint_buffer_t title_buffer;
static RECT title_menu_rects[16];
static int title_menu_count,title_compact;
static int title_height(void){return config_show_caption?fyto_caption_height(os_logical_high):0;}
static int title_border(HWND hwnd){(void)hwnd;return 0;}
#define FYTO_TITLE_PIN_HIT HTHELP
static int title_button_width(int width)
{
    return max(1,min(MulDiv(44,os_logical_wide,96),width/4));
}
static void title_release_fonts(void)
{
    if (title_font)DeleteObject(title_font);
    if (title_menu_font)DeleteObject(title_menu_font);
    title_font=title_menu_font=NULL;
}
static void title_paint(HWND hwnd)
{
    RECT wr,cr,r,bar;
    POINT pt={0,0};
    HDC dc, target, buffer;
    int width,height,i,x,button,padding,margin;
    wchar_t label[256],filename[STRING_SIZE];
    COLORREF bg=_viv_exif_background();

    if (!config_show_caption) {
        title_dirty = FALSE;
        return;
    }
    if (title_deferred) {
        title_dirty = TRUE;
        return;
    }
    title_dirty = FALSE;
    if (!GetWindowRect(hwnd,&wr))return;

    width=wr.right-wr.left;
    height=wr.bottom-wr.top;
    button=title_button_width(width);
    GetClientRect(hwnd,&cr);
    ClientToScreen(hwnd,&pt);
    OffsetRect(&cr,pt.x-wr.left,pt.y-wr.top);
    target=GetWindowDC(hwnd);
    if (!target)return;
    SaveDC(target);
    ExcludeClipRect(target,cr.left,cr.top,cr.right,cr.bottom);
    SetRect(&r,0,0,width,height);
    bar=r;
    bar.top=title_border(hwnd);
    bar.bottom=min(height,bar.top+title_height());
    buffer=fyto_paint_buffer_dc(&title_buffer,target,width,bar.bottom);
    if (buffer) {
        int saved=SaveDC(target);
        ExcludeClipRect(target,bar.left,bar.top,bar.right,bar.bottom);
        ui_fill(target,&r,bg);
        RestoreDC(target,saved);
    }
    dc=buffer?buffer:target;
    if (buffer) SaveDC(buffer);
    ui_fill(dc,buffer?&bar:&r,bg);

    if (title_font_dpi!=os_logical_high || !title_font || !title_menu_font)
    {
        title_release_fonts();
        title_font=fyto_caption_create_font(os_logical_high);
        title_menu_font=fyto_menu_create_font(os_logical_high);
        title_font_dpi=os_logical_high;
    }
    SelectObject(dc,title_menu_font?title_menu_font:title_font);
    SetTextColor(dc,ui_text(bg));
    SetBkMode(dc,TRANSPARENT);

    margin=MulDiv(12,os_logical_wide,96);
    padding=MulDiv(18,os_logical_wide,96);
    x=margin;
    title_menu_count=config_show_menu?GetMenuItemCount(_viv_hmenu):0;
    if (title_menu_count>16)title_menu_count=16;
    title_compact=width<MulDiv(820,os_logical_wide,96);
    for (i=0; i<title_menu_count; i++)
    {
        RECT size={0};
        title_label(i,label,ARRAYSIZE(label));
        fyto_draw_ui_text(dc,label,-1,&size,DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX,fyto_menu_font_pixels(os_logical_high));
        title_menu_rects[i]=bar;
        title_menu_rects[i].left=x;
        x+=size.right+padding;
        title_menu_rects[i].right=x;
    }
    if (x>width/2-MulDiv(100,os_logical_wide,96))title_compact=1;
    if (title_compact && title_menu_count)
    {
        r=bar;
        r.left=margin;
        r.right=MulDiv(84,os_logical_wide,96);
        fyto_draw_ui_text(dc,config_language?L"菜单":L"Menu",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX,fyto_menu_font_pixels(os_logical_high));
        x=MulDiv(88,os_logical_wide,96);
    }
    else for (i=0; i<title_menu_count; i++)
    {
        title_label(i,label,ARRAYSIZE(label));
        r=title_menu_rects[i];
        fyto_draw_ui_text(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX,fyto_menu_font_pixels(os_logical_high));
    }

    SelectObject(dc,title_font);
    fyto_caption_filename_rect(&bar,os_logical_wide,os_logical_high,x,&r);
    GetWindowTextW(hwnd,filename,STRING_SIZE);
    if (r.right>r.left)
        fyto_draw_ui_text(dc,filename,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX,fyto_caption_font_pixels(os_logical_high));
    r=bar;
    r.left=width-4*button;
    r.right=r.left+button;
    fyto_draw_pin(dc,&r,config_ontop==1,0,os_logical_high);
    for (i=0; i<3; i++)
    {
        const wchar_t *text=i==0?L"−":i==1?(IsZoomed(hwnd)?L"❐":L"□"):L"×";
        r=bar;
        r.left=width-(3-i)*button;
        r.right=r.left+button;
        DrawTextW(dc,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    if (buffer) {
        RestoreDC(buffer,-1);
        BitBlt(target,bar.left,bar.top,width,bar.bottom-bar.top,buffer,bar.left,bar.top,SRCCOPY);
    }
    RestoreDC(target,-1);
    ReleaseDC(hwnd,target);
}
static int title_hit(HWND hwnd,LPARAM lp)
{
    static const int buttons[]={FYTO_TITLE_PIN_HIT,HTMINBUTTON,HTMAXBUTTON,HTCLOSE};
    RECT wr;
    int x,y,width,button,controls,i;
    if (!config_show_caption || !GetWindowRect(hwnd,&wr))return 0;
    x=GET_X_LPARAM(lp)-wr.left;
    y=GET_Y_LPARAM(lp)-wr.top;
    if (y<title_border(hwnd) || y>=title_border(hwnd)+title_height())return 0;

    width=wr.right-wr.left;
    button=title_button_width(width);
    controls=width-ARRAYSIZE(buttons)*button;
    if (x>=controls && x<width)return buttons[(x-controls)/button];

    if (config_show_menu)
    {
        if (title_compact && x>=MulDiv(12,os_logical_wide,96) && x<MulDiv(88,os_logical_wide,96))return HTMENU;
        for (i=0; i<title_menu_count; i++)
            if (x>=title_menu_rects[i].left && x<title_menu_rects[i].right)return HTMENU;
    }
    return HTCAPTION;
}
static void title_popup(HWND hwnd,LPARAM lp){RECT wr;int i,id=0,x;HMENU popup;if(!config_show_caption)return;GetWindowRect(hwnd,&wr);x=GET_X_LPARAM(lp)-wr.left;_viv_check_menus(_viv_hmenu);if(title_compact){wchar_t label[256];popup=CreatePopupMenu();for(i=0;i<title_menu_count;i++){GetMenuStringW(_viv_hmenu,i,label,256,MF_BYPOSITION);AppendMenuW(popup,MF_POPUP,(UINT_PTR)GetSubMenu(_viv_hmenu,i),label);}id=fyto_track_popup(hwnd,popup,TPM_RIGHTBUTTON,wr.left+MulDiv(12,os_logical_wide,96),wr.top+title_border(hwnd)+title_height());for(i=title_menu_count-1;i>=0;i--)RemoveMenu(popup,i,MF_BYPOSITION);DestroyMenu(popup);}else for(i=0;i<title_menu_count;i++)if(x>=title_menu_rects[i].left && x<title_menu_rects[i].right){id=fyto_track_popup(hwnd,GetSubMenu(_viv_hmenu,i),TPM_RIGHTBUTTON,wr.left+title_menu_rects[i].left,wr.top+title_border(hwnd)+title_height());break;}if(id)PostMessage(hwnd,WM_COMMAND,id,0);title_paint(hwnd);}
