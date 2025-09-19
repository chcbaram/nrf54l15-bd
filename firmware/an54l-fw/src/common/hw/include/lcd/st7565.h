#ifndef ST7565_H_
#define ST7565_H_

#include "hw_def.h"


#ifdef _USE_HW_ST7565

#include "lcd.h"
#include "st7565_regs.h"



bool st7565Init(void);
bool st7565InitDriver(lcd_driver_t *p_driver);
bool st7565DrawAvailable(void);
bool st7565RequestDraw(void);
void st7565SetWindow(int32_t x, int32_t y, int32_t w, int32_t h);

uint32_t st7565GetFps(void);
uint32_t st7565GetFpsTime(void);

uint16_t st7565GetWidth(void);
uint16_t st7565GetHeight(void);

bool st7565SetCallBack(void (*p_func)(void));
bool st7565SendBuffer(uint8_t *p_data, uint32_t length, uint32_t timeout_ms);
bool st7565DrawBuffer(int16_t x, int16_t y, uint16_t *buffer, uint16_t w, uint16_t h);
bool st7565DrawBufferedLine(int16_t x, int16_t y, uint16_t *buffer, uint16_t w);


#endif

#endif 
