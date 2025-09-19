#include "spi.h"
#include "gpio.h"




#ifdef _USE_HW_ST7565
#include "lcd/st7565.h"
#include "lcd/st7565_regs.h"

#define _PIN_DEF_DC     LCD_CD
#define _PIN_DEF_CS     LCD_CS
#define _PIN_DEF_RST    LCD_RST

#define MADCTL_MY       0x80
#define MADCTL_MX       0x40
#define MADCTL_MV       0x20
#define MADCTL_ML       0x10
#define MADCTL_RGB      0x00
#define MADCTL_BGR      0x08
#define MADCTL_MH       0x04


#define ST7565_ON                1    // ON pixel
#define ST7565_OFF               0    // OFF pixel
#define ST7565_INVERSE           2    // inverse pixel

#define LCDWIDTH                 128
#define LCDHEIGHT                64

#define CMD_DISPLAY_OFF          0xAE // lcd off
#define CMD_DISPLAY_ON           0xAF // lcd on

#define CMD_SET_DISP_START_LINE  0x40 // start line address, 0x40
#define CMD_SET_PAGE             0xB0 // start page address

#define CMD_SET_COLUMN_UPPER     0x10
#define CMD_SET_COLUMN_LOWER     0x00

#define CMD_SET_ADC_NORMAL       0xA0
#define CMD_SET_ADC_REVERSE      0xA1

#define CMD_SET_DISP_NORMAL      0xA6
#define CMD_SET_DISP_REVERSE     0xA7

#define CMD_SET_ALLPTS_NORMAL    0xA4
#define CMD_SET_ALLPTS_ON        0xA5
#define CMD_SET_BIAS_9           0xA2
#define CMD_SET_BIAS_7           0xA3

#define CMD_RMW                  0xE0
#define CMD_RMW_CLEAR            0xEE
#define CMD_INTERNAL_RESET       0xE2
#define CMD_SET_COM_NORMAL       0xC0
#define CMD_SET_COM_REVERSE      0xC8
#define CMD_SET_POWER_CONTROL    0x28
#define CMD_SET_RESISTOR_RATIO   0x20
#define CMD_SET_VOLUME_FIRST     0x81
#define CMD_SET_VOLUME_SECOND    0
#define CMD_SET_STATIC_OFF       0xAC
#define CMD_SET_STATIC_ON        0xAD
#define CMD_SET_STATIC_REG       0x0
#define CMD_SET_BOOSTER_FIRST    0xF8
#define CMD_SET_BOOSTER_234      0
#define CMD_SET_BOOSTER_5        1
#define CMD_SET_BOOSTER_6        3
#define CMD_NOP                  0xE3
#define CMD_TEST                 0xF0


static uint8_t   spi_ch = _DEF_SPI1;

static const int32_t _width  = HW_LCD_WIDTH;
static const int32_t _height = HW_LCD_HEIGHT;
static void (*frameCallBack)(void) = NULL;
volatile static bool  is_write_frame = false;

#if HW_ST7565_MODEL == 0
const uint32_t colstart = 1;
const uint32_t rowstart = 26;
#else
const uint32_t colstart = 0;
const uint32_t rowstart = 0;
#endif

static void writecommand(uint8_t c);
static void writedata(uint8_t d);
static void st7565InitRegs(void);
static void st7565FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
static void st7565SetRotation(uint8_t m);
static bool st7565Reset(void);


static void TransferDoneISR(void)
{
  if (is_write_frame == true)
  {
    is_write_frame = false;    

    if (frameCallBack != NULL)
    {
      frameCallBack();
    }
  }
}


bool st7565Init(void)
{
  bool ret;

  ret = st7565Reset();

  return ret;
}

bool st7565InitDriver(lcd_driver_t *p_driver)
{
  p_driver->init = st7565Init;
  p_driver->reset = st7565Reset;
  p_driver->setWindow = st7565SetWindow;
  p_driver->getWidth = st7565GetWidth;
  p_driver->getHeight = st7565GetHeight;
  p_driver->setCallBack = st7565SetCallBack;
  p_driver->sendBuffer = st7565SendBuffer;
  return true;
}

