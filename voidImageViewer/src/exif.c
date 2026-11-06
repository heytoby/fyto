// Native read-only EXIF inspector. Image files are never modified.
// Based on the TIFF/EXIF directory format; no external runtime dependency.
#include "viv.h"
#include "exif.h"
#include "ui_theme.h"
#include "caption_metrics.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>
#pragma float_control(precise,on,push)

#define EXIF_MAX_ROWS 4096
#define EXIF_TEXT 2048
typedef struct { unsigned short id; const wchar_t *en,*zh; } exif_tag_t;
static const exif_tag_t tags[] = {
 {0x0100,L"Image width",L"图像宽度"},{0x0101,L"Image height",L"图像高度"},
 {0x0102,L"Bits per sample",L"每通道位深"},{0x0103,L"Compression",L"压缩方式"},
 {0x0106,L"Photometric interpretation",L"颜色模型"},{0x010e,L"Image description",L"图像描述"},
 {0x010f,L"Camera manufacturer",L"相机厂商"},{0x0110,L"Camera model",L"相机型号"},
 {0x0111,L"Strip offsets",L"条带偏移"},{0x0112,L"Orientation",L"方向"},
 {0x0115,L"Samples per pixel",L"每像素通道数"},{0x0116,L"Rows per strip",L"每条带行数"},
 {0x0117,L"Strip byte counts",L"条带字节数"},{0x011a,L"X resolution",L"水平分辨率"},
 {0x011b,L"Y resolution",L"垂直分辨率"},{0x011c,L"Planar configuration",L"平面配置"},
 {0x0128,L"Resolution unit",L"分辨率单位"},{0x012d,L"Transfer function",L"传递函数"},
 {0x0131,L"Software",L"处理软件"},{0x0132,L"Modified date/time",L"修改时间"},
 {0x013b,L"Artist",L"作者"},{0x013e,L"White point",L"白点"},
 {0x013f,L"Primary chromaticities",L"原色色度"},{0x0201,L"Thumbnail offset",L"缩略图偏移"},
 {0x0202,L"Thumbnail length",L"缩略图长度"},{0x0211,L"YCbCr coefficients",L"YCbCr 系数"},
 {0x0212,L"YCbCr subsampling",L"YCbCr 子采样"},{0x0213,L"YCbCr positioning",L"YCbCr 定位"},
 {0x0214,L"Reference black/white",L"参考黑白值"},{0x8298,L"Copyright",L"版权"},
 {0x829a,L"Exposure time (s)",L"曝光时间（秒）"},{0x829d,L"F number",L"光圈值"},
 {0x8769,L"EXIF directory",L"EXIF 子目录"},{0x8822,L"Exposure program",L"曝光程序"},
 {0x8824,L"Spectral sensitivity",L"光谱灵敏度"},{0x8825,L"GPS directory",L"GPS 子目录"},
 {0x8827,L"ISO sensitivity",L"ISO 感光度"},{0x8828,L"OECF",L"光电转换函数"},
 {0x8830,L"Sensitivity type",L"灵敏度类型"},{0x8831,L"Standard output sensitivity",L"标准输出灵敏度"},
 {0x8832,L"Recommended exposure index",L"推荐曝光指数"},{0x8833,L"ISO speed",L"ISO 速度"},
 {0x9000,L"EXIF version",L"EXIF 版本"},{0x9003,L"Date/time original",L"拍摄时间"},
 {0x9004,L"Date/time digitized",L"数字化时间"},{0x9010,L"UTC offset",L"时区偏移"},
 {0x9011,L"Original UTC offset",L"拍摄时区偏移"},{0x9012,L"Digitized UTC offset",L"数字化时区偏移"},
 {0x9101,L"Components configuration",L"通道配置"},{0x9102,L"Compressed bits/pixel",L"压缩后每像素位数"},
 {0x9201,L"Shutter speed (APEX)",L"快门速度（APEX）"},{0x9202,L"Aperture (APEX)",L"光圈（APEX）"},
 {0x9203,L"Brightness (APEX)",L"亮度（APEX）"},{0x9204,L"Exposure bias (EV)",L"曝光补偿（EV）"},
 {0x9205,L"Maximum aperture",L"最大光圈"},{0x9206,L"Subject distance (m)",L"主体距离（米）"},
 {0x9207,L"Metering mode",L"测光模式"},{0x9208,L"Light source",L"光源"},
 {0x9209,L"Flash",L"闪光灯"},{0x920a,L"Focal length (mm)",L"焦距（毫米）"},
 {0x9214,L"Subject area",L"主体区域"},{0x927c,L"Maker note (raw)",L"厂商私有数据（原始）"},
 {0x9286,L"User comment",L"用户注释"},{0x9290,L"Subsecond time",L"亚秒时间"},
 {0x9291,L"Original subsecond time",L"拍摄亚秒时间"},{0x9292,L"Digitized subsecond time",L"数字化亚秒时间"},
 {0x9c9b,L"XP title",L"标题"},{0x9c9c,L"XP comment",L"注释"},
 {0x9c9d,L"XP author",L"作者"},{0x9c9e,L"XP keywords",L"关键词"},{0x9c9f,L"XP subject",L"主题"},
 {0xa000,L"FlashPix version",L"FlashPix 版本"},{0xa001,L"Color space",L"色彩空间"},
 {0xa002,L"EXIF image width",L"EXIF 图像宽度"},{0xa003,L"EXIF image height",L"EXIF 图像高度"},
 {0xa004,L"Related sound file",L"关联音频文件"},{0xa005,L"Interoperability directory",L"互操作子目录"},
 {0xa20b,L"Flash energy",L"闪光能量"},{0xa20c,L"Spatial frequency response",L"空间频率响应"},
 {0xa20e,L"Focal-plane X resolution",L"焦平面水平分辨率"},{0xa20f,L"Focal-plane Y resolution",L"焦平面垂直分辨率"},
 {0xa210,L"Focal-plane resolution unit",L"焦平面分辨率单位"},{0xa214,L"Subject location",L"主体位置"},
 {0xa215,L"Exposure index",L"曝光指数"},{0xa217,L"Sensing method",L"传感方式"},
 {0xa300,L"File source",L"文件来源"},{0xa301,L"Scene type",L"场景类型"},
 {0xa302,L"CFA pattern",L"彩色滤光阵列"},{0xa401,L"Custom rendering",L"自定义处理"},
 {0xa402,L"Exposure mode",L"曝光模式"},{0xa403,L"White balance",L"白平衡"},
 {0xa404,L"Digital zoom ratio",L"数字变焦倍率"},{0xa405,L"35mm equivalent focal length",L"等效 35 毫米焦距"},
 {0xa406,L"Scene capture type",L"拍摄场景类型"},{0xa407,L"Gain control",L"增益控制"},
 {0xa408,L"Contrast",L"对比度"},{0xa409,L"Saturation",L"饱和度"},
 {0xa40a,L"Sharpness",L"锐度"},{0xa40b,L"Device setting description",L"设备设置描述"},
 {0xa40c,L"Subject distance range",L"主体距离范围"},{0xa420,L"Image unique ID",L"图像唯一标识"},
 {0xa430,L"Camera owner",L"相机所有者"},{0xa431,L"Camera serial number",L"机身序列号"},
 {0xa432,L"Lens specification",L"镜头规格"},{0xa433,L"Lens manufacturer",L"镜头厂商"},
 {0xa434,L"Lens model",L"镜头型号"},{0xa435,L"Lens serial number",L"镜头序列号"},
 {0xa460,L"Composite image",L"合成图像"},{0xa500,L"Gamma",L"伽马值"}
};
static const wchar_t *gps_en[]={L"Version",L"Latitude reference",L"Latitude (degrees, minutes, seconds)",L"Longitude reference",L"Longitude (degrees, minutes, seconds)",L"Altitude reference",L"Altitude (m)",L"UTC time",L"Satellites",L"Status",L"Measure mode",L"DOP",L"Speed reference",L"Speed",L"Track reference",L"Track",L"Image direction reference",L"Image direction",L"Map datum",L"Destination latitude reference",L"Destination latitude",L"Destination longitude reference",L"Destination longitude",L"Destination bearing reference",L"Destination bearing",L"Destination distance reference",L"Destination distance",L"Processing method",L"Area information",L"UTC date",L"Differential correction",L"Horizontal positioning error"};
static const wchar_t *gps_zh[]={L"版本",L"纬度方向",L"纬度（度、分、秒）",L"经度方向",L"经度（度、分、秒）",L"海拔基准",L"海拔（米）",L"UTC 时间",L"卫星",L"状态",L"定位模式",L"精度因子",L"速度单位",L"速度",L"航向基准",L"航向",L"图像方位基准",L"图像方位",L"大地基准",L"目标纬度方向",L"目标纬度",L"目标经度方向",L"目标经度",L"目标方位基准",L"目标方位",L"目标距离单位",L"目标距离",L"定位方法",L"区域信息",L"UTC 日期",L"差分修正",L"水平定位误差"};
typedef struct { const BYTE *data; size_t size; int little,rows,damaged; DWORD visited[128]; unsigned int visits; } exif_reader_t;
static HWND exif_hwnd,exif_list,exif_file,exif_count,exif_copy;
static wchar_t exif_last_path[32768];
static int exif_last_language=-1;
static HWND exif_heading,exif_close;
static HFONT exif_font,exif_heading_font;
static int exif_font_dpi;
static int exif_top,exif_bottom;
static HBRUSH exif_brush;
static COLORREF exif_bg=RGB(32,35,39);
static HWND exif_overview_button,exif_detail_button;
static const wchar_t *tr(const wchar_t *en,const wchar_t *zh){return config_language?zh:en;}
static void fmt(wchar_t *out,size_t cap,const wchar_t *format,...){va_list args;va_start(args,format);_vsnwprintf_s(out,cap,_TRUNCATE,format,args);va_end(args);}
static WORD rd16(const BYTE *p,int little){return little?(WORD)(p[0]|p[1]<<8):(WORD)(p[1]|p[0]<<8);}
static DWORD rd32(const BYTE *p,int little){return little?((DWORD)p[0]|(DWORD)p[1]<<8|(DWORD)p[2]<<16|(DWORD)p[3]<<24):((DWORD)p[3]|(DWORD)p[2]<<8|(DWORD)p[1]<<16|(DWORD)p[0]<<24);}
static int span(size_t size,size_t offset,size_t length){return offset<=size && length<=size-offset;}
static void row(const wchar_t *key,const wchar_t *value){LVITEMW item;int index=ListView_GetItemCount(exif_list);if(index>=EXIF_MAX_ROWS)return;ZeroMemory(&item,sizeof(item));item.mask=LVIF_TEXT;item.iItem=index;item.pszText=(LPWSTR)key;SendMessageW(exif_list,LVM_INSERTITEMW,0,(LPARAM)&item);item.iSubItem=1;item.pszText=(LPWSTR)value;SendMessageW(exif_list,LVM_SETITEMTEXTW,index,(LPARAM)&item);}
static void tagname(wchar_t *out,size_t cap,WORD id,int group){size_t i;const wchar_t *name=tr(L"Unknown tag",L"未知字段");const wchar_t *section=group==2?L"GPS":group==1?L"EXIF":group==3?L"Interop":group==4?tr(L"Thumbnail",L"缩略图"):L"IFD0";if(group==2 && id<sizeof(gps_en)/sizeof(gps_en[0]))name=config_language?gps_zh[id]:gps_en[id];else if(group==3 && id==1)name=tr(L"Interoperability index",L"互操作索引");else if(group!=2 && group!=3)for(i=0;i<sizeof(tags)/sizeof(tags[0]);i++)if(tags[i].id==id){name=config_language?tags[i].zh:tags[i].en;break;}fmt(out,cap,L"%s · %s [0x%04X]",section,name,id);}
static void append(wchar_t *out,size_t cap,const wchar_t *text){size_t n=wcslen(out);if(n<cap-1)wcsncpy_s(out+n,cap-n,text,_TRUNCATE);}
static void value_text_impl(wchar_t *out,size_t cap,WORD tag,WORD type,DWORD count,const BYTE *data,size_t length,int little){DWORD i,shown;wchar_t part[128];out[0]=0;
 if((tag>=0x9c9b && tag<=0x9c9f) || (tag==0x9286 && length>=8 && !memcmp(data,"UNICODE",7))){size_t pos=(tag==0x9286?8:0),n=0;for(;pos+1<length && n+1<cap;pos+=2){WORD ch=rd16(data+pos,tag==0x9286?little:1);if(!ch)break;out[n++]=ch;}out[n]=0;return;}
 if(type==2 || (tag==0x9286 && length>=8 && !memcmp(data,"ASCII",5))){size_t skip=tag==0x9286?8:0;size_t n=length-skip;if(n>cap-1)n=cap-1;if(n){int result=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(LPCCH)data+skip,(int)n,out,(int)cap-1);if(!result)result=MultiByteToWideChar(CP_ACP,0,(LPCCH)data+skip,(int)n,out,(int)cap-1);out[result]=0;for(i=0;out[i];i++)if(out[i]<32 && out[i]!=L'\t')out[i]=L' ';}return;}
 if(type==7 && (tag==0x9000 || tag==0xa000)){size_t n=length<cap-1?length:cap-1;for(i=0;i<n;i++)out[i]=(data[i]>=32 && data[i]<127)?data[i]:L'.';out[n]=0;return;}
 shown=count>64?64:count;
 for(i=0;i<shown;i++){if(i)append(out,cap,L", ");switch(type){
  case 1:fmt(part,128,L"%u",data[i]);break;
  case 6:fmt(part,128,L"%d",(signed char)data[i]);break;
  case 3:fmt(part,128,L"%u",rd16(data+i*2,little));break;
  case 8:fmt(part,128,L"%d",(short)rd16(data+i*2,little));break;
  case 4:case 13:fmt(part,128,L"%lu",(unsigned long)rd32(data+i*4,little));break;
  case 9:fmt(part,128,L"%ld",(long)rd32(data+i*4,little));break;
  case 5:case 10:{DWORD a=rd32(data+i*8,little),b=rd32(data+i*8+4,little);double numerator=type==10?(double)(LONG)a:(double)a,denominator=type==10?(double)(LONG)b:(double)b;if(!b)fmt(part,128,L"%.0f/0",numerator);else fmt(part,128,L"%.0f/%.0f (%.7g)",numerator,denominator,numerator/denominator);break;}
  case 11:{DWORD bits=rd32(data+i*4,little);float v;memcpy(&v,&bits,4);fmt(part,128,L"%.7g",(double)v);break;}
  case 12:{ULONGLONG bits=little?((ULONGLONG)rd32(data+i*8,1)|((ULONGLONG)rd32(data+i*8+4,1)<<32)):((ULONGLONG)rd32(data+i*8+4,0)|((ULONGLONG)rd32(data+i*8,0)<<32));double v;memcpy(&v,&bits,8);fmt(part,128,L"%.12g",v);break;}
  default:fmt(part,128,L"%02X",data[i]);break;
 }append(out,cap,part);}
 if(count>shown){fmt(part,128,tr(L" … (%lu values, %Iu bytes)",L" …（共 %lu 项，%Iu 字节）"),(unsigned long)count,length);append(out,cap,part);}
 if(!count)append(out,cap,tr(L"(empty)",L"（空）"));
}
// The viewer deliberately uses round-toward-zero for image transforms.
// Format EXIF numbers with normal rounding, then restore the viewer's setting.
static void value_text(wchar_t *out,size_t cap,WORD tag,WORD type,DWORD count,const BYTE *data,size_t length,int little)
{
 unsigned int saved,unused;
 _controlfp_s(&saved,0,0);
 _controlfp_s(&unused,_RC_NEAR,_MCW_RC);
 value_text_impl(out,cap,tag,type,count,data,length,little);
 _controlfp_s(&unused,saved,_MCW_RC);
}
#include "exif_summary.h"
static void directory(exif_reader_t *r,DWORD offset,int group,int depth){WORD count,i;size_t base;DWORD next;static const BYTE widths[]={0,1,1,2,4,8,1,1,2,4,8,4,8,4};unsigned int j;
 if(!offset || depth>8 || r->rows>=EXIF_MAX_ROWS)return;
 for(j=0;j<r->visits;j++)if(r->visited[j]==offset)return;
 if(r->visits>=128){r->damaged=1;return;}r->visited[r->visits++]=offset;
 if(!span(r->size,offset,2)){r->damaged=1;return;}count=rd16(r->data+offset,r->little);base=(size_t)offset+2;
 if(count>EXIF_MAX_ROWS || !span(r->size,base,(size_t)count*12+4)){r->damaged=1;return;}
 for(i=0;i<count && r->rows<EXIF_MAX_ROWS;i++){const BYTE *entry=r->data+base+(size_t)i*12,*value;WORD tag=rd16(entry,r->little),type=rd16(entry+2,r->little);DWORD n=rd32(entry+4,r->little),position=rd32(entry+8,r->little);size_t bytes;wchar_t key[256],text[EXIF_TEXT];
  tagname(key,256,tag,group);
  if(type>=sizeof(widths) || !widths[type] || n>((size_t)-1)/widths[type]){row(key,tr(L"Unsupported EXIF field type",L"不支持的 EXIF 字段类型"));r->rows++;continue;}
  bytes=(size_t)n*widths[type];value=bytes<=4?entry+8:span(r->size,position,bytes)?r->data+position:NULL;
  if(!value){row(key,tr(L"Invalid/truncated metadata offset",L"元数据偏移无效或数据不完整"));r->rows++;r->damaged=1;continue;}
  remember_field(tag,group,type,n,value,bytes,r->little);value_text(text,EXIF_TEXT,tag,type,n,value,bytes,r->little);row(key,text);r->rows++;
  if((tag==0x8769 || tag==0x8825 || tag==0xa005) && (type==4 || type==13) && n)directory(r,rd32(value,r->little),tag==0x8825?2:tag==0xa005?3:1,depth+1);
  else if(tag==0x014a && (type==4 || type==13)){DWORD k;for(k=0;k<n && k<32;k++)directory(r,rd32(value+k*4,r->little),group,depth+1);}
 }
 next=rd32(r->data+base+(size_t)count*12,r->little);if(next)directory(r,next,4,depth+1);
}
static void tiff(const BYTE *data,size_t length,exif_reader_t *r){if(length>=6 && !memcmp(data,"Exif\0\0",6)){data+=6;length-=6;}if(length<8 || !((data[0]=='I' && data[1]=='I') || (data[0]=='M' && data[1]=='M')))return;r->little=data[0]=='I';if(rd16(data+2,r->little)!=42){r->damaged=1;return;}r->data=data;r->size=length;r->visits=0;directory(r,rd32(data+4,r->little),0,0);}
static void metadata(const BYTE *data,size_t size,exif_reader_t *r){size_t pos;if(size>=2 && data[0]==0xff && data[1]==0xd8){pos=2;while(span(size,pos,2)){BYTE marker;size_t length;if(data[pos++]!=0xff)break;while(pos<size && data[pos]==0xff)pos++;if(pos>=size)break;marker=data[pos++];if(marker==0xda || marker==0xd9)break;if(marker==0x01 || (marker>=0xd0 && marker<=0xd7))continue;if(!span(size,pos,2))break;length=rd16(data+pos,0);if(length<2 || !span(size,pos,length)){r->damaged=1;break;}if(marker==0xe1 && length>=8 && !memcmp(data+pos+2,"Exif\0\0",6))tiff(data+pos+2,length-2,r);pos+=length;}}
 else if(size>=8 && !memcmp(data,"\211PNG\r\n\032\n",8)){pos=8;while(span(size,pos,12)){DWORD length=rd32(data+pos,0);if(!span(size,pos+8,(size_t)length+4)){r->damaged=1;break;}if(!memcmp(data+pos+4,"eXIf",4))tiff(data+pos+8,length,r);pos+=(size_t)length+12;}}
 else if(size>=12 && !memcmp(data,"RIFF",4) && !memcmp(data+8,"WEBP",4)){pos=12;while(span(size,pos,8)){DWORD length=rd32(data+pos+4,1);if(!span(size,pos+8,length)){r->damaged=1;break;}if(!memcmp(data+pos,"EXIF",4))tiff(data+pos+8,length,r);pos+=(size_t)length+8+(length&1);}}
 else tiff(data,size,r);
}
static void load_metadata(const wchar_t *path){HANDLE file=INVALID_HANDLE_VALUE,mapping=NULL;const BYTE *data=NULL;LARGE_INTEGER size;exif_reader_t reader;wchar_t text[256];LV_COLUMNW column;ZeroMemory(&reader,sizeof(reader));summary_field_count=0;summary_width=summary_height=summary_bits=0;summary_dpi=0;summary_bytes=0;SetWindowTextW(exif_hwnd,tr(L"EXIF information",L"EXIF 信息"));SetWindowTextW(exif_copy,tr(L"Copy",L"复制信息"));SetWindowTextW(exif_heading,tr(L"Photo information",L"照片信息"));SetWindowTextW(exif_overview_button,tr(L"Overview",L"信息概览"));SetWindowTextW(exif_detail_button,tr(L"All EXIF",L"全部 EXIF"));SetWindowTextW(exif_file,path && *path?path:tr(L"No image selected",L"尚未选择图像"));ZeroMemory(&column,sizeof(column));column.mask=LVCF_TEXT;column.pszText=(LPWSTR)tr(L"Field / Tag ID",L"字段 / 标签编号");SendMessageW(exif_list,LVM_SETCOLUMNW,0,(LPARAM)&column);column.pszText=(LPWSTR)tr(L"Value",L"值");SendMessageW(exif_list,LVM_SETCOLUMNW,1,(LPARAM)&column);
 SendMessageW(exif_list,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(exif_list);
 if(path && *path){file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(file!=INVALID_HANDLE_VALUE && GetFileSizeEx(file,&size) && size.QuadPart>0 && (ULONGLONG)size.QuadPart<=(size_t)-1){mapping=CreateFileMappingW(file,NULL,PAGE_READONLY,0,0,NULL);if(mapping)data=(const BYTE *)MapViewOfFile(mapping,FILE_MAP_READ,0,0,0);if(data){summary_bytes=size.QuadPart;basic_image(data,(size_t)size.QuadPart);metadata(data,(size_t)size.QuadPart,&reader);}}
  if(!data)row(tr(L"File",L"文件"),tr(L"Unable to read this image's metadata",L"无法读取此图像的元数据"));else if(!reader.rows)row(tr(L"EXIF",L"EXIF"),reader.damaged?tr(L"Malformed or unsupported EXIF metadata",L"EXIF 数据不完整或格式暂不支持"):tr(L"No EXIF metadata in this image",L"此图像没有 EXIF 信息"));
 }else row(L"EXIF",tr(L"Open an image to inspect its EXIF metadata",L"打开图像后显示 EXIF 信息"));
 {TCITEMW tab;const wchar_t *base=path?wcsrchr(path,L'\\'):NULL;ZeroMemory(&tab,sizeof(tab));tab.mask=TCIF_TEXT;tab.pszText=(LPWSTR)tr(L"Overview",L"信息概览");SendMessageW(exif_tabs,TCM_SETITEMW,0,(LPARAM)&tab);tab.pszText=(LPWSTR)tr(L"All EXIF",L"全部 EXIF");SendMessageW(exif_tabs,TCM_SETITEMW,1,(LPARAM)&tab);if(path && *path)SetWindowTextW(exif_file,base?base+1:path);build_summary(path);show_exif_page();}
 if(data)UnmapViewOfFile(data);if(mapping)CloseHandle(mapping);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
 fmt(text,256,reader.damaged?tr(L"%d fields · some metadata is incomplete",L"%d 个字段 · 部分元数据不完整"):tr(L"%d fields · updates when browsing images",L"%d 个字段 · 随图片切换自动更新"),reader.rows);SetWindowTextW(exif_count,text);SendMessageW(exif_list,WM_SETREDRAW,TRUE,0);InvalidateRect(exif_list,NULL,TRUE);
}
static void copy_all(void){int i,count=ListView_GetItemCount(exif_list);size_t cap=(size_t)(count+1)*(EXIF_TEXT+512),used=0;HGLOBAL global;wchar_t *text;if(!OpenClipboard(exif_hwnd))return;global=GlobalAlloc(GMEM_MOVEABLE,cap*sizeof(wchar_t));if(!global){CloseClipboard();return;}text=(wchar_t *)GlobalLock(global);if(!text){GlobalFree(global);CloseClipboard();return;}text[0]=0;if(!exif_details){GetWindowTextW(exif_summary,text,(int)cap);}else for(i=0;i<count;i++){wchar_t key[256],value[EXIF_TEXT];LVITEMW item;ZeroMemory(&item,sizeof(item));item.iSubItem=0;item.pszText=key;item.cchTextMax=256;SendMessageW(exif_list,LVM_GETITEMTEXTW,i,(LPARAM)&item);item.iSubItem=1;item.pszText=value;item.cchTextMax=EXIF_TEXT;SendMessageW(exif_list,LVM_GETITEMTEXTW,i,(LPARAM)&item);fmt(text+used,cap-used,L"%s\t%s\r\n",key,value);used=wcslen(text);}GlobalUnlock(global);EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,global))GlobalFree(global);CloseClipboard();}
static WNDPROC exif_old_header_proc;
static LRESULT CALLBACK exif_header_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){if(msg==WM_ERASEBKGND)return 1;if(msg==WM_PAINT){PAINTSTRUCT ps;RECT r;int i,n;HGDIOBJ old;BeginPaint(hwnd,&ps);GetClientRect(hwnd,&r);ui_fill(ps.hdc,&r,ui_mix(exif_bg,7));SetTextColor(ps.hdc,ui_text(exif_bg));SetBkMode(ps.hdc,TRANSPARENT);old=SelectObject(ps.hdc,exif_font);n=Header_GetItemCount(hwnd);for(i=0;i<n;i++){HDITEMW item;wchar_t label[200];ZeroMemory(&item,sizeof(item));item.mask=HDI_TEXT;item.pszText=label;item.cchTextMax=200;SendMessageW(hwnd,HDM_GETITEMW,i,(LPARAM)&item);Header_GetItemRect(hwnd,i,&r);r.left+=8;DrawTextW(ps.hdc,label,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);}SelectObject(ps.hdc,old);EndPaint(hwnd,&ps);return 0;}return CallWindowProcW(exif_old_header_proc,hwnd,msg,wp,lp);}
static int exif_scale(int value)
{
    return MulDiv(value,os_logical_high,96);
}

