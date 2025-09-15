#include "ble_uart.h"


#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/usb/usb_device.h>

#include <soc.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

#include <bluetooth/services/nus.h>
#include <zephyr/settings/settings.h>

#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>




#define LOG_MODULE_NAME peripheral_uart
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define STACKSIZE               CONFIG_BT_NUS_THREAD_STACK_SIZE
#define PRIORITY                7

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED          DK_LED1
#define RUN_LED_BLINK_INTERVAL  1000

#define CON_STATUS_LED          DK_LED2

#define KEY_PASSKEY_ACCEPT      DK_BTN1_MSK
#define KEY_PASSKEY_REJECT      DK_BTN2_MSK

#define UART_BUF_SIZE           CONFIG_BT_NUS_UART_BUFFER_SIZE
#define UART_WAIT_FOR_BUF_DELAY K_MSEC(50)
#define UART_WAIT_FOR_RX        CONFIG_BT_NUS_UART_RX_WAIT_TIME

static K_SEM_DEFINE(ble_init_ok, 0, 1);

static struct bt_conn *current_conn;
static struct bt_conn *auth_conn;
static struct k_work   adv_work;


struct uart_data_t
{
  void    *fifo_reserved;
  uint8_t  data[UART_BUF_SIZE];
  uint16_t len;
};

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

static const struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
  BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
  BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};


static void adv_work_handler(struct k_work *work)
{
  int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

  if (err)
  {
    LOG_ERR("Advertising failed to start (err %d)", err);
    return;
  }

  LOG_INF("Advertising successfully started");
}

static void advertising_start(void)
{
  k_work_submit(&adv_work);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
  char addr[BT_ADDR_LE_STR_LEN];

  if (err)
  {
    LOG_ERR("Connection failed, err 0x%02x %s", err, bt_hci_err_to_str(err));
    return;
  }

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  LOG_INF("Connected %s", addr);

  current_conn = bt_conn_ref(conn);

  // dk_set_led_on(CON_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  LOG_INF("Disconnected: %s, reason 0x%02x %s", addr, reason, bt_hci_err_to_str(reason));

  if (auth_conn)
  {
    bt_conn_unref(auth_conn);
    auth_conn = NULL;
  }

  if (current_conn)
  {
    bt_conn_unref(current_conn);
    current_conn = NULL;
    // dk_set_led_off(CON_STATUS_LED);
  }
}

static void recycled_cb(void)
{
  LOG_INF("Connection object available from previous conn. Disconnect is complete!");
  advertising_start();
}

#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  if (!err)
  {
    LOG_INF("Security changed: %s level %u", addr, level);
  }
  else
  {
    LOG_WRN("Security failed: %s level %u err %d %s", addr, level, err,
            bt_security_err_to_str(err));
  }
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
  .connected    = connected,
  .disconnected = disconnected,
  .recycled     = recycled_cb,
#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
  .security_changed = security_changed,
#endif
};

#if defined(CONFIG_BT_NUS_SECURITY_ENABLED)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  LOG_INF("Passkey for %s: %06u", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
  char addr[BT_ADDR_LE_STR_LEN];

  auth_conn = bt_conn_ref(conn);

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  LOG_INF("Passkey for %s: %06u", addr, passkey);

  if (IS_ENABLED(CONFIG_SOC_SERIES_NRF54HX) || IS_ENABLED(CONFIG_SOC_SERIES_NRF54LX))
  {
    LOG_INF("Press Button 0 to confirm, Button 1 to reject.");
  }
  else
  {
    LOG_INF("Press Button 1 to confirm, Button 2 to reject.");
  }
}

static void auth_cancel(struct bt_conn *conn)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  LOG_INF("Pairing cancelled: %s", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  LOG_INF("Pairing failed conn: %s, reason %d %s", addr, reason,
          bt_security_err_to_str(reason));
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
  .passkey_display = auth_passkey_display,
  .passkey_confirm = auth_passkey_confirm,
  .cancel          = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
  .pairing_complete = pairing_complete,
  .pairing_failed   = pairing_failed};
