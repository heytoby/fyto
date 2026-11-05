#ifndef FYTO_PIN_ICON_H
#define FYTO_PIN_ICON_H

static void fyto_draw_pin(HDC dc,const RECT *box,int pinned,int shadow,int dpi)
{
    static const POINT outline[] = {
        {5,2},{15,2},{15,5},{13,5},{13,10},{17,13},{17,15},
        {11,15},{11,20},{10,21},{9,20},{9,15},{3,15},{3,13},
        {7,10},{7,5},{5,5}
    };
    POINT points[ARRAYSIZE(outline)];
    int size = min(MulDiv(19,dpi,96),min(box->right-box->left,box->bottom-box->top)-2);
    int left,top,pass,i,saved;

    if (size<8)return;
    left=(box->left+box->right-size)/2;
    top=(box->top+box->bottom-size)/2;
    saved=SaveDC(dc);
    if (!saved)return;
    SelectObject(dc,GetStockObject(DC_PEN));
    SelectObject(dc,GetStockObject(pinned?DC_BRUSH:NULL_BRUSH));
    for (pass=shadow; pass>=0; pass--)
    {
        COLORREF color=pass?RGB(42,42,42):RGB(255,255,255);
        SetDCPenColor(dc,color);
        SetDCBrushColor(dc,color);
        for (i=0; i<ARRAYSIZE(outline); i++)
        {
            points[i].x=left+MulDiv(outline[i].x, size,22)+pass;
            points[i].y=top+MulDiv(outline[i].y, size,22)+pass;
        }
        Polygon(dc,points,ARRAYSIZE(points));
    }
    RestoreDC(dc,saved);
}
#endif