static void exif_resize_controls(int width,int height)
{
    int body=max(exif_scale(40),height-exif_scale(204));
    int tabs=max(0,(width-exif_scale(36))/2);
    int text=max(0,width-exif_scale(40));
    MoveWindow(exif_heading,exif_scale(20),exif_scale(18),max(0,width-exif_scale(76)),exif_scale(28),TRUE);
    MoveWindow(exif_close,width-exif_scale(48),exif_scale(12),exif_scale(32),exif_scale(32),TRUE);
    MoveWindow(exif_file,exif_scale(20),exif_scale(56),text,exif_scale(22),TRUE);
    MoveWindow(exif_overview_button,exif_scale(16),exif_scale(90),tabs,exif_scale(32),TRUE);
    MoveWindow(exif_detail_button,exif_scale(20)+tabs,exif_scale(90),tabs,exif_scale(32),TRUE);
    MoveWindow(exif_list,exif_scale(16),exif_scale(130),max(0,width-exif_scale(32)),body,TRUE);
    MoveWindow(exif_summary,exif_scale(2),exif_scale(130),max(0,width-exif_scale(4)),body,TRUE);
    MoveWindow(exif_count,exif_scale(20),height-exif_scale(62),text,exif_scale(22),TRUE);
    MoveWindow(exif_copy,exif_scale(20),height-exif_scale(36),text,exif_scale(28),TRUE);
    ListView_SetColumnWidth(exif_list,0,text/2);
    ListView_SetColumnWidth(exif_list,1,text/2);
}

