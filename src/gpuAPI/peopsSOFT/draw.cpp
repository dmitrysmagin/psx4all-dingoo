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

unsigned int   LUT16to32[65536];
unsigned int   RGBtoYUV[65536];

// prototypes
extern void hq2x_16( unsigned char * srcPtr, DWORD srcPitch, unsigned char * dstPtr, int width, int height);
extern void hq3x_16( unsigned char * srcPtr, DWORD srcPitch, unsigned char * dstPtr, int width, int height);
extern void hq2x_32( unsigned char * srcPtr, DWORD srcPitch, unsigned char * dstPtr, int width, int height);
extern void hq3x_32( unsigned char * srcPtr, DWORD srcPitch, unsigned char * dstPtr, int width, int height);
void NoStretchedBlit2x(void);
void NoStretchedBlit3x(void);
void StretchedBlit2x(void);
void StretchedBlit3x(void);



////////////////////////////////////////////////////////////////////////
// HQ Initialize Lookup Table
////////////////////////////////////////////////////////////////////////

int InitLUTs(void)
{
  int i, j, k, r, g, b, Y, u, v;
  int nMMXsupport = 0;
  for (i=0; i<65536; i++)
    LUT16to32[i] = ((i & 0xF800) << 8) + ((i & 0x07E0) << 5) + ((i & 0x001F) << 3);

  for (i=0; i<32; i++)
  for (j=0; j<64; j++)
  for (k=0; k<32; k++)
  {
    r = i << 3;
    g = j << 2;
    b = k << 3;
    Y = (r + g + b) >> 2;
    u = 128 + ((r - b) >> 2);
    v = 128 + ((-r + 2*g -b)>>3);
    RGBtoYUV[ (i << 11) + (j << 5) + k ] = (Y<<16) + (u<<8) + v;
  }

  return nMMXsupport;
}

////////////////////////////////////////////////////////////////////////
// generic 2xSaI helpers
////////////////////////////////////////////////////////////////////////

void *         pSaISmallBuff=NULL;
void *         pSaIBigBuff=NULL;

#define GET_RESULT(A, B, C, D) ((A != C || A != D) - (B != C || B != D))

static __inline int GetResult1(DWORD A, DWORD B, DWORD C, DWORD D, DWORD E)
{
 int x = 0;
 int y = 0;
 int r = 0;
 if (A == C) x+=1; else if (B == C) y+=1;
 if (A == D) x+=1; else if (B == D) y+=1;
 if (x <= 1) r+=1;
 if (y <= 1) r-=1;
 return r;
}

static __inline int GetResult2(DWORD A, DWORD B, DWORD C, DWORD D, DWORD E)
{
 int x = 0;
 int y = 0;
 int r = 0;
 if (A == C) x+=1; else if (B == C) y+=1;
 if (A == D) x+=1; else if (B == D) y+=1;
 if (x <= 1) r-=1;
 if (y <= 1) r+=1;
 return r;
}

#define colorMask8     0x00FEFEFE
#define lowPixelMask8  0x00010101
#define qcolorMask8    0x00FCFCFC
#define qlowpixelMask8 0x00030303

#define INTERPOLATE8(A, B) ((((A & colorMask8) >> 1) + ((B & colorMask8) >> 1) + (A & B & lowPixelMask8)))
#define Q_INTERPOLATE8(A, B, C, D) (((((A & qcolorMask8) >> 2) + ((B & qcolorMask8) >> 2) + ((C & qcolorMask8) >> 2) + ((D & qcolorMask8) >> 2) \
	+ ((((A & qlowpixelMask8) + (B & qlowpixelMask8) + (C & qlowpixelMask8) + (D & qlowpixelMask8)) >> 2) & qlowpixelMask8))))


void Super2xSaI_ex8(unsigned char *srcPtr, DWORD srcPitch,
	            unsigned char  *dstBitmap, int width, int height)
{
 DWORD dstPitch        = srcPitch<<1;
 DWORD srcPitchHalf    = srcPitch>>1;
 int   finWidth        = srcPitch>>2;
 DWORD line;
 DWORD *dP;
 DWORD *bP;
 int iXA,iXB,iXC,iYA,iYB,iYC,finish;
 DWORD color4, color5, color6;
 DWORD color1, color2, color3;
 DWORD colorA0, colorA1, colorA2, colorA3,
       colorB0, colorB1, colorB2, colorB3,
       colorS1, colorS2;
 DWORD product1a, product1b,
       product2a, product2b;

 line = 0;

  {
   for (; height; height-=1)
	{
     bP = (DWORD *)srcPtr;
	 dP = (DWORD *)(dstBitmap + line*dstPitch);
     for (finish = width; finish; finish -= 1 )
      {
//---------------------------------------    B1 B2
//                                         4  5  6 S2
//                                         1  2  3 S1
//                                           A1 A2
       if(finish==finWidth) iXA=0;
       else                 iXA=1;
       if(finish>4) {iXB=1;iXC=2;}
       else
       if(finish>3) {iXB=1;iXC=1;}
       else         {iXB=0;iXC=0;}
       if(line==0)  {iYA=0;}
       else         {iYA=finWidth;}
       if(height>4) {iYB=finWidth;iYC=srcPitchHalf;}
       else
       if(height>3) {iYB=finWidth;iYC=finWidth;}
       else         {iYB=0;iYC=0;}

       colorB0 = *(bP- iYA - iXA);
       colorB1 = *(bP- iYA);
       colorB2 = *(bP- iYA + iXB);
       colorB3 = *(bP- iYA + iXC);

       color4 = *(bP  - iXA);
       color5 = *(bP);
       color6 = *(bP  + iXB);
       colorS2 = *(bP + iXC);

       color1 = *(bP  + iYB  - iXA);
       color2 = *(bP  + iYB);
       color3 = *(bP  + iYB  + iXB);
       colorS1= *(bP  + iYB  + iXC);

       colorA0 = *(bP + iYC - iXA);
       colorA1 = *(bP + iYC);
       colorA2 = *(bP + iYC + iXB);
       colorA3 = *(bP + iYC + iXC);

       if (color2 == color6 && color5 != color3)
        {
         product2b = product1b = color2;
        }
       else
       if (color5 == color3 && color2 != color6)
        {
         product2b = product1b = color5;
        }
       else
       if (color5 == color3 && color2 == color6)
        {
         register int r = 0;

         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color1&0x00ffffff),  (colorA1&0x00ffffff));
         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color4&0x00ffffff),  (colorB1&0x00ffffff));
         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorA2&0x00ffffff), (colorS1&0x00ffffff));
         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorB2&0x00ffffff), (colorS2&0x00ffffff));

         if (r > 0)
          product2b = product1b = color6;
         else
         if (r < 0)
          product2b = product1b = color5;
         else
          {
           product2b = product1b = INTERPOLATE8(color5, color6);
          }
        }
       else
        {
         if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0)
             product2b = Q_INTERPOLATE8 (color3, color3, color3, color2);
         else
         if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3)
             product2b = Q_INTERPOLATE8 (color2, color2, color2, color3);
         else
             product2b = INTERPOLATE8 (color2, color3);

         if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0)
             product1b = Q_INTERPOLATE8 (color6, color6, color6, color5);
         else
         if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3)
             product1b = Q_INTERPOLATE8 (color6, color5, color5, color5);
         else
             product1b = INTERPOLATE8 (color5, color6);
        }

       if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2)
        product2a = INTERPOLATE8(color2, color5);
       else
       if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0)
        product2a = INTERPOLATE8(color2, color5);
       else
        product2a = color2;

       if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2)
        product1a = INTERPOLATE8(color2, color5);
       else
       if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0)
        product1a = INTERPOLATE8(color2, color5);
       else
        product1a = color5;

       *dP=product1a;
       *(dP+1)=product1b;
       *(dP+(srcPitchHalf))=product2a;
       *(dP+1+(srcPitchHalf))=product2b;

       bP += 1;
       dP += 2;
      }//end of for ( finish= width etc..)

     line += 2;
     srcPtr += srcPitch;
	}; //endof: for (; height; height--)
  }
}

////////////////////////////////////////////////////////////////////////

void Std2xSaI_ex8(unsigned char *srcPtr, DWORD srcPitch,
                  unsigned char *dstBitmap, int width, int height)
{
 DWORD dstPitch        = srcPitch<<1;
 DWORD srcPitchHalf    = srcPitch>>1;
 int   finWidth        = srcPitch>>2;
 DWORD line;
 DWORD *dP;
 DWORD *bP;
 int iXA,iXB,iXC,iYA,iYB,iYC,finish;

 DWORD colorA, colorB;
 DWORD colorC, colorD,
       colorE, colorF, colorG, colorH,
       colorI, colorJ, colorK, colorL,
       colorM, colorN, colorO, colorP;
 DWORD product, product1, product2;

 line = 0;

  {
   for (; height; height-=1)
	{
     bP = (DWORD *)srcPtr;
	 dP = (DWORD *)(dstBitmap + line*dstPitch);
     for (finish = width; finish; finish -= 1 )
      {
//---------------------------------------
// Map of the pixels:                    I|E F|J
//                                       G|A B|K
//                                       H|C D|L
//                                       M|N O|P
       if(finish==finWidth) iXA=0;
       else                 iXA=1;
       if(finish>4) {iXB=1;iXC=2;}
       else
       if(finish>3) {iXB=1;iXC=1;}
       else         {iXB=0;iXC=0;}
       if(line==0)  {iYA=0;}
       else         {iYA=finWidth;}
       if(height>4) {iYB=finWidth;iYC=srcPitchHalf;}
       else
       if(height>3) {iYB=finWidth;iYC=finWidth;}
       else         {iYB=0;iYC=0;}

       colorI = *(bP- iYA - iXA);
       colorE = *(bP- iYA);
       colorF = *(bP- iYA + iXB);
       colorJ = *(bP- iYA + iXC);

       colorG = *(bP  - iXA);
       colorA = *(bP);
       colorB = *(bP  + iXB);
       colorK = *(bP + iXC);

       colorH = *(bP  + iYB  - iXA);
       colorC = *(bP  + iYB);
       colorD = *(bP  + iYB  + iXB);
       colorL = *(bP  + iYB  + iXC);

       colorM = *(bP + iYC - iXA);
       colorN = *(bP + iYC);
       colorO = *(bP + iYC + iXB);
       colorP = *(bP + iYC + iXC);


       if((colorA == colorD) && (colorB != colorC))
        {
         if(((colorA == colorE) && (colorB == colorL)) ||
            ((colorA == colorC) && (colorA == colorF) &&
             (colorB != colorE) && (colorB == colorJ)))
          {
           product = colorA;
          }
         else
          {
           product = INTERPOLATE8(colorA, colorB);
          }

         if(((colorA == colorG) && (colorC == colorO)) ||
            ((colorA == colorB) && (colorA == colorH) &&
             (colorG != colorC) && (colorC == colorM)))
          {
           product1 = colorA;
          }
         else
          {
           product1 = INTERPOLATE8(colorA, colorC);
          }
         product2 = colorA;
        }
       else
       if((colorB == colorC) && (colorA != colorD))
        {
         if(((colorB == colorF) && (colorA == colorH)) ||
            ((colorB == colorE) && (colorB == colorD) &&
             (colorA != colorF) && (colorA == colorI)))
          {
           product = colorB;
          }
         else
          {
           product = INTERPOLATE8(colorA, colorB);
          }

         if(((colorC == colorH) && (colorA == colorF)) ||
            ((colorC == colorG) && (colorC == colorD) &&
             (colorA != colorH) && (colorA == colorI)))
          {
           product1 = colorC;
          }
         else
          {
           product1=INTERPOLATE8(colorA, colorC);
          }
         product2 = colorB;
        }
       else
       if((colorA == colorD) && (colorB == colorC))
        {
         if (colorA == colorB)
          {
           product = colorA;
           product1 = colorA;
           product2 = colorA;
          }
         else
          {
           register int r = 0;
           product1 = INTERPOLATE8(colorA, colorC);
           product = INTERPOLATE8(colorA, colorB);

           r += GetResult1 (colorA&0x00FFFFFF, colorB&0x00FFFFFF, colorG&0x00FFFFFF, colorE&0x00FFFFFF, colorI&0x00FFFFFF);
           r += GetResult2 (colorB&0x00FFFFFF, colorA&0x00FFFFFF, colorK&0x00FFFFFF, colorF&0x00FFFFFF, colorJ&0x00FFFFFF);
           r += GetResult2 (colorB&0x00FFFFFF, colorA&0x00FFFFFF, colorH&0x00FFFFFF, colorN&0x00FFFFFF, colorM&0x00FFFFFF);
           r += GetResult1 (colorA&0x00FFFFFF, colorB&0x00FFFFFF, colorL&0x00FFFFFF, colorO&0x00FFFFFF, colorP&0x00FFFFFF);

           if (r > 0)
            product2 = colorA;
           else
           if (r < 0)
            product2 = colorB;
           else
            {
             product2 = Q_INTERPOLATE8(colorA, colorB, colorC, colorD);
            }
          }
        }
       else
        {
         product2 = Q_INTERPOLATE8(colorA, colorB, colorC, colorD);

         if ((colorA == colorC) && (colorA == colorF) &&
             (colorB != colorE) && (colorB == colorJ))
          {
           product = colorA;
          }
         else
         if ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI))
          {
           product = colorB;
          }
         else
          {
           product = INTERPOLATE8(colorA, colorB);
          }

         if ((colorA == colorB) && (colorA == colorH) &&
             (colorG != colorC) && (colorC == colorM))
          {
           product1 = colorA;
          }
         else
         if ((colorC == colorG) && (colorC == colorD) &&
             (colorA != colorH) && (colorA == colorI))
          {
           product1 = colorC;
          }
         else
          {
           product1 = INTERPOLATE8(colorA, colorC);
          }
        }

//////////////////////////

       *dP=colorA;
       *(dP+1)=product;
       *(dP+(srcPitchHalf))=product1;
       *(dP+1+(srcPitchHalf))=product2;

       bP += 1;
       dP += 2;
      }//end of for ( finish= width etc..)

     line += 2;
     srcPtr += srcPitch;
	}; //endof: for (; height; height--)
  }
}

