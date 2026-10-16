#include "c_includ.h"
/*
 * cbzone_gpr.c
 *  -- Todd W Mummert, December 1990, CMU
 *
 * RCS info
 *  $Header: c_gpr.c,v 1.1 91/01/12 02:03:32 mummert Locked $
 *
 * emulate gpr/ftn on top of X11.
 *
 * I don't know who originally wrote this emulation routine, but
 * I've seriously changed it anyway.  Therefore, it should not
 * be used with any expectation that it will do anything similiar
 * to the original gpr routines.
 * Almost all of the routines have had their arguments shortened.
 * And why bother returning status when it's always the same.  Most
 * importantly I removed the mallocs that were in polyline and
 * multiline.
 *
 * Added support for window managers, iconification, color, etc...
 * Now, it's basically a file full of one line functions...except
 * for the event handling and initialization.
 *
 * let's define how we are going to use the GC's...
 * Erase GC...both foreground and background the color of the game
 *   background...
 * Text GC...foreground is red...as all text is currently
 * Draw GC...will be changed often...usually just the color
 * BitBlt GC...it will handle the printing of the
 *           lander/missile warning boxes after they are initially
 *           placed.
 */


#include "bitmaps/moon.bit"
#include "bitmaps/joystick.bit"
#include "bitmaps/hswitch.bit"
#include "bitmaps/lswitch.bit"
#include "bitmaps/tank.bit"
#include "bitmaps/cursor.bit"
#include "bitmaps/cshape.bit"

int ufc, unfc;                        /* number of fading/nonfading colors */
int cw, cr;                           /* number of colors wanted/received  */
int wid, hei, depth;
Rectange_t clipr;

unsigned char paws_colour, paws_text_colour;

#define NUMPIXMAPS 5
Bmap bmaps[NUMPIXMAPS] = {
  moon_width, moon_height, moon_bits, NULL, COLOR_MOON,
  joystick_width, joystick_height, joystick_bits, NULL, COLOR_JOYSTICK,
  hswitch_width, hswitch_height, hswitch_bits, NULL, COLOR_TEXT,
  lswitch_width, lswitch_height, lswitch_bits, NULL, COLOR_TEXT,
  tank_width, tank_height, tank_bits, NULL, COLOR_TEXT
  };

void grabpointer()
{
}

void ungrabpointer()
{
}

void setmonomap()
{
}

void createfadedcolors()
{
}

void setcoloroptvalues()
{
}

void standardizecolors(v)
     Visual* v;
{
}

int writecolormap(status)
     Bool* status;
{
  return 0;
}

void countcolors()
{
}

void reducecolors()
{
}

void setcolormap()
{
     for( int i = 0; i < MAX_COLORS; i++ ) {
          opt->cpi[i] = i;
     }
}

void gprinit ()
{
}

void buildgcs()
{
}

void buildpixmaps()
{
  int i;

//  for (i=0; i<NUMPIXMAPS; i++)
//    bmaps[i].p =
//      XCreatePixmapFromBitmapData(d, w, bmaps[i].bits,
//                                  bmaps[i].width, bmaps[i].height,
//                                  pixels[opt->cpi[bmaps[i].color]],
//                                  pixels[opt->cpi[COLOR_BG]],
//                                  depth);
}

unsigned char paws_colourmap[] = {
    TRANSPARENT, STEELBLUE, STEELBLUE, STEELBLUE,
    STEELBLUE, GREY3, GREY3, GREY3,
    RED, RED, RED, GREEN,
    GREEN, GREEN, GREEN, GREEN,
    GREEN, YELLOW, YELLOW
};

float xscale,yscale,txscale,tyscale;
void gprinqconfig(argc, argv)
     int* argc;
     char* argv[];
{
     opt->delay = DELAY;
     opt->mblocks = MBLOCKS;
     opt->mlanders = MLANDERS;
     opt->mtanks = MTANKS;
     opt->mmissiles = MMISSILES;
     opt->msalvos = MSALVOS;
     opt->copters = FALSE;
     opt->loud = TRUE;
     opt->scores = FALSE;
     opt->version = FALSE;
     opt->help = FALSE;
     opt->output = TRUE;
     opt->mono = FALSE;
     opt->max_colors = MAX_COLORS;
     opt->fading_colors = 0;
     opt->defaultcolormap = FALSE;

     setcolormap();

     parseopt(*argc, argv, True);

    wid = 1000;
    hei = 710;

    xscale = 320.0/(float)wid; txscale = 80.0/(float)wid;
    yscale = 240.0/(float)hei; tyscale = 60.0/(float)hei;
}