static BOOL exif_refresh_fonts(void)
{
    HFONT font,heading,old_font=exif_font,old_heading=exif_heading_font;
    HWND children[]={exif_close,exif_tabs,exif_summary,exif_list,exif_file,exif_count,exif_copy};
    int i;
    if (exif_font && exif_font_dpi==os_logical_high)return FALSE;
    font=fyto_caption_create_font(os_logical_high);
    heading=CreateFontW(-MulDiv(12,os_logical_high,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    if (!font || !heading)
    {
        if (font)DeleteObject(font);
        if (heading)DeleteObject(heading);
        return FALSE;
    }

    exif_font=font;
    exif_heading_font=heading;
    exif_font_dpi=os_logical_high;
    SendMessageW(exif_heading,WM_SETFONT,(WPARAM)heading,TRUE);
    for (i=0; i<ARRAYSIZE(children); i++)
        SendMessageW(children[i],WM_SETFONT,(WPARAM)font,TRUE);
    if (old_font)DeleteObject(old_font);
    if (old_heading)DeleteObject(old_heading);
    return TRUE;
}

static LRESULT CALLBACK exif_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){switch(msg){case WM_CREATE:{LV_COLUMNW col;exif_heading=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,20,18,220,28,hwnd,(HMENU)2007,os_hinstance,NULL);exif_close=CreateWindowExW(0,L"BUTTON",L"×",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,260,12,32,32,hwnd,(HMENU)2008,os_hinstance,NULL);{TCITEMW tab;exif_tabs=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD|WS_TABSTOP,8,36,520,26,hwnd,(HMENU)2006,os_hinstance,NULL);ZeroMemory(&tab,sizeof(tab));tab.mask=TCIF_TEXT;tab.pszText=L"Overview";SendMessageW(exif_tabs,TCM_INSERTITEMW,0,(LPARAM)&tab);tab.pszText=L"All EXIF";SendMessageW(exif_tabs,TCM_INSERTITEMW,1,(LPARAM)&tab);if(!exif_richedit)exif_richedit=LoadLibraryW(L"msftedit.dll");exif_summary=CreateWindowExW(0,exif_richedit?L"RICHEDIT50W":L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,8,70,520,360,hwnd,(HMENU)2005,os_hinstance,NULL);SendMessageW(exif_summary,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(18,18));if(exif_richedit)SendMessageW(exif_summary,EM_SETBKGNDCOLOR,0,exif_bg);}exif_file=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_PATHELLIPSIS,8,8,400,22,hwnd,(HMENU)2001,os_hinstance,NULL);exif_list=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SHOWSELALWAYS,8,36,520,400,hwnd,(HMENU)2002,os_hinstance,NULL);ListView_SetExtendedListViewStyle(exif_list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);ZeroMemory(&col,sizeof(col));col.mask=LVCF_TEXT|LVCF_WIDTH;col.cx=280;col.pszText=L"Field";SendMessageW(exif_list,LVM_INSERTCOLUMNW,0,(LPARAM)&col);col.cx=280;col.pszText=L"Value";SendMessageW(exif_list,LVM_INSERTCOLUMNW,1,(LPARAM)&col);exif_count=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,8,440,400,22,hwnd,(HMENU)2003,os_hinstance,NULL);exif_copy=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,430,438,96,26,hwnd,(HMENU)2004,os_hinstance,NULL);{HWND header=ListView_GetHeader(exif_list);exif_old_header_proc=(WNDPROC)SetWindowLongPtrW(header,GWLP_WNDPROC,(LONG_PTR)exif_header_proc);}exif_overview_button=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,16,90,140,32,hwnd,(HMENU)2011,os_hinstance,NULL);exif_detail_button=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,160,90,140,32,hwnd,(HMENU)2012,os_hinstance,NULL);return 0;}
 case WM_SIZE:exif_resize_controls(LOWORD(lp),HIWORD(lp));return 0;
 case WM_CTLCOLOREDIT:
 case WM_CTLCOLORSTATIC:SetTextColor((HDC)wp,ui_text(exif_bg));SetBkMode((HDC)wp,TRANSPARENT);return (LRESULT)exif_brush;
 case WM_ERASEBKGND:{RECT r;GetClientRect(hwnd,&r);FillRect((HDC)wp,&r,exif_brush);return 1;}
 case WM_GETMINMAXINFO:((MINMAXINFO *)lp)->ptMinTrackSize.x=440;((MINMAXINFO *)lp)->ptMinTrackSize.y=240;return 0;
 case WM_COMMAND:if(LOWORD(wp)==2011 || LOWORD(wp)==2012){exif_details=LOWORD(wp)==2012;TabCtrl_SetCurSel(exif_tabs,exif_details);show_exif_page();InvalidateRect(exif_overview_button,NULL,TRUE);InvalidateRect(exif_detail_button,NULL,TRUE);return 0;}if(LOWORD(wp)==2008){SendMessageW(GetParent(hwnd),WM_COMMAND,VIV_ID_VIEW_EXIF,0);return 0;}if(LOWORD(wp)==2004){copy_all();return 0;}break;
 case WM_DRAWITEM:{DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT *)lp;wchar_t label[80];GetWindowTextW(d->hwndItem,label,80);ui_fill(d->hDC,&d->rcItem,exif_bg);ui_button(d->hDC,d->rcItem,exif_bg,label,(d->CtlID==2011 && !exif_details)||(d->CtlID==2012 && exif_details),(d->itemState&ODS_SELECTED)!=0,1,exif_font);return TRUE;}
 case WM_NOTIFY:if(((NMHDR *)lp)->hwndFrom==exif_tabs && ((NMHDR *)lp)->code==TCN_SELCHANGE){exif_details=TabCtrl_GetCurSel(exif_tabs);show_exif_page();return 0;}if(((NMHDR *)lp)->code==LVN_KEYDOWN){NMLVKEYDOWN *key=(NMLVKEYDOWN *)lp;if(key->wVKey=='C' && GetKeyState(VK_CONTROL)<0)copy_all();else if(key->wVKey==VK_ESCAPE)SendMessageW(hwnd,WM_CLOSE,0,0);}break;
 case WM_CLOSE:SendMessageW(GetParent(hwnd),WM_COMMAND,VIV_ID_VIEW_EXIF,0);return 0;
 case WM_DESTROY:if(exif_font)DeleteObject(exif_font);if(exif_heading_font)DeleteObject(exif_heading_font);exif_font=exif_heading_font=NULL;exif_heading=exif_close=NULL;exif_summary=exif_tabs=NULL;exif_hwnd=exif_list=exif_file=exif_count=exif_copy=NULL;return 0;
 }return DefWindowProcW(hwnd,msg,wp,lp);}