////////////////////////////////////////////////////////////////////////

void SuperEagle_ex8(unsigned char *srcPtr, DWORD srcPitch,
	                unsigned char  *dstBitmap, int width, int height)
{
 DWORD dstPitch        = srcPitch<<1;
 DWORD srcPitchHalf    = srcPitch>>1;
 int   finWidth        = srcPitch>>2;
 DWORD line;
 DWORD *dP;
 DWORD *bP;
 int iXA,iXB,iXC,iYA,iYB,iYC,finish;
 DWORD color4, color5, color6;
 DWORD color1, color2, color3;
 DWORD colorA1, colorA2,
       colorB1, colorB2,
       colorS1, colorS2;
 DWORD product1a, product1b,
       product2a, product2b;

 line = 0;

  {
   for (; height; height-=1)
	{
     bP = (DWORD *)srcPtr;
	 dP = (DWORD *)(dstBitmap + line*dstPitch);
     for (finish = width; finish; finish -= 1 )
      {
       if(finish==finWidth) iXA=0;
       else                 iXA=1;
       if(finish>4) {iXB=1;iXC=2;}
       else
       if(finish>3) {iXB=1;iXC=1;}
       else         {iXB=0;iXC=0;}
       if(line==0)  {iYA=0;}
       else         {iYA=finWidth;}
       if(height>4) {iYB=finWidth;iYC=srcPitchHalf;}
       else
       if(height>3) {iYB=finWidth;iYC=finWidth;}
       else         {iYB=0;iYC=0;}

       colorB1 = *(bP- iYA);
       colorB2 = *(bP- iYA + iXB);

       color4 = *(bP  - iXA);
       color5 = *(bP);
       color6 = *(bP  + iXB);
       colorS2 = *(bP + iXC);

       color1 = *(bP  + iYB  - iXA);
       color2 = *(bP  + iYB);
       color3 = *(bP  + iYB  + iXB);
       colorS1= *(bP  + iYB  + iXC);

       colorA1 = *(bP + iYC);
       colorA2 = *(bP + iYC + iXB);

       if(color2 == color6 && color5 != color3)
        {
         product1b = product2a = color2;
         if((color1 == color2) ||
            (color6 == colorB2))
          {
           product1a = INTERPOLATE8(color2, color5);
           product1a = INTERPOLATE8(color2, product1a);
          }
         else
          {
           product1a = INTERPOLATE8(color5, color6);
          }

         if((color6 == colorS2) ||
            (color2 == colorA1))
          {
           product2b = INTERPOLATE8(color2, color3);
           product2b = INTERPOLATE8(color2, product2b);
          }
         else
          {
           product2b = INTERPOLATE8(color2, color3);
          }
        }
       else
       if (color5 == color3 && color2 != color6)
        {
         product2b = product1a = color5;

         if ((colorB1 == color5) ||
             (color3 == colorS1))
          {
           product1b = INTERPOLATE8(color5, color6);
           product1b = INTERPOLATE8(color5, product1b);
          }
         else
          {
           product1b = INTERPOLATE8(color5, color6);
          }

         if ((color3 == colorA2) ||
             (color4 == color5))
          {
           product2a = INTERPOLATE8(color5, color2);
           product2a = INTERPOLATE8(color5, product2a);
          }
         else
          {
           product2a = INTERPOLATE8(color2, color3);
          }
        }
       else
       if (color5 == color3 && color2 == color6)
        {
         register int r = 0;

         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color1&0x00ffffff),  (colorA1&0x00ffffff));
         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color4&0x00ffffff),  (colorB1&0x00ffffff));
         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorA2&0x00ffffff), (colorS1&0x00ffffff));
         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorB2&0x00ffffff), (colorS2&0x00ffffff));

         if (r > 0)
          {
           product1b = product2a = color2;
           product1a = product2b = INTERPOLATE8(color5, color6);
          }
         else
         if (r < 0)
          {
           product2b = product1a = color5;
           product1b = product2a = INTERPOLATE8(color5, color6);
          }
         else
          {
           product2b = product1a = color5;
           product1b = product2a = color2;
          }
        }
       else
        {
         product2b = product1a = INTERPOLATE8(color2, color6);
         product2b = Q_INTERPOLATE8(color3, color3, color3, product2b);
         product1a = Q_INTERPOLATE8(color5, color5, color5, product1a);

         product2a = product1b = INTERPOLATE8(color5, color3);
         product2a = Q_INTERPOLATE8(color2, color2, color2, product2a);
         product1b = Q_INTERPOLATE8(color6, color6, color6, product1b);
        }

////////////////////////////////

       *dP=product1a;
       *(dP+1)=product1b;
       *(dP+(srcPitchHalf))=product2a;
       *(dP+1+(srcPitchHalf))=product2b;

       bP += 1;
       dP += 2;
      }//end of for ( finish= width etc..)

     line += 2;
     srcPtr += srcPitch;
	}; //endof: for (; height; height--)
  }
}

/////////////////////////

static __inline void scale2x_32_def_whole(unsigned long* dst0, unsigned long* dst1, const unsigned long* src0, const unsigned long* src1, const unsigned long* src2, unsigned count)
{

	// first pixel
	if (src0[0] != src2[0] && src1[0] != src1[1]) {
		dst0[0] = src1[0] == src0[0] ? src0[0] : src1[0];
		dst0[1] = src1[1] == src0[0] ? src0[0] : src1[0];
		dst1[0] = src1[0] == src2[0] ? src2[0] : src1[0];
		dst1[1] = src1[1] == src2[0] ? src2[0] : src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
	}
	++src0;
	++src1;
	++src2;
	dst0 += 2;
	dst1 += 2;

	// central pixels
	count -= 2;
	while (count) {
		if (src0[0] != src2[0] && src1[-1] != src1[1]) {
			dst0[0] = src1[-1] == src0[0] ? src0[0] : src1[0];
			dst0[1] = src1[1] == src0[0] ? src0[0] : src1[0];
			dst1[0] = src1[-1] == src2[0] ? src2[0] : src1[0];
			dst1[1] = src1[1] == src2[0] ? src2[0] : src1[0];
		} else {
			dst0[0] = src1[0];
			dst0[1] = src1[0];
			dst1[0] = src1[0];
			dst1[1] = src1[0];
		}

		++src0;
		++src1;
		++src2;
		dst0 += 2;
		dst1 += 2;
		--count;
	}

	// last pixel
	if (src0[0] != src2[0] && src1[-1] != src1[0]) {
		dst0[0] = src1[-1] == src0[0] ? src0[0] : src1[0];
		dst0[1] = src1[0] == src0[0] ? src0[0] : src1[0];
		dst1[0] = src1[-1] == src2[0] ? src2[0] : src1[0];
		dst1[1] = src1[0] == src2[0] ? src2[0] : src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
	}
}



#ifndef MAX
#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))
#endif

void Scale2x_ex8(unsigned char *srcPtr, DWORD srcPitch,
				 unsigned char  *dstPtr, int width, int height)
{
#ifdef _WINDOWS
#else

  int looph, loopw;

 unsigned char * srcpix = srcPtr;
 unsigned char * dstpix = dstPtr;

 const int srcpitch = srcPitch;
 const int dstpitch = srcPitch<<1;

 unsigned long E0, E1, E2, E3, B, D, E, F, H;
 for(looph = 0; looph < height; ++looph)
  {
   for(loopw = 0; loopw < width; ++ loopw)
    {
     B = *(unsigned long*)(srcpix + (MAX(0,looph-1)*srcpitch) + (4*loopw));
     D = *(unsigned long*)(srcpix + (looph*srcpitch) + (4*MAX(0,loopw-1)));
     E = *(unsigned long*)(srcpix + (looph*srcpitch) + (4*loopw));
     F = *(unsigned long*)(srcpix + (looph*srcpitch) + (4*MIN(width-1,loopw+1)));
     H = *(unsigned long*)(srcpix + (MIN(height-1,looph+1)*srcpitch) + (4*loopw));

		    if (B != H && D != F) {
				E0 = D == B ? D : E;
				E1 = B == F ? F : E;
				E2 = D == H ? D : E;
				E3 = H == F ? F : E;
			} else {
				E0 = E;
				E1 = E;
				E2 = E;
				E3 = E;
			}


     *(unsigned long*)(dstpix + looph*2*dstpitch + loopw*2*4) = E0;
     *(unsigned long*)(dstpix + looph*2*dstpitch + (loopw*2+1)*4) = E1;
     *(unsigned long*)(dstpix + (looph*2+1)*dstpitch + loopw*2*4) = E2;
     *(unsigned long*)(dstpix + (looph*2+1)*dstpitch + (loopw*2+1)*4) = E3;
    }
  }
#endif
}

