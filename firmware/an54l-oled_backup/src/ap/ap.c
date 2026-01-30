#include "ap.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nrf54l_fw, LOG_LEVEL_DBG);




void apInit(void)
{  

  for (int i = 0; i < LCD_HEIGHT/2; i += 2) 
  {
    lcdClearBuffer(black);
    lcdPrintfResize(0, LCD_HEIGHT - 8 - i, white, 16, "  -- BARAM --");
    lcdDrawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, black);
    lcdUpdateDraw();
    delay(10);
  }  
  delay(500);
  lcdClear(black);

  systemInit();
}

void apMain(void)
{
  systemMain();
}

