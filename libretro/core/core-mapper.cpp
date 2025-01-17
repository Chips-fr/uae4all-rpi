/* 
 * Uae4all libretro core input implementation
 * (c) Chips 2021
 */

#include "libretro.h"
#include "libretro-core.h"
#include "retroscreen.h"
#include "graph.h"

#include "uae.h"
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"


//FIXME
extern int uae4all_keystate[256];
extern void changedisk( bool );


//TIME
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>


extern void virtual_kdb (char *buffer,int vx,int vy);
extern int  check_vkey2 (int x,int y);

//VIDEO
extern char *gfx_mem;

//EMU FLAGS
int NPAGE=-1, KCOL=1, BKGCOLOR=0;
int SHOWKEY=-1;

int MOUSE_EMULATED=-1;

int SHIFTON=-1,PAS=3;
int pauseg=0; //enter_gui



//MOUSE
int gmx,gmy; //gui mouse
int mouse_wu=0,mouse_wd=0;
//KEYBOARD
char Key_State[512];
static char old_Key_State[512];

static int mbt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//STATS GUI
int BOXDEC= 32+2;
int STAT_BASEY;

static retro_input_state_t input_state_cb;
static retro_input_poll_t input_poll_cb;

void retro_set_input_state(retro_input_state_t cb)
{
    input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
    input_poll_cb = cb;
}


long GetTicks(void)
{ // in MSec
#ifndef _ANDROID_
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (tv.tv_sec*1000000 + tv.tv_usec);///1000;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec*1000000 + now.tv_nsec/1000);///1000;
#endif

} 



// Joystick management


int second_joystick_enable = 0;

void read_joystick(int nr, unsigned int *dir, int *button)
{
    int up    = input_state_cb(nr, RETRO_DEVICE_JOYPAD, 0,RETRO_DEVICE_ID_JOYPAD_UP);
    int down  = input_state_cb(nr, RETRO_DEVICE_JOYPAD, 0,RETRO_DEVICE_ID_JOYPAD_DOWN);
    int left  = input_state_cb(nr, RETRO_DEVICE_JOYPAD, 0,RETRO_DEVICE_ID_JOYPAD_LEFT);
    int right = input_state_cb(nr, RETRO_DEVICE_JOYPAD, 0,RETRO_DEVICE_ID_JOYPAD_RIGHT);
    int b1    = input_state_cb(nr, RETRO_DEVICE_JOYPAD, 0,RETRO_DEVICE_ID_JOYPAD_B);
    int b2    = input_state_cb(nr, RETRO_DEVICE_JOYPAD, 0,RETRO_DEVICE_ID_JOYPAD_A);

    *dir = 0;
    *button = 0;

    if ((nr == 0) && (SHOWKEY==1) || (pauseg==1))
        return;

    if ((nr == 1) && (!second_joystick_enable))
        return;

    if (left) up = !up;
    if (right) down = !down;
    *dir = down | (right << 1) | (up << 8) | (left << 9);
    *button = b2 << 1 | b1;

}

void init_joystick(void)
{
}

void close_joystick(void)
{
}




int STATUTON=-1;
#define RETRO_DEVICE_AMIGA_KEYBOARD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0)
#define RETRO_DEVICE_AMIGA_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)



void texture_uninit(void)
{

}

void texture_init(void)
{
    initsmfont();
    initmfont();

    memset(old_Key_State,0, sizeof(old_Key_State));
    memset(uae4all_keystate ,0, sizeof(uae4all_keystate));

    gmx=(retrow/2)-1;
    gmy=(retroh/2)-1;
}



extern unsigned amiga_devices[ 2 ];

extern void vkbd_key(int key,int pressed);

//#include "target.h"
#include "keyboard.h"
#include "keybuf.h"
#include "libretro-keymap.h"

typedef struct {
    char norml[NLETT];
    char shift[NLETT];
    int val;	
    int box;
    int color;
} Mvk;

extern Mvk MVk[NPLGN*NLIGN*2];

void retro_key_down(int key)
{
    int iAmigaKeyCode = keyboard_translation[key];

    if (iAmigaKeyCode >= 0)
        if (!uae4all_keystate[iAmigaKeyCode])
        {
            uae4all_keystate[iAmigaKeyCode] = 1;
            record_key(iAmigaKeyCode << 1);
        }	
}

void retro_key_up(int key)
{
    int iAmigaKeyCode = keyboard_translation[key];

    if (iAmigaKeyCode >= 0)
    {
        uae4all_keystate[iAmigaKeyCode] = 0;
        record_key((iAmigaKeyCode << 1) | 1);
    }
}


void vkbd_key(int key,int pressed)
{
    int key2=key;

    if(pressed){
        if(SHIFTON==1){
            uae4all_keystate[AK_LSH] = 1;
            record_key((AK_LSH << 1));
        }
        uae4all_keystate[key2] = 1;
        record_key(key2 << 1);
    }
    else {

        if(SHIFTON==1){
            uae4all_keystate[AK_LSH] = 0;
            record_key((AK_LSH << 1) | 1);
        }	
        uae4all_keystate[key2] = 0;
        record_key((key2 << 1) | 1);
    }
}


