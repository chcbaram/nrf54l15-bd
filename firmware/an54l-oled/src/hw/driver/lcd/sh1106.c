#include "lcd/sh1106.h"




#ifdef _USE_HW_SH1106
#include "i2c.h"
#include "gpio.h"

#define _PIN_DEF_DC     LCD_CD
#define _PIN_DEF_CS     LCD_CS
#define _PIN_DEF_RST    LCD_RST


#define CHARGEPUMP          0x8D
#define COLUMNADDR          0x21
#define COMSCANDEC          0xC8
#define COMSCANINC          0xC0
#define DISPLAYALLON        0xA5
#define DISPLAYALLON_RESUME 0xA4
#define DISPLAYOFF          0xAE
#define DISPLAYON           0xAF
#define EXTERNALVCC         0x1
#define INVERTDISPLAY       0xA7
#define MEMORYMODE          0x20
#define NORMALDISPLAY       0xA6
#define PAGEADDR            0x22
#define PAGESTARTADDRESS    0xB0
#define SEGREMAP            0xA1
#define SETCOMPINS          0xDA
#define SETCONTRAST         0x81
#define SETDISPLAYCLOCKDIV  0xD5
#define SETDISPLAYOFFSET    0xD3
#define SETHIGHCOLUMN       0x10
#define SETLOWCOLUMN        0x00
#define SETMULTIPLEX        0xA8
#define SETPRECHARGE        0xD9
#define SETSEGMENTREMAP     0xA1
#define SETSTARTLINE        0x40
#define SETVCOMDETECT       0xDB
#define SWITCHCAPVCC        0x2



static uint8_t i2c_ch = _DEF_CH1;
static uint8_t i2c_dev = 0x3C; 
static uint8_t rotation_mode = 0;
static void (*frameCallBack)(void) = NULL;

static uint8_t frame_buffer[HW_LCD_WIDTH * HW_LCD_HEIGHT / 8];


const uint32_t colstart = 0;
const uint32_t rowstart = 0;

static void writecommand(uint8_t c);
// static void writedata(uint8_t d);
static void sh1106InitRegs(void);
static void sh1106Fill(uint32_t color);
static void sh1106SetRotation(uint8_t m);
static bool sh1106Reset(void);
static bool sh1106DrawFrame(void);
static void sh1106DrawPixel(uint8_t x, uint8_t y, uint16_t color);
static bool sh1106SendBuffer(uint8_t *p_data, uint32_t length, uint32_t timeout_ms);
static bool sh1106SetCallBack(void (*p_func)(void));
static uint16_t sh1106GetWidth(void);
static uint16_t sh1106GetHeight(void);
static void sh1106SetWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1);



bool sh1106Init(void)
{
  bool ret;

  ret = sh1106Reset();

  return ret;
}

bool sh1106InitDriver(lcd_driver_t *p_driver)
{
  p_driver->init = sh1106Init;
  p_driver->reset = sh1106Reset;
  p_driver->setWindow = sh1106SetWindow;
  p_driver->getWidth = sh1106GetWidth;
  p_driver->getHeight = sh1106GetHeight;
  p_driver->setCallBack = sh1106SetCallBack;
  p_driver->sendBuffer = sh1106SendBuffer;
  return true;
}

bool sh1106Reset(void)
{
  // spiBegin(spi_ch);
  // spiSetDataMode(spi_ch, SPI_MODE0);

  // gpioPinWrite(_PIN_DEF_DC,  _DEF_HIGH);
  // gpioPinWrite(_PIN_DEF_CS,  _DEF_HIGH);
  // delay(10);

  // gpioPinWrite(_PIN_DEF_RST, _DEF_LOW);
  // delay(10);
  // gpioPinWrite(_PIN_DEF_RST, _DEF_HIGH);
  // delay(100);

  if (!i2cIsBegin(i2c_ch))
  {
    i2cBegin(i2c_ch, 400);
  }

  sh1106InitRegs();

  sh1106Fill(black);
  sh1106DrawFrame();

  sh1106SetRotation(0);

  return true;
}

