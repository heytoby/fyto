#ifndef FYTO_HOVER_ANIMATION_H
#define FYTO_HOVER_ANIMATION_H
typedef struct {BYTE from,to;DWORD started;unsigned duration;} fyto_hover_fade_t;
static BYTE fyto_hover_fade_alpha(const fyto_hover_fade_t *fade,DWORD now)
{
    DWORD elapsed=now-fade->started; /* Unsigned subtraction also handles tick rollover. */
    int step,eased;
    if(!fade->duration || elapsed>=fade->duration)return fade->to;
    step=(int)(elapsed*255/fade->duration);
    eased=step*step*(765-2*step)/(255*255);
    return (BYTE)(fade->from+((int)fade->to-fade->from)*eased/255);
}
static void fyto_hover_fade_to(fyto_hover_fade_t *fade,BYTE target,DWORD now)
{
    BYTE current=fyto_hover_fade_alpha(fade,now);
    fade->from=current;fade->to=target;fade->started=now;
    fade->duration=current==target?0:target?140:180;
}
#endif
