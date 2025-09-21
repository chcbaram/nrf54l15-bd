#include "spi.h"
#include "gpio.h"




#ifdef _USE_HW_ST7567
#include "lcd/st7567.h"

#define _PIN_DEF_DC     LCD_CD
#define _PIN_DEF_CS     LCD_CS
#define _PIN_DEF_RST    LCD_RST



static uint8_t spi_ch = _DEF_SPI1;
static uint8_t rotation_mode = 0;
static void (*frameCallBack)(void) = NULL;

static uint8_t frame_buffer[HW_LCD_WIDTH * HW_LCD_HEIGHT / 8];


const uint32_t colstart = 0;
const uint32_t rowstart = 0;

static void writecommand(uint8_t c);
// static void writedata(uint8_t d);
static void st7567InitRegs(void);
static void st7567Fill(uint32_t color);
static void st7567SetRotation(uint8_t m);
static bool st7567Reset(void);
static bool st7567DrawFrame(void);
static void st7567DrawPixel(uint8_t x, uint8_t y, uint16_t color);




bool st7567Init(void)
{
  bool ret;

  ret = st7567Reset();

  return ret;
}

bool st7567InitDriver(lcd_driver_t *p_driver)
{
  p_driver->init = st7567Init;
  p_driver->reset = st7567Reset;
  p_driver->setWindow = st7567SetWindow;
  p_driver->getWidth = st7567GetWidth;
  p_driver->getHeight = st7567GetHeight;
  p_driver->setCallBack = st7567SetCallBack;
  p_driver->sendBuffer = st7567SendBuffer;
  return true;
}

bool st7567Reset(void)
{
  spiBegin(spi_ch);
  spiSetDataMode(spi_ch, SPI_MODE0);

  gpioPinWrite(_PIN_DEF_DC,  _DEF_HIGH);
  gpioPinWrite(_PIN_DEF_CS,  _DEF_HIGH);
  delay(10);

  gpioPinWrite(_PIN_DEF_RST, _DEF_LOW);
  delay(10);
  gpioPinWrite(_PIN_DEF_RST, _DEF_HIGH);
  delay(100);

  st7567InitRegs();

  st7567Fill(black);
  st7567DrawFrame();

  st7567SetRotation(0);

  return true;
}

uint16_t st7567GetWidth(void)
{
  return HW_LCD_WIDTH;
}

uint16_t st7567GetHeight(void)
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

// void writedata(uint8_t d)
// {
//   gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
//   gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

//   spiTransfer8(spi_ch, d);

//   gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
// }

void st7567InitRegs(void)
{

  writecommand(ST7567_EXIT_SOFTRST);
	writecommand(ST7567_BIAS_1_7);
	writecommand(ST7567_DISPON);
	writecommand(SSD1305_MAPCOL0);
	writecommand(ST7567_SETCOMREVERSE);
	writecommand(ST7567_REG_RES_RR1);

  // Set Contrast
  writecommand(ST7567_SETEV); 
	writecommand(8);

	writecommand(ST7567_POWERCTRL);
	writecommand(ST7567_SETSTARTLINE);
	writecommand(ST7567_SETPAGESTART);
	writecommand(ST7567_SETCOLH);
	writecommand(ST7567_SETCOLL);
	writecommand(ST7567_DISPINVERSE);
	writecommand(ST7567_DISPON);
	writecommand(ST7567_DISPRAM);
  writecommand(ST7567_REGULATION_RATIO | 0x6);



  int page, j;
  int col = 0;
  for (page = 0; page < 4; page++)
  {
    writecommand(ST7567_SETPAGESTART + page);
    writecommand(ST7567_SETCOLL + (col & 0x0f));
    writecommand(ST7567_SETCOLH + (col >> 4));

    gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
    gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

    for (j = 0; j < 132; j++)
    {
      spiTransfer8(spi_ch, 0x00);
    }
    gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
  }
}

void st7567SetRotation(uint8_t mode)
{
  rotation_mode = mode;
}

void st7567SetWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
}

void st7567Fill(uint32_t color)
{
  uint32_t i;

  for(i = 0; i < sizeof(frame_buffer); i++)
  {
    frame_buffer[i] = (color > 0) ? 0x00 : 0xFF;
  }
}

bool st7567SendBuffer(uint8_t *p_data, uint32_t length, uint32_t timeout_ms)
{
  uint16_t *p_buf = (uint16_t *)p_data;


  for (int y=0; y<HW_LCD_HEIGHT; y++)
  {
    for (int x=0; x<HW_LCD_WIDTH; x++)
    {
      st7567DrawPixel(x, y, p_buf[y*LCD_WIDTH + x]);
    }
  }

  st7567DrawFrame();

  if (frameCallBack != NULL)
  {
    frameCallBack();
  }

  return true;
}

bool st7567DrawFrame(void)
{
  int16_t page;
  int16_t col = 0;
  
  for (page = 0; page < 4; page++)
  {
    writecommand(ST7567_SETPAGESTART + page);
    writecommand(ST7567_SETCOLL + (col & 0x0F));
    writecommand(ST7567_SETCOLH + (col >> 4));

    gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
    gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

    spiTransfer(spi_ch, &frame_buffer[HW_LCD_WIDTH * page], NULL, HW_LCD_WIDTH, 50);

    gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
  }

  return true;
}

void st7567DrawPixel(uint8_t x, uint8_t y, uint16_t color)
{
  if (x >= HW_LCD_WIDTH || y >= HW_LCD_HEIGHT)
  {
    return;
  }


  if (color == 0)
  {
    frame_buffer[x + (y / 8) * HW_LCD_WIDTH] |= 1 << (y % 8);
  }
  else
  {
    frame_buffer[x + (y / 8) * HW_LCD_WIDTH] &= ~(1 << (y % 8));
  }
}

bool st7567SetCallBack(void (*p_func)(void))
{
  frameCallBack = p_func;

  return true;
}


#endif