static __inline void scale3x_16_def_whole(unsigned short* dst0, unsigned short* dst1, unsigned short* dst2, const unsigned short* src0, const unsigned short* src1, const unsigned short* src2, unsigned count)
{
	// first pixel
	if (src0[0] != src2[0] && src1[0] != src1[1]) {
		dst0[0] = src1[0];
		dst0[1] = (src1[0] == src0[0] && src1[0] != src0[1]) || (src1[1] == src0[0] && src1[0] != src0[0]) ? src0[0] : src1[0];
		dst0[2] = src1[1] == src0[0] ? src1[1] : src1[0];
		dst1[0] = (src1[0] == src0[0] && src1[0] != src2[0]) || (src1[0] == src2[0] && src1[0] != src0[0]) ? src1[0] : src1[0];
		dst1[1] = src1[0];
		dst1[2] = (src1[1] == src0[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src0[1]) ? src1[1] : src1[0];
		dst2[0] = src1[0];
		dst2[1] = (src1[0] == src2[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src2[0]) ? src2[0] : src1[0];
		dst2[2] = src1[1] == src2[0] ? src1[1] : src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst0[2] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
		dst1[2] = src1[0];
		dst2[0] = src1[0];
		dst2[1] = src1[0];
		dst2[2] = src1[0];
	}
	++src0;
	++src1;
	++src2;
	dst0 += 3;
	dst1 += 3;
	dst2 += 3;

	// central pixels
	count -= 2;
	while (count) {
		if (src0[0] != src2[0] && src1[-1] != src1[1]) {
			dst0[0] = src1[-1] == src0[0] ? src1[-1] : src1[0];
			dst0[1] = (src1[-1] == src0[0] && src1[0] != src0[1]) || (src1[1] == src0[0] && src1[0] != src0[-1]) ? src0[0] : src1[0];
			dst0[2] = src1[1] == src0[0] ? src1[1] : src1[0];
			dst1[0] = (src1[-1] == src0[0] && src1[0] != src2[-1]) || (src1[-1] == src2[0] && src1[0] != src0[-1]) ? src1[-1] : src1[0];
			dst1[1] = src1[0];
			dst1[2] = (src1[1] == src0[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src0[1]) ? src1[1] : src1[0];
			dst2[0] = src1[-1] == src2[0] ? src1[-1] : src1[0];
			dst2[1] = (src1[-1] == src2[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src2[-1]) ? src2[0] : src1[0];
			dst2[2] = src1[1] == src2[0] ? src1[1] : src1[0];
		} else {
			dst0[0] = src1[0];
			dst0[1] = src1[0];
			dst0[2] = src1[0];
			dst1[0] = src1[0];
			dst1[1] = src1[0];
			dst1[2] = src1[0];
			dst2[0] = src1[0];
			dst2[1] = src1[0];
			dst2[2] = src1[0];
		}

		++src0;
		++src1;
		++src2;
		dst0 += 3;
		dst1 += 3;
		dst2 += 3;
		--count;
	}

	// last pixel
	if (src0[0] != src2[0] && src1[-1] != src1[0]) {
		dst0[0] = src1[-1] == src0[0] ? src1[-1] : src1[0];
		dst0[1] = (src1[-1] == src0[0] && src1[0] != src0[0]) || (src1[0] == src0[0] && src1[0] != src0[-1]) ? src0[0] : src1[0];
		dst0[2] = src1[0];
		dst1[0] = (src1[-1] == src0[0] && src1[0] != src2[-1]) || (src1[-1] == src2[0] && src1[0] != src0[-1]) ? src1[-1] : src1[0];
		dst1[1] = src1[0];
		dst1[2] = (src1[0] == src0[0] && src1[0] != src2[0]) || (src1[0] == src2[0] && src1[0] != src0[0]) ? src1[0] : src1[0];
		dst2[0] = src1[-1] == src2[0] ? src1[-1] : src1[0];
		dst2[1] = (src1[-1] == src2[0] && src1[0] != src2[0]) || (src1[0] == src2[0] && src1[0] != src2[-1]) ? src2[0] : src1[0];
		dst2[2] = src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst0[2] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
		dst1[2] = src1[0];
		dst2[0] = src1[0];
		dst2[1] = src1[0];
		dst2[2] = src1[0];
	}
}


static __inline void scale3x_32_def_whole(unsigned long* dst0, unsigned long* dst1, unsigned long* dst2, const unsigned long* src0, const unsigned long* src1, const unsigned long* src2, unsigned count)
{

	// first pixel
	if (src0[0] != src2[0] && src1[0] != src1[1]) {
		dst0[0] = src1[0];
		dst0[1] = (src1[0] == src0[0] && src1[0] != src0[1]) || (src1[1] == src0[0] && src1[0] != src0[0]) ? src0[0] : src1[0];
		dst0[2] = src1[1] == src0[0] ? src1[1] : src1[0];
		dst1[0] = (src1[0] == src0[0] && src1[0] != src2[0]) || (src1[0] == src2[0] && src1[0] != src0[0]) ? src1[0] : src1[0];
		dst1[1] = src1[0];
		dst1[2] = (src1[1] == src0[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src0[1]) ? src1[1] : src1[0];
		dst2[0] = src1[0];
		dst2[1] = (src1[0] == src2[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src2[0]) ? src2[0] : src1[0];
		dst2[2] = src1[1] == src2[0] ? src1[1] : src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst0[2] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
		dst1[2] = src1[0];
		dst2[0] = src1[0];
		dst2[1] = src1[0];
		dst2[2] = src1[0];
	}
	++src0;
	++src1;
	++src2;
	dst0 += 3;
	dst1 += 3;
	dst2 += 3;

	// central pixels
	count -= 2;
	while (count) {
		if (src0[0] != src2[0] && src1[-1] != src1[1]) {
			dst0[0] = src1[-1] == src0[0] ? src1[-1] : src1[0];
			dst0[1] = (src1[-1] == src0[0] && src1[0] != src0[1]) || (src1[1] == src0[0] && src1[0] != src0[-1]) ? src0[0] : src1[0];
			dst0[2] = src1[1] == src0[0] ? src1[1] : src1[0];
			dst1[0] = (src1[-1] == src0[0] && src1[0] != src2[-1]) || (src1[-1] == src2[0] && src1[0] != src0[-1]) ? src1[-1] : src1[0];
			dst1[1] = src1[0];
			dst1[2] = (src1[1] == src0[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src0[1]) ? src1[1] : src1[0];
			dst2[0] = src1[-1] == src2[0] ? src1[-1] : src1[0];
			dst2[1] = (src1[-1] == src2[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src2[-1]) ? src2[0] : src1[0];
			dst2[2] = src1[1] == src2[0] ? src1[1] : src1[0];
		} else {
			dst0[0] = src1[0];
			dst0[1] = src1[0];
			dst0[2] = src1[0];
			dst1[0] = src1[0];
			dst1[1] = src1[0];
			dst1[2] = src1[0];
			dst2[0] = src1[0];
			dst2[1] = src1[0];
			dst2[2] = src1[0];
		}

		++src0;
		++src1;
		++src2;
		dst0 += 3;
		dst1 += 3;
		dst2 += 3;
		--count;
	}

	// last pixel
	if (src0[0] != src2[0] && src1[-1] != src1[0]) {
		dst0[0] = src1[-1] == src0[0] ? src1[-1] : src1[0];
		dst0[1] = (src1[-1] == src0[0] && src1[0] != src0[0]) || (src1[0] == src0[0] && src1[0] != src0[-1]) ? src0[0] : src1[0];
		dst0[2] = src1[0];
		dst1[0] = (src1[-1] == src0[0] && src1[0] != src2[-1]) || (src1[-1] == src2[0] && src1[0] != src0[-1]) ? src1[-1] : src1[0];
		dst1[1] = src1[0];
		dst1[2] = (src1[0] == src0[0] && src1[0] != src2[0]) || (src1[0] == src2[0] && src1[0] != src0[0]) ? src1[0] : src1[0];
		dst2[0] = src1[-1] == src2[0] ? src1[-1] : src1[0];
		dst2[1] = (src1[-1] == src2[0] && src1[0] != src2[0]) || (src1[0] == src2[0] && src1[0] != src2[-1]) ? src2[0] : src1[0];
		dst2[2] = src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst0[2] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
		dst1[2] = src1[0];
		dst2[0] = src1[0];
		dst2[1] = src1[0];
		dst2[2] = src1[0];
	}
}

void Scale3x_ex6_5(unsigned char *srcPtr, DWORD srcPitch,
				   unsigned char  *dstPtr, int width, int height)
{
#ifdef _WINDOWS
#else

  int looph, loopw;

 unsigned char * srcpix = srcPtr;
 unsigned char * dstpix = dstPtr;

 const int srcpitch = srcPitch;
 const int dstpitch = srcPitch*3;

 unsigned short E0, E1, E2, E3, E4, E5, E6, E7, E8;
 unsigned short A, B, C, D, E, F, G, H, I;
 for(looph = 0; looph < height; ++looph)
  {
   for(loopw = 0; loopw < width; ++ loopw)
    {
     A = *(unsigned short*)(srcpix + (MAX(0,looph-1)*srcpitch) + (2*MAX(0,loopw-1)));
     B = *(unsigned short*)(srcpix + (MAX(0,looph-1)*srcpitch) + (2*loopw));
     C = *(unsigned short*)(srcpix + (MAX(0,looph-1)*srcpitch) + (2*MIN(width-1,loopw+1)));
     D = *(unsigned short*)(srcpix + (looph*srcpitch) + (2*MAX(0,loopw-1)));
     E = *(unsigned short*)(srcpix + (looph*srcpitch) + (2*loopw));
     F = *(unsigned short*)(srcpix + (looph*srcpitch) + (2*MIN(width-1,loopw+1)));
     G = *(unsigned short*)(srcpix + (MIN(height-1,looph+1)*srcpitch) + (2*MAX(0,loopw-1)));
     H = *(unsigned short*)(srcpix + (MIN(height-1,looph+1)*srcpitch) + (2*loopw));
     I = *(unsigned short*)(srcpix + (MIN(height-1,looph+1)*srcpitch) + (2*MIN(width-1,loopw+1)));

	if (B != H && D != F) {
 	   E0 = D == B ? D : E;
	   E1 = (D == B && E != C) || (B == F && E != A) ? B : E;
	   E2 = B == F ? F : E;
    	   E3 = (D == B && E != G) || (D == H && E != A) ? D : E;
	   E4 = E;
    	   E5 = (B == F && E != I) || (H == F && E != C) ? F : E;
    	   E6 = D == H ? D : E;
           E7 = (D == H && E != I) || (H == F && E != G) ? H : E;
           E8 = H == F ? F : E;
	} else {
    	   E0 = E;
    	   E1 = E;
    	   E2 = E;
    	   E3 = E;
    	   E4 = E;
    	   E5 = E;
    	   E6 = E;
    	   E7 = E;
    	   E8 = E;
}


     *(unsigned long*)(dstpix + looph*3*dstpitch + loopw*3*2) = E0;
     *(unsigned long*)(dstpix + looph*3*dstpitch + (loopw*3+1)*2) = E1;
     *(unsigned long*)(dstpix + looph*3*dstpitch + (loopw*3+2)*2) = E2;
     *(unsigned long*)(dstpix + (looph*3+1)*dstpitch + loopw*3*2) = E3;
     *(unsigned long*)(dstpix + (looph*3+1)*dstpitch + (loopw*3+1)*2) = E4;
     *(unsigned long*)(dstpix + (looph*3+1)*dstpitch + (loopw*3+2)*2) = E5;
     *(unsigned long*)(dstpix + (looph*3+2)*dstpitch + loopw*3*2) = E6;
     *(unsigned long*)(dstpix + (looph*3+2)*dstpitch + (loopw*3+1)*2) = E7;
     *(unsigned long*)(dstpix + (looph*3+2)*dstpitch + (loopw*3+2)*2) = E8;
    }
  }
#endif
}

void Scale3x_ex8(unsigned char *srcPtr, DWORD srcPitch,
				 unsigned char  *dstPtr, int width, int height)
{
#ifdef _WINDOWS
#else

  int looph, loopw;

 unsigned char * srcpix = srcPtr;
 unsigned char * dstpix = dstPtr;

 const int srcpitch = srcPitch;
 const int dstpitch = srcPitch*3;

 unsigned long E0, E1, E2, E3, E4, E5, E6, E7, E8;
 unsigned long A, B, C, D, E, F, G, H, I;
 for(looph = 0; looph < height; ++looph)
  {
   for(loopw = 0; loopw < width; ++ loopw)
    {
     A = *(unsigned long*)(srcpix + (MAX(0,looph-1)*srcpitch) + (4*MAX(0,loopw-1)));
     B = *(unsigned long*)(srcpix + (MAX(0,looph-1)*srcpitch) + (4*loopw));
     C = *(unsigned long*)(srcpix + (MAX(0,looph-1)*srcpitch) + (4*MIN(width-1,loopw+1)));
     D = *(unsigned long*)(srcpix + (looph*srcpitch) + (4*MAX(0,loopw-1)));
     E = *(unsigned long*)(srcpix + (looph*srcpitch) + (4*loopw));
     F = *(unsigned long*)(srcpix + (looph*srcpitch) + (4*MIN(width-1,loopw+1)));
     G = *(unsigned long*)(srcpix + (MIN(height-1,looph+1)*srcpitch) + (4*MAX(0,loopw-1)));
     H = *(unsigned long*)(srcpix + (MIN(height-1,looph+1)*srcpitch) + (4*loopw));
     I = *(unsigned long*)(srcpix + (MIN(height-1,looph+1)*srcpitch) + (4*MIN(width-1,loopw+1)));

	if (B != H && D != F) {
 	   E0 = D == B ? D : E;
	   E1 = (D == B && E != C) || (B == F && E != A) ? B : E;
	   E2 = B == F ? F : E;
    	   E3 = (D == B && E != G) || (D == H && E != A) ? D : E;
	   E4 = E;
    	   E5 = (B == F && E != I) || (H == F && E != C) ? F : E;
    	   E6 = D == H ? D : E;
           E7 = (D == H && E != I) || (H == F && E != G) ? H : E;
           E8 = H == F ? F : E;
	} else {
    	   E0 = E;
    	   E1 = E;
    	   E2 = E;
    	   E3 = E;
    	   E4 = E;
    	   E5 = E;
    	   E6 = E;
    	   E7 = E;
    	   E8 = E;
}


     *(unsigned long*)(dstpix + looph*3*dstpitch + loopw*3*4) = E0;
     *(unsigned long*)(dstpix + looph*3*dstpitch + (loopw*3+1)*4) = E1;
     *(unsigned long*)(dstpix + looph*3*dstpitch + (loopw*3+2)*4) = E2;
     *(unsigned long*)(dstpix + (looph*3+1)*dstpitch + loopw*3*4) = E3;
     *(unsigned long*)(dstpix + (looph*3+1)*dstpitch + (loopw*3+1)*4) = E4;
     *(unsigned long*)(dstpix + (looph*3+1)*dstpitch + (loopw*3+2)*4) = E5;
     *(unsigned long*)(dstpix + (looph*3+2)*dstpitch + loopw*3*4) = E6;
     *(unsigned long*)(dstpix + (looph*3+2)*dstpitch + (loopw*3+1)*4) = E7;
     *(unsigned long*)(dstpix + (looph*3+2)*dstpitch + (loopw*3+2)*4) = E8;
    }
  }
#endif
}


////////////////////////////////////////////////////////////////////////

#define colorMask6     0x0000F7DE
#define lowPixelMask6  0x00000821
#define qcolorMask6    0x0000E79c
#define qlowpixelMask6 0x00001863

#define INTERPOLATE6(A, B) ((((A & colorMask6) >> 1) + ((B & colorMask6) >> 1) + (A & B & lowPixelMask6)))
#define Q_INTERPOLATE6(A, B, C, D) (((((A & qcolorMask6) >> 2) + ((B & qcolorMask6) >> 2) + ((C & qcolorMask6) >> 2) + ((D & qcolorMask6) >> 2) \
	+ ((((A & qlowpixelMask6) + (B & qlowpixelMask6) + (C & qlowpixelMask6) + (D & qlowpixelMask6)) >> 2) & qlowpixelMask6))))

void Super2xSaI_ex6(unsigned char *srcPtr, DWORD srcPitch,
	            unsigned char  *dstBitmap, int width, int height)
{
 DWORD dstPitch        = srcPitch<<1;
 int   finWidth        = srcPitch>>1;
 DWORD line;
 unsigned short *dP;
 unsigned short *bP;
 int iXA,iXB,iXC,iYA,iYB,iYC,finish;
 DWORD color4, color5, color6;
 DWORD color1, color2, color3;
 DWORD colorA0, colorA1, colorA2, colorA3,
       colorB0, colorB1, colorB2, colorB3,
       colorS1, colorS2;
 DWORD product1a, product1b,
       product2a, product2b;

 line = 0;

  {
   for (; height; height-=1)
	{
     bP = (unsigned short *)srcPtr;
	 dP = (unsigned short *)(dstBitmap + line*dstPitch);
     for (finish = width; finish; finish -= 1 )
      {
//---------------------------------------    B1 B2
//                                         4  5  6 S2
//                                         1  2  3 S1
//                                           A1 A2
       if(finish==finWidth) iXA=0;
       else                 iXA=1;
       if(finish>4) {iXB=1;iXC=2;}
       else
       if(finish>3) {iXB=1;iXC=1;}
       else         {iXB=0;iXC=0;}
       if(line==0) iYA=0;
       else        iYA=finWidth;
       if(height>4) {iYB=finWidth;iYC=srcPitch;}
       else
       if(height>3) {iYB=finWidth;iYC=finWidth;}
       else         {iYB=0;iYC=0;}


       colorB0 = *(bP- iYA - iXA);
       colorB1 = *(bP- iYA);
       colorB2 = *(bP- iYA + iXB);
       colorB3 = *(bP- iYA + iXC);

       color4 = *(bP  - iXA);
       color5 = *(bP);
       color6 = *(bP  + iXB);
       colorS2 = *(bP + iXC);

       color1 = *(bP  + iYB  - iXA);
       color2 = *(bP  + iYB);
       color3 = *(bP  + iYB  + iXB);
       colorS1= *(bP  + iYB  + iXC);

       colorA0 = *(bP + iYC - iXA);
       colorA1 = *(bP + iYC);
       colorA2 = *(bP + iYC + iXB);
       colorA3 = *(bP + iYC + iXC);

//--------------------------------------
       if (color2 == color6 && color5 != color3)
        {
         product2b = product1b = color2;
        }
       else
       if (color5 == color3 && color2 != color6)
        {
         product2b = product1b = color5;
        }
       else
       if (color5 == color3 && color2 == color6)
        {
         register int r = 0;

         r += GET_RESULT ((color6), (color5), (color1),  (colorA1));
         r += GET_RESULT ((color6), (color5), (color4),  (colorB1));
         r += GET_RESULT ((color6), (color5), (colorA2), (colorS1));
         r += GET_RESULT ((color6), (color5), (colorB2), (colorS2));

         if (r > 0)
          product2b = product1b = color6;
         else
         if (r < 0)
          product2b = product1b = color5;
         else
          {
           product2b = product1b = INTERPOLATE6 (color5, color6);
          }
        }
       else
        {
         if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0)
             product2b = Q_INTERPOLATE6 (color3, color3, color3, color2);
         else
         if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3)
             product2b = Q_INTERPOLATE6 (color2, color2, color2, color3);
         else
             product2b = INTERPOLATE6 (color2, color3);

         if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0)
             product1b = Q_INTERPOLATE6 (color6, color6, color6, color5);
         else
         if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3)
             product1b = Q_INTERPOLATE6 (color6, color5, color5, color5);
         else
             product1b = INTERPOLATE6 (color5, color6);
        }

       if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2)
        product2a = INTERPOLATE6 (color2, color5);
       else
       if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0)
        product2a = INTERPOLATE6(color2, color5);
       else
        product2a = color2;

       if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2)
        product1a = INTERPOLATE6(color2, color5);
       else
       if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0)
        product1a = INTERPOLATE6(color2, color5);
       else
        product1a = color5;

       *dP=(unsigned short)product1a;
       *(dP+1)=(unsigned short)product1b;
       *(dP+(srcPitch))=(unsigned short)product2a;
       *(dP+1+(srcPitch))=(unsigned short)product2b;

       bP += 1;
       dP += 2;
      }//end of for ( finish= width etc..)

     line += 2;
     srcPtr += srcPitch;
	}; //endof: for (; height; height--)
  }
}

