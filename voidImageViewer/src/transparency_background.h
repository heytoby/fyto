// Transparency visualization is confined to the decoded image surface.
// The source file is never modified; opaque pixels still replace the backdrop.
static void fyto_fill_transparency_background(HDC dc,int width,int height)
{
    DWORD pixels[32*32];int x,y;HBITMAP tile;HBRUSH brush=NULL;RECT rect;POINT origin;
    for(y=0;y<32;y++)for(x=0;x<32;x++)pixels[y*32+x]=(((x/16)^(y/16))&1)?0x00D8D8D8:0x00F0F0F0;
    tile=CreateBitmap(32,32,1,32,pixels);
    if(tile)brush=CreatePatternBrush(tile);
    SetRect(&rect,0,0,width,height);
    SetBrushOrgEx(dc,0,0,&origin);
    FillRect(dc,&rect,brush?brush:(HBRUSH)GetStockObject(LTGRAY_BRUSH));
    SetBrushOrgEx(dc,origin.x,origin.y,NULL);
    if(brush)DeleteObject(brush);
    if(tile)DeleteObject(tile);
}
