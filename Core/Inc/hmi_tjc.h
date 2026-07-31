/**
  ******************************************************************************
  * @file    hmi_tjc.h
  * @brief   TJC USART HMI command helper (simplified for F407 acquisition board).
  ******************************************************************************
  */
#ifndef HMI_TJC_H
#define HMI_TJC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void HMI_SendCmd(const char *cmd);
void HMI_SetText(const char *obj, const char *text);
void HMI_SetValue(const char *obj, int32_t value);
void HMI_Addt_Send(const char *curve_id, uint8_t ch, const uint8_t *data, uint16_t len);
void HMI_ClearWave(const char *obj, uint8_t channel_mask);
void HMI_GotoPage(const char *page_name);

#ifdef __cplusplus
}
#endif

#endif /* HMI_TJC_H */
