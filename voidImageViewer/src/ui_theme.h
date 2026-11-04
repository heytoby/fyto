// Native application colors and independently drawn controls.
#ifndef VIV_UI_THEME_H
#define VIV_UI_THEME_H
static int ui_dark(COLORREF c){return GetRValue(c)*299+GetGValue(c)*587+GetBValue(c)*114<140000;}
static COLORREF ui_text(COLORREF c){return ui_dark(c)?RGB(239,241,244):RGB(27,32,38);}
static COLORREF ui_muted(COLORREF c){return ui_dark(c)?RGB(174,183,194):RGB(82,93,106);}
static COLORREF ui_mix(COLORREF c,int amount){int r=GetRValue(c),g=GetGValue(c),b=GetBValue(c);return RGB(r+(ui_dark(c)?255-r:-r)*amount/100,g+(ui_dark(c)?255-g:-g)*amount/100,b+(ui_dark(c)?255-b:-b)*amount/100);}
static void ui_fill(HDC dc,RECT *r,COLORREF color){HBRUSH brush=CreateSolidBrush(color);FillRect(dc,r,brush);DeleteObject(brush);}
static void ui_button(HDC dc,RECT r,COLORREF bg,const wchar_t *label,int selected,int hot,int enabled,HFONT font){HGDIOBJ oldfont=SelectObject(dc,font),oldpen,oldbrush;HBRUSH brush;HPEN pen;COLORREF surface=ui_mix(bg,selected?19:hot?12:5);InflateRect(&r,-2,-2);brush=CreateSolidBrush(surface);pen=CreatePen(PS_SOLID,1,surface);oldpen=SelectObject(dc,pen);oldbrush=SelectObject(dc,brush);RoundRect(dc,r.left,r.top,r.right,r.bottom,10,10);SelectObject(dc,oldbrush);SelectObject(dc,oldpen);DeleteObject(brush);DeleteObject(pen);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,enabled?ui_text(bg):ui_muted(bg));DrawTextW(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);if(selected){r.top=r.bottom-3;r.left+=12;r.right-=12;ui_fill(dc,&r,ui_dark(bg)?RGB(137,196,206):RGB(34,102,113));}SelectObject(dc,oldfont);}
#endif
