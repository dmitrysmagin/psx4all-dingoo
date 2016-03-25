/***************************************************************************
                          draw.c  -  description
                             -------------------
    begin                : Sun Oct 28 2001
    copyright            : (C) 2001 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

//*************************************************************************//
// History of changes:
//
// 2008/05/17 - Pete  
// - added "visual rumble" stuff to buffer swap func
//
// 2007/10/27 - MxC
// - added HQ2X/HQ3X MMX versions, and fixed stretching
//
// 2005/06/11 - MxC
// - added HQ2X,HQ3X,Scale3X screen filters
//
// 2004/01/31 - Pete
// - added zn stuff
//
// 2003/01/31 - stsp
// - added zn stuff
//
// 2003/12/30 - Stefan Sperling <stsp@guerila.com>
// - improved XF86VM fullscreen switching a little (refresh frequency issues).
//
// 2002/12/30 - Pete
// - added Scale2x display mode - Scale2x (C) 2002 Andrea Mazzoleni - http://scale2x.sourceforge.net
//
// 2002/12/29 - Pete
// - added gun cursor display
//
// 2002/12/21 - linuzappz
// - some more messages for DGA2 errors
// - improved XStretch funcs a little
// - fixed non-streched modes for DGA2
//
// 2002/11/10 - linuzappz
// - fixed 5bit masks for 2xSai/etc
//
// 2002/11/06 - Pete
// - added 2xSai, Super2xSaI, SuperEagle
//
// 2002/08/09 - linuzappz
// - added DrawString calls for DGA2 (FPS display)
//
// 2002/03/10 - lu
// - Initial SDL-only blitting function
// - Initial SDL stretch function (using an undocumented SDL 1.2 func)
// - Boht are triggered by -D_SDL -D_SDL2
//
// 2002/02/18 - linuzappz
// - NoStretch, PIC and Scanlines support for DGA2 (32bit modes untested)
// - Fixed PIC colors in CreatePic for 16/15 bit modes
//
// 2002/02/17 - linuzappz
// - Added DGA2 support, support only with no strecthing disabled (also no FPS display)
//
// 2002/01/13 - linuzappz
// - Added timing for the szDebugText (to 2 secs)
//
// 2002/01/05 - Pete
// - fixed linux stretch centering (no more garbled screens)
//
// 2001/12/30 - Pete
// - Added linux fullscreen desktop switching (non-SDL version, define USE_XF86VM in Makefile)
//
// 2001/12/19 - syo
// - support refresh rate change
// - added  wait VSYNC
//
// 2001/12/16 - Pete
// - Added Windows FPSE RGB24 mode switch
//
// 2001/12/05 - syo (syo68k@geocities.co.jp)
// - modified for "Use system memory" option
//   (Pete: fixed "system memory" save state pic surface)
//
// 2001/11/11 - lu
// - SDL additions
//
// 2001/10/28 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

#include "stdafx.h"

#define _IN_DRAW

#include "externals.h"
#include "gpu.h"
#include "draw.h"
#include "prim.h"
#include "menu.h"

////////////////////////////////////////////////////////////////////////////////////
// misc globals
////////////////////////////////////////////////////////////////////////////////////
int            iResX;
int            iResY;
long           lLowerpart;
BOOL           bIsFirstFrame = TRUE;
BOOL           bCheckMask=FALSE;
unsigned short sSetMask=0;
unsigned long  lSetMask=0;
int            iDesktopCol=16;
int            iShowFPS=0;
int            iWinSize;
int            iUseScanLines=0;
int            iUseNoStretchBlt=0;
int            iFastFwd=0;
int            iDebugMode=0;
int            iFVDisplay=0;
PSXPoint_t     ptCursorPoint[8];
unsigned short usCursorActive=0;

////////////////////////////////////////////////////////////////////////
// SDL Stuff ^^
////////////////////////////////////////////////////////////////////////

int           Xpitch,depth=32;
char *        Xpixels;
char *        pCaptionText;

SDL_Surface *display,*XFimage,*XPimage=NULL;
SDL_Surface *Ximage = NULL;
//static Uint32 sdl_mask=SDL_HWSURFACE|SDL_HWACCEL;/*place or remove some flags*/
Uint32 sdl_mask=SDL_HWSURFACE;
SDL_Rect rectdst,rectsrc;



