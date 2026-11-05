#include <wchar.h>
#include "caption_metrics.h"
#include "pin_icon.h"
// One native non-client row for menus, filename and window controls.
static void title_label(int index,wchar_t *out,int cap){wchar_t input[256];int i,j=0,n;GetMenuStringW(_viv_hmenu,index,input,256,MF_BYPOSITION);for(i=0;input[i] && j<cap-1;i++)if(input[i]!=L'&')out[j++]=input[i];out[j]=0;n=(int)wcslen(out);if(config_language && n>=3 && out[n-3]==L'(' && out[n-1]==L')')out[n-3]=0;}
static HFONT title_font;
static int title_font_dpi;
static RECT title_menu_rects[16];
static int title_menu_count,title_compact;
static int title_height(void){return config_show_caption?fyto_caption_height(os_logical_high):0;}
static int title_border(HWND hwnd){(void)hwnd;return 0;}
#define FYTO_TITLE_PIN_HIT HTHELP
static int title_button_width(int width)
{
    return max(1,min(MulDiv(44,os_logical_wide,96),width/4));
}
static void title_paint(HWND hwnd){RECT wr,cr,r,bar;POINT pt;HDC dc;HGDIOBJ old;int width,height,i,x=12,button;wchar_t label[256],filename[STRING_SIZE];COLORREF bg=_viv_exif_background();if(!config_show_caption)return;GetWindowRect(hwnd,&wr);width=wr.right-wr.left;height=wr.bottom-wr.top;button=title_button_width(width);GetClientRect(hwnd,&cr);pt.x=pt.y=0;ClientToScreen(hwnd,&pt);OffsetRect(&cr,pt.x-wr.left,pt.y-wr.top);dc=GetWindowDC(hwnd);if(!dc)return;SaveDC(dc);ExcludeClipRect(dc,cr.left,cr.top,cr.right,cr.bottom);r.left=r.top=0;r.right=width;r.bottom=height;ui_fill(dc,&r,bg);bar=r;bar.top=title_border(hwnd);bar.bottom=bar.top+title_height();if(!title_font || title_font_dpi!=os_logical_high){if(title_font)DeleteObject(title_font);title_font=fyto_caption_create_font(os_logical_high);title_font_dpi=os_logical_high;}old=SelectObject(dc,title_font);SetTextColor(dc,ui_text(bg));SetBkMode(dc,TRANSPARENT);title_menu_count=config_show_menu?GetMenuItemCount(_viv_hmenu):0;if(title_menu_count>16)title_menu_count=16;title_compact=width<MulDiv(820,os_logical_wide,96);for(i=0;i<title_menu_count;i++){SIZE size;title_label(i,label,256);GetTextExtentPoint32W(dc,label,(int)wcslen(label),&size);title_menu_rects[i]=bar;title_menu_rects[i].left=x;title_menu_rects[i].right=x+size.cx+18;x+=size.cx+18;}if(x>width/2-100)title_compact=1;if(title_compact && title_menu_count){r=bar;r.left=12;r.right=84;DrawTextW(dc,config_language?L"菜单":L"Menu",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);x=88;}else for(i=0;i<title_menu_count;i++){title_label(i,label,256);r=title_menu_rects[i];DrawTextW(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}fyto_caption_filename_rect(&bar,os_logical_wide,os_logical_high,x,&r);GetWindowTextW(hwnd,filename,STRING_SIZE);if(r.right>r.left)DrawTextW(dc,filename,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);r=bar;r.left=width-4*button;r.right=r.left+button;fyto_draw_pin(dc,&r,config_ontop==1,0,os_logical_high);for(i=0;i<3;i++){const wchar_t *text=i==0?L"−":i==1?(IsZoomed(hwnd)?L"❐":L"□"):L"×";r=bar;r.left=width-(3-i)*button;r.right=r.left+button;DrawTextW(dc,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}SelectObject(dc,old);RestoreDC(dc,-1);ReleaseDC(hwnd,dc);}
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
        if (title_compact && x>=12 && x<88)return HTMENU;
        for (i=0; i<title_menu_count; i++)
            if (x>=title_menu_rects[i].left && x<title_menu_rects[i].right)return HTMENU;
    }
    return HTCAPTION;
}
static void title_popup(HWND hwnd,LPARAM lp){RECT wr;int i,id=0,x;HMENU popup;if(!config_show_caption)return;GetWindowRect(hwnd,&wr);x=GET_X_LPARAM(lp)-wr.left;_viv_check_menus(_viv_hmenu);if(title_compact){wchar_t label[256];popup=CreatePopupMenu();for(i=0;i<title_menu_count;i++){GetMenuStringW(_viv_hmenu,i,label,256,MF_BYPOSITION);AppendMenuW(popup,MF_POPUP,(UINT_PTR)GetSubMenu(_viv_hmenu,i),label);}id=fyto_track_popup(hwnd,popup,TPM_RIGHTBUTTON,wr.left+12,wr.top+title_border(hwnd)+title_height());for(i=title_menu_count-1;i>=0;i--)RemoveMenu(popup,i,MF_BYPOSITION);DestroyMenu(popup);}else for(i=0;i<title_menu_count;i++)if(x>=title_menu_rects[i].left && x<title_menu_rects[i].right){id=fyto_track_popup(hwnd,GetSubMenu(_viv_hmenu,i),TPM_RIGHTBUTTON,wr.left+title_menu_rects[i].left,wr.top+title_border(hwnd)+title_height());break;}if(id)PostMessage(hwnd,WM_COMMAND,id,0);title_paint(hwnd);}