void gprsettextfont(font)
     Font font;
{
//  XSetFont(d, TextGC, font);
}

void printstring(x, y, string, nchars)
     int x, y;
     char* string;
     int nchars;
{
     tpu_set( x * txscale, y * txscale, TRANSPARENT, paws_text_colour, TPU_NORMAL );
     tpu_print( 0, string );
}

void polyline(points, number)
     XPoint *points;
     int number;
{
  int i;
  for (i = 0; i < number-1; i++)
    {
      gpu_line( paws_colour, points[i].x * xscale, points[i].y * yscale, points[i+1].x * xscale, points[i+1].y * yscale );
    }
}

void multiline(segments, number)
     XSegment *segments;
     int number;
{
  int i;
  for (i = 0; i < number; i++)
    {
      gpu_line( paws_colour, segments[i].x1 * xscale, segments[i].y1 * yscale, segments[i].x2 * xscale, segments[i].y2 * yscale );
    }
}

void drawrectangle(x, y, width, height)
     int x, y, width, height;
{
  gpu_box( paws_colour, x * xscale, y * yscale, ( x + width ) * xscale, ( y + height ) * yscale, 1 );
}

void bitblt(window)
     Window_t *window;
{
//  XFillRectangle(d, w, BitBltGC, (window->base.x-XOFF)*XM/XD, (window->base.y-YOFF)*YM/YD,
//                 window->size.x*XM/XD, window->size.y*YM/YD);
}

void clearrectangle(window, dsto)
     Window_t *window;
     Position_t *dsto;
{
  gpu_cs();
//  XFillRectangle(d,w,EraseGC,(dsto->x-XOFF)*XM/XD, (dsto->y-YOFF)*YM/YD,
//                 window->size.x*XM/XD, window->size.y*YM/YD);
}

void gprsetclippingactive(flag)
     Bool flag;
{
  if( flag ) {
    gpu_crop( 53 * xscale, 53 * yscale, ( 53 + 894 ) * xscale, ( 53 + 394 ) * yscale );
  } else {
    gpu_crop( CROPFULLSCREEN );
  }
//  if (flag) {
//    XSetClipRectangles (d, DrawGC, 0, 0, &clipr, 1, YXBanded);
//    XSetClipRectangles (d, EraseGC, 0, 0, &clipr, 1, YXBanded);
//  } else {
//    XSetClipMask (d, DrawGC, None);
//    XSetClipMask (d, EraseGC, None);
//  }
}

void tonetime()
{
  // Plays a brief alert tone on the left channel using a square wave
  beep( CHANNEL_LEFT_0, WAVE_SQUARE, 49, 100, 15 ); //[cite: 25, 27]
}

void timeclock(tval)
     struct timeval* tval;
{
  await_vblank();
  gettimeofday(tval, 0);
}

void gprinqcursor(posn)
     Position_t *posn;
{
     short x, y, buttons;
     get_mouse( &x, &y, &buttons );
     posn->x = x; posn->y = y;
}

Bool paused = False;