////////////////////////////////////////////////////////////////////////

void Std2xSaI_ex6(unsigned char *srcPtr, DWORD srcPitch,
	            unsigned char  *dstBitmap, int width, int height)
{
 DWORD dstPitch        = srcPitch<<1;
 int   finWidth        = srcPitch>>1;
 DWORD line;
 unsigned short *dP;
 unsigned short *bP;
 int iXA,iXB,iXC,iYA,iYB,iYC,finish;

 DWORD colorA, colorB;
 DWORD colorC, colorD,
       colorE, colorF, colorG, colorH,
       colorI, colorJ, colorK, colorL,
       colorM, colorN, colorO, colorP;
 DWORD product, product1, product2;

 line = 0;

  {
   for (; height; height-=1)
	{
     bP = (unsigned short *)srcPtr;
	 dP = (unsigned short *)(dstBitmap + line*dstPitch);
     for (finish = width; finish; finish -= 1 )
      {
//---------------------------------------
// Map of the pixels:                    I|E F|J
//                                       G|A B|K
//                                       H|C D|L
//                                       M|N O|P
       if(finish==finWidth) iXA=0;
       else                 iXA=1;
       if(finish>4) {iXB=1;iXC=2;}
       else
       if(finish>3) {iXB=1;iXC=1;}
       else         {iXB=0;iXC=0;}
       if(line==0) iYA=0;
       else        iYA=finWidth;
       if(height>4) {iYB=finWidth;iYC=srcPitch;}
       else
       if(height>3) {iYB=finWidth;iYC=finWidth;}
       else         {iYB=0;iYC=0;}

       colorI = *(bP- iYA - iXA);
       colorE = *(bP- iYA);
       colorF = *(bP- iYA + iXB);
       colorJ = *(bP- iYA + iXC);

       colorG = *(bP  - iXA);
       colorA = *(bP);
       colorB = *(bP  + iXB);
       colorK = *(bP + iXC);

       colorH = *(bP  + iYB  - iXA);
       colorC = *(bP  + iYB);
       colorD = *(bP  + iYB  + iXB);
       colorL = *(bP  + iYB  + iXC);

       colorM = *(bP + iYC - iXA);
       colorN = *(bP + iYC);
       colorO = *(bP + iYC + iXB);
       colorP = *(bP + iYC + iXC);

       if((colorA == colorD) && (colorB != colorC))
        {
         if(((colorA == colorE) && (colorB == colorL)) ||
            ((colorA == colorC) && (colorA == colorF) &&
             (colorB != colorE) && (colorB == colorJ)))
          {
           product = colorA;
          }
         else
          {
           product = INTERPOLATE6(colorA, colorB);
          }

         if(((colorA == colorG) && (colorC == colorO)) ||
            ((colorA == colorB) && (colorA == colorH) &&
             (colorG != colorC) && (colorC == colorM)))
          {
           product1 = colorA;
          }
         else
          {
           product1 = INTERPOLATE6(colorA, colorC);
          }
         product2 = colorA;
        }
       else
       if((colorB == colorC) && (colorA != colorD))
        {
         if(((colorB == colorF) && (colorA == colorH)) ||
            ((colorB == colorE) && (colorB == colorD) &&
             (colorA != colorF) && (colorA == colorI)))
          {
           product = colorB;
          }
         else
          {
           product = INTERPOLATE6(colorA, colorB);
          }

         if(((colorC == colorH) && (colorA == colorF)) ||
            ((colorC == colorG) && (colorC == colorD) &&
             (colorA != colorH) && (colorA == colorI)))
          {
           product1 = colorC;
          }
         else
          {
           product1=INTERPOLATE6(colorA, colorC);
          }
         product2 = colorB;
        }
       else
       if((colorA == colorD) && (colorB == colorC))
        {
         if (colorA == colorB)
          {
           product = colorA;
           product1 = colorA;
           product2 = colorA;
          }
         else
          {
           register int r = 0;
           product1 = INTERPOLATE6(colorA, colorC);
           product = INTERPOLATE6(colorA, colorB);

           r += GetResult1 (colorA, colorB, colorG, colorE, colorI);
           r += GetResult2 (colorB, colorA, colorK, colorF, colorJ);
           r += GetResult2 (colorB, colorA, colorH, colorN, colorM);
           r += GetResult1 (colorA, colorB, colorL, colorO, colorP);

           if (r > 0)
            product2 = colorA;
           else
           if (r < 0)
            product2 = colorB;
           else
            {
             product2 = Q_INTERPOLATE6(colorA, colorB, colorC, colorD);
            }
          }
        }
       else
        {
         product2 = Q_INTERPOLATE6(colorA, colorB, colorC, colorD);

         if ((colorA == colorC) && (colorA == colorF) &&
             (colorB != colorE) && (colorB == colorJ))
          {
           product = colorA;
          }
         else
         if ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI))
          {
           product = colorB;
          }
         else
          {
           product = INTERPOLATE6(colorA, colorB);
          }

         if ((colorA == colorB) && (colorA == colorH) &&
             (colorG != colorC) && (colorC == colorM))
          {
           product1 = colorA;
          }
         else
         if ((colorC == colorG) && (colorC == colorD) &&
             (colorA != colorH) && (colorA == colorI))
          {
           product1 = colorC;
          }
         else
          {
           product1 = INTERPOLATE6(colorA, colorC);
          }
        }

       *dP=(unsigned short)colorA;
       *(dP+1)=(unsigned short)product;
       *(dP+(srcPitch))=(unsigned short)product1;
       *(dP+1+(srcPitch))=(unsigned short)product2;

       bP += 1;
       dP += 2;
      }//end of for ( finish= width etc..)

     line += 2;
     srcPtr += srcPitch;
	}; //endof: for (; height; height--)
  }
 }

////////////////////////////////////////////////////////////////////////

