#include "ap.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nrf54l_fw, LOG_LEVEL_DBG);




void apInit(void)
{  

  for (int i = 0; i < 32; i += 1) 
  {
    lcdClearBuffer(black);
    lcdPrintfResize(0, 40 - i, white, 16, "  -- BARAM --");
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