void DestroyDisplay(void)
{
if(display){
if(Ximage) SDL_FreeSurface(Ximage);
if(XFimage) SDL_FreeSurface(XFimage);

SDL_FreeSurface(display);//the display is also a surface in SDL
}
SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
void SetDisplay(void){
if(iWindowMode)
display = SDL_SetVideoMode(iResX,iResY,depth,sdl_mask);
else display = SDL_SetVideoMode(iResX,iResY,depth,SDL_FULLSCREEN|sdl_mask);
}

void CreateDisplay(void)
{

if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0)
   {
	  fprintf (stderr,"(x) Failed to Init SDL!!!\n");
	  return;
   }

//display = SDL_SetVideoMode(iResX,iResY,depth,sdl_mask);
display = SDL_SetVideoMode(iResX,iResY,depth,!iWindowMode*SDL_FULLSCREEN|sdl_mask);
Ximage = SDL_CreateRGBSurface(sdl_mask,iResX,iResY,depth,0x00ff0000,0x0000ff00,0x000000ff,0);
XFimage= SDL_CreateRGBSurface(sdl_mask,170,15,depth,0x00ff0000,0x0000ff00,0x000000ff,0);

iColDepth=depth;
//memset(XFimage->pixels,255,170*15*4);//really needed???
//memset(Ximage->pixels,0,ResX*ResY*4);

//Xpitch=iResX*32; no more use
Xpixels=(char *)Ximage->pixels;

if(pCaptionText)
      SDL_WM_SetCaption(pCaptionText,NULL);
 else SDL_WM_SetCaption("FPSE Display - P.E.Op.S SoftSDL PSX Gpu",NULL);

}

////////////////////////////////////////////////////////////////////////
void (*BlitScreen) (unsigned char *,long,long);
void (*BlitScreenNS) (unsigned char *,long,long);
void (*XStretchBlt)(unsigned char * pBB,int sdx,int sdy,int ddx,int ddy);
unsigned char * pBackBuffer=0;
////////////////////////////////////////////////////////////////////////