void SuperEagle_ex6(unsigned char *srcPtr, DWORD srcPitch,
	            unsigned char  *dstBitmap, int width, int height)
{
 DWORD dstPitch        = srcPitch<<1;
 int   finWidth        = srcPitch>>1;
 DWORD line;
 unsigned short *dP;
 unsigned short *bP;
 int iXA,iXB,iXC,iYA,iYB,iYC,finish;
 DWORD color4, color5, color6;
 DWORD color1, color2, color3;
 DWORD colorA1, colorA2,
       colorB1, colorB2,
       colorS1, colorS2;
 DWORD product1a, product1b,
       product2a, product2b;

 line = 0;

  {
   for (; height; height-=1)
	{
     bP = (unsigned short *)srcPtr;
	 dP = (unsigned short *)(dstBitmap + line*dstPitch);
     for (finish = width; finish; finish -= 1 )
      {
       if(finish==finWidth) iXA=0;
       else                 iXA=1;
       if(finish>4) {iXB=1;iXC=2;}
       else
       if(finish>3) {iXB=1;iXC=1;}
       else         {iXB=0;iXC=0;}
       if(line==0) iYA=0;
       else        iYA=finWidth;
       if(height>4) {iYB=finWidth;iYC=srcPitch;}
       else
       if(height>3) {iYB=finWidth;iYC=finWidth;}
       else         {iYB=0;iYC=0;}

       colorB1 = *(bP- iYA);
       colorB2 = *(bP- iYA + iXB);

       color4 = *(bP  - iXA);
       color5 = *(bP);
       color6 = *(bP  + iXB);
       colorS2 = *(bP + iXC);

       color1 = *(bP  + iYB  - iXA);
       color2 = *(bP  + iYB);
       color3 = *(bP  + iYB  + iXB);
       colorS1= *(bP  + iYB  + iXC);

       colorA1 = *(bP + iYC);
       colorA2 = *(bP + iYC + iXB);

       if(color2 == color6 && color5 != color3)
        {
         product1b = product2a = color2;
         if((color1 == color2) ||
            (color6 == colorB2))
          {
           product1a = INTERPOLATE6(color2, color5);
           product1a = INTERPOLATE6(color2, product1a);
          }
         else
          {
           product1a = INTERPOLATE6(color5, color6);
          }

         if((color6 == colorS2) ||
            (color2 == colorA1))
          {
           product2b = INTERPOLATE6(color2, color3);
           product2b = INTERPOLATE6(color2, product2b);
          }
         else
          {
           product2b = INTERPOLATE6(color2, color3);
          }
        }
       else
       if (color5 == color3 && color2 != color6)
        {
         product2b = product1a = color5;

         if ((colorB1 == color5) ||
             (color3 == colorS1))
          {
           product1b = INTERPOLATE6(color5, color6);
           product1b = INTERPOLATE6(color5, product1b);
          }
         else
          {
           product1b = INTERPOLATE6(color5, color6);
          }

         if ((color3 == colorA2) ||
             (color4 == color5))
          {
           product2a = INTERPOLATE6(color5, color2);
           product2a = INTERPOLATE6(color5, product2a);
          }
         else
          {
           product2a = INTERPOLATE6(color2, color3);
          }
        }
       else
       if (color5 == color3 && color2 == color6)
        {
         register int r = 0;

         r += GET_RESULT ((color6), (color5), (color1),  (colorA1));
         r += GET_RESULT ((color6), (color5), (color4),  (colorB1));
         r += GET_RESULT ((color6), (color5), (colorA2), (colorS1));
         r += GET_RESULT ((color6), (color5), (colorB2), (colorS2));

         if (r > 0)
          {
           product1b = product2a = color2;
           product1a = product2b = INTERPOLATE6(color5, color6);
          }
         else
         if (r < 0)
          {
           product2b = product1a = color5;
           product1b = product2a = INTERPOLATE6(color5, color6);
          }
         else
          {
           product2b = product1a = color5;
           product1b = product2a = color2;
          }
        }
       else
        {
         product2b = product1a = INTERPOLATE6(color2, color6);
         product2b = Q_INTERPOLATE6(color3, color3, color3, product2b);
         product1a = Q_INTERPOLATE6(color5, color5, color5, product1a);

         product2a = product1b = INTERPOLATE6(color5, color3);
         product2a = Q_INTERPOLATE6(color2, color2, color2, product2a);
         product1b = Q_INTERPOLATE6(color6, color6, color6, product1b);
        }

       *dP=(unsigned short)product1a;
       *(dP+1)=(unsigned short)product1b;
       *(dP+(srcPitch))=(unsigned short)product2a;
       *(dP+1+(srcPitch))=(unsigned short)product2b;

       bP += 1;
       dP += 2;
      }//end of for ( finish= width etc..)

     line += 2;
     srcPtr += srcPitch;
	}; //endof: for (; height; height--)
  }
}

////////////////////////////////////////////////////////////////////////

#ifndef MAX
#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))
#endif

void Scale2x_ex6_5(unsigned char *srcPtr, DWORD srcPitch,
	               unsigned char  *dstBitmap, int width, int height)
{
 int looph, loopw;

 unsigned char * srcpix = srcPtr;
 unsigned char * dstpix = dstBitmap;

 const int srcpitch = srcPitch;
 const int dstpitch = srcPitch<<1;

 unsigned short E0, E1, E2, E3, B, D, E, F, H;
 for(looph = 0; looph < height; ++looph)
  {
   for(loopw = 0; loopw < width; ++ loopw)
    {
     B = *(unsigned short*)(srcpix + (MAX(0,looph-1)*srcpitch) + (2*loopw));
     D = *(unsigned short*)(srcpix + (looph*srcpitch) + (2*MAX(0,loopw-1)));
     E = *(unsigned short*)(srcpix + (looph*srcpitch) + (2*loopw));
     F = *(unsigned short*)(srcpix + (looph*srcpitch) + (2*MIN(width-1,loopw+1)));
     H = *(unsigned short*)(srcpix + (MIN(height-1,looph+1)*srcpitch) + (2*loopw));

		    if (B != H && D != F) {
				E0 = D == B ? D : E;
				E1 = B == F ? F : E;
				E2 = D == H ? D : E;
				E3 = H == F ? F : E;
			} else {
				E0 = E;
				E1 = E;
				E2 = E;
				E3 = E;
			}


     *(unsigned short*)(dstpix + looph*2*dstpitch + loopw*2*2) = E0;
     *(unsigned short*)(dstpix + looph*2*dstpitch + (loopw*2+1)*2) = E1;
     *(unsigned short*)(dstpix + (looph*2+1)*dstpitch + loopw*2*2) = E2;
     *(unsigned short*)(dstpix + (looph*2+1)*dstpitch + (loopw*2+1)*2) = E3;
    }
  }
}

////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////

/*
#define colorMask5     0x0000F7DE
#define lowPixelMask5  0x00000821
#define qcolorMask5    0x0000E79c
#define qlowpixelMask5 0x00001863
*/

#define colorMask5     0x00007BDE
#define lowPixelMask5  0x00000421
#define qcolorMask5    0x0000739c
#define qlowpixelMask5 0x00000C63

#define INTERPOLATE5(A, B) ((((A & colorMask5) >> 1) + ((B & colorMask5) >> 1) + (A & B & lowPixelMask5)))
#define Q_INTERPOLATE5(A, B, C, D) (((((A & qcolorMask5) >> 2) + ((B & qcolorMask5) >> 2) + ((C & qcolorMask5) >> 2) + ((D & qcolorMask5) >> 2) \
	+ ((((A & qlowpixelMask5) + (B & qlowpixelMask5) + (C & qlowpixelMask5) + (D & qlowpixelMask5)) >> 2) & qlowpixelMask5))))

void Super2xSaI_ex5(unsigned char *srcPtr, DWORD srcPitch,
	            unsigned char  *dstBitmap, int width, int height)
{
 DWORD dstPitch        = srcPitch<<1;
 int   finWidth        = srcPitch>>1;
 DWORD line;
 unsigned short *dP;
 unsigned short *bP;
 int iXA,iXB,iXC,iYA,iYB,iYC,finish;
 DWORD color4, color5, color6;
 DWORD color1, color2, color3;
 DWORD colorA0, colorA1, colorA2, colorA3,
       colorB0, colorB1, colorB2, colorB3,
       colorS1, colorS2;
 DWORD product1a, product1b,
       product2a, product2b;

 line = 0;

  {
   for (; height; height-=1)
	{
     bP = (unsigned short *)srcPtr;
	 dP = (unsigned short *)(dstBitmap + line*dstPitch);
     for (finish = width; finish; finish -= 1 )
      {
//---------------------------------------    B1 B2
//                                         4  5  6 S2
//                                         1  2  3 S1
//                                           A1 A2
       if(finish==finWidth) iXA=0;
       else                 iXA=1;
       if(finish>4) {iXB=1;iXC=2;}
       else
       if(finish>3) {iXB=1;iXC=1;}
       else         {iXB=0;iXC=0;}
       if(line==0) iYA=0;
       else        iYA=finWidth;
       if(height>4) {iYB=finWidth;iYC=srcPitch;}
       else
       if(height>3) {iYB=finWidth;iYC=finWidth;}
       else         {iYB=0;iYC=0;}


       colorB0 = *(bP- iYA - iXA);
       colorB1 = *(bP- iYA);
       colorB2 = *(bP- iYA + iXB);
       colorB3 = *(bP- iYA + iXC);

       color4 = *(bP  - iXA);
       color5 = *(bP);
       color6 = *(bP  + iXB);
       colorS2 = *(bP + iXC);

       color1 = *(bP  + iYB  - iXA);
       color2 = *(bP  + iYB);
       color3 = *(bP  + iYB  + iXB);
       colorS1= *(bP  + iYB  + iXC);

       colorA0 = *(bP + iYC - iXA);
       colorA1 = *(bP + iYC);
       colorA2 = *(bP + iYC + iXB);
       colorA3 = *(bP + iYC + iXC);

//--------------------------------------
       if (color2 == color6 && color5 != color3)
        {
         product2b = product1b = color2;
        }
       else
       if (color5 == color3 && color2 != color6)
        {
         product2b = product1b = color5;
        }
       else
       if (color5 == color3 && color2 == color6)
        {
         register int r = 0;

         r += GET_RESULT ((color6), (color5), (color1),  (colorA1));
         r += GET_RESULT ((color6), (color5), (color4),  (colorB1));
         r += GET_RESULT ((color6), (color5), (colorA2), (colorS1));
         r += GET_RESULT ((color6), (color5), (colorB2), (colorS2));

         if (r > 0)
          product2b = product1b = color6;
         else
         if (r < 0)
          product2b = product1b = color5;
         else
          {
           product2b = product1b = INTERPOLATE5 (color5, color6);
          }
        }
       else
        {
         if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0)
             product2b = Q_INTERPOLATE5 (color3, color3, color3, color2);
         else
         if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3)
             product2b = Q_INTERPOLATE5 (color2, color2, color2, color3);
         else
             product2b = INTERPOLATE5 (color2, color3);

         if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0)
             product1b = Q_INTERPOLATE5 (color6, color6, color6, color5);
         else
         if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3)
             product1b = Q_INTERPOLATE5 (color6, color5, color5, color5);
         else
             product1b = INTERPOLATE5 (color5, color6);
        }

       if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2)
        product2a = INTERPOLATE5 (color2, color5);
       else
       if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0)
        product2a = INTERPOLATE5(color2, color5);
       else
        product2a = color2;

       if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2)
        product1a = INTERPOLATE5(color2, color5);
       else
       if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0)
        product1a = INTERPOLATE5(color2, color5);
       else
        product1a = color5;

       *dP=(unsigned short)product1a;
       *(dP+1)=(unsigned short)product1b;
       *(dP+(srcPitch))=(unsigned short)product2a;
       *(dP+1+(srcPitch))=(unsigned short)product2b;

       bP += 1;
       dP += 2;
      }//end of for ( finish= width etc..)

     line += 2;
     srcPtr += srcPitch;
	}; //endof: for (; height; height--)
  }
}

////////////////////////////////////////////////////////////////////////