int exif_sidebar_width(HWND owner)
{
    RECT r;
    if (!config_show_exif)return 0;
    GetClientRect(owner,&r);
    return min(exif_scale(340),(r.right-r.left)/2);
}
void exif_set_background(COLORREF color){int changed=color!=exif_bg;exif_bg=color;if(!exif_hwnd)return;if(!exif_brush || changed){HBRUSH old=exif_brush;exif_brush=CreateSolidBrush(color);if(old)DeleteObject(old);}if(exif_list){ListView_SetBkColor(exif_list,color);ListView_SetTextBkColor(exif_list,color);ListView_SetTextColor(exif_list,ui_text(color));}if(exif_summary && exif_richedit){CHARRANGE range;CHARFORMAT2W cf;int line=(int)SendMessageW(exif_summary,EM_GETFIRSTVISIBLELINE,0,0);SendMessageW(exif_summary,EM_EXGETSEL,0,(LPARAM)&range);ShowScrollBar(exif_summary,SB_VERT,FALSE);SendMessageW(exif_summary,EM_SETBKGNDCOLOR,0,color);ZeroMemory(&cf,sizeof(cf));cf.cbSize=sizeof(cf);cf.dwMask=CFM_COLOR;cf.crTextColor=ui_text(color);SendMessageW(exif_summary,EM_SETSEL,0,-1);SendMessageW(exif_summary,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&cf);SendMessageW(exif_summary,EM_EXSETSEL,0,(LPARAM)&range);SendMessageW(exif_summary,EM_LINESCROLL,0,line-(int)SendMessageW(exif_summary,EM_GETFIRSTVISIBLELINE,0,0));}if(changed)RedrawWindow(exif_hwnd,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);}
void exif_layout(HWND owner,int top,int bottom)
{
    RECT r;
    int width,height;
    BOOL font_changed;
    exif_top=top;
    exif_bottom=bottom;
    if (!exif_hwnd)return;
    if (!config_show_exif)
    {
        ShowWindow(exif_hwnd,SW_HIDE);
        return;
    }
    GetClientRect(owner,&r);
    width=exif_sidebar_width(owner);
    height=max(1,r.bottom-top-bottom);
    font_changed=exif_refresh_fonts();
    SetWindowPos(exif_hwnd,HWND_TOP,r.right-width,top,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    if (font_changed)exif_resize_controls(width,height);
}
void exif_sync(HWND owner,const wchar_t *filename,int force){if(!config_show_exif){if(exif_hwnd)ShowWindow(exif_hwnd,SW_HIDE);return;}if(!exif_hwnd){WNDCLASSW wc;ZeroMemory(&wc,sizeof(wc));wc.hInstance=os_hinstance;wc.lpfnWndProc=exif_proc;wc.lpszClassName=L"fyto.Exif";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!exif_brush)exif_brush=CreateSolidBrush(exif_bg);wc.hbrBackground=NULL;RegisterClassW(&wc);exif_hwnd=CreateWindowExW(0,wc.lpszClassName,L"",WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,340,580,owner,(HMENU)2010,os_hinstance,NULL);if(!exif_hwnd)return;force=1;}
 if(force || exif_last_language!=config_language || lstrcmpW(exif_last_path,filename?filename:L"")){load_metadata(filename);wcsncpy_s(exif_last_path,32768,filename?filename:L"",_TRUNCATE);exif_last_language=config_language;}exif_set_background(exif_bg);exif_layout(owner,exif_top,exif_bottom);
}
void exif_shutdown(void){if(exif_hwnd)DestroyWindow(exif_hwnd);if(exif_brush){DeleteObject(exif_brush);exif_brush=NULL;}if(exif_richedit){FreeLibrary(exif_richedit);exif_richedit=NULL;}}
#pragma float_control(pop)
