/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the functions prototypes for the USART1
  *          driver (TJC serial HMI).
  ******************************************************************************
  */
#ifndef __USART_H
#define __USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart1;

void MX_USART1_UART_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __USART_H */
