// Center controls on the complete window; fit status in the remaining right space.
typedef struct { int height,y,info_x,info_width,tools_x,tools_width; } viv_footer_layout_t;
static viv_footer_layout_t viv_footer_layout(int width,int height,int dpi_x,int dpi_y,int status,int controls,int tools)
{
    viv_footer_layout_t r; int free_right;
    r.height=controls?(32*dpi_y+48)/96:status?(24*dpi_y+48)/96:0;
    r.y=height-r.height;
    r.tools_width=controls?tools:0;
    if(r.tools_width>width)r.tools_width=width;
    if(r.tools_width<0)r.tools_width=0;
    r.tools_x=controls?(width-r.tools_width)/2:0;
    free_right=controls?width-r.tools_x-r.tools_width-(8*dpi_x+48)/96:width;
    if(free_right<0)free_right=0;
    r.info_width=status?free_right:0;
    if(controls && r.info_width>(360*dpi_x+48)/96)r.info_width=(360*dpi_x+48)/96;
    r.info_x=width-r.info_width;
    return r;
}
