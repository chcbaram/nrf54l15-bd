#include "lcd/ssd1315.h"




#ifdef _USE_HW_SSD1315
#include "spi.h"
#include "gpio.h"

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
static void ssd1315InitRegs(void);
static void ssd1315Fill(uint32_t color);
static void ssd1315SetRotation(uint8_t m);
static bool ssd1315Reset(void);
static bool ssd1315DrawFrame(void);
static void ssd1315DrawPixel(uint8_t x, uint8_t y, uint16_t color);
static bool ssd1315SendBuffer(uint8_t *p_data, uint32_t length, uint32_t timeout_ms);
static bool ssd1315SetCallBack(void (*p_func)(void));
static uint16_t ssd1315GetWidth(void);
static uint16_t ssd1315GetHeight(void);
static void ssd1315SetWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1);



bool ssd1315Init(void)
{
  bool ret;

  ret = ssd1315Reset();

  return ret;
}

bool ssd1315InitDriver(lcd_driver_t *p_driver)
{
  p_driver->init = ssd1315Init;
  p_driver->reset = ssd1315Reset;
  p_driver->setWindow = ssd1315SetWindow;
  p_driver->getWidth = ssd1315GetWidth;
  p_driver->getHeight = ssd1315GetHeight;
  p_driver->setCallBack = ssd1315SetCallBack;
  p_driver->sendBuffer = ssd1315SendBuffer;
  return true;
}

bool ssd1315Reset(void)
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

  ssd1315InitRegs();

  ssd1315Fill(black);
  ssd1315DrawFrame();

  ssd1315SetRotation(0);

  return true;
}

uint16_t ssd1315GetWidth(void)
{
  return HW_LCD_WIDTH;
}

uint16_t ssd1315GetHeight(void)
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

void ssd1315InitRegs(void)
{
  /* Init LCD */

  #if 1
  writecommand(0xAE); //display off
  writecommand(0x20); //Set Memory Addressing Mode
  writecommand(0x10); //00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
  writecommand(0xB0); //Set Page Start Address for Page Addressing Mode,0-7
  writecommand(0xC8); //Set COM Output Scan Direction
  writecommand(0x00); //---set low column address
  writecommand(0x10); //---set high column address
  writecommand(0x40); //--set start line address
  writecommand(0x81); //--set contrast control register
  writecommand(0xFF);
  writecommand(0xA1); //--set segment re-map 0 to 127
  writecommand(0xA6); //--set normal display
  writecommand(0xA8); //--set multiplex ratio(1 to 64)
  writecommand(0x3F); //
  writecommand(0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
  writecommand(0xD3); //-set display offset
  writecommand(0x00); //-not offset
  writecommand(0xD5); //--set display clock divide ratio/oscillator frequency
  writecommand(0xF0); //--set divide ratio
  writecommand(0xD9); //--set pre-charge period
  writecommand(0x22); //
  writecommand(0xDA); //--set com pins hardware configuration
  writecommand(0x12);
  writecommand(0xDB); //--set vcomh
  writecommand(0x20); //0x20,0.77xVcc
  writecommand(0x8D); //--set DC-DC enable
  writecommand(0x14); //
  writecommand(0xAF); //--turn on SSD1306 panel
  #else
	writecommand(0xAE);//--turn off oled panel
	writecommand(0x00);//---set low column address
	writecommand(0x10);//---set high column address
	writecommand(0x40);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
	writecommand(0x81);//--set contrast control register
	writecommand(0x80); // Set SEG Output Current Brightness
	writecommand(0xA1);//--Set SEG/Column Mapping     0xa0���ҷ��� 0xa1����
	writecommand(0xC8);//Set COM/Row Scan Direction   0xc0���·��� 0xc8����
	writecommand(0xA6);//--set normal display
	writecommand(0xA8);//--set multiplex ratio(1 to 64)
	writecommand(0x3f);//--1/64 duty
	writecommand(0xD3);//-set display offset	Shift Mapping RAM Counter (0x00~0x3F)
	writecommand(0x00);//-not offset
	writecommand(0xd5);//--set display clock divide ratio/oscillator frequency
	writecommand(0x80);//--set divide ratio, Set Clock as 100 Frames/Sec
	writecommand(0xD9);//--set pre-charge period
	writecommand(0xF1);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
	writecommand(0xDA);//--set com pins hardware configuration
	writecommand(0x12);
	writecommand(0xDB);//--set vcomh
	writecommand(0x40);//Set VCOM Deselect Level
	writecommand(0x20);//-Set Page Addressing Mode (0x00/0x01/0x02)
	writecommand(0x02);//
	writecommand(0x8D);//--set Charge Pump enable/disable
	writecommand(0x94);//--set(0x10) disable
	writecommand(0xA4);// Disable Entire Display On (0xa4/0xa5)
	writecommand(0xA6);// Disable Inverse Display On (0xa6/a7) 
	writecommand(0xAF); /*display ON*/ 

  #endif

}

void ssd1315SetRotation(uint8_t mode)
{
  rotation_mode = mode;
}

void ssd1315SetWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
}

void ssd1315Fill(uint32_t color)
{
  uint32_t i;

  for(i = 0; i < sizeof(frame_buffer); i++)
  {
    frame_buffer[i] = (color > 0) ? 0x00 : 0xFF;
  }
}

bool ssd1315SendBuffer(uint8_t *p_data, uint32_t length, uint32_t timeout_ms)
{
  uint16_t *p_buf = (uint16_t *)p_data;


  for (int y=0; y<HW_LCD_HEIGHT; y++)
  {
    for (int x=0; x<HW_LCD_WIDTH; x++)
    {
      ssd1315DrawPixel(x, y, p_buf[y*LCD_WIDTH + x]);
    }
  }

  ssd1315DrawFrame();

  if (frameCallBack != NULL)
  {
    frameCallBack();
  }

  return true;
}

bool ssd1315DrawFrame(void)
{
  uint8_t i;
  

  for (i = 0; i < 8; i++)
  {
    writecommand(0xB0 + i);
    writecommand(0x00);
    writecommand(0x10);

    gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
    gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

    spiTransfer(spi_ch, &frame_buffer[HW_LCD_WIDTH * i], NULL, HW_LCD_WIDTH, 50);

    gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
  }

  return true;
}

void ssd1315DrawPixel(uint8_t x, uint8_t y, uint16_t color)
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

bool ssd1315SetCallBack(void (*p_func)(void))
{
  frameCallBack = p_func;

  return true;
}


#endif