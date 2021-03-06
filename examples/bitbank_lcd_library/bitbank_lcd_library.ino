#include <AnimatedGIF.h>
#include <bb_spi_lcd.h>
#include "../test_images/badgers.h"

#define LED_PIN       -1
#define CS_PIN        -1
#define RESET_PIN      5
#define DC_PIN         4
#define MOSI_PIN -1
#define SCK_PIN -1
#define MISO_PIN -1

//#define MOSI_PIN       7
//#define SCK_PIN        5
//#define MISO_PIN       6

AnimatedGIF gif;
uint8_t ucTXBuf[1024];

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
          spilcdSetPosition(pDraw->iX+x, y, iCount, 1, 1);
          spilcdWriteDataBlock((uint8_t *)usTemp, iCount*2, 1);
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
      spilcdSetPosition(pDraw->iX, y, pDraw->iWidth, 1, 1);
      spilcdWriteDataBlock((uint8_t *)usTemp, pDraw->iWidth*2, 1);
    }
} /* GIFDraw() */

void setup() {
  Serial.begin(115200);
  while (!Serial);

  spilcdSetTXBuffer(ucTXBuf, sizeof(ucTXBuf));
  spilcdInit(LCD_ST7735R, 0, 0, 1, 20000000, CS_PIN, DC_PIN, RESET_PIN, LED_PIN, MISO_PIN, MOSI_PIN, SCK_PIN); // custom ESP32 rig
  spilcdSetOrientation(LCD_ORIENTATION_ROTATED);
  spilcdFill(0,1);

  gif.begin(BIG_ENDIAN_PIXELS);
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