bool st7565Reset(void)
{
  spiBegin(spi_ch);
  spiSetDataMode(spi_ch, SPI_MODE0);

  spiAttachTxInterrupt(spi_ch, TransferDoneISR);

    
  gpioPinWrite(_PIN_DEF_DC,  _DEF_HIGH);
  gpioPinWrite(_PIN_DEF_CS,  _DEF_HIGH);
  delay(10);

  st7565InitRegs();

  // st7565SetRotation(1);
  // st7565FillRect(0, 0, HW_LCD_WIDTH, HW_LCD_HEIGHT, white);

  return true;
}

uint16_t st7565GetWidth(void)
{
  return HW_LCD_WIDTH;
}

uint16_t st7565GetHeight(void)
{
  return HW_LCD_HEIGHT;
}

void writecommand(uint8_t c)
{
  gpioPinWrite(_PIN_DEF_DC, _DEF_LOW);
  gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

  spiTransfer8(spi_ch, c);

  gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
}

void writedata(uint8_t d)
{
  gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
  gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

  spiTransfer8(spi_ch, d);

  gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
}

void st7565InitRegs(void)
{
  // LCD bias select
  writecommand(CMD_SET_BIAS_7);
  // ADC select
  writecommand(CMD_SET_ADC_NORMAL);
  // SHL select
  writecommand(CMD_SET_COM_NORMAL);
  // Initial display line
  writecommand(CMD_SET_DISP_START_LINE);

  // turn on voltage converter (VC=1, VR=0, VF=0)
  writecommand(CMD_SET_POWER_CONTROL | 0x4);
  // wait for 50% rising
  delay(50);

  // turn on voltage regulator (VC=1, VR=1, VF=0)
  writecommand(CMD_SET_POWER_CONTROL | 0x6);
  // wait >=50ms
  delay(50);

  // turn on voltage follower (VC=1, VR=1, VF=1)
  writecommand(CMD_SET_POWER_CONTROL | 0x7);
  // wait
  delay(10);

  // set lcd operating voltage (regulator resistor, ref voltage resistor)
  writecommand(CMD_SET_RESISTOR_RATIO | 0x6);
  writecommand(CMD_DISPLAY_ON);
  writecommand(CMD_SET_ALLPTS_NORMAL);

  writecommand(CMD_SET_VOLUME_FIRST);
  writecommand(CMD_SET_VOLUME_SECOND | (60 & 0x3f));
  return;


  writecommand(ST7565_SWRESET); //  1: Software reset, 0 args, w/delay
  delay(10);

  writecommand(ST7565_SLPOUT);  //  2: Out of sleep mode, 0 args, w/delay
  delay(10);

  writecommand(ST7565_FRMCTR1); //  3: Frame rate ctrl - normal mode, 3 args:
  writedata(0x01);              //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
  writedata(0x2C);
  writedata(0x2D);

  writecommand(ST7565_FRMCTR2); //  4: Frame rate control - idle mode, 3 args:
  writedata(0x01);              //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
  writedata(0x2C);
  writedata(0x2D);

  writecommand(ST7565_FRMCTR3); //  5: Frame rate ctrl - partial mode, 6 args:
  writedata(0x01);              //     Dot inversion mode
  writedata(0x2C);
  writedata(0x2D);
  writedata(0x01);              //     Line inversion mode
  writedata(0x2C);
  writedata(0x2D);

  writecommand(ST7565_INVCTR);  //  6: Display inversion ctrl, 1 arg, no delay:
  writedata(0x07);              //     No inversion

  writecommand(ST7565_PWCTR1);  //  7: Power control, 3 args, no delay:
  writedata(0xA2);
  writedata(0x02);              //     -4.6V
  writedata(0x84);              //     AUTO mode

  writecommand(ST7565_PWCTR2);  //  8: Power control, 1 arg, no delay:
  writedata(0xC5);              //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD

  writecommand(ST7565_PWCTR3);  //  9: Power control, 2 args, no delay:
  writedata(0x0A);              //     Opamp current small
  writedata(0x00);              //     Boost frequency

  writecommand(ST7565_PWCTR4);  // 10: Power control, 2 args, no delay:
  writedata(0x8A);              //     BCLK/2, Opamp current small & Medium low
  writedata(0x2A);

  writecommand(ST7565_PWCTR5);  // 11: Power control, 2 args, no delay:
  writedata(0x8A);
  writedata(0xEE);

  writecommand(ST7565_VMCTR1);  // 12: Power control, 1 arg, no delay:
  writedata(0x0E);

#if HW_ST7565_MODEL == 0
  writecommand(ST7565_INVON);   // 13: Don't invert display, no args, no delay
#else
  writecommand(ST7565_INVOFF);  // 13: Don't invert display, no args, no delay
#endif

  writecommand(ST7565_MADCTL);  // 14: Memory access control (directions), 1 arg:
  writedata(0xC8);              //     row addr/col addr, bottom to top refresh

  writecommand(ST7565_COLMOD);  // 15: set color mode, 1 arg, no delay:
  writedata(0x05);              //     16-bit color


  writecommand(ST7565_CASET);   //  1: Column addr set, 4 args, no delay:
  writedata(0x00);
  writedata(0x00);              //     XSTART = 0
  writedata(0x00);
  writedata(HW_LCD_WIDTH-1);    //     XEND = 159

  writecommand(ST7565_RASET);   //  2: Row addr set, 4 args, no delay:
  writedata(0x00);
  writedata(0x00);              //     XSTART = 0
  writedata(0x00);
  writedata(HW_LCD_HEIGHT-1);   //     XEND = 79


  writecommand(ST7565_NORON);   //  3: Normal display on, no args, w/delay
  delay(10);
  writecommand(ST7565_DISPON);  //  4: Main screen turn on, no args w/delay
  delay(10);
}

