// adafruit_gfx_memory
//
// Example sketch which shows how to display an
// animated GIF image stored in FLASH memory
// using the Adafruit_GFX_TFT library
//
// written by Larry Bank
// bitbank@pobox.com
//
// To display a GIF from memory, a single callback function
// must be provided - GIFDRAW
// This function is called after each scan line is decoded
// and is passed the 8-bit pixels, RGB565 palette and info
// about how and where to display the line. The palette entries
// can be in little-endian or big-endian order; this is specified
// in the begin() method.
//
// The AnimatedGIF class doesn't allocate or free any memory, but the
// instance data occupies about 22.5K of RAM.
// 
#include <AnimatedGIF.h>

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "../test_images/badgers.h"

#define TFT_CS         10
#define TFT_RST        8
#define TFT_DC         9
#define TFT_MOSI       11
#define TFT_CLK        13
#define TFT_MISO       12

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
AnimatedGIF gif;

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y;

    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    
    s = pDraw->pPixels;
    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
      int x, iCount;
      pEnd = s + pDraw->iWidth;
      x = 0;
      iCount = 0; // count non-transparent pixels
      while(x < pDraw->iWidth)
      {
        c = ucTransparent-1;
        d = usTemp;
        while (c != ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent) // done, stop
          {
            s--; // back up to treat it like transparent
          }
          else // opaque
          {
             *d++ = usPalette[c];
             iCount++;
          }
        } // while looking for opaque pixels
        if (iCount) // any opaque pixels?
        {
          tft.setAddrWindow(pDraw->iX+x, y, iCount, 1);
          tft.startWrite();
          tft.writePixels(usTemp, iCount, false, false);
          tft.endWrite();
          x += iCount;
          iCount = 0;
        }
        // no, look for a run of transparent pixels
        c = ucTransparent;
        while (c == ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent)
             iCount++;
          else
             s--; 
        }
        if (iCount)
        {
          x += iCount; // skip these
          iCount = 0;
        }
      }
    }
    else
    {
      s = pDraw->pPixels;
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      for (x=0; x<pDraw->iWidth; x++)
        usTemp[x] = usPalette[*s++];
      tft.setAddrWindow(pDraw->iX, y, pDraw->iWidth, 1);
      tft.startWrite();
      tft.writePixels(usTemp, pDraw->iWidth, false, false);
      tft.endWrite();
    }
} /* GIFDraw() */


void setup() {
  Serial.begin(115200);
  
  // put your setup code here, to run once:
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  gif.begin(LITTLE_ENDIAN_PIXELS);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (gif.open((uint8_t *)ucBadgers, sizeof(ucBadgers), GIFDraw))
  {
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    while (gif.playFrame(true, NULL))
    {      
    }
    gif.close();
  }
}