Bool gprcondeventwait(char* key, Position_t *posn)
{
  static unsigned short last_buttons = 0;

  // 1. Process ULX3S Hardware Buttons
  unsigned short current_buttons = get_buttons(); //[cite: 28]

  // Button 1 / JOY_FIRE1 (Fire: 'a' on press, 'A' on release)[cite: 25]
  if ((current_buttons & JOY_FIRE1) != (last_buttons & JOY_FIRE1)) {
    if (current_buttons & JOY_FIRE1) {
      *key = 'a';
      last_buttons |= JOY_FIRE1;
    } else {
      *key = 'A';
      last_buttons &= ~JOY_FIRE1;
    }
    return True;
  }

  // Button 2 / JOY_FIRE2 (Centre: 'b' on press, 'B' on release)[cite: 25]
  if ((current_buttons & JOY_FIRE2) != (last_buttons & JOY_FIRE2)) {
    if (current_buttons & JOY_FIRE2) {
      *key = 'b';
      last_buttons |= JOY_FIRE2;
    } else {
      *key = 'B';
      last_buttons &= ~JOY_FIRE2;
    }
    return True;
  }

  // 2. Process PS/2 Keyboard Events (Raw Scancode Pipeline)[cite: 28]
  if (ps2_event_available()) { //[cite: 28]
    unsigned short keyevent = ps2_event_get(); //[cite: 28]
    unsigned short scancode = keyevent & 0x1ff; //[cite: 24]
    Bool is_keydown = (keyevent & 0x200) != 0; //[cite: 24]

    // Scancode 0x15 = 'Q' (Quit)[cite: 24]
    if (scancode == 0x15 && is_keydown) {
      *key = 'Q';
      return True;
    }

    // Scancode 0x29 = Spacebar (Sensitivity Toggle: 'c'/'C')[cite: 24]
    if (scancode == 0x29) {
      *key = is_keydown ? 'c' : 'C';
      return True;
    }

    // Optional: Map keyboard 'A' (0x1c) and 'B' (0x32) as redundant inputs[cite: 24]
    if (scancode == 0x1c) {
      *key = is_keydown ? 'a' : 'A';
      return True;
    }
    if (scancode == 0x32) {
      *key = is_keydown ? 'b' : 'B';
      return True;
    }
  }

  // Keep state clean for unmapped buttons
  last_buttons = current_buttons;

  return False;
}

void gprsetcursorposition(posn)
     Position_t *posn;
{
//  XWarpPointer (d, None, w, 0, 0, 0, 0, (posn->x-XOFF)*XM/XD, (posn->y-YOFF)*YM/YD);
}

Font gprloadfontfile(name)
     char* name;
{
//  XFontStruct *xfs;
//
//  if ((xfs = XLoadQueryFont(d, name)) == NULL)
//    return XLoadFont(d, "fixed");
//  else
//    return xfs->fid;
}

void gprsettextvalue(index)
     int index;
{
     paws_text_colour = paws_colourmap[ index ];
  //XSetForeground(d, TextGC, pixels[index]);
}

void gprsettextbackgroundvalue(index)
     int index;
{
  //XSetBackground(d, TextGC, pixels[index]);
}

void gprsetdrawvalue(index)
     int index;
{
  paws_colour = paws_colourmap[ index ];;
}

void gprcircle(center, radius)
     Position_t *center;
     int radius;
{
  gpu_ellipse( paws_colour, ( center->x ) * xscale, ( center->y ) * yscale, radius * xscale, radius * yscale, FALSE );
}

void gprcirclefilled(center, radius)
     Position_t *center;
     int radius;
{
  gpu_ellipse( paws_colour, ( center->x ) * xscale, ( center->y ) * yscale, radius * xscale, radius * yscale, TRUE );
}

void gprsetclipwindow(window)
     Window_t *window;
{
  clipr.x = (window->base.x-XOFF)*XM/XD;
  clipr.y = (window->base.y-YOFF)*XM/XD;
  clipr.width = window->size.x*XM/XD;
  clipr.height =  window->size.y*YM/YD;
}

void clearentirescreen()
{
  gpu_cs();
}

void flushwindow()
{
}

void waitforkey(c)
     char c;
{
}

void putpixmap(i, p)
     int i;
     int* p;
{
     short x = p[0] * xscale, y = p[1] * yscale;

     switch(i) {
          case 0:
               gpu_ellipse( paws_colourmap[COLOR_MOON], x, y, moon_width * xscale, moon_height * yscale, FALSE );
               break;
          case 1:
               gpu_ellipse( paws_colourmap[COLOR_JOYSTICK], x, y, joystick_width * xscale, joystick_height * yscale, TRUE );
          default:
               break;
     }

}

void removepixmap(i, p)
     int i;
     int* p;
{
     short x = p[0] * xscale, y = p[1] * yscale;

     switch(i) {
          case 0:
               gpu_ellipse( TRANSPARENT, x, y, moon_width * xscale, moon_height * yscale, FALSE );
               break;
          case 1:
               gpu_ellipse( TRANSPARENT, x, y, joystick_width * xscale, joystick_height * yscale, TRUE );
          default:
               break;
     }
}
