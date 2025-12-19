#include "spi.h"



#ifdef _USE_HW_SPI
#include <zephyr/drivers/spi.h>




typedef struct
{
  struct spi_dt_spec h_dt;
} spi_hw_t;

typedef struct
{
  bool is_open;
  bool is_tx_done;
  bool is_rx_done;
  bool is_error;

  void (*func_tx)(void);

  spi_hw_t *p_hw;
} spi_t;



static spi_t spi_tbl[SPI_MAX_CH];


const static spi_hw_t spi_hw_tbl[SPI_MAX_CH] = 
  {
    // {SPI_DT_SPEC_GET(DT_NODELABEL(gendev), SPI_WORD_SET(8) | SPI_TRANSFER_MSB, 0)},
    {SPI_DT_SPEC_GET(DT_NODELABEL(gendev), (SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE | SPI_TRANSFER_MSB | SPI_MODE_CPHA | SPI_MODE_CPOL), 0)},    
  };

static bool spiInitHw(uint8_t ch);






bool spiInit(void)
{
  bool ret = true;


  for (int i=0; i<SPI_MAX_CH; i++)
  {
    spi_tbl[i].is_open    = false;
    spi_tbl[i].is_tx_done = true;
    spi_tbl[i].is_rx_done = true;
    spi_tbl[i].is_error   = false;
    spi_tbl[i].func_tx    = NULL;
    spi_tbl[i].p_hw       = (spi_hw_t *)&spi_hw_tbl[i];
  }

  return ret;
}

bool spiBegin(uint8_t ch)
{
  bool ret = false;
  spi_t *p_spi = &spi_tbl[ch];

  switch(ch)
  {
    case _DEF_SPI1:
    case _DEF_SPI2:
      spiInitHw(ch);
      p_spi->is_open = true;
      ret = true;
      break;
  }

  return ret;
}

bool spiInitHw(uint8_t ch)
{
  bool ret = true;
  int err;

  switch(ch)
  {
    case _DEF_SPI1:
      err = spi_is_ready_dt(&spi_hw_tbl[ch].h_dt);
      if (!err)
      {
        logPrintf("[E_] spi : SPI device is not ready, err: %d\n", err);
        return 0;
      }
      break;

    case _DEF_SPI2:
      break;

    default:
      ret = false;
      break;
  }

  return ret;
}

void spiSetDataMode(uint8_t ch, uint8_t dataMode)
{
  // spi_t  *p_spi = &spi_tbl[ch];


  // if (p_spi->is_open == false) return;


  // switch( dataMode )
  // {
  //   // CPOL=0, CPHA=0
  //   case SPI_MODE0:
  //     p_spi->p_hw->p_cfg->polarity    = SPI_CLKPOL_LOW;
  //     p_spi->p_hw->p_cfg->phase       = SPI_CLKPHA_1EDGE;      
  //     SPI_Config(p_spi->p_hw->h_spi, p_spi->p_hw->p_cfg);
  //     break;

  //   // CPOL=0, CPHA=1
  //   case SPI_MODE1:
  //     p_spi->p_hw->p_cfg->polarity    = SPI_CLKPOL_LOW;
  //     p_spi->p_hw->p_cfg->phase       = SPI_CLKPHA_2EDGE;      
  //     SPI_Config(p_spi->p_hw->h_spi, p_spi->p_hw->p_cfg);      
  //     break;

  //   // CPOL=1, CPHA=0
  //   case SPI_MODE2:
  //     p_spi->p_hw->p_cfg->polarity    = SPI_CLKPOL_HIGH;
  //     p_spi->p_hw->p_cfg->phase       = SPI_CLKPHA_1EDGE;      
  //     SPI_Config(p_spi->p_hw->h_spi, p_spi->p_hw->p_cfg);         
  //     break;

  //   // CPOL=1, CPHA=1
  //   case SPI_MODE3:
  //     p_spi->p_hw->p_cfg->polarity    = SPI_CLKPOL_HIGH;
  //     p_spi->p_hw->p_cfg->phase       = SPI_CLKPHA_2EDGE;      
  //     SPI_Config(p_spi->p_hw->h_spi, p_spi->p_hw->p_cfg);         
  //     break;
  // }
}

