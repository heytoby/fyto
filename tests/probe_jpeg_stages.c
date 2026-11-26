#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "../voidImageViewer/src/fast_jpeg.h"
#include "../voidImageViewer/src/file_stream.h"
static double stamp(void){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return t.QuadPart*1000.0/f.QuadPart;}
int wmain(int argc,wchar_t **argv){
 int i,j;CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 for(i=1;i<argc;i++)for(j=0;j<3;j++){
  HANDLE file=CreateFileW(argv[i],GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
  IStream *stream;fyto_wic_image jpeg={0};HBITMAP bitmap;UINT w,h;double a,b,d,e,raw;
  if(file==INVALID_HANDLE_VALUE)return 2;
  stream=fyto_file_stream_take(file,NULL,NULL);if(!stream)return 3;
  a=stamp();
  bitmap=fyto_jpeg_decode_stream(stream,6000,4000,4,32,&w,&h,NULL,NULL);
  b=stamp();raw=b-a;if(!bitmap)return 4;DeleteObject(bitmap);
  {LARGE_INTEGER zero={0};stream->lpVtbl->Seek(stream,zero,STREAM_SEEK_SET,NULL);}
  a=b;
  fyto_jpeg_set_input(&jpeg,stream,NULL,NULL);
  if(!fyto_jpeg_open(&jpeg,stream,TRUE))return 5;
  b=stamp();
  fyto_jpeg_orientation(&jpeg);d=stamp();
  bitmap=fyto_jpeg_decode_image(&jpeg,6000,4000,4,32,&w,&h,NULL,NULL);
  e=stamp();if(!bitmap)return 6;DeleteObject(bitmap);
  printf("JPEG_STAGE file=%ls round=%d metadata_ms=%.4f orientation_ms=%.4f turbo_decode_ms=%.4f raw_decode_ms=%.4f\n",argv[i],j,b-a,d-b,e-d,raw);
  fyto_wic_close(&jpeg);stream->lpVtbl->Release(stream);
 }
 CoUninitialize();return 0;
}
