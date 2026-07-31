/**
 * @file screen_i2c_protocol.h
 * @brief Raw-parameter frame shared by the two STM32F407 boards.
 */
#ifndef SCREEN_I2C_PROTOCOL_H
#define SCREEN_I2C_PROTOCOL_H

#include <stdint.h>

#define SCREEN_I2C_SLAVE_ADDRESS_7BIT 0x32u
#define SCREEN_I2C_SLAVE_ADDRESS_HAL (SCREEN_I2C_SLAVE_ADDRESS_7BIT << 1u)
#define SCREEN_I2C_FRAME_MAGIC 0xC25Au
#define SCREEN_I2C_FRAME_VERSION 1u

typedef struct __attribute__((packed))
{
  uint16_t magic;
  uint8_t version;
  uint8_t byte_len;
  uint32_t sequence;
  uint32_t frequency_mhz;
  uint32_t vpp_uv;
  uint32_t vrms_uv;
  uint32_t sample_rate_hz;
  uint16_t range_mv;
  uint16_t status_flags;
  uint32_t checksum;
} ScreenI2cRawParamsFrame;

#endif /* SCREEN_I2C_PROTOCOL_H */
