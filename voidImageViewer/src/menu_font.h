#ifndef FYTO_MENU_FONT_H
#define FYTO_MENU_FONT_H
#include <oleacc.h>
#include <wctype.h>
#include "caption_metrics.h"

typedef struct fyto_menu_item {
    MSAAMENUINFO accessible;
    struct fyto_menu_item *next;
    HMENU menu;
    UINT position;
    ULONG_PTR native_data;
    wchar_t text[256];
} fyto_menu_item_t;

static HFONT fyto_menu_font;

static void fyto_set_popup_font(HMENU menu,fyto_menu_item_t **items)
{
    int i, count = GetMenuItemCount(menu);
    for (i=0; i<count; i++)
    {
        wchar_t text[256];
        MENUITEMINFOW info = {sizeof(info)};
        fyto_menu_item_t *item;

        info.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_SUBMENU | MIIM_DATA;
        info.dwTypeData = text;
        info.cch = ARRAYSIZE(text);
        if (!GetMenuItemInfoW(menu,i,TRUE,&info))continue;
        if (info.hSubMenu)fyto_set_popup_font(info.hSubMenu,items);
        if (!info.cch || (info.fType & (MFT_SEPARATOR | MFT_OWNERDRAW | MFT_BITMAP)))continue;

        item = HeapAlloc(GetProcessHeap(),0,sizeof(*item));
        if (!item)continue;
        item->accessible.dwMSAASignature = MSAA_MENU_SIG;
        item->accessible.cchWText = info.cch;
        item->accessible.pszWText = item->text;
        item->menu = menu;
        item->position = i;
        item->native_data = info.dwItemData;
        wcscpy_s(item->text,ARRAYSIZE(item->text),text);

        info.fMask = MIIM_FTYPE | MIIM_DATA;
        info.fType |= MFT_OWNERDRAW;
        info.dwItemData = (ULONG_PTR)item;
        if (SetMenuItemInfoW(menu,i,TRUE,&info))
        {
            item->next = *items;
            *items = item;
        }
        else HeapFree(GetProcessHeap(),0,item);
    }
}

static UINT fyto_track_popup(HWND hwnd,HMENU menu,UINT flags,int x,int y)
{
    fyto_menu_item_t *items = NULL;
    UINT command;

    flags |= TPM_RETURNCMD;
    if (!menu || fyto_menu_font)return 0;
    fyto_menu_font = fyto_menu_create_font(os_logical_high);
    if (fyto_menu_font)fyto_set_popup_font(menu,&items);
    command = TrackPopupMenu(menu,flags,x,y,0,hwnd,NULL);

    while (items)
    {
        fyto_menu_item_t *item = items;
        MENUITEMINFOW info = {sizeof(info)};
        items = item->next;
        info.fMask = MIIM_FTYPE | MIIM_DATA;
        if (GetMenuItemInfoW(item->menu,item->position,TRUE,&info))
        {
            info.fType &= ~MFT_OWNERDRAW;
            info.dwItemData = item->native_data;
            SetMenuItemInfoW(item->menu,item->position,TRUE,&info);
        }
        HeapFree(GetProcessHeap(),0,item);
    }
    if (fyto_menu_font)DeleteObject(fyto_menu_font);
    fyto_menu_font = NULL;
    return command;
}

static int fyto_menu_label_length(const fyto_menu_item_t *item)
{
    const wchar_t *tab = wcschr(item->text,L'\t');
    return tab ? (int)(tab-item->text) : (int)item->accessible.cchWText;
}