void BlitScreen32(unsigned char * surf,long x,long y)
{
 unsigned char * pD;
 unsigned int startxy;
 unsigned long lu;unsigned short s;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 long lPitch=(dx+PreviousPSXDisplay.Range.x0)<<2;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 surf+=PreviousPSXDisplay.Range.x0<<2;

 if(PSXDisplay.RGB24)
  {
   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;
     pD=(unsigned char *)&psxVuw[startxy];

     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *((unsigned long *)((surf)+(column*lPitch)+(row<<2)))=
          0xff000000|(RED(lu)<<16)|(GREEN(lu)<<8)|(BLUE(lu));
       pD+=3;
      }
    }
  }
 else
  {
   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;
     for(row=0;row<dx;row++)
      {
       s=psxVuw[startxy++];
       *((unsigned long *)((surf)+(column*lPitch)+(row<<2)))=
        ((((s<<19)&0xf80000)|((s<<6)&0xf800)|((s>>7)&0xf8))&0xffffff)|0xff000000;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////

void BlitScreen32NS(unsigned char * surf,long x,long y)
{
 unsigned char * pD;
 unsigned int startxy;
 unsigned long lu;unsigned short s;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 long lPitch=iResX<<2;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 surf+=PreviousPSXDisplay.Range.x0<<2;

 if(PSXDisplay.RGB24)
  {
   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;
     pD=(unsigned char *)&psxVuw[startxy];

     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *((unsigned long *)((surf)+(column*lPitch)+(row<<2)))=
          0xff000000|(RED(lu)<<16)|(GREEN(lu)<<8)|(BLUE(lu));
       pD+=3;
      }
    }
  }
 else
  {
   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;
     for(row=0;row<dx;row++)
      {
       s=psxVuw[startxy++];
       *((unsigned long *)((surf)+(column*lPitch)+(row<<2)))=
        ((((s<<19)&0xf80000)|((s<<6)&0xf800)|((s>>7)&0xf8))&0xffffff)|0xff000000;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////

void BlitScreen32NSSL(unsigned char * surf,long x,long y)
{
 unsigned char * pD;
 unsigned int startxy;
 unsigned long lu;unsigned short s;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 long lPitch=iResX<<2;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 surf+=PreviousPSXDisplay.Range.x0<<2;

 if(PSXDisplay.RGB24)
  {
   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;
     pD=(unsigned char *)&psxVuw[startxy];
     if(column&1)
     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *((unsigned long *)((surf)+(column*lPitch)+(row<<2)))=
          0xff000000|(RED(lu)<<16)|(GREEN(lu)<<8)|(BLUE(lu));
       pD+=3;
      }
    }
  }
 else
  {
   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;
     if(column&1)
     for(row=0;row<dx;row++)
      {
       s=psxVuw[startxy++];
       *((unsigned long *)((surf)+(column*lPitch)+(row<<2)))=
        ((((s<<19)&0xf80000)|((s<<6)&0xf800)|((s>>7)&0xf8))&0xffffff)|0xff000000;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////

void BlitScreen15(unsigned char * surf,long x,long y)
{
 unsigned long lu;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 unsigned short LineOffset,SurfOffset;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*((dx+PreviousPSXDisplay.Range.x0)<<1);
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;
   unsigned short * DSTPtr =(unsigned short *)surf;
   DSTPtr+=PreviousPSXDisplay.Range.x0;

   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;

     pD=(unsigned char *)&psxVuw[startxy];

     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *DSTPtr++=
         ((RED(lu)<<7)&0x7c00)|
         ((GREEN(lu)<<2)&0x3e0)|
          (BLUE(lu)>>3);
       pD+=3;
      }
     DSTPtr+=PreviousPSXDisplay.Range.x0;
    }
  }
 else
  {
   unsigned long * SRCPtr,* DSTPtr;

   SurfOffset=PreviousPSXDisplay.Range.x0>>1;

   SRCPtr = (unsigned long *)(psxVuw +
                             (y<<10) + x);
   DSTPtr = ((unsigned long *)surf) + SurfOffset;

   dx>>=1;
   LineOffset = 512 - dx;

   for(column=0;column<dy;column++)
    {
     for(row=0;row<dx;row++)
      {
       lu=*SRCPtr++;
       *DSTPtr++=
        ((lu<<10)&0x7c007c00)|
        ((lu)&0x3e003e0)|
        ((lu>>10)&0x1f001f);
      }
     SRCPtr += LineOffset;
     DSTPtr += SurfOffset;
    }
  }
}

////////////////////////////////////////////////////////////////////////

void BlitScreen15NS(unsigned char * surf,long x,long y)
{
 unsigned long lu;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 unsigned short LineOffset,SurfOffset;
 long lPitch=iResX<<1;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   surf+=PreviousPSXDisplay.Range.x0<<1;

   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;

     pD=(unsigned char *)&psxVuw[startxy];

     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *((unsigned short *)((surf)+(column*lPitch)+(row<<1)))=
         ((RED(lu)<<7)&0x7c00)|
         ((GREEN(lu)<<2)&0x3e0)|
          (BLUE(lu)>>3);
       pD+=3;
      }
    }
  }
 else
  {
   unsigned long * SRCPtr = (unsigned long *)(psxVuw +
                             (y<<10) + x);

   unsigned long * DSTPtr =
    ((unsigned long *)surf)+(PreviousPSXDisplay.Range.x0>>1);

   dx>>=1;

   LineOffset = 512 - dx;
   SurfOffset = (lPitch>>2) - dx;

   for(column=0;column<dy;column++)
    {
     for(row=0;row<dx;row++)
      {
       lu=*SRCPtr++;

       *DSTPtr++=
        ((lu<<10)&0x7c007c00)|
        ((lu)&0x3e003e0)|
        ((lu>>10)&0x1f001f);
      }
     SRCPtr += LineOffset;
     DSTPtr += SurfOffset;
    }
  }
}

////////////////////////////////////////////////////////////////////////