void retro_virtualkb(void)
{
    int i;
    //   RETRO        B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
    //   INDEX        0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
    static int oldi=-1;
    static int vkx=0,vky=0;

    int page= (NPAGE==-1) ? 0 : NPLGN*NLIGN;

    if(oldi!=-1)
    {
       vkbd_key(oldi,0);
       oldi=-1;
    }

    if(SHOWKEY==1)
    {
        static int vkflag[5]={0,0,0,0,0};		

        if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) && vkflag[0]==0 )
            vkflag[0]=1;
        else if (vkflag[0]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) )
        {
            vkflag[0]=0;
            vky -= 1; 
            if(vky<0)
                vky=NLIGN-1;

            while(MVk[(vky*NPLGN)+vkx+page].box==0){
                vkx -= 1;
                if(vkx<0)
                    vkx=NPLGN-1;
            }
        }

        if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) && vkflag[1]==0 )
            vkflag[1]=1;
        else if (vkflag[1]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) )
        {
             vkflag[1]=0;
             vky += 1; 
             if(vky>NLIGN-1)
                 vky=0;

             while(MVk[(vky*NPLGN)+vkx+page].box==0){
                 vkx -= 1;
                 if(vkx<0)
                     vkx=NPLGN-1;
             }
        }

        if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) && vkflag[2]==0 )
           vkflag[2]=1;
        else if (vkflag[2]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) )
        {
            vkflag[2]=0;
            vkx -= 1;
            if(vkx<0)
                vkx=NPLGN-1;

            while(MVk[(vky*NPLGN)+vkx+page].box==0){
                vkx -= 1;
                if(vkx<0)
                    vkx=NPLGN-1;
            }
        }

        if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) && vkflag[3]==0 )
            vkflag[3]=1;
        else if (vkflag[3]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) )
        {
            vkflag[3]=0;
            vkx += 1;
	    if(vkx>NPLGN-1)
                vkx=0;

            while(MVk[(vky*NPLGN)+vkx+page].box==0){
                vkx += 1;
                if(vkx>NPLGN-1)
                    vkx=0;
            }

        }

        if(vkx<0)vkx=NPLGN-1;
        if(vkx>NPLGN-1)vkx=0;
        if(vky<0)vky=NLIGN-1;
        if(vky>NLIGN-1)vky=0;

        virtual_kdb(( char *)gfx_mem,vkx,vky);
 
        i=RETRO_DEVICE_ID_JOYPAD_A;
        if(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i)  && vkflag[4]==0) 	
            vkflag[4]=1;
        else if( !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i)  && vkflag[4]==1)
        {
            vkflag[4]=0;
            i=check_vkey2(vkx,vky);

            if(i==-1){
                oldi=-1;
	    }
            if(i==-2)
            {
                // SWAP PAGE
                NPAGE=-NPAGE;oldi=-1;
            }
            else if(i==-3)
            {
                //KDB bgcolor
                KCOL=-KCOL;
                oldi=-1;
            }
            else if(i==-4)
            {
                //VKBD show/hide 			
                oldi=-1;
                SHOWKEY=-SHOWKEY;
            }
            else if(i==-5)
            {
                oldi=-1;
            }
            else if(i==-6)
            {
                //Exit
                retro_deinit();
                oldi=-1;
                exit(0);
            }
            else if(i==-7)
            {
                oldi=-1;
            }
            else if(i==-8)
            {
                oldi=-1;
            }
            else
            {
                if(i==AK_LSH) //SHIFT
                {
                    SHIFTON=-SHIFTON;
                    oldi=-1;
                }
                else if(i==0x27) //CTRL
                {     
                    oldi=-1;
                }
                else if(i==-12) 
                {
                    oldi=-1;
                }
                else if(i==-13) //GUI. Not implemented...
                {     
                    //pauseg=1;
                   oldi=-1;
                }
                else if(i==-14) //JOY PORT TOGGLE
                {    
                    SHOWKEY=-SHOWKEY;
                    oldi=-1;
                }
                else
                {
                    oldi=i;
                    vkbd_key(oldi,1);            
                }
            }
        }
    }
}


void Process_keyboard()
{
    int i;

    for(i=0;i<320;i++)
        Key_State[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i) ? 0x80: 0;
   
    if(memcmp( Key_State,old_Key_State , sizeof(Key_State) ) )
        for(i=0;i<320;i++)
            if(Key_State[i] && Key_State[i]!=old_Key_State[i]  )
            {	
                if(i==RETROK_F12){					
                    continue;
                }
                retro_key_down(i);
	
            }	
            else if ( !Key_State[i] && Key_State[i]!=old_Key_State[i]  )
            {
                if(i==RETROK_F12){
                    continue;
                }
                retro_key_up(i);
            }	

    memcpy(old_Key_State,Key_State , sizeof(Key_State) );
}