void Std2xSaI_ex5(unsigned char *srcPtr, DWORD srcPitch,
	              unsigned char  *dstBitmap, int width, int height)
{
 DWORD dstPitch        = srcPitch<<1;
 int   finWidth        = srcPitch>>1;
 DWORD line;
 unsigned short *dP;
 unsigned short *bP;
 int iXA,iXB,iXC,iYA,iYB,iYC,finish;
 DWORD colorA, colorB;
 DWORD colorC, colorD,
       colorE, colorF, colorG, colorH,
       colorI, colorJ, colorK, colorL,
       colorM, colorN, colorO, colorP;
 DWORD product, product1, product2;

 line = 0;

  {
   for (; height; height-=1)
	{
     bP = (unsigned short *)srcPtr;
	 dP = (unsigned short *)(dstBitmap + line*dstPitch);
     for (finish = width; finish; finish -= 1 )
      {
//---------------------------------------
// Map of the pixels:                    I|E F|J
//                                       G|A B|K
//                                       H|C D|L
//                                       M|N O|P
       if(finish==finWidth) iXA=0;
       else                 iXA=1;
       if(finish>4) {iXB=1;iXC=2;}
       else
       if(finish>3) {iXB=1;iXC=1;}
       else         {iXB=0;iXC=0;}
       if(line==0) iYA=0;
       else        iYA=finWidth;
       if(height>4) {iYB=finWidth;iYC=srcPitch;}
       else
       if(height>3) {iYB=finWidth;iYC=finWidth;}
       else         {iYB=0;iYC=0;}

       colorI = *(bP- iYA - iXA);
       colorE = *(bP- iYA);
       colorF = *(bP- iYA + iXB);
       colorJ = *(bP- iYA + iXC);

       colorG = *(bP  - iXA);
       colorA = *(bP);
       colorB = *(bP  + iXB);
       colorK = *(bP + iXC);

       colorH = *(bP  + iYB  - iXA);
       colorC = *(bP  + iYB);
       colorD = *(bP  + iYB  + iXB);
       colorL = *(bP  + iYB  + iXC);

       colorM = *(bP + iYC - iXA);
       colorN = *(bP + iYC);
       colorO = *(bP + iYC + iXB);
       colorP = *(bP + iYC + iXC);

       if((colorA == colorD) && (colorB != colorC))
        {
         if(((colorA == colorE) && (colorB == colorL)) ||
            ((colorA == colorC) && (colorA == colorF) &&
             (colorB != colorE) && (colorB == colorJ)))
          {
           product = colorA;
          }
         else
          {
           product = INTERPOLATE5(colorA, colorB);
          }

         if(((colorA == colorG) && (colorC == colorO)) ||
            ((colorA == colorB) && (colorA == colorH) &&
             (colorG != colorC) && (colorC == colorM)))
          {
           product1 = colorA;
          }
         else
          {
           product1 = INTERPOLATE5(colorA, colorC);
          }
         product2 = colorA;
        }
       else
       if((colorB == colorC) && (colorA != colorD))
        {
         if(((colorB == colorF) && (colorA == colorH)) ||
            ((colorB == colorE) && (colorB == colorD) &&
             (colorA != colorF) && (colorA == colorI)))
          {
           product = colorB;
          }
         else
          {
           product = INTERPOLATE5(colorA, colorB);
          }

         if(((colorC == colorH) && (colorA == colorF)) ||
            ((colorC == colorG) && (colorC == colorD) &&
             (colorA != colorH) && (colorA == colorI)))
          {
           product1 = colorC;
          }
         else
          {
           product1=INTERPOLATE5(colorA, colorC);
          }
         product2 = colorB;
        }
       else
       if((colorA == colorD) && (colorB == colorC))
        {
         if (colorA == colorB)
          {
           product = colorA;
           product1 = colorA;
           product2 = colorA;
          }
         else
          {
           register int r = 0;
           product1 = INTERPOLATE5(colorA, colorC);
           product = INTERPOLATE5(colorA, colorB);

           r += GetResult1 (colorA, colorB, colorG, colorE, colorI);
           r += GetResult2 (colorB, colorA, colorK, colorF, colorJ);
           r += GetResult2 (colorB, colorA, colorH, colorN, colorM);
           r += GetResult1 (colorA, colorB, colorL, colorO, colorP);

           if (r > 0)
            product2 = colorA;
           else
           if (r < 0)
            product2 = colorB;
           else
            {
             product2 = Q_INTERPOLATE5(colorA, colorB, colorC, colorD);
            }
          }
        }
       else
        {
         product2 = Q_INTERPOLATE5(colorA, colorB, colorC, colorD);

         if ((colorA == colorC) && (colorA == colorF) &&
             (colorB != colorE) && (colorB == colorJ))
          {
           product = colorA;
          }
         else
         if ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI))
          {
           product = colorB;
          }
         else
          {
           product = INTERPOLATE5(colorA, colorB);
          }

         if ((colorA == colorB) && (colorA == colorH) &&
             (colorG != colorC) && (colorC == colorM))
          {
           product1 = colorA;
          }
         else
         if ((colorC == colorG) && (colorC == colorD) &&
             (colorA != colorH) && (colorA == colorI))
          {
           product1 = colorC;
          }
         else
          {
           product1 = INTERPOLATE5(colorA, colorC);
          }
        }

       *dP=(unsigned short)colorA;
       *(dP+1)=(unsigned short)product;
       *(dP+(srcPitch))=(unsigned short)product1;
       *(dP+1+(srcPitch))=(unsigned short)product2;

       bP += 1;
       dP += 2;
      }//end of for ( finish= width etc..)

     line += 2;
     srcPtr += srcPitch;
	}; //endof: for (; height; height--)
  }
}

////////////////////////////////////////////////////////////////////////

void SuperEagle_ex5(unsigned char *srcPtr, DWORD srcPitch,
	            unsigned char  *dstBitmap, int width, int height)
{
 DWORD dstPitch        = srcPitch<<1;
 int   finWidth        = srcPitch>>1;
 DWORD line;
 unsigned short *dP;
 unsigned short *bP;
 int iXA,iXB,iXC,iYA,iYB,iYC,finish;
 DWORD color4, color5, color6;
 DWORD color1, color2, color3;
 DWORD colorA1, colorA2,
       colorB1, colorB2,
       colorS1, colorS2;
 DWORD product1a, product1b,
       product2a, product2b;

 line = 0;

  {
   for (; height; height-=1)
	{
     bP = (unsigned short *)srcPtr;
	 dP = (unsigned short *)(dstBitmap + line*dstPitch);
     for (finish = width; finish; finish -= 1 )
      {
       if(finish==finWidth) iXA=0;
       else                 iXA=1;
       if(finish>4) {iXB=1;iXC=2;}
       else
       if(finish>3) {iXB=1;iXC=1;}
       else         {iXB=0;iXC=0;}
       if(line==0) iYA=0;
       else        iYA=finWidth;
       if(height>4) {iYB=finWidth;iYC=srcPitch;}
       else
       if(height>3) {iYB=finWidth;iYC=finWidth;}
       else         {iYB=0;iYC=0;}

       colorB1 = *(bP- iYA);
       colorB2 = *(bP- iYA + iXB);

       color4 = *(bP  - iXA);
       color5 = *(bP);
       color6 = *(bP  + iXB);
       colorS2 = *(bP + iXC);

       color1 = *(bP  + iYB  - iXA);
       color2 = *(bP  + iYB);
       color3 = *(bP  + iYB  + iXB);
       colorS1= *(bP  + iYB  + iXC);

       colorA1 = *(bP + iYC);
       colorA2 = *(bP + iYC + iXB);

       if(color2 == color6 && color5 != color3)
        {
         product1b = product2a = color2;
         if((color1 == color2) ||
            (color6 == colorB2))
          {
           product1a = INTERPOLATE5(color2, color5);
           product1a = INTERPOLATE5(color2, product1a);
          }
         else
          {
           product1a = INTERPOLATE5(color5, color6);
          }

         if((color6 == colorS2) ||
            (color2 == colorA1))
          {
           product2b = INTERPOLATE5(color2, color3);
           product2b = INTERPOLATE5(color2, product2b);
          }
         else
          {
           product2b = INTERPOLATE5(color2, color3);
          }
        }
       else
       if (color5 == color3 && color2 != color6)
        {
         product2b = product1a = color5;

         if ((colorB1 == color5) ||
             (color3 == colorS1))
          {
           product1b = INTERPOLATE5(color5, color6);
           product1b = INTERPOLATE5(color5, product1b);
          }
         else
          {
           product1b = INTERPOLATE5(color5, color6);
          }

         if ((color3 == colorA2) ||
             (color4 == color5))
          {
           product2a = INTERPOLATE5(color5, color2);
           product2a = INTERPOLATE5(color5, product2a);
          }
         else
          {
           product2a = INTERPOLATE5(color2, color3);
          }
        }
       else
       if (color5 == color3 && color2 == color6)
        {
         register int r = 0;

         r += GET_RESULT ((color6), (color5), (color1),  (colorA1));
         r += GET_RESULT ((color6), (color5), (color4),  (colorB1));
         r += GET_RESULT ((color6), (color5), (colorA2), (colorS1));
         r += GET_RESULT ((color6), (color5), (colorB2), (colorS2));

         if (r > 0)
          {
           product1b = product2a = color2;
           product1a = product2b = INTERPOLATE5(color5, color6);
          }
         else
         if (r < 0)
          {
           product2b = product1a = color5;
           product1b = product2a = INTERPOLATE5(color5, color6);
          }
         else
          {
           product2b = product1a = color5;
           product1b = product2a = color2;
          }
        }
       else
        {
         product2b = product1a = INTERPOLATE5(color2, color6);
         product2b = Q_INTERPOLATE5(color3, color3, color3, product2b);
         product1a = Q_INTERPOLATE5(color5, color5, color5, product1a);

         product2a = product1b = INTERPOLATE5(color5, color3);
         product2a = Q_INTERPOLATE5(color2, color2, color2, product2a);
         product1b = Q_INTERPOLATE5(color6, color6, color6, product1b);
        }

       *dP=(unsigned short)product1a;
       *(dP+1)=(unsigned short)product1b;
       *(dP+(srcPitch))=(unsigned short)product2a;
       *(dP+1+(srcPitch))=(unsigned short)product2b;

       bP += 1;
       dP += 2;
      }//end of for ( finish= width etc..)

     line += 2;
     srcPtr += srcPitch;
	}; //endof: for (; height; height--)
  }
}

////////////////////////////////////////////////////////////////////////
// Win code starts here
////////////////////////////////////////////////////////////////////////

#ifdef _WINDOWS
#else

#ifndef _SDL
////////////////////////////////////////////////////////////////////////
// X STUFF :)
////////////////////////////////////////////////////////////////////////
#else  //SDL
////////////////////////////////////////////////////////////////////////
// SDL Stuff ^^
////////////////////////////////////////////////////////////////////////

int           Xpitch,depth=32;
char *        Xpixels;
char *        pCaptionText;

SDL_Surface *display,*XFimage,*XPimage=NULL;
#ifndef _SDL2
SDL_Surface *Ximage=NULL,*XCimage=NULL;
#else
SDL_Surface *Ximage16,*Ximage24;
#endif
//static Uint32 sdl_mask=SDL_HWSURFACE|SDL_HWACCEL;/*place or remove some flags*/
Uint32 sdl_mask=SDL_HWSURFACE;
SDL_Rect rectdst,rectsrc;



void DestroyDisplay(void)
{
if(display){
#ifdef _SDL2
if(Ximage16) SDL_FreeSurface(Ximage16);
if(Ximage24) SDL_FreeSurface(Ximage24);
#else
if(XCimage) SDL_FreeSurface(XCimage);
if(Ximage) SDL_FreeSurface(Ximage);
#endif
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
#ifndef _SDL2
Ximage = SDL_CreateRGBSurface(sdl_mask,iResX,iResY,depth,0x00ff0000,0x0000ff00,0x000000ff,0);
XCimage= SDL_CreateRGBSurface(sdl_mask,iResX,iResY,depth,0x00ff0000,0x0000ff00,0x000000ff,0);
#else
//Ximage16= SDL_CreateRGBSurface(sdl_mask,iResX,iResY,16,0x1f,0x1f<<5,0x1f<<10,0);

Ximage16= SDL_CreateRGBSurfaceFrom((void*)psxVub, 1024,512,16,2048 ,0x1f,0x1f<<5,0x1f<<10,0);
Ximage24= SDL_CreateRGBSurfaceFrom((void*)psxVub, 1024*2/3,512 ,24,2048 ,0xFF0000,0xFF00,0xFF,0);
#endif


XFimage= SDL_CreateRGBSurface(sdl_mask,170,15,depth,0x00ff0000,0x0000ff00,0x000000ff,0);

iColDepth=depth;
//memset(XFimage->pixels,255,170*15*4);//really needed???
//memset(Ximage->pixels,0,ResX*ResY*4);
#ifndef _SDL2
memset(XCimage->pixels,0,iResX*iResY*4);
#endif

//Xpitch=iResX*32; no more use
#ifndef _SDL2
Xpixels=(char *)Ximage->pixels;
#endif
if(pCaptionText)
      SDL_WM_SetCaption(pCaptionText,NULL);
 else SDL_WM_SetCaption("FPSE Display - P.E.Op.S SoftSDL PSX Gpu",NULL);

}
#endif


////////////////////////////////////////////////////////////////////////
#ifndef _SDL2
void (*BlitScreen) (unsigned char *,long,long);
void (*BlitScreenNS) (unsigned char *,long,long);
void (*XStretchBlt)(unsigned char * pBB,int sdx,int sdy,int ddx,int ddy);
void (*p2XSaIFunc) (unsigned char *,DWORD,unsigned char *,int,int);
unsigned char * pBackBuffer=0;
#endif
////////////////////////////////////////////////////////////////////////

#ifndef _SDL2

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
#ifdef USE_DGA2
 if (!iWindowMode && (char*)surf == Xpixels)
  lPitch = (dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth) * 4;
#endif

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
#ifdef USE_DGA2
 if (!iWindowMode && (char*)surf == Xpixels)
  lPitch = (dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth) * 4;
#endif

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
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)surf == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
  } else DGA2fix = dga2Fix = 0;
#endif

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   surf+=PreviousPSXDisplay.Range.x0<<1;
#ifdef USE_DGA2
   if (DGA2fix) lPitch+= dga2Fix*2;
#endif

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
#ifdef USE_DGA2
     if (DGA2fix) DSTPtr+= dga2Fix/2;
#endif
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
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)surf == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
  } else DGA2fix = dga2Fix = 0;
#endif

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   surf+=PreviousPSXDisplay.Range.x0<<1;
#ifdef USE_DGA2
   if (DGA2fix) lPitch+= dga2Fix*2;
#endif

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
#ifdef USE_DGA2
       if (DGA2fix) DSTPtr+= dga2Fix/2;
#endif
      }
     else
      {
#ifdef USE_DGA2
       if (DGA2fix) DSTPtr+= dga2Fix/2;
#endif
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
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)surf == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
  } else DGA2fix = dga2Fix = 0;
#endif

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   surf+=PreviousPSXDisplay.Range.x0<<1;
#ifdef USE_DGA2
   if (DGA2fix) lPitch+= dga2Fix*2;
#endif

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

#ifdef USE_DGA2
   dga2Fix/=2;
#endif
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
#ifdef USE_DGA2
     if (DGA2fix) DSTPtr+= dga2Fix;