static BOOL fyto_measure_menu(HWND hwnd,MEASUREITEMSTRUCT *measure)
{
    fyto_menu_item_t *item = (fyto_menu_item_t *)measure->itemData;
    RECT label = {0}, shortcut = {0};
    int length;
    HDC dc;
    HGDIOBJ old;

    if (!fyto_menu_font || measure->CtlType!=ODT_MENU || !item ||
        item->accessible.dwMSAASignature!=MSAA_MENU_SIG)return FALSE;
    dc = GetDC(hwnd);
    if (!dc)return FALSE;
    old = SelectObject(dc,fyto_menu_font);
    length = fyto_menu_label_length(item);
    DrawTextW(dc,item->text,length,&label,DT_SINGLELINE | DT_CALCRECT);
    if (item->text[length])
        DrawTextW(dc,item->text+length+1,-1,&shortcut,DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(dc,old);
    ReleaseDC(hwnd,dc);

    measure->itemWidth = label.right+shortcut.right+MulDiv(shortcut.right?62:40,os_logical_wide,96);
    measure->itemHeight = max(label.bottom+MulDiv(6,os_logical_high,96),MulDiv(22,os_logical_high,96));
    return TRUE;
}

static BOOL fyto_draw_menu(DRAWITEMSTRUCT *draw)
{
    fyto_menu_item_t *item = (fyto_menu_item_t *)draw->itemData;
    MENUITEMINFOW info = {sizeof(info)};
    RECT text;
    HDC dc;
    int length, saved, selected, disabled, x, y, radius;
    UINT flags = DT_SINGLELINE | DT_VCENTER;
    COLORREF color;

    if (!fyto_menu_font || draw->CtlType!=ODT_MENU || !item ||
        item->accessible.dwMSAASignature!=MSAA_MENU_SIG)return FALSE;

    dc = draw->hDC;
    text = draw->rcItem;
    saved = SaveDC(dc);
    selected = (draw->itemState & ODS_SELECTED)!=0;
    disabled = (draw->itemState & (ODS_DISABLED | ODS_GRAYED))!=0;
    color = GetSysColor(disabled?COLOR_GRAYTEXT:selected?COLOR_HIGHLIGHTTEXT:COLOR_MENUTEXT);
    FillRect(dc,&text,GetSysColorBrush(selected?COLOR_HIGHLIGHT:COLOR_MENU));

    SelectObject(dc,fyto_menu_font);
    SetTextColor(dc,color);
    SetBkMode(dc,TRANSPARENT);
    if (draw->itemState & ODS_NOACCEL)flags |= DT_HIDEPREFIX;
    text.left += MulDiv(22,os_logical_wide,96);
    text.right -= MulDiv(18,os_logical_wide,96);
    length = fyto_menu_label_length(item);
    DrawTextW(dc,item->text,length,&text,flags);
    if (item->text[length])
        DrawTextW(dc,item->text+length+1,-1,&text,flags | DT_RIGHT | DT_NOPREFIX);

    info.fMask = MIIM_FTYPE | MIIM_SUBMENU | MIIM_STATE;
    GetMenuItemInfoW(item->menu,item->position,TRUE,&info);
    SelectObject(dc,GetStockObject(DC_PEN));
    SetDCPenColor(dc,color);
    x = draw->rcItem.left+MulDiv(10,os_logical_wide,96);
    y = (text.top+text.bottom)/2;
    radius = max(2,MulDiv(3,os_logical_high,96));
    if ((draw->itemState & ODS_CHECKED) || (info.fState & MFS_CHECKED))
    {
        if (info.fType & MFT_RADIOCHECK)
        {
            SelectObject(dc,GetStockObject(DC_BRUSH));
            SetDCBrushColor(dc,color);
            Ellipse(dc,x-radius,y-radius,x+radius+1,y+radius+1);
        }
        else
        {
            MoveToEx(dc,x-radius,y,NULL);
            LineTo(dc,x-1,y+radius);
            LineTo(dc,x+radius+2,y-radius);
        }
    }
    if (info.hSubMenu)
    {
        x = draw->rcItem.right-MulDiv(10,os_logical_wide,96);
        MoveToEx(dc,x-1,y-radius,NULL);
        LineTo(dc,x+radius-1,y);
        LineTo(dc,x-1,y+radius);
    }

    RestoreDC(dc,saved);
    return TRUE;
}

static LRESULT fyto_menu_mnemonic(HMENU menu,wchar_t key)
{
    int i, count = GetMenuItemCount(menu), first = -1, next = -1, selected = -1, matches = 0;
    for (i=0; i<count; i++)
    {
        MENUITEMINFOW info = {sizeof(info)};
        const fyto_menu_item_t *item;
        const wchar_t *p;

        info.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STATE;
        if (!GetMenuItemInfoW(menu,i,TRUE,&info))continue;
        if (info.fState & MFS_HILITE)selected = i;
        if (!(info.fType & MFT_OWNERDRAW) || (info.fState & MFS_DISABLED) || !info.dwItemData)continue;
        item = (const fyto_menu_item_t *)info.dwItemData;
        if (item->accessible.dwMSAASignature!=MSAA_MENU_SIG)continue;

        for (p=item->text; *p && *p!=L'\t'; p++)
        {
            if (*p!=L'&' || !p[1])continue;
            if (*++p==L'&')continue;
            if (towupper(*p)==towupper(key))
            {
                if (first<0)first = i;
                if (next<0 && i>selected)next = i;
                matches++;
                break;
            }
        }
    }
    if (matches>1)return MAKELRESULT(next>=0?next:first,MNC_SELECT);
    if (matches==1)return MAKELRESULT(first,MNC_EXECUTE);
    return MAKELRESULT(0,MNC_IGNORE);
}
#endif
