/**
  ******************************************************************************
  * @file    hmi_tjc.c
  * @brief   TJC USART HMI command helper (simplified for F407 acquisition board).
  ******************************************************************************
  */
#include "hmi_tjc.h"

#include "usart.h"
#include <stdio.h>
#include <string.h>

#define HMI_TX_TIMEOUT_MS 20u

static const uint8_t kHmiEndMarker[3] = {0xFFu, 0xFFu, 0xFFu};

static void HMI_SendBytes(const uint8_t *data, uint16_t len)
{
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, len, HMI_TX_TIMEOUT_MS);
}

void HMI_SendCmd(const char *cmd)
{
  if (cmd == NULL)
  {
    return;
  }

  HMI_SendBytes((const uint8_t *)cmd, (uint16_t)strlen(cmd));
  HMI_SendBytes(kHmiEndMarker, 3u);
}

void HMI_SetText(const char *obj, const char *text)
{
  char buf[128];

  if ((obj == NULL) || (text == NULL))
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "%s.txt=\"%s\"", obj, text);
  HMI_SendCmd(buf);
}

void HMI_SetValue(const char *obj, int32_t value)
{
  char buf[64];

  if (obj == NULL)
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "%s.val=%ld", obj, (long)value);
  HMI_SendCmd(buf);
}

void HMI_Addt_Send(const char *curve_id, uint8_t ch, const uint8_t *data, uint16_t len)
{
  char cmd_buf[32];

  if ((curve_id == NULL) || (data == NULL) || (len == 0u))
  {
    return;
  }

  (void)snprintf(cmd_buf, sizeof(cmd_buf), "addt %s.id,%u,%u", curve_id, (unsigned int)ch, (unsigned int)len);
  HMI_SendCmd(cmd_buf);

  HAL_Delay(50);
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, len, HMI_TX_TIMEOUT_MS);
  HAL_Delay(20);
}

void HMI_ClearWave(const char *obj, uint8_t channel_mask)
{
  char buf[64];

  if (obj == NULL)
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "cle %s.id,%u", obj, (unsigned int)channel_mask);
  HMI_SendCmd(buf);
}

void HMI_GotoPage(const char *page_name)
{
  char buf[64];

  if (page_name == NULL)
  {
    return;
  }

  (void)snprintf(buf, sizeof(buf), "page %s", page_name);
  HMI_SendCmd(buf);
}