void st7565SetRotation(uint8_t mode)
{
  writecommand(ST7565_MADCTL);

  switch (mode)
  {
   case 0:
     writedata(MADCTL_MX | MADCTL_MY | MADCTL_BGR);
     break;

   case 1:
     writedata(MADCTL_MY | MADCTL_MV | MADCTL_BGR);
     break;

  case 2:
    writedata(MADCTL_BGR);
    break;

   case 3:
     writedata(MADCTL_MX | MADCTL_MV | MADCTL_BGR);
     break;
  }
}

void st7565SetWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
  spiSetBitWidth(spi_ch, 8);

  writecommand(ST7565_CASET); // Column addr set
  writedata(0x00);
  writedata(x0+colstart);     // XSTART
  writedata(0x00);
  writedata(x1+colstart);     // XEND

  writecommand(ST7565_RASET); // Row addr set
  writedata(0x00);
  writedata(y0+rowstart);     // YSTART
  writedata(0x00);
  writedata(y1+rowstart);     // YEND

  writecommand(ST7565_RAMWR); // write to RAM
}

void st7565FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
  uint16_t line_buf[w];

  // Clipping
  if ((x >= _width) || (y >= _height)) return;

  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }

  if ((x + w) > _width)  w = _width  - x;
  if ((y + h) > _height) h = _height - y;

  if ((w < 1) || (h < 1)) return;


  st7565SetWindow(x, y, x + w - 1, y + h - 1);
  spiSetBitWidth(spi_ch, 16);

  gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
  gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

  for (int i=0; i<w; i++)
  {
    line_buf[i] = color;
  }
  for (int i=0; i<h; i++)
  {
    // if (spiDmaTxTransfer(_DEF_SPI1, (void *)line_buf, w, 10) != true)
    // {
    //   break;
    // }
      if (!spiTransfer(_DEF_SPI1, (uint8_t *)line_buf, NULL, w, 10))
      {
        break;
      }
  }
  gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
}

bool st7565SendBuffer(uint8_t *p_data, uint32_t length, uint32_t timeout_ms)
{
  is_write_frame = true;

  spiSetBitWidth(spi_ch, 16);

  gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
  gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

  spiDmaTxTransfer(_DEF_SPI1, (void *)p_data, length, 0);
  return true;
}

bool st7565SetCallBack(void (*p_func)(void))
{
  frameCallBack = p_func;

  return true;
}


#endif