#endif
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
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)surf == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
  } else DGA2fix = dga2Fix = 0;
#endif

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   surf+=PreviousPSXDisplay.Range.x0<<1;
#ifdef USE_DGA2
   if (DGA2fix) lPitch+= dga2Fix*2;
#endif

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

#ifdef USE_DGA2
   dga2Fix/=2;
#endif
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
#ifdef USE_DGA2
       if (DGA2fix) DSTPtr+= dga2Fix;
#endif
      }
     else
      {
       DSTPtr+=iResX>>1;
       SRCPtr+=512;
#ifdef USE_DGA2
       if (DGA2fix) DSTPtr+= dga2Fix;
#endif
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
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)pBB == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
  } else DGA2fix = dga2Fix = 0;
#endif

 // 2xsai stretching
if(iUseNoStretchBlt>=2)
{
	//p2XSaIFunc(pBackBuffer,sdx<<1,(unsigned char *)pSaIBigBuff,sdx,sdy);
    if(p2XSaIFunc==hq2x_16 )
	p2XSaIFunc(pBackBuffer,sdx<<2,(unsigned char *)pSaIBigBuff,sdx,sdy);
    else if(p2XSaIFunc==hq3x_16)
	p2XSaIFunc(pBackBuffer,sdx*6,(unsigned char *)pSaIBigBuff,sdx,sdy);
    else
	p2XSaIFunc(pBackBuffer,sdx<<1,(unsigned char *)pSaIBigBuff,sdx,sdy);
    pSrc=(unsigned short *)pSaIBigBuff;
    //sdx+=sdx;sdy+=sdy;
   if(iUseNoStretchBlt>=12)
   {
	sdx= sdx*3;
	sdy=sdy*3;
   }
   else
	sdx+=sdx;sdy+=sdy;
}

 xinc = (sdx << 16) / ddx;

 ypos=0;
 yinc = (sdy << 16) / ddy;

 for(y=0;y<ddy;y++,ypos+=yinc)
  {
   cy=(ypos>>16);

   if(cy==cyo)
    {
#ifndef USE_DGA2
     pDstR=(unsigned long *)(pDst-ddx);
#else
     pDstR=(unsigned long *)(pDst-(ddx+dga2Fix));
#endif
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
#ifdef USE_DGA2
   if (DGA2fix) pDst+= dga2Fix;
#endif
  }
}

////////////////////////////////////////////////////////////////////////

// non-stretched stretched 2xsai linux helper

void XStretchBlt16NS(unsigned char * pBB,int sdx,int sdy,int ddx,int ddy)
{
 static int iOldDX=0;
 static int iOldDY=0;
 int iDX2, iDY2;
 unsigned short * pSrc,* pDst=(unsigned short *)pBB;

 int iX,iY,iDX,iDY,x,y,iOffS,iOffD,iS;
if(iUseNoStretchBlt>=12 && sdx < 341)
{
if(p2XSaIFunc==hq2x_16)
  p2XSaIFunc=hq3x_16;
if(p2XSaIFunc==Scale2x_ex6_5)
  p2XSaIFunc=Scale3x_ex6_5;
 iDX2=PreviousPSXDisplay.DisplayMode.x*3;
 iDY2=PreviousPSXDisplay.DisplayMode.y*3;
}
else
{
 iDX2=PreviousPSXDisplay.DisplayMode.x<<1;
 iDY2=PreviousPSXDisplay.DisplayMode.y<<1;
if(p2XSaIFunc==hq3x_16)
  p2XSaIFunc=hq2x_16;
if(p2XSaIFunc==Scale3x_ex6_5)
  p2XSaIFunc=Scale2x_ex6_5;
}
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)pBB == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
  } else DGA2fix = dga2Fix = 0;
#endif

 iX=iResX-iDX2;
 iY=iResY-iDY2;

 iOffS=0;
 iOffD=0;

 if(iX<0) {iOffS=iDX2-iResX;iX=0;   iDX=iResX;}
 else     {iOffD=iX;        iX=iX/2;iDX=iDX2;}

 if(iY<0) {iY=0;iDY=iResY;}
 else     {iY=iY/2;iDY=iDY2;}

 if(iOldDX!=iDX || iOldDY!=iDY)
  {
   memset(Xpixels,0,iResY*iResX*2);
   iOldDX=iDX;iOldDY=iDY;
  }

 p2XSaIFunc(pBackBuffer,sdx<<1,(unsigned char *)pSaIBigBuff,sdx,sdy);
    if(p2XSaIFunc==hq2x_16 )
	p2XSaIFunc(pBackBuffer,sdx<<2,(unsigned char *)pSaIBigBuff,sdx,sdy);
    else if(p2XSaIFunc==hq3x_16)
	p2XSaIFunc(pBackBuffer,sdx*6,(unsigned char *)pSaIBigBuff,sdx,sdy);
    else
 	p2XSaIFunc(pBackBuffer,sdx<<1,(unsigned char *)pSaIBigBuff,sdx,sdy);
 pSrc=(unsigned short *)pSaIBigBuff;

 if(iUseScanLines) {iS=2;iOffD+=iResX;iOffS+=iDX2;} else iS=1;
 pDst+=iX+iY*iResX;

 for(y=0;y<iDY;y+=iS)
  {
   for(x=0;x<iDX;x++)
    {
     *pDst++=*pSrc++;
    }
#ifdef USE_DGA2
   if (DGA2fix) pDst+= dga2Fix;
#endif
   pDst+=iOffD;pSrc+=iOffS;
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
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)pBB == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
  } else DGA2fix = dga2Fix = 0;
#endif

 // 2xsai stretching

 if(iUseNoStretchBlt>=2)
  {
    if(p2XSaIFunc==hq2x_16 )
	p2XSaIFunc(pBackBuffer,sdx<<2,(unsigned char *)pSaIBigBuff,sdx,sdy);
    else if(p2XSaIFunc==hq3x_16)
	p2XSaIFunc(pBackBuffer,sdx*6,(unsigned char *)pSaIBigBuff,sdx,sdy);
    else
 	p2XSaIFunc(pBackBuffer,sdx<<1,(unsigned char *)pSaIBigBuff,sdx,sdy);
   pSrc=(unsigned short *)pSaIBigBuff;
   //sdx+=sdx;sdy+=sdy;
   if(iUseNoStretchBlt>=12)
   {
	sdx= sdx*3;
	sdy=sdy*3;
   }
   else
	sdx+=sdx;sdy+=sdy;
  }

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
#ifdef USE_DGA2
   if (DGA2fix) pDst+= dga2Fix*2;
#endif
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
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)pBB == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
   dga2Fix/=2;
  } else DGA2fix = dga2Fix = 0;
#endif

 //!P!
 // 2xsai stretching
 if(iUseNoStretchBlt>=2)
  {

if( p2XSaIFunc==hq3x_32)
 p2XSaIFunc(pBackBuffer,sdx*12,
           (unsigned char *)pSaIBigBuff,
            sdx,sdy);
else if(p2XSaIFunc==hq2x_32)
 p2XSaIFunc(pBackBuffer,sdx<<3,
            (unsigned char *)pSaIBigBuff,
            sdx,sdy);
else
 p2XSaIFunc(pBackBuffer,sdx<<2,
            (unsigned char *)pSaIBigBuff,
            sdx,sdy);


   pSrc=(unsigned long *)pSaIBigBuff;
   //sdx+=sdx;sdy+=sdy;
   if(iUseNoStretchBlt>=12)
   {
	sdx= sdx*3;sdy=sdy*3;
	//sdx+=sdx;sdy+=sdy;
   }
   else
	sdx+=sdx;sdy+=sdy;
  }

 xinc = (sdx << 16) / ddx;

 ypos=0;
 yinc = (sdy << 16) / ddy;

 for(y=0;y<ddy;y++,ypos+=yinc)
  {
   cy=(ypos>>16);

   if(cy==cyo)
    {
#ifndef USE_DGA2
     pDstR=pDst-ddx;
#else
     pDstR=pDst-(ddx+dga2Fix);
#endif
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
#ifdef USE_DGA2
   if (DGA2fix) pDst+= dga2Fix;
#endif
  }
}

////////////////////////////////////////////////////////////////////////

// new linux non-stretched 2xSaI helper

void XStretchBlt32NS(unsigned char * pBB,int sdx,int sdy,int ddx,int ddy)
{
 static int iOldDX=0;
 static int iOldDY=0;

 unsigned long * pSrc,* pDst=(unsigned long *)pBB;

 int iX,iY,iDX,iDY,x,y,iOffS,iOffD,iS;
 int iDX2;
 int iDY2;
if(iUseNoStretchBlt>=12 && sdx < 341)
{
if(p2XSaIFunc==hq2x_32)
  p2XSaIFunc=hq3x_32;
  if(p2XSaIFunc==Scale2x_ex8)
  p2XSaIFunc=Scale3x_ex8;
 iDX2=PreviousPSXDisplay.DisplayMode.x*3;
 iDY2=PreviousPSXDisplay.DisplayMode.y*3;
}
else
{
 iDX2=PreviousPSXDisplay.DisplayMode.x<<1;
 iDY2=PreviousPSXDisplay.DisplayMode.y<<1;
if(p2XSaIFunc==hq3x_32)
  p2XSaIFunc=hq2x_32;
    if(p2XSaIFunc==Scale3x_ex8)
  p2XSaIFunc=Scale2x_ex8;
}
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)pBB == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
   dga2Fix/=2;
  } else DGA2fix = dga2Fix = 0;
#endif

 iX=iResX-iDX2;
 iY=iResY-iDY2;

 iOffS=0;
 iOffD=0;

 if(iX<0) {iOffS=iDX2-iResX;iX=0;   iDX=iResX;}
 else     {iOffD=iX;        iX=iX/2;iDX=iDX2;}

 if(iY<0) {iY=0;iDY=iResY;}
 else     {iY=iY/2;iDY=iDY2;}

 if(iOldDX!=iDX || iOldDY!=iDY)
  {
   memset(Xpixels,0,iResY*iResX*4);
   iOldDX=iDX;iOldDY=iDY;
  }

if( p2XSaIFunc==hq3x_32)
 p2XSaIFunc(pBackBuffer,sdx*12,
            (unsigned char *)pSaIBigBuff,
            sdx,sdy);
else if(p2XSaIFunc==hq2x_32)
 p2XSaIFunc(pBackBuffer,sdx<<3,
            (unsigned char *)pSaIBigBuff,
            sdx,sdy);
else
 p2XSaIFunc(pBackBuffer,sdx<<2,
            (unsigned char *)pSaIBigBuff,
            sdx,sdy);
 pSrc=(unsigned long *)pSaIBigBuff;

 if(iUseScanLines) {iS=2;iOffD+=iResX;iOffS+=iDX2;} else iS=1;
 pDst+=iX+iY*iResX;

 for(y=0;y<iDY;y+=iS)
  {
   for(x=0;x<iDX;x++)
    {
     *pDst++=*pSrc++;
    }
#ifdef USE_DGA2
   if (DGA2fix) pDst+= dga2Fix;
#endif
   pDst+=iOffD;pSrc+=iOffS;
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
#ifdef USE_DGA2
 int DGA2fix;
 int dga2Fix;
 if (!iWindowMode)
  {
   DGA2fix = (char*)pBB == Xpixels;
   dga2Fix = dgaDev->mode.imageWidth - dgaDev->mode.viewportWidth;
  } else DGA2fix = dga2Fix = 0;
#endif

 // 2xsai stretching
 if(iUseNoStretchBlt>=2)
  {
if( p2XSaIFunc==hq3x_32)
 p2XSaIFunc(pBackBuffer,sdx*12,
            (unsigned char *)pSaIBigBuff,
            sdx,sdy);
else if(p2XSaIFunc==hq2x_32)
 p2XSaIFunc(pBackBuffer,sdx<<3,
            (unsigned char *)pSaIBigBuff,
            sdx,sdy);
else
 p2XSaIFunc(pBackBuffer,sdx<<2,
            (unsigned char *)pSaIBigBuff,
            sdx,sdy);
   pSrc=(unsigned long *)pSaIBigBuff;
//   sdx+=sdx;sdy+=sdy;
   if(iUseNoStretchBlt>=12)
   {
	sdx= sdx*3;
	sdy=sdy*3;
   }
   else
	sdx+=sdx;sdy+=sdy;
  }

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
#ifdef USE_DGA2
   if (DGA2fix) pDst+= dga2Fix;
#endif
   pDst+=iResX;
  }
}

///////////////////////////////////////////////////////////////////////
#ifdef USE_DGA2

void XDGABlit(unsigned char *pSrc, int sw, int sh, int dx, int dy)
{
 unsigned char *pDst;
 int bytesPerPixel = dgaDev->mode.bitsPerPixel / 8;

 for(;dy<sh;dy++)
  {
   pDst = dgaDev->data + dgaDev->mode.imageWidth * dy * bytesPerPixel + dx * bytesPerPixel;
   memcpy(pDst, pSrc, sw * bytesPerPixel);
   pSrc+= sw * bytesPerPixel;
  }
}

