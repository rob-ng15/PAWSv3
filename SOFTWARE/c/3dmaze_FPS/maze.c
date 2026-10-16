#include <stdlib.h>
#define min(a,b) \
({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

// INCLUDE TOMBSTONE IMAGE
#include "graphics/TOMBSTONE_BMP.h"

// INCLUDE CONTROLS IMAGE
#include "graphics/ULX3S_BMP.h"

// INCLUDE 3D PACMAN BACKDROP
#include "graphics/3DPACMAN_BMP.h"

#include <PAWSlibrary.h>

// MAZE SIZES
#define MAXWIDTH 40
#define MAXHEIGHT 30
#define MAXLEVEL 6

// DRAWING VARIABLES
#define MAXDEPTH 12

// CURRENT FRAMEBUFFER
unsigned char framebuffer = 1, drawsector[] = { 0b11111111, 0b01111110, 0b00111100, 0b00011000, 0b00000000 };

// LEVEL - DETERMINES SIZE OF MAZE
unsigned short level = 0;

// POWER STATUS
unsigned short powerstatus = 0, powerpills = 0;

// WIDTH OF MAZE DEPENDING ON LEVEL
unsigned short levelwidths[] = {  10, 16, 16, 20, 20, 40, 40 };
unsigned short levelheights[] = { 10, 10, 12, 12, 20, 20, 30 };

// TOP LEFT COORDINATES FOR THE PERSPECTIVE DRAWING
short perspectivex[] = { 0, 50, 85, 109, 125, 136, 144, 149, 152, 155, 157, 159, 160 };
short perspectivey[] = { 0, 37, 64,  82,  94, 102, 108, 112, 114, 116, 118, 119, 120 };

// DIRECTION STEPS IN X and Y
short directionx[] = { 0, 1, 0, -1 }, leftdirectionx[] = { -1, 0, 1, 0 }, rightdirectionx[] = { 1, 0, -1, 0 };
short directiony[] = { -1, 0, 1, 0 }, leftdirectiony[] = { 0, -1, 0, 1 }, rightdirectiony[] = { 0, 1, 0, -1 };

// STORAGE FOR MAZE and MAP PLUS ACCESS FUNCTIONS USING A BIT ARRAY
// WALLS WILL BE '#' '*' GAPS ' ' ENTRANCE 'E' EXIT 'X'
unsigned long maze[MAXHEIGHT];
unsigned long map[MAXHEIGHT];

// POSITION AND DIRECTION OF THE GHOST
unsigned short ghostx[4], ghosty[4], ghostdirection[4];
short ghost_substep[4] = {0, 0, 0, 0}; // Tracks ghost smooth movement
unsigned short ghosteyes[4][4] = { { 0, 1, 2, 3 }, { 3, 0, 1, 2 }, { 2, 3, 0, 1 }, { 1, 2, 3, 0 } };

// PACMAN TUNE - PLAYS WHEN ENTERING LEVEL 0
unsigned char tune_treble[] = {  24, 36, 31, 28, 36, 30, 24, 27,  0,
    36, 37, 32, 29, 37, 32, 25, 29,  0,
    24, 36, 31, 28, 36, 30, 24, 27,  0,
    28, 29, 30,  0, 30, 31, 32,  0, 32, 33, 34,  0, 36,  0, 0xff };
    unsigned short size_treble[] = { 16, 16, 16, 16,  8,  8, 16, 16, 16,
        16, 16, 16, 16,  8,  8, 16, 16, 16,
        16, 16, 16, 16,  8,  8, 16, 16, 16,
        8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8, 24,  8, 0xff };

        unsigned char tune_bass[] = {   12,  0,  0, 19, 12,  0,  0, 20,
            13,  0,  0, 20, 13,  0,  0, 19,
            12,  0,  0, 19, 12,  0,  0, 20,
            19,  0, 20,  0, 22,  0,  24, 0, 0xff };

            // INTERRUPT THREAD TO PLAY THE INTRO TUNE
            short trebleposition = 0, bassposition = 0;
            void __attribute__((interrupt ("machine"))) playtune( void ) {
                IRQ_ACK( IRQ_VBLANK );

                if( ( tune_treble[ trebleposition ] != 0xff ) || ( tune_bass[ bassposition ] != 0xff ) ) {
                    if( tune_treble[ trebleposition ] != 0xff ) {
                        if( !get_beep_active( ACTIVE_CHANNEL_LEFT_0 ) ) {
                            beep( CHANNEL_LEFT_0, WAVE_SINE, tune_treble[ trebleposition ] * 2 + 3, size_treble[ trebleposition ] << 3, 7 );
                            trebleposition++;
                        }
                    }
                    if( tune_bass[ bassposition ] != 0xff ) {
                        if( !get_beep_active( ACTIVE_CHANNEL_RIGHT_0 ) ) {
                            beep( CHANNEL_RIGHT_0, WAVE_SINE, tune_bass[ bassposition ] * 2 + 3, 16 << 3, 7 );
                            bassposition++;
                        }
                    }
                } else {
                    IRQ_OFF( IRQ_VBLANK, TRUE ); trebleposition = 0; bassposition = 0;
                }
            }

            // DRAW WELCOME SCREEN
            void drawwelcome( void ) {
                // DISPLAY ULX3 BITMAP
                gpu_pixelblock( 0, 10, 320, 219, TRANSPARENT, ulx3sbitmap );
                gpu_printf_centre( YELLOW, 160, 8, BOLD, 1, 0, "3D MONSTER MAZE" );

                // DRAW JOYSTICK AND LABEL
                gpu_printf_centre( YELLOW, 229, 102, BOLD, 0, 0, "STEP" );
                gpu_printf_centre( YELLOW, 242, 123, BOLD, 0, 0, "BACK" );
                gpu_printf_centre( YELLOW, 211, 135, BOLD, 0, 0, "LEFT" );
                gpu_printf_centre( YELLOW, 272, 109, BOLD, 0, 0, "RIGHT" );
                gpu_printf_centre( YELLOW, 98, 128, BOLD, 0, 0, "PEEK" );
                gpu_printf_centre( YELLOW, 66, 142, BOLD, 0, 0, "POWER" );
            }

            // RETURN CONTENTS OF == 0 MAZE == 1 MAP at ( X, Y )
            unsigned char whatisat( unsigned short x, unsigned short y, unsigned char mapmaze ) {
                if( ( x == 0 ) && ( y == 1 ) ) {
                    return( 'E' );
                }
                if( ( ( x == levelwidths[ level ] - 2 ) && ( y == levelheights[ level ] - 3 ) ) ) {
                    return( 'X' );
                }
                if( ( x < MAXWIDTH ) && ( y < MAXHEIGHT ) ) {
                    switch( mapmaze ) {
                        case 0:
                            return( _rv64_bext( maze[ y ], x ) ? '#' : ' ' );
                        case 1:
                            return( _rv64_bext( map[ y ], x ) ? '#' : ' ' );
                        default:
                            return( '?' );
                    }
                } else {
                    return( '#' );
                }
            }

            // SET THE CONTENTS OF == 0 MAZE == 1 MAP at ( X, Y )
            void setat( unsigned short x, unsigned short y, unsigned char value, unsigned char mapmaze ) {
                switch( mapmaze ) {
                    case 0:
                        switch( value ) {
                            case ' ':
                                maze[ y ] = _rv64_bclr( maze[ y ], x );
                                break;
                            case '#':
                                maze[ y ] = _rv64_bset( maze[ y ], x );
                                break;
                        }
                        break;
                            case 1:
                                switch( value ) {
                                    case ' ':
                                        map[ y ] = _rv64_bclr( map[ y ], x );
                                        break;
                                    case '#':
                                        map[ y ] = _rv64_bset( map[ y ], x );
                                        break;
                                }
                                break;
                }
            }

            // FIND WHAT IS TO THE FRONT AT STEPS IN FRONT
            unsigned char whatisfront( unsigned short currentx, unsigned short currenty, unsigned short direction, unsigned short steps ) {
                return( whatisat( currentx + directionx[ direction ] * steps, currenty + directiony[ direction ] * steps, 0 ) );
            }

            // FIND WHAT IS BEHIND AT STEPS IN REAR
            unsigned char whatisbehind( unsigned short currentx, unsigned short currenty, unsigned short direction, unsigned short steps ) {
                return( whatisat( currentx - directionx[ direction ] * steps, currenty - directiony[ direction ] * steps, 0 ) );
            }

            // FIND WHAT IS TO THE LEFT AT STEPS IN FRONT
            unsigned char whatisleft( unsigned short currentx, unsigned short currenty, unsigned short direction, unsigned short steps ) {
                return( whatisat( currentx + directionx[ direction ] * steps + leftdirectionx[ direction ], currenty + directiony[ direction ] * steps + leftdirectiony[ direction ], 0 ) );
            }

            // FIND WHAT IS TO THE RIGHT AT STEPS IN FRONT
            unsigned char whatisright( unsigned short currentx, unsigned short currenty, unsigned short direction, unsigned short steps ) {
                return( whatisat( currentx + directionx[ direction ] * steps + rightdirectionx[ direction ], currenty + directiony[ direction ] * steps + rightdirectiony[ direction ], 0 ) );
            }

            // PACMAN GHOSTS AS DRAWLISTS
            struct DrawList2D GHOSTBODY[] = {
                { DLRECT, CYAN, DITHERSOLID, { -60, -60 }, { 60, 60 }, },
                { DLCIRC, CYAN, DITHERSOLID, { 0, -60 }, { 60, 0b11000011 }, },
                { DLCIRC, CYAN, DITHERSOLID, { -40, 60 }, { 20, 0b00111100 }, },
                { DLCIRC, CYAN, DITHERSOLID, { 0, 60 }, { 20, 0b00111100 }, },
                { DLCIRC, CYAN, DITHERSOLID, { 40, 60 }, { 20, 0b00111100 }, },
            };

            struct DrawList2D GHOSTLEYE[] = {
                { DLCIRC, WHITE, DITHERSOLID, { -30, -30 }, { 25, 0xff }, },
                { DLCIRC, BLACK, DITHERSOLID, { -30, -30 }, { 12, 0xff }, },
            };

            struct DrawList2D GHOSTREYE[] = {
                { DLCIRC, WHITE, DITHERSOLID, { 30, -30 }, { 25, 0xff }, },
                { DLCIRC, BLACK, DITHERSOLID, { 30, -30 }, { 12, 0xff }, },
            };

            struct DrawList2D POWERGHOSTLEYE[] = {
                { DLLINE, 237, DITHERSOLID, { -28, -28 }, { -12, -12 }, { 2, 0 }, },
                { DLLINE, 237, DITHERSOLID, { -12, -28 }, { -28, -12 }, { 2, 0 }, }
            };

            struct DrawList2D POWERGHOSTREYE[] = {
                { DLLINE, 237, DITHERSOLID, { 12, -28 }, { 28, -12 }, { 2, 0 }, },
                { DLLINE, 237, DITHERSOLID, { 28, -28 }, { 12, -12 }, { 2, 0 }, }
            };

            struct DrawList2D POWERGHOSTMOUTH[] = {
                { DLARC, 237, DITHERSOLID, { -40, 20 }, { 10, 0b11000011 }, },
                { DLARC, 237, DITHERSOLID, { -20, 20 }, { 10, 0b00111100 }, },
                { DLARC, 237, DITHERSOLID, { 0, 20 }, { 10, 0b11000011 }, },
                { DLARC, 237, DITHERSOLID, { 20, 20 }, { 10, 0b00111100 }, },
                { DLARC, 237, DITHERSOLID, { 40, 20 }, { 10, 0b11000011 }, },
            };

            // SET GHOST BODY COLOUR
            unsigned char ghost_body_colour[] = { CYAN, PINK, ORANGE, RED };
            void setghostcolour( unsigned short ghost ) {
                for( int i = 0; i < 5; i++ ) {
                    GHOSTBODY[i].colour = powerstatus ? ( powerstatus > 40 ) ? BLUE : WHITE : ghost_body_colour[ ghost ];
                }
            }

            // SMOOTH PERSPECTIVE HELPERS
            short smooth_px(unsigned short steps, short substep) {
                if (steps == 0) return perspectivex[0];
                short diff = perspectivex[steps] - perspectivex[steps - 1];
                return perspectivex[steps] - ((diff * substep) >> 3);
            }

            short smooth_py(unsigned short steps, short substep) {
                if (steps == 0) return perspectivey[0];
                short diff = perspectivey[steps] - perspectivey[steps - 1];
                return perspectivey[steps] - ((diff * substep) >> 3);
            }

            // DRAW GHOST AT CORRECT DISTANCE WITH SMOOTH INTERPOLATION
            void draw_ghost( unsigned short steps, unsigned short ghostnumber, unsigned short playerdirection, short player_substep, short turn_offset ) {
                float precise_steps = (float)steps - (player_substep / 8.0f);

                // Find ghost's direction relative to player's camera (0 = away, 1 = right, 2 = towards, 3 = left)
                int rel_dir = (ghostdirection[ghostnumber] - playerdirection + 4) % 4;

                // Adjust distance if ghost is moving along the camera's Z-axis
                if (rel_dir == 0) precise_steps += (ghost_substep[ghostnumber] / 8.0f);
                else if (rel_dir == 2) precise_steps -= (ghost_substep[ghostnumber] / 8.0f);

                if (precise_steps < 0.0f) precise_steps = 0.0f;
                if (precise_steps >= 12.0f) precise_steps = 11.9f;

                // Interpolate the perspective array for smooth scaling
                int idx = (int)precise_steps;
                float frac = precise_steps - idx;
                float interpolated_px = perspectivex[idx] + frac * (perspectivex[idx+1] - perspectivex[idx]);

                float scale = (160.0f - interpolated_px) / 160.0f;

                // Calculate sideways sliding if moving left/right into a side-corridor
                float screen_x = 160.0f + turn_offset;
                float corridor_width = (160.0f - interpolated_px) * 2.0f;

                if (rel_dir == 1) screen_x += (ghost_substep[ghostnumber] / 8.0f) * corridor_width;
                else if (rel_dir == 3) screen_x -= (ghost_substep[ghostnumber] / 8.0f) * corridor_width;

                // MAIN BODY and HEAD
                setghostcolour( ghostnumber );
                DoDrawList2Dscale( GHOSTBODY, 5, (short)screen_x, 120, scale );

                // EYES - CROSSES IF POWER
                switch( ghosteyes[playerdirection][ghostdirection[ ghostnumber ]] ) {
                    case 0: break;
                    case 1:
                        switch( powerstatus ) {
                            case 0: DoDrawList2Dscale( GHOSTREYE, 2, (short)screen_x, 120, scale ); break;
                            default: DoDrawList2Dscale( POWERGHOSTREYE, 2, (short)screen_x, 120, scale );
                        }
                        break;
                            case 2:
                                switch( powerstatus ) {
                                    case 0:
                                        DoDrawList2Dscale( GHOSTREYE, 2, (short)screen_x, 120, scale );
                                        DoDrawList2Dscale( GHOSTLEYE, 2, (short)screen_x, 120, scale );
                                        break;
                                    default:
                                        DoDrawList2Dscale( POWERGHOSTREYE, 2, (short)screen_x, 120, scale );
                                        DoDrawList2Dscale( POWERGHOSTLEYE, 2, (short)screen_x, 120, scale );
                                        DoDrawList2Dscale( POWERGHOSTMOUTH, 5, (short)screen_x, 120, scale );
                                }
                                break;
                                    case 3:
                                        switch( powerstatus ) {
                                            case 0: DoDrawList2Dscale( GHOSTLEYE, 2, (short)screen_x, 120, scale ); break;
                                            default: DoDrawList2Dscale( POWERGHOSTLEYE, 2, (short)screen_x, 120, scale );
                                        }
                                        break;
                }
                gpu_dither( DITHEROFF );
            }

            // MOVE A GHOST RANDOMLY IN THE MAZE
            void move_ghost( unsigned short ghostnumber) {
                if( whatisfront( ghostx[ ghostnumber ], ghosty[ ghostnumber ], ghostdirection[ ghostnumber ], 1 ) == ' ' ) {
                    if( ( whatisleft( ghostx[ ghostnumber ], ghosty[ ghostnumber ], ghostdirection[ ghostnumber ], 0 ) == ' ' ) && ( rng( 4 ) == 0 ) ) {
                        ghostdirection[ ghostnumber ] = ( ghostdirection[ ghostnumber ] == 0 ) ? 3 : ghostdirection[ ghostnumber ] - 1;
                    } else {
                        if( ( whatisright( ghostx[ ghostnumber ], ghosty[ ghostnumber ], ghostdirection[ ghostnumber ], 0 ) == ' ' ) && ( rng( 4 ) == 0 ) ) {
                            ghostdirection[ ghostnumber ] = ( ghostdirection[ ghostnumber ] == 3 ) ? 0 : ghostdirection[ ghostnumber ] + 1;
                        } else {
                            switch( rng( 16 ) ) {
                                case 0: break;
                                case 1: break;
                                default:
                                    ghostx[ ghostnumber ] += directionx[ ghostdirection[ ghostnumber ] ];
                                    ghosty[ ghostnumber ] += directiony[ ghostdirection[ ghostnumber ] ];
                                    break;
                            }
                        }
                    }
                } else {
                    if( rng( 2 ) == 0 ) {
                        ghostdirection[ ghostnumber ] = ( ghostdirection[ ghostnumber ] == 0 ) ? 3 : ghostdirection[ ghostnumber ] - 1;
                    } else {
                        ghostdirection[ ghostnumber ] = ( ghostdirection[ ghostnumber ] == 3 ) ? 0 : ghostdirection[ ghostnumber ] + 1;
                    }
                }
            }

            // DRAW A PILL WITH SMOOTH INTERPOLATION AND TURN OFFSET
            void draw_pill( unsigned short steps, short substep, short turn_offset ) {
                short py = smooth_py(steps, substep);
                unsigned short pillsize = ( 160 - smooth_px(steps, substep) ) / 8;

                gpu_circle( WHITE, 160 + turn_offset, 240 - py, pillsize, 0xff, 1 );
                gpu_circle( GREY6, 160 + turn_offset, 240 - py, pillsize, 0xff, 0 );
            }

            // ADAPTED FROM https://weblog.jamisbuck.org/2011/2/3/maze-generation-sidewinder-algorithm.html#
            void generate_maze( unsigned short width, unsigned short height ) {
                unsigned short x, y;
                unsigned short lastx, count;

                for( x = 0; x < width; x++ ) {
                    for( y = 0; y < height; y++ ) {
                        setat( x, y, '#', 0 );
                        setat( x, y, '#', 1 );
                    }
                }

                for( y = 1; y < height - 1; y += 2 ) {
                    lastx = 1; count = 1;
                    tpu_set( 0, 59, TRANSPARENT, BLUE, TPU_NORMAL ); tpu_printf( TPU_NORMAL, "%2d", y );

                    for( x = 1; x < width - 1; x += 2 ) {
                        setat( x, y, ' ', 0 );
                        if( y > 1 ) {
                            if( x != width - 3 ) {
                                switch( rng( 3 ) ) {
                                    case 0:
                                    case 1:
                                        setat( x + 1, y, ' ', 0 );
                                        count++;
                                        break;
                                    case 2:
                                        if( count == 1 ) {
                                            setat( x, y - 1, ' ', 0 );
                                        } else {
                                            for( unsigned short connectors = 0; connectors < ( count >> 2 ) + 1; connectors++ ) {
                                                setat( lastx + rng( count ) * 2, y - 1, ' ', 0 );
                                            }
                                        }
                                        lastx = x + 2;
                                        count = 0;
                                        break;
                                }
                            } else {
                                if( count == 1 ) {
                                    setat( x, y - 1, ' ', 0 );
                                } else {
                                    for( unsigned short connectors = 0; connectors < ( count >> 2 ) + 1; connectors++ ) {
                                        setat( lastx + rng( count ) * 2, y - 1, ' ', 0 );
                                    }
                                }
                            }
                        } else {
                            if( x != width - 3 ) {
                                setat( x + 1, y, ' ', 0 );
                            }
                        }
                    }
                }

                unsigned short potentialx, potentialy;
                for( unsigned short ghost = 0; ghost < 4; ghost++ ) {
                    tpu_set( 0, 59, TRANSPARENT, RED, TPU_NORMAL ); tpu_printf( TPU_NORMAL, "%2d", ghost );
                    potentialx = width / 2; potentialy= height / 2;
                    if( ghost == 0 ) {
                        ghostx[ ghost ] = width - 3;
                        ghosty[ ghost ] = height - 3;
                        ghostdirection[ ghost ] = 3;
                    } else {
                        switch( rng( 2 ) ) {
                            case 0:
                                potentialx -= 2 * rng( level );
                                potentialy += rng( level );
                                break;
                            case 1:
                                potentialx += 2 * rng( level );
                                potentialy -= rng( level );
                                break;
                        }
                        if( whatisat( potentialx, potentialy, 0 ) != ' ' ) potentialx--;

                        if( ghost <= level ) {
                            ghostx[ ghost ] = potentialx;
                            ghosty[ ghost ] = potentialy;
                            ghostdirection[ ghost ] = ghost;
                        } else {
                            ghostx[ ghost ] = width + 1;
                            ghosty[ ghost ] = height - 3;
                            ghostdirection[ ghost ] = ghost;
                        }
                    }
                }
            }

            unsigned char ghostcolour( unsigned short ghost ) {
                return( ghost_body_colour[ghost] );
            }

            void draw_map( unsigned short width, unsigned short height, unsigned short currentx, unsigned short currenty, unsigned short direction, unsigned char mapmaze, unsigned short mappeeks )
            {
                short x, y, dox, doy;
                unsigned char colour;

                unsigned short boxwidth = 80 / width;
                unsigned short boxheight = 60 / height;

                gpu_rectangle( ORANGE, 225, 0, 319, 63 );
                gpu_rectangle( BLUE, 237, 2, 317, 61 );

                switch( mapmaze ) {
                    case 0:
                        for( x = 0; x < width; x++ ) {
                            for( y = 0; y < height; y++ ) {
                                switch( whatisat( x, y, 0 ) ) {
                                    case ' ': colour = WHITE; break;
                                    case 'E': colour = MAGENTA; break;
                                    case 'X': colour = YELLOW; break;
                                    default: colour = BLUE; break;
                                }
                                if( colour != BLUE )
                                    switch( boxwidth ) {
                                        case 1: gpu_pixel( colour, 237 + x, 2 + y ); break;
                                        default: gpu_rectangle( colour, 237 + x * boxwidth, 2 + y * boxheight, 236 + x * boxwidth + boxwidth, 1 + y * boxheight + boxheight );
                                    }
                            }
                        }
                        gpu_rectangle( GREEN, 237 + currentx * boxwidth, 2 + currenty * boxheight, 236 + currentx * boxwidth + boxwidth, 1 + currenty * boxheight + boxheight );
                        for( unsigned short ghost = 0; ghost < 4; ghost++ ) {
                            if( ghost <= level ) {
                                gpu_rectangle( ( powerstatus ? BLUE : ghostcolour( ghost ) ), 237 + ghostx[ ghost ] * boxwidth, 2 + ghosty[ ghost ] * boxheight, 236 + ghostx[ ghost ] * boxwidth + boxwidth, 1 + ghosty[ ghost ] * boxheight + boxheight );
                            }
                        }
                        break;

                                        case 1:
                                            for( x = 0; x < 15; x++ ) {
                                                for( y = 0; y < 12; y++ ) {
                                                    if( ( currentx - 8 ) < 0 ) dox = x;
                                                    else dox = currentx - 8 + x;

                                                    if( ( currenty - 5 ) < 0 ) doy = y;
                                                    else doy = currenty - 5 + y;

                                                    if( ( dox >=0 ) && ( dox < width ) && ( doy >= 0 ) && ( doy < height ) ) {
                                                        switch( whatisat( dox, doy, 1 ) ) {
                                                            case ' ': colour = WHITE; break;
                                                            case 'E': colour = MAGENTA; break;
                                                            case 'X': colour = YELLOW; break;
                                                            default: colour = BLUE; break;
                                                        }
                                                        gpu_rectangle( colour, 237 + x * 5, 2 + y * 5, 241 + x * 5, 6 + y * 5 );

                                                        if( ( dox == currentx ) && ( doy == currenty ) )
                                                            gpu_rectangle( GREEN, 237 + x * 5, 2 + y * 5, 241 + x * 5, 6 + y * 5 );

                                                        for( unsigned short ghost = 0; ghost < 4; ghost++ ) {
                                                            if( ( ghost <= level ) && ( ghostx[ ghost ] == dox ) && ( ghosty[ ghost ] == doy ) ) {
                                                                gpu_rectangle( ( powerstatus ? BLUE : ghostcolour( ghost ) ), 237 + x * 5, 2 + y * 5, 241 + x * 5, 6 + y * 5 );
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            break;
                }

                switch( direction ) {
                    case 0: gpu_character_blit( GREEN, 226, 1, 30, 0, 0 ); break;
                    case 1: gpu_character_blit( GREEN, 226, 1, 16, 0, 0 ); break;
                    case 2: gpu_character_blit( GREEN, 226, 1, 31, 0, 0 ); break;
                    case 3: gpu_character_blit( GREEN, 226, 1, 17, 0, 0 ); break;
                }

                for( unsigned peek = 0; peek < mappeeks; peek++ )
                    gpu_character_blit( GREEN, 226, 53 - ( peek * 6 ), 1, 0, 0 );

                for( unsigned power = 0; power < powerpills; power++ )
                    gpu_character_blit( DKPURPLE, 226, 29 - ( power * 6 ), 4, 0, 0 );
            }

            unsigned short counttowall( unsigned short currentx, unsigned short currenty, unsigned short direction )
            {
                unsigned short steps = 0, foundwall = 0;
                while( foundwall == 0 ) {
                    switch( whatisfront( currentx, currenty, direction, steps ) ) {
                        case '#': case 'E': case 'X': foundwall = 1; break;
                        default: steps++; break;
                    }
                }
                return( steps );
            }

            void left_wall( unsigned char colour, unsigned char colour_alt, short steps, short substep, short turn_offset )
            {
                switch( colour ) {
                    case YELLOW: gpu_dither( DITHERLSLOPE, colour_alt); break;
                    case MAGENTA: gpu_dither( DITHERLSLOPE, colour_alt); break;
                    default: gpu_dither( DITHERCHECK1, colour_alt); break;
                }
                short x1 = smooth_px(steps, substep) + turn_offset;
                short y1 = smooth_py(steps, substep);
                short x2 = smooth_px(steps + 1, substep) + turn_offset;
                short y2 = smooth_py(steps + 1, substep);

                gpu_quadrilateral( colour, x1, y1, x2, y2, x2, 240 - y2, x1, 240 - y1 );
                gpu_dither( DITHEROFF );
                gpu_line( colour, x1, y1, x2, y2 );
                gpu_line( colour, x2, 240 - y2, x1, 240 - y1 );
            }

            void right_wall( unsigned char colour, unsigned char colour_alt, unsigned short steps, short substep, short turn_offset )
            {
                switch( colour ) {
                    case YELLOW: gpu_dither( DITHERRSLOPE, colour_alt); break;
                    case MAGENTA: gpu_dither( DITHERRSLOPE, colour_alt); break;
                    default: gpu_dither( DITHERCHECK1, colour_alt); break;
                }
                short x1 = (320 - smooth_px(steps, substep)) + turn_offset;
                short x2 = (320 - smooth_px(steps + 1, substep)) + turn_offset;
                short y1 = smooth_py(steps, substep);
                short y2 = smooth_py(steps + 1, substep);

                gpu_quadrilateral( colour, x2, y2, x1, y1, x1, 240 - y1, x2, 240 - y2 );
                gpu_dither( DITHEROFF );
                gpu_line( colour, x1, 240 - y1, x2, 240 - y2 );
                gpu_line( colour, x2, y2, x1, y1 );
            }

            void drawleft( unsigned short steps, unsigned char totheleft, short substep, short turn_offset ) {
                short x1 = smooth_px(steps, substep) + turn_offset;
                short x2 = smooth_px(steps + 1, substep) + turn_offset;
                short y2 = smooth_py(steps + 1, substep);

                switch( totheleft ) {
                    case 'X': left_wall( YELLOW, DKYELLOW, steps, substep, turn_offset ); break;
                    case ' ':
                        gpu_rectangle( GREY5, x1, y2, x2, 240 - y2 );
                        gpu_line( GREY1, x2, y2, x2, 240 - y2 );
                        break;
                    case 'E': left_wall( MAGENTA, DKMAGENTA, steps, substep, turn_offset ); break;
                    case '#': left_wall( GREY2, GREY3, steps, substep, turn_offset ); break;
                }
            }

            void drawright( unsigned short steps, unsigned char totheright, short substep, short turn_offset ) {
                short x1 = (320 - smooth_px(steps, substep)) + turn_offset;
                short x2 = (320 - smooth_px(steps + 1, substep)) + turn_offset;
                short y2 = smooth_py(steps + 1, substep);

                switch( totheright ) {
                    case 'X': right_wall( YELLOW, DKYELLOW, steps, substep, turn_offset ); break;
                    case ' ':
                        gpu_rectangle( GREY5, x2, y2, x1, 240 - y2 );
                        gpu_line( GREY1, x2, y2, x2, 240 - y2 );
                        break;
                    case 'E': right_wall( MAGENTA, DKMAGENTA, steps, substep, turn_offset ); break;
                    case '#': right_wall( GREY3, GREY4, steps, substep, turn_offset ); break;
                }
            }

            // ENCAPSULATED VIEW DRAWING FUNCTION
            // This allows us to draw the maze twice per frame during a turn to eliminate blank areas
            void draw_maze_view(unsigned short cx, unsigned short cy, unsigned short dir, short sub, short offset) {
                unsigned short visiblesteps = counttowall( cx, cy, dir );

                // Draw horizon wall
                if( visiblesteps <= MAXDEPTH - 1 ) {
                    short front_py = smooth_py(visiblesteps, sub);
                    short left_x = smooth_px(visiblesteps, sub) + offset;
                    short right_x = (320 - smooth_px(visiblesteps, sub)) + offset;

                    switch( whatisfront( cx, cy, dir, visiblesteps ) ) {
                        case 'X':
                            gpu_rectangle( YELLOW, left_x, front_py, right_x, 240 - front_py );
                            if( visiblesteps <= 4 ) {
                                gpu_printf_centre( DKGREEN, 160 + offset, front_py + ( 2 << ( 4 - visiblesteps ) ), BOLD, 4 - visiblesteps, 0, "EXIT" );
                            }
                            break;
                        case 'E':
                            gpu_rectangle( MAGENTA, left_x, front_py, right_x, 240 - front_py );
                            break;
                        case '#':
                            gpu_rectangle( GREY5, left_x, front_py, right_x, 240 - front_py );
                            break;
                    }
                }

                // DRAW MAZE FROM BACK TO FRONT
                // Drawing backward ensures closer walls overlap ghosts and gaps naturally (Painter's Algorithm)
                for( short steps = min( visiblesteps - 1, MAXDEPTH - 1 ); steps > 0; steps-- ) {

                    // DRAW PILL
                    if( ( whatisfront( cx, cy, dir, steps ) == ' ' ) && ( whatisat( cx + directionx[ dir ] * steps, cy + directiony[ dir ] * steps, 1 ) != ' ' ) ) {
                        draw_pill( steps, sub, offset );
                    }

                    // DRAW SIDE WALLS
                    drawleft( steps, whatisleft( cx, cy, dir, steps ), sub, offset );
                    drawright( steps, whatisright( cx, cy, dir, steps ), sub, offset );

                    // DRAW GHOST (It will get painted over by closer walls on the next loop iteration!)
                    for( unsigned ghost = 0; ghost < 4; ghost++ ) {
                        if( ghost <= level ) {
                            if( ( cx + directionx[ dir ] * steps == ghostx[ ghost ] ) && ( cy + directiony[ dir ] * steps == ghosty[ ghost ] ) ) {
                                draw_ghost( steps, ghost, dir, sub, offset );
                            }
                        }
                    }
                }

                // FINISH UPTO CORNERS
                drawleft( 0, whatisleft( cx, cy, dir, 0 ), sub, offset );
                drawright( 0, whatisright( cx, cy, dir, 0 ), sub, offset );
            }

            // WALK THE MAZE IN 3D
            unsigned short walk_maze( unsigned short width, unsigned short height )
            {
                unsigned short newx = 1, newy = 1, direction = 1, newdirection = 1, currentx = 1, currenty = 1, mappeeks = 4, peekactive = 100, dead = 0;
                unsigned short olddirection = 1; // Used to hold the old view during a turn
                short substep = 0, turn_offset = 0;

                // SET move timers - timer 0 ~16ms (matches 60fps) for player substeps, timer 1 125ms (1000/8) for ghosts
                set_timer1khz( 16, 0 ); set_timer1khz( 125, 1 );

                tpu_cs();
                // LOOP UNTIL REACHED THE EXIT OR DEAD
                while( ( ( currentx != width - 2 ) || ( currenty != height - 3 ) ) && ( dead == 0 ) ) {
                    setat( currentx, currenty, ' ', 1 );

                    bitmap_draw( 3 - framebuffer );
                    gpu_cs();

                    // RENDER THE VIEW(S)
                    if (turn_offset > 0) {
                        // Turning right. Slide old view left, slide new view in from right
                        draw_maze_view(currentx, currenty, olddirection, 0, turn_offset - 320);
                        draw_maze_view(currentx, currenty, direction, 0, turn_offset);
                        turn_offset -= 16; // 16px per frame = smooth 20 frame animation
                    } else if (turn_offset < 0) {
                        // Turning left. Slide old view right, slide new view in from left
                        draw_maze_view(currentx, currenty, olddirection, 0, turn_offset + 320);
                        draw_maze_view(currentx, currenty, direction, 0, turn_offset);
                        turn_offset += 16;
                    } else {
                        // Normal forward/backward movement
                        draw_maze_view(currentx, currenty, direction, substep, 0);
                    }

                    draw_map( width, height, currentx, currenty, direction, peekactive ? 0 : 1, mappeeks );

                    framebuffer = 3 - framebuffer;
                    await_vblank(); // Lock to screen refresh rate for smooth speeds
                    screen_order( LAYER_CHARACTERMAP, framebuffer, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE );

                    // CHECK IF PLAYER MOVE ALLOWED (ONLY WHEN NOT TURNING!)
                    if (turn_offset == 0 && get_timer1khz( 0 ) == 0) {

                        // POWER UP
                        if( ( get_buttons() & JOY_FIRE1 ) && ( powerpills > 0 ) ) {
                            powerstatus += 200;
                            powerpills--;
                        }

                        // LEFT
                        if( get_buttons() & JOY_LEFT ) {
                            olddirection = direction;
                            newdirection = ( direction == 0 ) ? 3 : direction - 1;
                            substep = 0;
                            turn_offset = -320;
                        }

                        // RIGHT
                        else if( get_buttons() & JOY_RIGHT ) {
                            olddirection = direction;
                            newdirection = ( direction == 3 ) ? 0 : direction + 1;
                            substep = 0;
                            turn_offset = 320;
                        }

                        // FORWARD
                        else if( get_buttons() & JOY_UP ) {
                            substep++;
                            if (substep > 7) {
                                switch( whatisfront( currentx, currenty, direction, 1 ) ) {
                                    case ' ':
                                    case 'X':
                                        newx += directionx[direction];
                                        newy += directiony[direction];
                                        substep = 0;
                                        break;
                                    default:
                                        substep = 7;
                                        break;
                                }
                            }
                        }

                        // BACKWARD
                        else if( get_buttons() & JOY_DOWN ) {
                            substep--;
                            if (substep < 0) {
                                switch( whatisbehind( currentx, currenty, direction, 1 ) ) {
                                    case ' ':
                                    case 'X':
                                        newx -= directionx[direction];
                                        newy -= directiony[direction];
                                        substep = 7;
                                        break;
                                    default:
                                        substep = 0;
                                        break;
                                }
                            }
                        }

                        // FIRE2 - PEEK
                        if( ( get_buttons() & JOY_FIRE2 ) && ( mappeeks != 0 ) ) {
                            peekactive = peekactive + 200;
                            mappeeks--;
                        }

                        set_timer1khz( 16, 0 );
                    }

                    currentx = newx; currenty = newy; direction = newdirection;

                    if( powerstatus == 0 ) {
                        // Instant collision check
                        for( unsigned short ghost = 0; ghost < 4; ghost++ ) {
                            if( ghost <= level ) {
                                if( ( ghostx[ ghost ] == currentx ) && ( ghosty[ ghost ] == currenty ) )
                                    dead = 1;
                            }
                        }

                        // Ghost sub-step handling (runs independently of the player turning)
                        if( get_timer1khz( 1 ) == 0 ) {
                            for( unsigned short ghost = 0; ghost < 4; ghost++ ) {
                                if( ghost <= level ) {
                                    ghost_substep[ghost]++;

                                    if (ghost_substep[ghost] > 7) {
                                        move_ghost( ghost );
                                        ghost_substep[ghost] = 0;
                                    }

                                    if( ( ghostx[ ghost ] == currentx ) && ( ghosty[ ghost ] == currenty ) )
                                        dead = 1;
                                }
                            }
                            set_timer1khz( 125, 1 );
                        }
                    }

                    powerstatus = ( powerstatus > 0 ) ? powerstatus - 1 : 0;
                    peekactive = ( peekactive > 0 ) ? peekactive - 1 : 0;
                }

                return dead;
            }

            int main( int argc, char **argv ) {
                unsigned short firstrun = 1;

                // SWITCH SCREEN TO OLD PAWSv2 MODE
                set_background( 0, 0, BKG_RAINBOW );
                IRQ_VECTOR( (void *)playtune );

                unsigned short levelselected;

                while(1) {
                    // SETUP THE SCREEN BLUE/GREEN BACKGROUND
                    gpu_cs();
                    tpu_cs();
                    set_background( 0, 0, BKG_RAINBOW );

                    if( firstrun ) {
                        drawwelcome();
                        firstrun = 0;
                    } else {
                        // DISPLAY 3D PACMAN BITMAP
                        gpu_pixelblock( 0, 0, 320, 240, TRANSPARENT, pacman3dbitmap );
                    }

                    // RESET POWER PILL STATUS
                    powerstatus = 0;

                    levelselected = 0;
                    do {
                        tpu_print_centre( 57, TRANSPARENT, YELLOW, TPU_BOLD, "Select Level" );
                        tpu_print_centre( 58, TRANSPARENT, YELLOW, TPU_NORMAL, "Increase/Decrease by LEFT/RIGHT - Select by FIRE" );
                        tpu_set( 0, 59, TRANSPARENT, BLACK, TPU_BOLD ); tpu_printf( 1, "Level: %3d", level );
                        tpu_set( 60, 59, TRANSPARENT, BLACK, TPU_BOLD ); tpu_printf( 1, "Size: %5d x %5d", levelwidths[level], levelheights[level] );

                        while( get_buttons() == JOY_NONE );
                        // LEFT / RIGHT to change level, FIRE to select
                        if( get_buttons() & JOY_LEFT ) {
                            while( get_buttons() & JOY_LEFT );
                            level = ( level == 0 ) ? MAXLEVEL : level - 1;
                        }
                        if( get_buttons() & JOY_RIGHT ) {
                            while( get_buttons() & JOY_RIGHT );
                            level = ( level < MAXLEVEL ) ? level + 1 : 0;
                        }
                        if( get_buttons() & JOY_FIRE1 ) {
                            while( get_buttons() & JOY_FIRE1 );
                            levelselected = 1;
                        }
                    } while( levelselected == 0 );

                    tpu_print_centre( 57, TRANSPARENT, YELLOW, TPU_BOLD, "Level Selected" );
                    tpu_print_centre( 58, TRANSPARENT, YELLOW, TPU_BOLD | TPU_X2 | TPU_Y2, "Please Wait - Generating Maze" );

                    // GENERATE THE MAZE
                    set_timer1khz( 1500, 0 );
                    generate_maze( levelwidths[level], levelheights[level] );
                    while( get_timer1khz( 0 ) );

                    // SET NUMBER OF POWER PILLS
                    powerpills = ( level < 4 ) ? level + 1 : 4;

                    // ENTER THE MAZE IN 3D - Play tune if level 1
                    set_background( DKBLUE, DKGREEN, BKG_5050_H ); if( !level ) IRQ_ON( IRQ_VBLANK, TRUE );
                    if( walk_maze( levelwidths[level], levelheights[level] ) ) {
                        // PACMAN WILT GRAPHICS
                        for( unsigned char i = 0; i < 5; i++ ) {
                            bitmap_draw( 3 - framebuffer );
                            gpu_cs();
                            gpu_circle( YELLOW, 160, 120, 80, drawsector[i], 1 );
                            framebuffer = 3 - framebuffer;
                            await_vblank(); screen_order( LAYER_CHARACTERMAP, framebuffer, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE );
                            sleep1khz( 250 );
                        }
                        // DISPLAY TOMBSTONE BITMAP AND RESET TO BEGINNING
                        bitmap_draw( 3 - framebuffer );
                        gpu_cs();
                        gpu_pixelblock( 37, 0, 246, 240, TRANSPARENT, tombstonebitmap );
                        framebuffer = 3 - framebuffer;
                        screen_order( LAYER_CHARACTERMAP, framebuffer, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE );
                        firstrun = 1;
                    } else {
                        // COMPLETED THE MAZE
                        set_background( 0, 0, BKG_STATIC );

                        // GO TO THE NEXT LEVEL
                        level = ( level < MAXLEVEL ) ? level + 1 : MAXLEVEL;
                    }

                    tpu_print_centre( 58, TRANSPARENT, GREEN, 1, "Press FIRE to restart!" ); while( ( get_buttons() & JOY_FIRE1 ) == 0 );
                    tpu_print_centre( 58, TRANSPARENT, PURPLE, 0, "Release FIRE!" ); while( get_buttons() & JOY_FIRE1  );
                }
            }
