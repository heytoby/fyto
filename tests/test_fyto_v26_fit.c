#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../voidImageViewer/src/image_fit.h"
int main(void){
 int areas[][2]={{2560,1392},{1080,1872},{1024,720},{1920,1040},{2160,3760},{1080,1080},{1280,720},{800,600}};
 int photos[][2]={{8000,4000},{4000,8000},{6000,4000},{4000,6000},{10000,10000},{3840,2160},{32,32},{6000,1},{1,6000},{1,1}};
 int chrome[]={0,68,102,136},sidebar[]={0,340,510,680};int a,p,h,b,e,cases=0;fyto_image_fit_t f;
 for(a=0;a<8;a++)for(p=0;p<10;p++)for(h=0;h<4;h++)for(b=0;b<4;b++)for(e=0;e<2;e++){
  long long difference;
  assert(fyto_image_fit(areas[a][0],areas[a][1],photos[p][0],photos[p][1],chrome[h],e,sidebar[b],&f));
  assert(f.window_width<=areas[a][0] && f.window_height<=areas[a][1]);
  assert(f.image_width>=1 && f.image_height>=1 && f.image_width<=photos[p][0] && f.image_height<=photos[p][1]);
  assert(f.window_width==f.image_width+f.sidebar && f.window_height==f.image_height+chrome[h]);
  difference=(long long)f.image_width*photos[p][1]-(long long)f.image_height*photos[p][0];if(difference<0)difference=-difference;
  assert(difference<=(photos[p][0]>photos[p][1]?photos[p][0]:photos[p][1]));cases++;
 }
 assert(!fyto_image_fit(0,100,100,100,0,0,0,&f));assert(!fyto_image_fit(100,68,100,100,68,0,0,&f));
 assert(fyto_fixed_tile(0,0,1280,1392,0,0,2560,1392,12));
 assert(fyto_fixed_tile(2560,252,3100,1188,2560,252,3640,2124,12));
 assert(fyto_fixed_tile(0,0,1706,1392,0,0,2560,1392,12));
 assert(!fyto_fixed_tile(80,80,1040,780,0,0,2560,1392,12));
 printf("image-fit matrix passed: %d cases plus fixed-tile and invalid-bound checks\n",cases);return 0;
}
