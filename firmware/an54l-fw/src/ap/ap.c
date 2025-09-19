#include "ap.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nrf54l_fw, LOG_LEVEL_DBG);




void apInit(void)
{  
  systemInit();
}

void apMain(void)
{
  systemMain();
}