uint16_t sh1106GetWidth(void)
{
  return HW_LCD_WIDTH;
}

uint16_t sh1106GetHeight(void)
{
  return HW_LCD_HEIGHT;
}

void writecommand(uint8_t c)
{
  i2cWriteByte(i2c_ch, i2c_dev, 0x80, c, 10);
}

void sh1106InitRegs(void)
{
  /* Init LCD */

  writecommand(0xAE); /*display off*/

  writecommand(0x02);  /*set lower column address*/
  writecommand(0x10);  /*set higher column address*/

  writecommand(0x40);  /*set display start line*/

  writecommand(0xB0);  /*set page address*/

  writecommand(0x81);  /*contract control*/
  writecommand(0xff);  /*128*/

  writecommand(0xA1);  /*set segment remap*/

  writecommand(0xA6);  /*normal / reverse*/

  writecommand(0xA8);  /*multiplex ratio*/
  writecommand(0x3F);  /*duty = 1/64*/

  writecommand(0xad);  /*set charge pump enable*/
  writecommand(0x8b);  /*    0x8B    코묩VCC   */

  writecommand(0x33);  /*0X30---0X33  set VPP   9V */

  writecommand(0xC8);  /*Com scan direction*/

  writecommand(0xD3);  /*set display offset*/
  writecommand(0x00);  /*   0x20  */

  writecommand(0xD5);  /*set osc division*/
  writecommand(0x80);

  writecommand(0xD9);  /*set pre-charge period*/
  writecommand(0x1f);  /*0x22*/

  writecommand(0xDA);  /*set COM pins*/
  writecommand(0x12);

  writecommand(0xdb);  /*set vcomh*/
  writecommand(0x40);

  writecommand(0xAF);  /*display ON*/
}

void sh1106SetRotation(uint8_t mode)
{
  rotation_mode = mode;
}

void sh1106SetWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
}

void sh1106Fill(uint32_t color)
{
  uint32_t i;

  for(i = 0; i < sizeof(frame_buffer); i++)
  {
    frame_buffer[i] = (color > 0) ? 0xFF : 0x00;
  }
}

bool sh1106SendBuffer(uint8_t *p_data, uint32_t length, uint32_t timeout_ms)
{
  uint16_t *p_buf = (uint16_t *)p_data;


  for (int y=0; y<HW_LCD_HEIGHT; y++)
  {
    for (int x=0; x<HW_LCD_WIDTH; x++)
    {
      sh1106DrawPixel(x, y, p_buf[y*LCD_WIDTH + x]);
    }
  }

  sh1106DrawFrame();

  if (frameCallBack != NULL)
  {
    frameCallBack();
  }

  return true;
}

bool sh1106DrawFrame(void)
{
  uint8_t i;
  

  for (i = 0; i < 8; i++)
  {
    writecommand(0xB0 + i);
    writecommand(0x02);
    writecommand(0x10);

    if (i2cWriteBytes(i2c_ch, i2c_dev, 0x40, &frame_buffer[HW_LCD_WIDTH * i], HW_LCD_WIDTH, 100) == false)
    {
      return false;
    }    
  }

  return true;
}

void sh1106DrawPixel(uint8_t x, uint8_t y, uint16_t color)
{
  if (x >= HW_LCD_WIDTH || y >= HW_LCD_HEIGHT)
  {
    return;
  }


  if (color > 0)
  {
    frame_buffer[x + (y / 8) * HW_LCD_WIDTH] |= 1 << (y % 8);
  }
  else
  {
    frame_buffer[x + (y / 8) * HW_LCD_WIDTH] &= ~(1 << (y % 8));
  }
}

bool sh1106SetCallBack(void (*p_func)(void))
{
  frameCallBack = p_func;

  return true;
}


#endif