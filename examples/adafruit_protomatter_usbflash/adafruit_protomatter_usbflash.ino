// Play GIFs from CIRCUITPY drive (USB-accessible filesystem) to LED matrix.
// Adapted from msc_external_flash example in Adafruit_TinyUSB_Arduino
// Prerequisite libraroes: Adafruit_SPIFlash and SdFat (Adafruit fork)
// Set ENABLE_EXTENDED_TRANSFER_CLASS and FAT12_SUPPORT in SdFatConfig.h.
// Tools->USB Stack->TinyUSB

#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>
#include <AnimatedGIF.h>
#include <Adafruit_Protomatter.h>

// External flash macros for QSPI or SPI are defined in board variant file.
#if defined(EXTERNAL_FLASH_USE_QSPI)
  Adafruit_FlashTransport_QSPI flashTransport;
#elif defined(EXTERNAL_FLASH_USE_SPI)
  Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);
#else
  #error No QSPI/SPI flash are defined in your board variant.h!
#endif

Adafruit_SPIFlash flash(&flashTransport);

FatFileSystem filesys; // Filesystem object from SdFat
FatFile root;
FatFile file;

Adafruit_USBD_MSC usb_msc; // USB mass storage object

bool changed; // Is set true when host writes to filesystem

#if defined(_VARIANT_METRO_M4_)
  uint8_t rgbPins[]  = {2, 3, 4, 5, 6, 7};
  uint8_t addrPins[] = {A0, A1, A2, A3};
  uint8_t clockPin   = A4;
  uint8_t latchPin   = 10;
  uint8_t oePin      = 9;
#elif defined(_VARIANT_FEATHER_M4_)
  uint8_t rgbPins[]  = {6, 5, 9, 11, 10, 12};
  uint8_t addrPins[] = {A5, A4, A3, A2};
  uint8_t clockPin   = 13;
  uint8_t latchPin   = 0;
  uint8_t oePin      = 1;
#endif

Adafruit_Protomatter matrix(
  64, 10, 1, rgbPins, 4, addrPins, clockPin, latchPin, oePin, true);

AnimatedGIF gif;
File f;

void *GIFOpenFile(char *fname, int32_t *pSize) {
  f = filesys.open(fname);
  if (f) {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) { 
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

bool first_line = 0;

// Draw a line of image directly on the matrix
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y;

  usPalette = pDraw->pPalette;

#if 0
  // Gamma-correct the color palette on first frame.
  // This is bad news because it's already been 565 decimated,
  // so here we're taking those numbers, applying gamma to that,
  // and re-565-decimating. There's much loss here. Hmm.
  // This might require a different solution.
  // Preprocessing GIF with ImageMagick can salvage some
  // color fidelity, but not a lot. Problem is largely in
  // Protomatter's limited depth and lack of gamma correction.
  if(first_line) {
    first_line = 0;
    for(int i=0; i<256; i++) {
      uint16_t r = usPalette[i] >> 11; // 0 to 31
      uint16_t g = (usPalette[i] >> 5) & 0x3F; // 0 to 63
      uint16_t b = usPalette[i] & 0x1F; // 0 to 31
      r = (uint16_t)(pow(((float)r / 31.0), 2.5) * 31.0);
      g = (uint16_t)(pow(((float)g / 63.0), 2.5) * 63.0);
      b = (uint16_t)(pow(((float)b / 31.0), 2.5) * 31.0);
      usPalette[i] = (r << 11) | (g << 5) | b;
    }
  }
#endif

  y = pDraw->iY + pDraw->y; // current line

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) { // restore to background color
    for (x=0; x<pDraw->iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
         s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) { // if transparency used
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while(x < pDraw->iWidth) {
      c = ucTransparent-1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) { // done, stop
          s--; // back up to treat it like transparent
        } else { // opaque
           *d++ = usPalette[c];
           iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) { // any opaque pixels?
        uint16_t *ptr = matrix.getBuffer();
        memcpy(&ptr[y * matrix.width() + pDraw->iX + x], usTemp, iCount * 2);
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
           iCount++;
        else
           s--;
      }
      if (iCount) {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  } else {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x=0; x<pDraw->iWidth; x++)
      usTemp[x] = usPalette[*s++];
    uint16_t *ptr = matrix.getBuffer();
    memcpy(&ptr[y * matrix.width() + pDraw->iX], usTemp, pDraw->iWidth * 2);
  }

} /* GIFDraw() */

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  flash.begin();

  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "External Flash", "1.0");

  // Set disk size, block size should be 512 regardless of spi flash page size
  usb_msc.setCapacity(flash.pageSize() * flash.numPages() / 512, 512);

  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  usb_msc.setUnitReady(true); // MSC is ready for read/write
  
  usb_msc.begin();

  filesys.begin(&flash); // Start filesystem on the flash

  Serial.begin(115200);
  // while (!Serial);

  Serial.println("Adafruit TinyUSB Mass Storage External Flash example");
  Serial.print("JEDEC ID: "); Serial.println(flash.getJEDECID(), HEX);
  Serial.print("Flash size: "); Serial.println(flash.size());

  changed = true; // Print contents on start

  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  matrix.fillScreen(0);
  matrix.show();

  gif.begin(LITTLE_ENDIAN_PIXELS);
}

void loop() {
  Serial.println("About to call gif.open");
  if (gif.open((char *)"/gifs/dragons64x32.gif",
    GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    matrix.fillScreen(0);
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    first_line = 1;
    while (gif.playFrame(true, NULL)) {
      matrix.show();
    }
    gif.close();
  } else {
    Serial.println("Error opening file");
    for(;;);
  }
}

// Callback on READ10 command.
int32_t msc_read_cb (uint32_t lba, void *buffer, uint32_t bufsize) {
  return flash.readBlocks(lba, (uint8_t*)buffer, bufsize / 512) ? bufsize : -1;
}

// Callback on WRITE10 command.
int32_t msc_write_cb (uint32_t lba, uint8_t *buffer, uint32_t bufsize) {
  digitalWrite(LED_BUILTIN, HIGH);
  return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}

// Callback on WRITE10 completion.
void msc_flush_cb(void) {
  flash.syncBlocks(); // Sync with flash
  filesys.cacheClear(); // Clear filesystem cache to force refresh
  digitalWrite(LED_BUILTIN, LOW);
  changed = true;
}