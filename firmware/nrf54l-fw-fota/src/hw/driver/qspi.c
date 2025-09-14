#include "qspi.h"



#ifdef _USE_HW_QSPI
#include "cli.h"
#include <zephyr/drivers/flash.h>
#include <jesd216.h>


#define QSPI_FLASH_SIZE           (8 * 1024 * 1024)
#define QSPI_FLASH_SECTOR_SIZE    (64 * 1024)
#define QSPI_FLASH_SUBSECTOR_SIZE 4096   
#define QSPI_FLASH_PAGE_SIZE      256       

#define QSPI_BASE_ADDRESS         0x90000000

#define SPI_FLASH_COMPAT          jedec_mspi_nor


/* QSPI Info */
typedef struct {
  uint32_t FlashSize;          /*!< Size of the flash */
  uint32_t EraseSectorSize;    /*!< Size of sectors for the erase operation */
  uint32_t EraseSectorsNumber; /*!< Number of sectors for the erase operation */
  uint32_t ProgPageSize;       /*!< Size of pages for the program operation */
  uint32_t ProgPagesNumber;    /*!< Number of pages for the program operation */

  uint8_t  device_id[20];
} QSPI_Info;


static bool is_init = false;

#if CLI_USE(HW_QSPI)
static void cliCmd(cli_args_t *args);
#endif

const struct device *qspi_dev = DEVICE_DT_GET_ONE(SPI_FLASH_COMPAT);





bool qspiInit(void)
{
  bool ret = true;
  QSPI_Info info;


  if (!device_is_ready(qspi_dev))
  {
    logPrintf("[E_] qspi device not ready : %s.\n", qspi_dev->name);
    return false;
  }




  flash_read_jedec_id(qspi_dev, info.device_id);
  if (info.device_id[0] == 0xC2 && info.device_id[1] == 0x28 && info.device_id[2] == 0x17)
  {
    logPrintf("[OK] qspiInit()\n");
    logPrintf("     MX25R64 Found\r\n");
    ret = true;
  }
  else
  {
    logPrintf("[E_] qspiInit()\n");
    logPrintf("     MX25R64 Not Found %X %X %X\r\n", info.device_id[0], info.device_id[1], info.device_id[2]);
    ret = false;
  }

  is_init = ret;

#if CLI_USE(HW_QSPI)
  cliAdd("qspi", cliCmd);
#endif
  return ret;
}

bool qspiReset(void)
{
  return true;
}

bool qspiAbort(void)
{
  return true;
}

bool qspiIsInit(void)
{
  return is_init;
}

bool qspiRead(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  int ret;

	ret = flash_read(qspi_dev, addr, p_data, length);
	if (ret != 0) 
  {
		return false;
	}

  return true;
}

bool qspiWrite(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  int ret;

  if (addr >= qspiGetLength())
  {
    return false;
  }

  ret = flash_write(qspi_dev, addr, p_data, length);
  if (ret != 0)
  {
    return false;
  }

  return true;
}

bool qspiEraseBlock(uint32_t block_addr)
{
  int ret;

	ret = flash_erase(qspi_dev, block_addr, QSPI_FLASH_SUBSECTOR_SIZE);
  if (ret != 0)
  {
    return false;
  }

  return true;
}

bool qspiEraseSector(uint32_t sector_addr)
{
  int ret;

	ret = flash_erase(qspi_dev, sector_addr, QSPI_FLASH_SECTOR_SIZE);
  if (ret != 0)
  {
    return false;
  }

  return true;
}

bool qspiErase(uint32_t addr, uint32_t length)
{
  bool ret = true;
  uint32_t flash_length;
  uint32_t block_size;
  uint32_t block_begin;
  uint32_t block_end;
  uint32_t i;

  flash_length = QSPI_FLASH_SIZE;
  block_size   = QSPI_FLASH_SECTOR_SIZE;


  if ((addr > flash_length) || ((addr+length) > flash_length))
  {
    return false;
  }
  if (length == 0)
  {
    return false;
  }


  block_begin = addr / block_size;
  block_end   = (addr + length - 1) / block_size;


  for (i=block_begin; i<=block_end; i++)
  {
    ret = qspiEraseSector(block_size*i);
    if (ret == false)
    {
      break;
    }
  }

  return ret;
}

bool qspiEraseChip(void)
{
  return true;
}

bool qspiGetStatus(void)
{
  return true;
}

bool qspiGetInfo(qspi_info_t* p_info)
{
  return true;
}

bool qspiEnableMemoryMappedMode(void)
{
  return true;
}

bool qspiSetXipMode(bool enable)
{
  return true;
}

bool qspiGetXipMode(void)
{
  return false;
}

uint32_t qspiGetAddr(void)
{
  return QSPI_BASE_ADDRESS;
}

uint32_t qspiGetLength(void)
{
  return QSPI_FLASH_SIZE;
}


