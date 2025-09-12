#include "ap.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nrf54l_fw, LOG_LEVEL_DBG);





void apInit(void)
{  
  logBoot(false);
  cliOpen(HW_UART_CH_CLI, 115200);  
  cliBegin();  
}

void apMain(void)
{
  uint32_t pre_time;

  pre_time = millis();
  while(1)
  {
    if (millis()-pre_time >= 500)
    {
      pre_time = millis();

      ledToggle(_DEF_LED1);
      ledToggle(_DEF_LED2);
    }

    cliMain();

    delay(5);
  }
}
