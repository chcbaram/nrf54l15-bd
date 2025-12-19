#ifndef SSD1306_H_
#define SSD1306_H_

#include "hw_def.h"


#ifdef _USE_HW_SSD1306

#include "lcd.h"
#include "ssd1306_regs.h"

bool ssd1306Init(void);
bool ssd1306InitDriver(lcd_driver_t *p_driver);

#endif


#endif /* SRC_COMMON_HW_INCLUDE_LCD_SSD1306_H_ */