void spiSetBitWidth(uint8_t ch, uint8_t bit_width)
{
  // spi_t  *p_spi = &spi_tbl[ch];

  // if (p_spi->is_open == false) return;

  
  // switch(bit_width)
  // {
  //   case 16:
  //     p_spi->p_hw->p_cfg->length = SPI_DATA_LENGTH_16B;
  //     SPI_ConfigDataSize(p_spi->p_hw->h_spi, p_spi->p_hw->p_cfg->length);
  //     break;

  //   default:
  //     p_spi->p_hw->p_cfg->length = SPI_DATA_LENGTH_8B;
  //     SPI_ConfigDataSize(p_spi->p_hw->h_spi, p_spi->p_hw->p_cfg->length);
  //     break;
  // }
}

bool spiTransmitReceive8(uint8_t ch, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t length, uint32_t timeout)
{
  bool      ret = true;
  spi_hw_t *p_hw = spi_tbl[ch].p_hw;
  int       err;

  struct spi_buf     tx_spi_buf     = {.buf = (void *)tx_buf, .len = length};
  struct spi_buf_set tx_spi_buf_set = {.buffers = &tx_spi_buf, .count = 1};
  struct spi_buf     rx_spi_bufs    = {.buf = rx_buf, .len = length};
  struct spi_buf_set rx_spi_buf_set = {.buffers = &rx_spi_bufs, .count = 1};


  err = spi_transceive_dt(&p_hw->h_dt, &tx_spi_buf_set, &rx_spi_buf_set);
  if (err < 0) 
  {
    return false;
  }

  return ret;
}

bool spiTransmitReceive16(uint8_t ch, uint16_t *tx_buf, uint16_t *rx_buf, uint32_t length, uint32_t timeout)
{
  bool      ret = true;

  return ret;
}

uint8_t spiTransfer8(uint8_t ch, uint8_t data)
{
  uint8_t ret = 0;
  spi_t  *p_spi = &spi_tbl[ch];

  if (p_spi->is_open == false) return 0;

  spiTransmitReceive8(ch, &data, &ret, 1, 10);

  return ret;
}

uint16_t spiTransfer16(uint8_t ch, uint16_t data)
{
  uint8_t tBuf[2];
  uint8_t rBuf[2];
  uint16_t ret;
  spi_t  *p_spi = &spi_tbl[ch];


  if (p_spi->is_open == false) 
    return 0;

  tBuf[1] = (uint8_t)data;
  tBuf[0] = (uint8_t)(data>>8);
  spiTransmitReceive8(ch, (uint8_t *)&tBuf, (uint8_t *)&rBuf, 2, 10);

  ret = rBuf[0];
  ret <<= 8;
  ret += rBuf[1];

  return ret;
}

bool spiTransfer(uint8_t ch, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t length, uint32_t timeout)
{
  bool      ret = true;
  spi_hw_t *p_hw = spi_tbl[ch].p_hw;
  int       err;

  struct spi_buf     tx_spi_buf     = {.buf = (void *)tx_buf, .len = length};
  struct spi_buf_set tx_spi_buf_set = {.buffers = &tx_spi_buf, .count = 1};
  struct spi_buf     rx_spi_bufs    = {.buf = rx_buf, .len = length};
  struct spi_buf_set rx_spi_buf_set = {.buffers = &rx_spi_bufs, .count = 1};

  if (spi_tbl[ch].is_open == false) 
    return false;


  if (rx_buf == NULL)
  {
    err = spi_write_dt(&p_hw->h_dt, &tx_spi_buf_set);
    if (err < 0) 
      ret = false;
  }
  else if (tx_buf == NULL)
  {
    err = spi_read_dt(&p_hw->h_dt, &rx_spi_buf_set);
    if (err < 0) 
      ret = false;
  }
  else
  {
    err = spi_transceive_dt(&p_hw->h_dt, &tx_spi_buf_set, &rx_spi_buf_set);
    if (err < 0) 
      ret = false;
  }

  return ret;
}

void spiAttachTxInterrupt(uint8_t ch, void (*func)())
{
  spi_t  *p_spi = &spi_tbl[ch];


  if (p_spi->is_open == false)     return;

  p_spi->func_tx = func;
}
#endif