#if CLI_USE(HW_QSPI)
void cliCmd(cli_args_t *args)
{
  bool ret = false;
  uint32_t i;
  uint32_t addr;
  uint32_t length;
  uint8_t  data;
  uint32_t pre_time;
  bool flash_ret;



  if(args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("qspi flash addr  : 0x%X\n", 0);
    cliPrintf("qspi xip   addr  : 0x%X\n", qspiGetAddr());
    cliPrintf("qspi xip   mode  : %s\n", qspiGetXipMode() ? "True":"False");
    ret = true;
  }
  
  if(args->argc == 1 && args->isStr(0, "test"))
  {
    uint8_t rx_buf[256];

    for (int i=0; i<100; i++)
    {
      if (qspiRead(0x1000*i, rx_buf, 256))
      {
        cliPrintf("%d : OK\n", i);
      }
      else
      {
        cliPrintf("%d : FAIL\n", i);
        break;
      }
    }
    ret = true;
  }    

  if (args->argc == 2 && args->isStr(0, "xip"))
  {
    bool xip_enable;

    xip_enable = args->isStr(1, "on") ? true:false;

    if (qspiSetXipMode(xip_enable))
      cliPrintf("qspiSetXipMode() : OK\n");
    else
      cliPrintf("qspiSetXipMode() : Fail\n");
    
    cliPrintf("qspi xip mode  : %s\n", qspiGetXipMode() ? "True":"False");

    ret = true;
  } 

  if (args->argc == 3 && args->isStr(0, "read"))
  {
    addr   = (uint32_t)args->getData(1);
    length = (uint32_t)args->getData(2);

    for (i=0; i<length; i++)
    {
      flash_ret = qspiRead(addr+i, &data, 1);

      if (flash_ret == true)
      {
        cliPrintf( "addr : 0x%X\t 0x%02X\n", addr+i, data);
      }
      else
      {
        cliPrintf( "addr : 0x%X\t Fail\n", addr+i);
      }
    }
    ret = true;
  }
  
  if(args->argc == 3 && args->isStr(0, "erase") == true)
  {
    addr   = (uint32_t)args->getData(1);
    length = (uint32_t)args->getData(2);

    pre_time = millis();
    flash_ret = qspiErase(addr, length);

    cliPrintf( "addr : 0x%X\t len : %d %d ms\n", addr, length, (millis()-pre_time));
    if (flash_ret)
    {
      cliPrintf("OK\n");
    }
    else
    {
      cliPrintf("FAIL\n");
    }
    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "erase-block") == true)
  {
    addr   = (uint32_t)args->getData(1);
    length = (uint32_t)args->getData(2);

    pre_time = millis();
    flash_ret = qspiEraseBlock(addr);

    cliPrintf( "addr : 0x%X\t len : %d %d ms\n", addr, length, (millis()-pre_time));
    if (flash_ret)
    {
      cliPrintf("OK\n");
    }
    else
    {
      cliPrintf("FAIL\n");
    }
    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "write") == true)
  {
    uint32_t flash_data;

    addr = (uint32_t)args->getData(1);
    flash_data = (uint32_t )args->getData(2);

    pre_time = millis();
    flash_ret = qspiWrite(addr, (uint8_t *)&flash_data, 4);

    cliPrintf( "addr : 0x%X\t 0x%X %dms\n", addr, flash_data, millis()-pre_time);
    if (flash_ret)
    {
      cliPrintf("OK\n");
    }
    else
    {
      cliPrintf("FAIL\n");
    }
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "speed-test") == true)
  {
    uint32_t buf[512/4];
    uint32_t cnt;
    uint32_t pre_time;
    uint32_t exe_time;
    uint32_t xip_addr;

    xip_addr = qspiGetAddr();
    cnt = 1024*1024 / 512;
    pre_time = millis();
    for (int i=0; i<cnt; i++)
    {
      if (qspiGetXipMode())
      {
        memcpy(buf, (void *)(xip_addr + i*512), 512);
      }
      else
      {
        if (qspiRead(i*512, (uint8_t *)buf, 512) == false)
        {
          cliPrintf("qspiRead() Fail:%d\n", i);
          break;
        }
      }
    }
    exe_time = millis()-pre_time;
    if (exe_time > 0)
    {
      cliPrintf("%d KB/sec\n", 1024 * 1000 / exe_time);
    }
    ret = true;
  }


  if (ret == false)
  {
    cliPrintf("qspi info\n");
    cliPrintf("qspi xip on:off\n");
    cliPrintf("qspi test\n");
    cliPrintf("qspi speed-test\n");
    cliPrintf("qspi read  [addr] [length]\n");
    cliPrintf("qspi erase [addr] [length]\n");
    cliPrintf("qspi erase-block [addr] [length]\n");
    cliPrintf("qspi write [addr] [data]\n");
  }
}
#endif

#endif