void BlitScreen15NSSL(unsigned char * surf,long x,long y)
{
 unsigned long lu;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 unsigned short LineOffset,SurfOffset;
 long lPitch=iResX<<1;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   surf+=PreviousPSXDisplay.Range.x0<<1;

   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;

     pD=(unsigned char *)&psxVuw[startxy];
     if(column&1)
     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *((unsigned short *)((surf)+(column*lPitch)+(row<<1)))=
         ((RED(lu)<<7)&0x7c00)|
         ((GREEN(lu)<<2)&0x3e0)|
          (BLUE(lu)>>3);
       pD+=3;
      }
    }
  }
 else
  {
   unsigned long * SRCPtr = (unsigned long *)(psxVuw +
                             (y<<10) + x);

   unsigned long * DSTPtr =
    ((unsigned long *)surf)+(PreviousPSXDisplay.Range.x0>>1);

   dx>>=1;

   LineOffset = 512 - dx;
   SurfOffset = (lPitch>>2) - dx;

   for(column=0;column<dy;column++)
    {
     if(column&1)
      {
       for(row=0;row<dx;row++)
        {
         lu=*SRCPtr++;

         *DSTPtr++=
          ((lu<<10)&0x7c007c00)|
          ((lu)&0x3e003e0)|
          ((lu>>10)&0x1f001f);
        }
       SRCPtr += LineOffset;
       DSTPtr += SurfOffset;
      }
     else
      {
       DSTPtr+=iResX>>1;
       SRCPtr+=512;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////

void BlitScreen16(unsigned char * surf,long x,long y)  // BLIT IN 16bit COLOR MODE
{
 unsigned long lu;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 unsigned short LineOffset,SurfOffset;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*((dx+PreviousPSXDisplay.Range.x0)<<1);
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;
   unsigned short * DSTPtr =(unsigned short *)surf;
   DSTPtr+=PreviousPSXDisplay.Range.x0;

   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;

     pD=(unsigned char *)&psxVuw[startxy];

     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *DSTPtr++=
         ((RED(lu)<<8)&0xf800)|((GREEN(lu)<<3)&0x7e0)|(BLUE(lu)>>3);
       pD+=3;
      }
     DSTPtr+=PreviousPSXDisplay.Range.x0;
    }
  }
 else
  {
   unsigned long * SRCPtr,* DSTPtr;

   SurfOffset=PreviousPSXDisplay.Range.x0>>1;

   SRCPtr = (unsigned long *)(psxVuw +
                             (y<<10) + x);
   DSTPtr = ((unsigned long *)surf) + SurfOffset;

   dx>>=1;
   LineOffset = 512 - dx;

   for(column=0;column<dy;column++)
    {
     for(row=0;row<dx;row++)
      {
       lu=*SRCPtr++;
       *DSTPtr++=
        ((lu<<11)&0xf800f800)|((lu<<1)&0x7c007c0)|((lu>>10)&0x1f001f);
      }
     SRCPtr += LineOffset;
     DSTPtr += SurfOffset;
    }
  }
}

////////////////////////////////////////////////////////////////////////

void BlitScreen16NS(unsigned char * surf,long x,long y)
{
 unsigned long lu;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 unsigned short LineOffset,SurfOffset;
 long lPitch=iResX<<1;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   surf+=PreviousPSXDisplay.Range.x0<<1;

   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;

     pD=(unsigned char *)&psxVuw[startxy];

     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *((unsigned short *)((surf)+(column*lPitch)+(row<<1)))=
         ((RED(lu)<<8)&0xf800)|((GREEN(lu)<<3)&0x7e0)|(BLUE(lu)>>3);
       pD+=3;
      }
    }
  }
 else
  {
   unsigned long * SRCPtr = (unsigned long *)(psxVuw +
                             (y<<10) + x);

   unsigned long * DSTPtr =
    ((unsigned long *)surf)+(PreviousPSXDisplay.Range.x0>>1);

   dx>>=1;

   LineOffset = 512 - dx;
   SurfOffset = (lPitch>>2) - dx;

   for(column=0;column<dy;column++)
    {
     for(row=0;row<dx;row++)
      {
       lu=*SRCPtr++;

       *DSTPtr++=
        ((lu<<11)&0xf800f800)|((lu<<1)&0x7c007c0)|((lu>>10)&0x1f001f);
      }
     SRCPtr += LineOffset;
     DSTPtr += SurfOffset;
    }
  }
}

////////////////////////////////////////////////////////////////////////

