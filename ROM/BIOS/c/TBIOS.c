#include <stdint.h>
#include "PAWS.h"
#include "PAWS_BIOS_LIBRARY.h"

void draw_paws_logo( void ) {
    set_blitter_bitmap( 3, &PAWSLOGO[0] );
}

void reset_system( void ) {
    for( int channel = 0; channel < 6; channel++) beep_stop( channel );
    for( int i = 0; i < 6; i++ ) AUDIO_DURATION[ i ] = 0;

    set_background( BLACK, BLACK, BKG_SOLID );
    *GPU_DITHERMODE = 0;
    *CROP_LEFT = 0; *CROP_RIGHT = 319; *CROP_TOP = 0; *CROP_BOTTOM = 239;
    *FRAMEBUFFER_DRAW = 3; gpu_cs(); while( !*GPU_FINISHED );
    *FRAMEBUFFER_DRAW = 1; *BITMAP_DISPLAY256 = 0;
    *SCREENORDER = ( 3 << 0 ) | ( 1 << 4 ) | ( 4 << 8 ) | ( 8 << 12 ) | ( 9 << 16 ) | ( 10 << 20 ) | ( 11 << 24 );
    *COLOUR = 0;
    tpu_cs(); *TPU_LOREZ = 0; *TPU_CURSOR = 1;
    *TM_LOREZ = 0;
    for( int i = 0; i < 4; i++ ) {
        tm_cs( i );
    }
    for( int i = 0; i < 63; i++ ) SPRITE_ACTIVE[i] = 0;
}


extern int _bss_start, _bss_end;
void main( void ) {
    reset_system();

    copper_program(0,1,0,1,0);
    copper_program(1,0,7,1,0);
    copper_program(2,0,4,1,0);
    copper_program(3,1,4,0,2);
    copper_program(4,3,4,1,0x78);
    copper_program(5,0xa,4,0,4);
    copper_program(6,8,4,1,1);
    copper_program(7,1,1,0,4);
    copper_program(8,1,5,0,2);
    copper_program(9,0xc,2,0,0x205);
    copper_program(10,0,0,1,9);
    copper_program(11,1,6,0,0);
    copper_program(12,0xc,6,1,0x200);
    copper_program(13,0,0,1,3);
    copper_program(14,0,0,1,1);

    copper_startstop( 1 ); sleep1khz( 2000, 1 );
}