/*
Joy
L2  LEFT MOUSE BUTTON
R2  RIGHT MOUSE BUTTON
L   PREVIOUS DISK
R   NEXT DISK
SEL MOUSE/JOY SELECT
STR VKBD ON/OFF
A   FIRE1/VKBD KEY
B   FIRE2
*/
extern int lastmx, lastmy, newmousecounters;
extern int buttonstate[3];

// Handle non joystick part (keyboard, mouse, emulated mouse...)

int Retro_PollEvent()
{
    //    RETRO          B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
    //    INDEX          0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15

    static char vbt[16]={0x10,0x00,0x00,0x00,0x01,0x02,0x04,0x08,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

    int i;

    input_poll_cb();

    int mouse_l;
    int mouse_r;
    int16_t rmouse_x,rmouse_y;
    rmouse_x=rmouse_y=0;

    if(SHOWKEY==-1 && pauseg==0)
    { 
        // if emulation running

        Process_keyboard();

        if(second_joystick_enable)
        {
         
        }
        else
        {
            if (input_state_cb(1, RETRO_DEVICE_JOYPAD, 0,RETRO_DEVICE_ID_JOYPAD_B) ||
                input_state_cb(1, RETRO_DEVICE_JOYPAD, 0,RETRO_DEVICE_ID_JOYPAD_A))
            {
                LOGI("Switch to joystick mode for Port 0.\n");
                second_joystick_enable = 1;
            }
        }
    }
    else // if pauseg=0
    {
        // In gui
    }


    i=RETRO_DEVICE_ID_JOYPAD_SELECT;     //show vkbd toggle
    if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
    {
        mbt[i]=1;
    }
    else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
    {
        mbt[i]=0;
        SHOWKEY=-SHOWKEY;
    }

    i=RETRO_DEVICE_ID_JOYPAD_START;     //mouse/joy toggle
    if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
        mbt[i]=1;
    else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) ){
        mbt[i]=0;
        MOUSE_EMULATED=-MOUSE_EMULATED;
        if (MOUSE_EMULATED==1)
        {
            LOGI("Switch to mouse emulation.\n");
            second_joystick_enable = 0;   // disable 2nd joystick if mouse activated...
        }
        else
            LOGI("Switch-off mouse emulation.\n");
    }

    i=RETRO_DEVICE_ID_JOYPAD_L;     //select previous disk
    if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
        mbt[i]=1;
    else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
    {
        mbt[i]=0;
        changedisk((bool) false);
    }

    i=RETRO_DEVICE_ID_JOYPAD_R;     //select next disk
    if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
        mbt[i]=1;
    else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
    {
        mbt[i]=0;
        changedisk((bool) true);
    }

    if(MOUSE_EMULATED==1 && SHOWKEY==-1 ){

        if(pauseg!=0 )return 1;

        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))rmouse_x += PAS;
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT ))rmouse_x -= PAS;
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN ))rmouse_y += PAS;
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP   ))rmouse_y -= PAS;
        mouse_l=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
        mouse_r=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
    }
    else {

        mouse_wu = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
        mouse_wd = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
        rmouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
        rmouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
        mouse_l  = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
        mouse_r  = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
    }

    // Emulate mouse as PUAE default configuration...
    int analog_deadzone=0;
    unsigned int opt_analogmouse_deadzone = 20; // hardcoded deadzone...
    analog_deadzone = (opt_analogmouse_deadzone * 32768 / 100);
    int analog_right[2]={0};
    analog_right[0] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X));
    analog_right[1] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y));

    if (abs(analog_right[0]) > analog_deadzone)
        rmouse_x += analog_right[0] * 10 *  0.7 / (32768 );

    if (abs(analog_right[1]) > analog_deadzone)
        rmouse_y += analog_right[1] * 10 *  0.7 / (32768 );

    // L2 & R2 as mouse button...
    mouse_l    |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2);
    mouse_r    |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2);

    if ((mouse_l || mouse_r) && (second_joystick_enable))
    {
        mouse_l = mouse_r = 0;
        LOGI("Switch to mouse mode for Port 0.\n");
        second_joystick_enable = 0;   // disable 2nd joystick if mouse activated...
    }

    static int mmbL=0,mmbR=0;

    if(mmbL==0 && mouse_l){
        mmbL=1;
    }
    else if(mmbL==1 && !mouse_l) {
        mmbL=0;
    }

    if(mmbR==0 && mouse_r){
        mmbR=1;
    }
    else if(mmbR==1 && !mouse_r) {
        mmbR=0;
    }

    gmx+=rmouse_x;
    gmy+=rmouse_y;
    if(gmx<0)gmx=0;
    if(gmx>retrow-1)gmx=retrow-1;
    if(gmy<0)gmy=0;
    if(gmy>retroh-1)gmy=retroh-1;

    // Update emulator mouse variables
    lastmx +=rmouse_x;
    lastmy +=rmouse_y;
    newmousecounters=1;

    if ((!second_joystick_enable) && (pauseg==0))
    {
        buttonstate[0] = mmbL;
        buttonstate[2] = mmbR;
    }
   
    return 1;
}

