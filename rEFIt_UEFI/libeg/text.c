/*
 * libeg/text.c
 * Text drawing functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "libegint.h"

#include "egemb_font.h"
//#define FONT_CELL_WIDTH (7)
//#define FONT_CELL_HEIGHT (12)

#define DEBUG_TEXT 1

#if DEBUG_TEXT == 2
#define DBG(x...) AsciiPrint(x)
#elif DEBUG_TEXT == 1
#define DBG(x...) MsgLog(x)
#else
#define DBG(x...)
#endif


static EG_IMAGE *FontImage = NULL;
UINTN FontWidth = 7;
UINTN FontHeight = 12;
UINTN TextHeight;

//
// Text rendering
//

VOID egMeasureText(IN CHAR16 *Text, OUT UINTN *Width, OUT UINTN *Height)
{
    if (Width != NULL)
        *Width = StrLen(Text) * GlobalConfig.CharWidth;
    if (Height != NULL)
        *Height = FontHeight;
}

EG_IMAGE * egLoadFontImage(IN BOOLEAN WantAlpha)
{
  EG_IMAGE            *NewImage;
  EG_IMAGE            *NewFontImage;
//  UINTN     FontWidth;  //using global variables
//  UINTN     FontHeight;
  UINTN     ImageWidth, ImageHeight;
  UINTN     x, y, Ypos, j;
  EG_PIXEL    *PixelPtr;
  EG_PIXEL    FirstPixel;
    
  NewImage = egLoadImage(SelfDir, PoolPrint(L"font\\%s", GlobalConfig.FontFileName), WantAlpha);
  if (!NewImage) {
    DBG("Font %s is not loaded, using default \n", PoolPrint(L"font\\%s", GlobalConfig.FontFileName));
    return NULL;
  }
  ImageWidth = NewImage->Width;
  ImageHeight = NewImage->Height;
  PixelPtr = NewImage->PixelData;
  DBG("Font loaded: ImageWidth=%d ImageHeight=%d\n", ImageWidth, ImageHeight);
  NewFontImage = egCreateImage(ImageWidth << 4, ImageHeight >> 4, WantAlpha);
  if (NewFontImage == NULL)
    return NULL;
  
  FontWidth = ImageWidth >> 4;
  FontHeight = ImageHeight >> 4;
  FirstPixel = *PixelPtr;
  for (y=0; y<16; y++) {
    for (j=0; j<FontHeight; j++) {
      Ypos = ((j << 4) + y) * ImageWidth;
      for (x=0; x<ImageWidth; x++) {
       if (WantAlpha && 
           (PixelPtr->b == FirstPixel.b) &&
           (PixelPtr->g == FirstPixel.g) &&
           (PixelPtr->r == FirstPixel.r)
           ) {
          PixelPtr->a = 0;
        }
        NewFontImage->PixelData[Ypos + x] = *PixelPtr++;
      }
    }    
  }
  egFreeImage(NewImage);
  
  return NewFontImage;  
} 

VOID PrepareFont(VOID)
{
  BOOLEAN ChangeFont = FALSE;
  // load the font
  if (FontImage == NULL){
    switch (GlobalConfig.Font) {
      case FONT_ALFA:
        FontImage = egPrepareEmbeddedImage(&egemb_font, TRUE);
        break;
      case FONT_GRAY:
        FontImage = egPrepareEmbeddedImage(&egemb_font_gray, TRUE);
        break;
      case FONT_LOAD:
        FontImage = egLoadFontImage(TRUE);
        if (!FontImage) {
          ChangeFont = TRUE;
          FontImage = egPrepareEmbeddedImage(&egemb_font, TRUE);
        }
        break;
      default:
        FontImage = egPrepareEmbeddedImage(&egemb_font, TRUE);
        break;
    }    
  }
  if (ChangeFont) {
    GlobalConfig.Font = FONT_ALFA;
    GlobalConfig.CharWidth = 7;
  }
  TextHeight = FontHeight + TEXT_YMARGIN * 2;
  DBG("Font prepared WxH=%dx%d CharWidth=%d\n", FontWidth, FontHeight, GlobalConfig.CharWidth);
}

VOID egRenderText(IN CHAR16 *Text, IN OUT EG_IMAGE *CompImage,
                  IN UINTN PosX, IN UINTN PosY, IN UINTN Cursor)
{
  EG_PIXEL        *BufferPtr;
  EG_PIXEL        *FontPixelData;
  UINTN           BufferLineOffset, FontLineOffset;
  UINTN           TextLength;
  UINTN           i, c, c1;
  UINTN           Shift = 0;
  
  // clip the text
  TextLength = StrLen(Text);
  if (TextLength * GlobalConfig.CharWidth + PosX > CompImage->Width){
    if (GlobalConfig.CharWidth) {
      TextLength = (CompImage->Width - PosX) / GlobalConfig.CharWidth;
    } else
      TextLength = (CompImage->Width - PosX) / FontWidth;
  }
//  DBG("TextLength =%d PosX=%d PosY=%d\n", TextLength, PosX, PosY);
  // render it
  BufferPtr = CompImage->PixelData;
  BufferLineOffset = CompImage->Width;
  BufferPtr += PosX + PosY * BufferLineOffset;
  FontPixelData = FontImage->PixelData;
  FontLineOffset = FontImage->Width;
  if (GlobalConfig.CharWidth < FontWidth) {
    Shift = (FontWidth - GlobalConfig.CharWidth) >> 1;
  }
  for (i = 0; i < TextLength; i++) {
    c = Text[i];
    if (GlobalConfig.Font != FONT_LOAD) {
      if (c < 0x20 || c >= 0x7F)
        c = 0x5F;
      else
        c -= 0x20;        
    } else {
      c1 = (((c >=0x410) ? c -= 0x350 : c) & 0xff); //Russian letters
      c = c1;
    }
    
    egRawCompose(BufferPtr, FontPixelData + c * FontWidth + Shift,
                 GlobalConfig.CharWidth, FontHeight,
                 BufferLineOffset, FontLineOffset);
    if (i == Cursor) {
      egRawCompose(BufferPtr, FontPixelData + 0x5F * FontWidth + Shift,
                   GlobalConfig.CharWidth, FontHeight,
                   BufferLineOffset, FontLineOffset);
      
    }
    BufferPtr += GlobalConfig.CharWidth;
  }
}

/* EOF */