#else
static struct bt_conn_auth_cb      conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data,
                          uint16_t len)
{
  char addr[BT_ADDR_LE_STR_LEN] = {0};

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

  LOG_INF("Received data from: %s", addr);
}

static struct bt_nus_cb nus_cb = {
  .received = bt_receive_cb,
};

void error(void)
{
  // dk_set_leds_state(DK_ALL_LEDS_MSK, DK_NO_LEDS_MSK);

  while (true)
  {
    /* Spin for ever */
    k_sleep(K_MSEC(1000));
  }
}

#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
static void num_comp_reply(bool accept)
{
  if (accept)
  {
    bt_conn_auth_passkey_confirm(auth_conn);
    LOG_INF("Numeric Match, conn %p", (void *)auth_conn);
  }
  else
  {
    bt_conn_auth_cancel(auth_conn);
    LOG_INF("Numeric Reject, conn %p", (void *)auth_conn);
  }

  bt_conn_unref(auth_conn);
  auth_conn = NULL;
}

void button_changed(uint32_t button_state, uint32_t has_changed)
{
  uint32_t buttons = button_state & has_changed;

  if (auth_conn)
  {
    if (buttons & KEY_PASSKEY_ACCEPT)
    {
      num_comp_reply(true);
    }

    if (buttons & KEY_PASSKEY_REJECT)
    {
      num_comp_reply(false);
    }
  }
}
#endif /* CONFIG_BT_NUS_SECURITY_ENABLED */


bool bleUartInit(void)
{
  int err = 0;


  if (IS_ENABLED(CONFIG_BT_NUS_SECURITY_ENABLED))
  {
    err = bt_conn_auth_cb_register(&conn_auth_callbacks);
    if (err)
    {
      LOG_ERR("Failed to register authorization callbacks. (err: %d)", err);
      return false;
    }

    err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
    if (err)
    {
      LOG_ERR("Failed to register authorization info callbacks. (err: %d)", err);
      return false;
    }
  }

  err = bt_enable(NULL);
  if (err)
  {
    return false;
  }

  LOG_INF("Bluetooth initialized");

  k_sem_give(&ble_init_ok);

  if (IS_ENABLED(CONFIG_SETTINGS))
  {
    settings_load();
  }

  err = bt_nus_init(&nus_cb);
  if (err)
  {
    LOG_ERR("Failed to initialize UART service (err: %d)", err);
    return false;
  }

  k_work_init(&adv_work, adv_work_handler);
  advertising_start();

  return true;
}

void ble_write_thread(void)
{
  /* Don't go any further until BLE is initialized */
  k_sem_take(&ble_init_ok, K_FOREVER);


  for (;;)
  {
    // /* Wait indefinitely for data to be sent over bluetooth */
    // struct uart_data_t *buf = k_fifo_get(&fifo_uart_rx_data,
    //                                      K_FOREVER);

    // int plen = MIN(sizeof(nus_data.data) - nus_data.len, buf->len);
    // int loc  = 0;

    // while (plen > 0)
    // {
    //   memcpy(&nus_data.data[nus_data.len], &buf->data[loc], plen);
    //   nus_data.len += plen;
    //   loc          += plen;

    //   if (nus_data.len >= sizeof(nus_data.data) ||
    //       (nus_data.data[nus_data.len - 1] == '\n') ||
    //       (nus_data.data[nus_data.len - 1] == '\r'))
    //   {
    //     if (bt_nus_send(NULL, nus_data.data, nus_data.len))
    //     {
    //       LOG_WRN("Failed to send data over BLE connection");
    //     }
    //     nus_data.len = 0;
    //   }

    //   plen = MIN(sizeof(nus_data.data), buf->len - loc);
    // }

    // k_free(buf);

    delay(10);
  }
}

K_THREAD_DEFINE(ble_write_thread_id, STACKSIZE, ble_write_thread, NULL, NULL,
                NULL, PRIORITY, 0, 0);