void BlitScreen16NSSL(unsigned char * surf,long x,long y)
{
 unsigned long lu;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 unsigned short LineOffset,SurfOffset;
 long lPitch=iResX<<1;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   surf+=PreviousPSXDisplay.Range.x0<<1;

   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;

     pD=(unsigned char *)&psxVuw[startxy];

     if(column&1)
     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *((unsigned short *)((surf)+(column*lPitch)+(row<<1)))=
         ((RED(lu)<<8)&0xf800)|((GREEN(lu)<<3)&0x7e0)|(BLUE(lu)>>3);
       pD+=3;
      }
    }
  }
 else
  {
   unsigned long * SRCPtr = (unsigned long *)(psxVuw +
                             (y<<10) + x);

   unsigned long * DSTPtr =
    ((unsigned long *)surf)+(PreviousPSXDisplay.Range.x0>>1);

   dx>>=1;

   LineOffset = 512 - dx;
   SurfOffset = (lPitch>>2) - dx;

   for(column=0;column<dy;column++)
    {
     if(column&1)
      {
       for(row=0;row<dx;row++)
        {
         lu=*SRCPtr++;

         *DSTPtr++=
          ((lu<<11)&0xf800f800)|((lu<<1)&0x7c007c0)|((lu>>10)&0x1f001f);
        }
       DSTPtr += SurfOffset;
       SRCPtr += LineOffset;
      }
     else
      {
       DSTPtr+=iResX>>1;
       SRCPtr+=512;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////

void XStretchBlt16(unsigned char * pBB,int sdx,int sdy,int ddx,int ddy)
{
 unsigned short * pSrc=(unsigned short *)pBackBuffer;
 unsigned short * pSrcR=NULL;
 unsigned short * pDst=(unsigned short *)pBB;
 unsigned long * pDstR=NULL;
 int x,y,cyo=-1,cy;
 int xpos, xinc;
 int ypos, yinc,ddx2=ddx>>1;

 xinc = (sdx << 16) / ddx;

 ypos=0;
 yinc = (sdy << 16) / ddy;

 for(y=0;y<ddy;y++,ypos+=yinc)
  {
   cy=(ypos>>16);

   if(cy==cyo)
    {
     pDstR=(unsigned long *)(pDst-ddx);
     for(x=0;x<ddx2;x++) *(unsigned long*)pDst++=*pDstR++;
    }
   else
    {
     cyo=cy;
     pSrcR=pSrc+(cy*sdx);
     xpos = 0;
     for(x=ddx;x>0;--x)
      {
	   pSrcR+= xpos>>16;
       xpos -= xpos&0xffff0000;
       *pDst++=*pSrcR;
       xpos += xinc;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////

void XStretchBlt16SL(unsigned char * pBB,int sdx,int sdy,int ddx,int ddy)
{
 unsigned short * pSrc=(unsigned short *)pBackBuffer;
 unsigned short * pSrcR=NULL;
 unsigned short * pDst=(unsigned short *)pBB;
 int x,y,cy;
 int xpos, xinc;
 int ypos, yinc;

 xinc = (sdx << 16) / ddx;

 ypos=0;
 yinc = ((sdy << 16) / ddy)<<1;

 for(y=0;y<ddy;y+=2,ypos+=yinc)
  {
   cy=(ypos>>16);
   pSrcR=pSrc+(cy*sdx);
   xpos = 0;
   for(x=ddx;x>0;--x)
    {
     pSrcR+= xpos>>16;
     xpos -= xpos&0xffff0000;
     *pDst++=*pSrcR;
     xpos += xinc;
    }
   pDst+=iResX;
  }
}

////////////////////////////////////////////////////////////////////////

void XStretchBlt32(unsigned char * pBB,int sdx,int sdy,int ddx,int ddy)
{
 unsigned long * pSrc=(unsigned long *)pBackBuffer;
 unsigned long * pSrcR=NULL;
 unsigned long * pDst=(unsigned long *)pBB;
 unsigned long * pDstR=NULL;
 int x,y,cyo=-1,cy;
 int xpos, xinc;
 int ypos, yinc;

 xinc = (sdx << 16) / ddx;

 ypos=0;
 yinc = (sdy << 16) / ddy;

 for(y=0;y<ddy;y++,ypos+=yinc)
  {
   cy=(ypos>>16);

   if(cy==cyo)
    {
     pDstR=pDst-ddx;
     for(x=0;x<ddx;x++) *pDst++=*pDstR++;
    }
   else
    {
     cyo=cy;
     pSrcR=pSrc+(cy*sdx);
     xpos = 0L;
     for(x=ddx;x>0;--x)
      {
       pSrcR+= xpos>>16;
       xpos -= xpos&0xffff0000;
       *pDst++=*pSrcR;
       xpos += xinc;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////

void XStretchBlt32SL(unsigned char * pBB,int sdx,int sdy,int ddx,int ddy)
{
 unsigned long * pSrc=(unsigned long *)pBackBuffer;
 unsigned long * pSrcR=NULL;
 unsigned long * pDst=(unsigned long *)pBB;
 int x,y,cy;
 int xpos, xinc;
 int ypos, yinc;

 xinc = (sdx << 16) / ddx;

 ypos=0;
 yinc = ((sdy << 16) / ddy)<<1;

 for(y=0;y<ddy;y+=2,ypos+=yinc)
  {
   cy=(ypos>>16);
   pSrcR=pSrc+(cy*sdx);
   xpos = 0;
   for(x=ddx;x>0;--x)
    {
     pSrcR+= xpos>>16;
     xpos -= xpos&0xffff0000;
     *pDst++=*pSrcR;
     xpos += xinc;
    }
   pDst+=iResX;
  }
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

void ShowGunCursor(unsigned char * surf,int iPitch)
{
 unsigned short dx=(unsigned short)PreviousPSXDisplay.Range.x1;
 unsigned short dy=(unsigned short)PreviousPSXDisplay.DisplayMode.y;
 int x,y,iPlayer,sx,ex,sy,ey;

 if(iColDepth==32) iPitch=iPitch<<2;
 else              iPitch=iPitch<<1;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*iPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(iColDepth==32)                                     // 32 bit color depth
  {
   const unsigned long crCursorColor32[8]={0xffff0000,0xff00ff00,0xff0000ff,0xffff00ff,0xffffff00,0xff00ffff,0xffffffff,0xff7f7f7f};

   surf+=PreviousPSXDisplay.Range.x0<<2;               // -> add x left border

   for(iPlayer=0;iPlayer<8;iPlayer++)                  // -> loop all possible players
    {
     if(usCursorActive&(1<<iPlayer))                   // -> player active?
      {
       const int ty=(ptCursorPoint[iPlayer].y*dy)/256;  // -> calculate the cursor pos in the current display
       const int tx=(ptCursorPoint[iPlayer].x*dx)/512;
       sx=tx-5;if(sx<0) {if(sx&1) sx=1; else sx=0;}
       sy=ty-5;if(sy<0) {if(sy&1) sy=1; else sy=0;}
       ex=tx+6;if(ex>dx) ex=dx;
       ey=ty+6;if(ey>dy) ey=dy;

       for(x=tx,y=sy;y<ey;y+=2)                        // -> do dotted y line
        *((unsigned long *)((surf)+(y*iPitch)+x*4))=crCursorColor32[iPlayer];
       for(y=ty,x=sx;x<ex;x+=2)                        // -> do dotted x line
        *((unsigned long *)((surf)+(y*iPitch)+x*4))=crCursorColor32[iPlayer];
      }
    }
  }
 else                                                  // 16 bit color depth
  {
   const unsigned short crCursorColor16[8]={0xf800,0x07c0,0x001f,0xf81f,0xffc0,0x07ff,0xffff,0x7bdf};

   surf+=PreviousPSXDisplay.Range.x0<<1;               // -> same stuff as above

   for(iPlayer=0;iPlayer<8;iPlayer++)
    {
     if(usCursorActive&(1<<iPlayer))
      {
       const int ty=(ptCursorPoint[iPlayer].y*dy)/256;
       const int tx=(ptCursorPoint[iPlayer].x*dx)/512;
       sx=tx-5;if(sx<0) {if(sx&1) sx=1; else sx=0;}
       sy=ty-5;if(sy<0) {if(sy&1) sy=1; else sy=0;}
       ex=tx+6;if(ex>dx) ex=dx;
       ey=ty+6;if(ey>dy) ey=dy;

       for(x=tx,y=sy;y<ey;y+=2)
        *((unsigned short *)((surf)+(y*iPitch)+x*2))=crCursorColor16[iPlayer];
       for(y=ty,x=sx;x<ex;x+=2)
        *((unsigned short *)((surf)+(y*iPitch)+x*2))=crCursorColor16[iPlayer];
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////

#include <time.h>
extern time_t tStart;

void NoStretchSwap(void)
{
 static int iOldDX=0;
 static int iOldDY=0;

 int iDX,iDY,iODX=0,iODY=0;
 int iX=iResX-(PreviousPSXDisplay.Range.x1+PreviousPSXDisplay.Range.x0);
 int iY=iResY-PreviousPSXDisplay.DisplayMode.y;

 if(iX<0)
  {
   iX=0;iDX=iResX;
   iODX=PreviousPSXDisplay.Range.x1;
   PreviousPSXDisplay.Range.x1=iResX-PreviousPSXDisplay.Range.x0;
  }
 else {iX=iX/2;iDX=PreviousPSXDisplay.Range.x1+PreviousPSXDisplay.Range.x0;}

 if(iY<0)
  {
   iY=0;iDY=iResY;
   iODY=PreviousPSXDisplay.DisplayMode.y;
   PreviousPSXDisplay.DisplayMode.y=iResY;
  }
 else {iY=iY/2;iDY=PreviousPSXDisplay.DisplayMode.y;}

 if(iOldDX!=iDX || iOldDY!=iDY)
  {
	  memset(Xpixels,0,iResY*iResX*4);

 rectdst.x=iX;
 rectdst.y=iY;
 rectdst.w=iDX;
 rectdst.h=iDY;

   SDL_FillRect(display,NULL,0);

   iOldDX=iDX;iOldDY=iDY;
  }

 BlitScreenNS((unsigned char *)Xpixels,
              PSXDisplay.DisplayPosition.x,
              PSXDisplay.DisplayPosition.y);

 if(usCursorActive) ShowGunCursor((unsigned char *)Xpixels,iResX);

 if(iODX) PreviousPSXDisplay.Range.x1=iODX;
 if(iODY) PreviousPSXDisplay.DisplayMode.y=iODY;

 SDL_BlitSurface(Ximage,NULL,display,&rectdst);

 if(ulKeybits&KEY_SHOWFPS) //DisplayText();               // paint menu text
  {
   if(szDebugText[0] && ((time(NULL) - tStart) < 2))
    {
     strcpy(szDispBuf,szDebugText);
    }
   else
    {
     szDebugText[0]=0;
     strcat(szDispBuf,szMenuBuf);
    }

    SDL_WM_SetCaption(szDispBuf,NULL); //just a quick fix,
  }

 if(XPimage) DisplayPic();

 SDL_Flip(display);
}

////////////////////////////////////////////////////////////////////////

void DoBufferSwap(void)                                // SWAP BUFFERS
{                                                      // (we don't swap... we blit only)
 if(iUseNoStretchBlt<2)
  {
   if(iUseNoStretchBlt ||
     (PreviousPSXDisplay.Range.x1+PreviousPSXDisplay.Range.x0 == iResX &&
      PreviousPSXDisplay.DisplayMode.y == iResY))
    {NoStretchSwap();return;}
  }

 BlitScreen(pBackBuffer,
            PSXDisplay.DisplayPosition.x,
            PSXDisplay.DisplayPosition.y);

 if(usCursorActive) ShowGunCursor(pBackBuffer,PreviousPSXDisplay.Range.x0+PreviousPSXDisplay.Range.x1);

 //----------------------------------------------------//

 XStretchBlt((unsigned char *)Xpixels,
               PreviousPSXDisplay.Range.x1+PreviousPSXDisplay.Range.x0,
               PreviousPSXDisplay.DisplayMode.y,
               iResX,iResY);

 //----------------------------------------------------//
 SDL_BlitSurface(Ximage,NULL,display,NULL);

 if(ulKeybits&KEY_SHOWFPS) //DisplayText();               // paint menu text
  {
   if(szDebugText[0] && ((time(NULL) - tStart) < 2))
    {
     strcpy(szDispBuf,szDebugText);
    }
   else
    {
     szDebugText[0]=0;
     strcat(szDispBuf,szMenuBuf);
    }

    SDL_WM_SetCaption(szDispBuf,NULL); //just a quick fix,
  }

 if(XPimage) DisplayPic();

 SDL_Flip(display);
}

////////////////////////////////////////////////////////////////////////

void DoClearScreenBuffer(void)                         // CLEAR DX BUFFER
{
 SDL_FillRect(display,NULL,0);
 SDL_Flip(display);
}

////////////////////////////////////////////////////////////////////////

void DoClearFrontBuffer(void)                          // CLEAR DX BUFFER
{
 SDL_FillRect(display,NULL,0);
 SDL_Flip(display);
}


////////////////////////////////////////////////////////////////////////

int Xinitialize()
{
 pBackBuffer=(unsigned char *)malloc(640*512*sizeof(unsigned long));
 memset(pBackBuffer,0,640*512*sizeof(unsigned long));


 if(iColDepth==16)
  {
   BlitScreen=BlitScreen16;iDesktopCol=16;
   if(iUseScanLines) XStretchBlt=XStretchBlt16SL;
   else              XStretchBlt=XStretchBlt16;
   if(iUseScanLines) BlitScreenNS=BlitScreen16NSSL;
   else              BlitScreenNS=BlitScreen16NS;
  }
 else
 if(iColDepth==15)
  {
   BlitScreen=BlitScreen15;iDesktopCol=15;
   if(iUseScanLines) XStretchBlt=XStretchBlt16SL;
   else              XStretchBlt=XStretchBlt16;
   if(iUseScanLines) BlitScreenNS=BlitScreen15NSSL;
   else              BlitScreenNS=BlitScreen15NS;
  }
 else
  {
   BlitScreen=BlitScreen32;iDesktopCol=32;
   if(iUseScanLines) XStretchBlt=XStretchBlt32SL;
   else              XStretchBlt=XStretchBlt32;
   if(iUseScanLines) BlitScreenNS=BlitScreen32NSSL;
   else              BlitScreenNS=BlitScreen32NS;
  }

 InitMenu();

 bIsFirstFrame = FALSE;                                // done

 if(iShowFPS)
  {
   iShowFPS=0;
   ulKeybits|=KEY_SHOWFPS;
   szDispBuf[0]=0;
   BuildDispMenu(0);
  }

 return 0;
}

////////////////////////////////////////////////////////////////////////

void Xcleanup()                                        // X CLEANUP
{
 CloseMenu();

 if(pBackBuffer)  free(pBackBuffer);
 pBackBuffer=0;
}

////////////////////////////////////////////////////////////////////////

unsigned long ulInitDisplay(void)
{
 CreateDisplay();                                      // x stuff
 Xinitialize();                                        // init x
 return (unsigned long)display;
}

////////////////////////////////////////////////////////////////////////

void CloseDisplay(void)
{
 Xcleanup();                                           // cleanup dx
 DestroyDisplay();
}

////////////////////////////////////////////////////////////////////////

void CreatePic(unsigned char * pMem)
{
 unsigned char * p=(unsigned char *)malloc(128*96*4);
 unsigned char * ps; int x,y;

 ps=p;

 if(iDesktopCol==16)
  {
   unsigned short s;
   for(y=0;y<96;y++)
    {
     for(x=0;x<128;x++)
      {
       s=(*(pMem+0))>>3;
       s|=((*(pMem+1))&0xfc)<<3;
       s|=((*(pMem+2))&0xf8)<<8;
       pMem+=3;
       *((unsigned short *)(ps+y*256+x*2))=s;
      }
    }
  }
 else
 if(iDesktopCol==15)
  {
   unsigned short s;
   for(y=0;y<96;y++)
    {
     for(x=0;x<128;x++)
      {
       s=(*(pMem+0))>>3;
       s|=((*(pMem+1))&0xfc)<<2;
       s|=((*(pMem+2))&0xf8)<<7;
       pMem+=3;
       *((unsigned short *)(ps+y*256+x*2))=s;
      }
    }
  }
 else
 if(iDesktopCol==32)
  {
   unsigned long l;
   for(y=0;y<96;y++)
    {
     for(x=0;x<128;x++)
      {
       l=  *(pMem+0);
       l|=(*(pMem+1))<<8;
       l|=(*(pMem+2))<<16;
       pMem+=3;
       *((unsigned long *)(ps+y*512+x*4))=l;
      }
    }
  }

 XPimage = SDL_CreateRGBSurfaceFrom((void *)p,128,96,
			depth,depth*16,
			0x00ff0000,0x0000ff00,0x000000ff,
			0);/*hmm what about having transparency?
			    *Set a nonzero value here.
			    *and set the ALPHA flag ON
			    */
}

///////////////////////////////////////////////////////////////////////////////////////

void DestroyPic(void)
{
 if(XPimage)
  {
   SDL_FillRect(display,NULL,0);
   SDL_FreeSurface(XPimage);
   XPimage=0;
  }
}

///////////////////////////////////////////////////////////////////////////////////////

void DisplayPic(void)
{
 rectdst.x=iResX-128;
 rectdst.y=0;
 rectdst.w=128;
 rectdst.h=96;
 SDL_BlitSurface(XPimage,NULL,display,&rectdst);
}

///////////////////////////////////////////////////////////////////////////////////////

void ShowGpuPic(void)
{
}

///////////////////////////////////////////////////////////////////////////////////////

void ShowTextGpuPic(void)
{
}

///////////////////////////////////////////////////////////////////////
