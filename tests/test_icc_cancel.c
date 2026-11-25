#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../voidImageViewer/src/icc_transform.h"
typedef struct {volatile LONG calls;LONG after;} stop_t;
static BOOL stopped(void *p){stop_t *s=p;return InterlockedIncrement(&s->calls)>=s->after && s->after>0;}
static BYTE encoded(int value){
    double linear=value/255.0;
    double out=linear<=0.0031308?12.92*linear:1.055*pow(linear,1.0/2.4)-0.055;
    return (BYTE)(out*255+0.5);
}
static void fill(BYTE *p,UINT count){UINT i;for(i=0;i<count;i++){p[4*i]=192;p[4*i+1]=128;p[4*i+2]=64;p[4*i+3]=127;}}
int main(void){
    const UINT width=1024,height=1024,count=width*height;BYTE *pixels=malloc((SIZE_T)count*4);
    fyto_icc_t table={0};stop_t stop={0};DWORD before,after;int i,j;
    SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
    assert(pixels);table.state=FYTO_ICC_MATRIX;
    for(i=0;i<3;i++)for(j=0;j<256;j++)table.decode[i][j]=j/255.0f;
    table.matrix[0]=table.matrix[4]=table.matrix[8]=1;
    fyto_icc_build_encode(&table);fill(pixels,count);
    assert(fyto_icc_apply_cancellable(pixels,width,height,width*4,32,&table,NULL,NULL));
    for(i=0;i<count;i+=37){
        assert(abs(pixels[4*i]-(int)encoded(192))<=1);
        assert(abs(pixels[4*i+1]-(int)encoded(128))<=1);
        assert(abs(pixels[4*i+2]-(int)encoded(64))<=1);
        assert(pixels[4*i+3]==127);
    }
    stop.after=1;stop.calls=0;fill(pixels,count);
    assert(!fyto_icc_apply_cancellable(pixels,width,height,width*4,32,&table,stopped,&stop));
    assert(pixels[0]==192&&pixels[1]==128&&pixels[2]==64);
    stop.after=10;stop.calls=0;
    assert(!fyto_icc_apply_cancellable(pixels,width,height,width*4,32,&table,stopped,&stop)&&stop.calls>=10);
    // Warm the shared Windows pool before checking repeated slice/event cleanup.
    for(i=0;i<32;i++){fill(pixels,count);assert(fyto_icc_apply_cancellable(pixels,width,height,width*4,32,&table,NULL,NULL));}
    assert(GetProcessHandleCount(GetCurrentProcess(),&before));
    for(i=0;i<64;i++){stop.calls=0;assert(!fyto_icc_apply_cancellable(pixels,width,height,width*4,32,&table,stopped,&stop));}
    assert(GetProcessHandleCount(GetCurrentProcess(),&after));assert(after==before);
    free(pixels);puts("ICC linear-RGB reference, alpha preservation, pre/mid conversion cancellation and pool/event lifetime checks passed");
    return 0;
}