#endif
#endif //SDL2

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
#ifndef _SDL2
	  memset(Xpixels,0,iResY*iResX*4);
#endif
#ifndef _SDL
#ifdef USE_DGA2
   if(iWindowMode)
#endif
   XPutImage(display,window,hGC, XCimage,
             0, 0, 0, 0, iResX,iResY);
#else
 rectdst.x=iX;
 rectdst.y=iY;
 rectdst.w=iDX;
 rectdst.h=iDY;
//   SDL_BlitSurface(XCimage,NULL,display,NULL);

   SDL_FillRect(display,NULL,0);
#endif

   iOldDX=iDX;iOldDY=iDY;
  }
#ifndef _SDL2
 BlitScreenNS((unsigned char *)Xpixels,
              PSXDisplay.DisplayPosition.x,
              PSXDisplay.DisplayPosition.y);

 if(usCursorActive) ShowGunCursor((unsigned char *)Xpixels,iResX);



#else
 rectsrc.x=PSXDisplay.DisplayPosition.x;
 rectsrc.y=PSXDisplay.DisplayPosition.y;
 rectsrc.h=PreviousPSXDisplay.DisplayMode.y;
 if(PSXDisplay.RGB24)
 {

	 rectsrc.w=PreviousPSXDisplay.Range.x1/*2/3*/;
	 SDL_BlitSurface(Ximage24,&rectsrc,display,&rectdst);
 }
 else
 {

	 rectsrc.w=PreviousPSXDisplay.Range.x1;
	 SDL_BlitSurface(Ximage16,&rectsrc,display,&rectdst);
 }
#endif
 if(iODX) PreviousPSXDisplay.Range.x1=iODX;
 if(iODY) PreviousPSXDisplay.DisplayMode.y=iODY;

#ifndef _SDL
#ifdef USE_DGA2
 if(iWindowMode)
#endif
 XPutImage(display,window,hGC, Ximage,
           0, 0, iX, iY, iDX,iDY);
#else

#ifndef _SDL2
 SDL_BlitSurface(Ximage,NULL,display,&rectdst);
#endif

#endif

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
#ifndef _SDL
#ifdef USE_DGA2
   if(iWindowMode)
    {
#endif
   XPutImage(display,window,hGC, XFimage,
             0, 0, 0, 0, 230,15);
   XDrawString(display,window,hGC,2,13,szDispBuf,strlen(szDispBuf));
#ifdef USE_DGA2
    }
   else
    {
     DrawString(dgaDev->data, dgaDev->mode.imageWidth * (dgaDev->mode.bitsPerPixel / 8)
	 			, dgaDev->mode.bitsPerPixel, 0, 0, iResX, 15, szDispBuf, strlen(szDispBuf), DSM_NORMAL);
	}
#endif
#else
    SDL_WM_SetCaption(szDispBuf,NULL); //just a quick fix,
#endif
  }

 if(XPimage) DisplayPic();

#ifndef _SDL
 XSync(display,False);
#else
 SDL_Flip(display);
#endif

}

////////////////////////////////////////////////////////////////////////

void DoBufferSwap(void)                                // SWAP BUFFERS
{                                                      // (we don't swap... we blit only)

 // TODO: visual rumble

/*     
  if(iRumbleTime) 
   {
    ScreenRect.left+=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2); 
    ScreenRect.right+=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2); 
    ScreenRect.top+=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2); 
    ScreenRect.bottom+=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2); 
    iRumbleTime--;
   }
*/

 #ifdef _SDL2
	SDL_Surface *buf;
 #endif

 if(iUseNoStretchBlt<2)
  {
   if(iUseNoStretchBlt ||
     (PreviousPSXDisplay.Range.x1+PreviousPSXDisplay.Range.x0 == iResX &&
      PreviousPSXDisplay.DisplayMode.y == iResY))
    {NoStretchSwap();return;}
  }

#ifndef _SDL2
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
#else

 rectdst.x=0;
 rectdst.y=0;
 rectdst.w=iResX;
 rectdst.h=iResY;

 rectsrc.x=PSXDisplay.DisplayPosition.x;
 rectsrc.y=PSXDisplay.DisplayPosition.y;
 rectsrc.h=PreviousPSXDisplay.DisplayMode.y;
 rectsrc.w=PreviousPSXDisplay.Range.x1;
 if(PSXDisplay.RGB24)
 {

	 SDL_SoftStretch(buf=SDL_DisplayFormat(Ximage24), &rectsrc,
                    display, &rectdst);
 }
 else
 {
	 SDL_SoftStretch(buf=SDL_DisplayFormat(Ximage16), &rectsrc,
                    display, &rectdst);
 }
SDL_FreeSurface(buf);
#endif
#ifndef _SDL2
#ifndef _SDL
#ifdef USE_DGA2
 if (iWindowMode)
#endif
 XPutImage(display,window,hGC, Ximage,
           0, 0, 0, 0,
           iResX, iResY);
#else
 SDL_BlitSurface(Ximage,NULL,display,NULL);
#endif
#endif
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
#ifndef _SDL
#ifdef USE_DGA2
   if (iWindowMode)
    {
#endif
   XPutImage(display,window,hGC, XFimage,
             0, 0, 0, 0, 230,15);
   XDrawString(display,window,hGC,2,13,szDispBuf,strlen(szDispBuf));
#ifdef USE_DGA2
    }
   else
    {
     DrawString(dgaDev->data, dgaDev->mode.imageWidth * (dgaDev->mode.bitsPerPixel / 8)
	 			, dgaDev->mode.bitsPerPixel, 0, 0, iResX, 15, szDispBuf, strlen(szDispBuf), DSM_NORMAL);
	}
#endif
#else
    SDL_WM_SetCaption(szDispBuf,NULL); //just a quick fix,
#endif
  }

 if(XPimage) DisplayPic();

#ifndef _SDL
 XSync(display,False);
#else
 SDL_Flip(display);
#endif

}

////////////////////////////////////////////////////////////////////////

void DoClearScreenBuffer(void)                         // CLEAR DX BUFFER
{
#ifndef _SDL
#ifdef USE_DGA2
 if (iWindowMode)
 {
#endif
 XPutImage(display,window,hGC, XCimage,
           0, 0, 0, 0, iResX, iResY);
 XSync(display,False);
#ifdef USE_DGA2
 }
#endif
#else
 /*
 SDL_BlitSurface(XCimage,NULL,display,NULL);*/
 SDL_FillRect(display,NULL,0);
 SDL_Flip(display);
#endif
}

////////////////////////////////////////////////////////////////////////

void DoClearFrontBuffer(void)                          // CLEAR DX BUFFER
{
#ifndef _SDL
#ifdef USE_DGA2
 if (iWindowMode)
 {
#endif
 XPutImage(display,window,hGC, XCimage,
           0, 0, 0, 0, iResX, iResY);
 XSync(display,False);
#ifdef USE_DGA2
 }
#endif
#else
 SDL_FillRect(display,NULL,0);
 SDL_Flip(display);
#endif
}


////////////////////////////////////////////////////////////////////////

int Xinitialize()
{
#ifndef _SDL2

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

   pSaIBigBuff=malloc(1280*1024*sizeof(unsigned long));
   memset(pSaIBigBuff,0,1280*1024*sizeof(unsigned long));

 p2XSaIFunc=NULL;

 if(iUseNoStretchBlt==2 || iUseNoStretchBlt==3)
  {
   if     (iDesktopCol==15) p2XSaIFunc=Std2xSaI_ex5;
   else if(iDesktopCol==16) p2XSaIFunc=Std2xSaI_ex6;
   else                     p2XSaIFunc=Std2xSaI_ex8;
  }

 if(iUseNoStretchBlt==4 || iUseNoStretchBlt==5)
  {
   if     (iDesktopCol==15) p2XSaIFunc=Super2xSaI_ex5;
   else if(iDesktopCol==16) p2XSaIFunc=Super2xSaI_ex6;
   else                     p2XSaIFunc=Super2xSaI_ex8;
  }

 if(iUseNoStretchBlt==6 || iUseNoStretchBlt==7)
  {
   if     (iDesktopCol==15) p2XSaIFunc=SuperEagle_ex5;
   else if(iDesktopCol==16) p2XSaIFunc=SuperEagle_ex6;
   else                     p2XSaIFunc=SuperEagle_ex8;
  }

 if(iUseNoStretchBlt==8 || iUseNoStretchBlt==9)
  {
   if     (iDesktopCol==15) p2XSaIFunc=Scale2x_ex6_5;
   else if(iDesktopCol==16) p2XSaIFunc=Scale2x_ex6_5;
   else                     p2XSaIFunc=Scale2x_ex8;
  }
 if(iUseNoStretchBlt==10 || iUseNoStretchBlt==11)
  {
   InitLUTs();
   if     (iDesktopCol==15) p2XSaIFunc=hq2x_16;
   else if(iDesktopCol==16) p2XSaIFunc=hq2x_16;
   else
	{
	  BlitScreen=BlitScreen16;
          p2XSaIFunc=hq2x_32;
	}
  }
   if(iUseNoStretchBlt==12 || iUseNoStretchBlt==13)
  {
   if     (iDesktopCol==15) p2XSaIFunc=Scale3x_ex6_5;
   else if(iDesktopCol==16) p2XSaIFunc=Scale3x_ex6_5;
   else                     p2XSaIFunc=Scale3x_ex8;
  }
  if(iUseNoStretchBlt==14 || iUseNoStretchBlt==15)
  {
   InitLUTs();
   if     (iDesktopCol==15) p2XSaIFunc=hq3x_16;
   else if(iDesktopCol==16) p2XSaIFunc=hq3x_16;
   else
	{
	  BlitScreen=BlitScreen16;
          p2XSaIFunc=hq3x_32;
	}
  }

 if(iUseNoStretchBlt==3  ||
    iUseNoStretchBlt==5  ||
    iUseNoStretchBlt==7  ||
    iUseNoStretchBlt==9  ||
    iUseNoStretchBlt==11 ||
    iUseNoStretchBlt==13 ||
    iUseNoStretchBlt==15)
  {
   if(iDesktopCol<=16)
    {
     XStretchBlt=XStretchBlt16NS;
    }
   else
    {
    XStretchBlt=XStretchBlt32NS;
    }
  }


#endif
 bUsingTWin=FALSE;

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
#ifndef _SDL2
 if(pBackBuffer)  free(pBackBuffer);
 pBackBuffer=0;
 if(iUseNoStretchBlt>=2)
  {
   if(pSaIBigBuff) free(pSaIBigBuff);
   pSaIBigBuff=0;
  }

#endif
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

#ifndef _SDL
#ifdef USE_DGA2
 if (!iWindowMode) { Xpic = p; XPimage = (XImage*)1; }
 else
#endif
 XPimage = XCreateImage(display,myvisual->visual,
                        depth, ZPixmap, 0,
                        (char *)p,
                        128, 96,
                        depth>16 ? 32 : 16,
                        0);
#else
 XPimage = SDL_CreateRGBSurfaceFrom((void *)p,128,96,
			depth,depth*16,
			0x00ff0000,0x0000ff00,0x000000ff,
			0);/*hmm what about having transparency?
			    *Set a nonzero value here.
			    *and set the ALPHA flag ON
			    */
#endif
}

///////////////////////////////////////////////////////////////////////////////////////

void DestroyPic(void)
{
 if(XPimage)
  {
#ifndef _SDL
#ifdef USE_DGA2
   if (iWindowMode)
    {
#endif
   XPutImage(display,window,hGC, XCimage,
             0, 0, 0, 0, iResX, iResY);
   XDestroyImage(XPimage);
#ifdef USE_DGA2
    }
#endif
#else
   SDL_FillRect(display,NULL,0);
   SDL_FreeSurface(XPimage);
#endif
   XPimage=0;
  }
}

///////////////////////////////////////////////////////////////////////////////////////

void DisplayPic(void)
{
#ifndef _SDL
#ifdef USE_DGA2
 if (!iWindowMode) XDGABlit(Xpic, 128, 96, iResX-128, 0);
 else
  {
#endif
 XPutImage(display,window,hGC, XPimage,
           0, 0, iResX-128, 0,128,96);
#ifdef USE_DGA2
  }
#endif
#else
 rectdst.x=iResX-128;
 rectdst.y=0;
 rectdst.w=128;
 rectdst.h=96;
 SDL_BlitSurface(XPimage,NULL,display,&rectdst);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////

void ShowGpuPic(void)
{
}

///////////////////////////////////////////////////////////////////////////////////////

void ShowTextGpuPic(void)
{
}

#endif


///////////////////////////////////////////////////////////////////////
