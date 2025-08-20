#ifndef _INA226_H_
#define _INA226_H_

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

typedef enum
{
    INA226_AVERAGES_1             = 0b000,
    INA226_AVERAGES_4             = 0b001,
    INA226_AVERAGES_16            = 0b010,
    INA226_AVERAGES_64            = 0b011,
    INA226_AVERAGES_128           = 0b100,
    INA226_AVERAGES_256           = 0b101,
    INA226_AVERAGES_512           = 0b110,
    INA226_AVERAGES_1024          = 0b111
} ina226_averages_t;

typedef enum
{
    INA226_BUS_CONV_TIME_140_US    = 0b000,
    INA226_BUS_CONV_TIME_204_US    = 0b001,
    INA226_BUS_CONV_TIME_332_US    = 0b010,
    INA226_BUS_CONV_TIME_588_US    = 0b011,
    INA226_BUS_CONV_TIME_1100_US   = 0b100,
    INA226_BUS_CONV_TIME_2116_US   = 0b101,
    INA226_BUS_CONV_TIME_4156_US   = 0b110,
    INA226_BUS_CONV_TIME_8244_US   = 0b111
} ina226_bus_conv_time_t;


typedef enum
{
    INA226_SHUNT_CONV_TIME_140_US   = 0b000,
    INA226_SHUNT_CONV_TIME_204_US   = 0b001,
    INA226_SHUNT_CONV_TIME_332_US   = 0b010,
    INA226_SHUNT_CONV_TIME_588_US   = 0b011,
    INA226_SHUNT_CONV_TIME_1100_US  = 0b100,
    INA226_SHUNT_CONV_TIME_2116_US  = 0b101,
    INA226_SHUNT_CONV_TIME_4156_US  = 0b110,
    INA226_SHUNT_CONV_TIME_8244_US  = 0b111
} ina226_shunt_conv_time_t;

typedef enum
{

    INA226_MODE_POWER_DOWN      = 0b000,
    INA226_MODE_SHUNT_TRIG      = 0b001,
    INA226_MODE_BUS_TRIG        = 0b010,
    INA226_MODE_SHUNT_BUS_TRIG  = 0b011,
    INA226_MODE_ADC_OFF         = 0b100,
    INA226_MODE_SHUNT_CONT      = 0b101,
    INA226_MODE_BUS_CONT        = 0b110,
    INA226_MODE_SHUNT_BUS_CONT  = 0b111,
} ina226_mode_t;

typedef enum
{
    INA226_ALERT_SHUNT_OVER_VOLTAGE     = 0xf,
    INA226_ALERT_SHUNT_UNDER_VOLTAGE    = 0xe,
    INA226_ALERT_BUS_OVER_VOLTAGE       = 0xd,
    INA226_ALERT_BUS_UNDER_VOLTAGE      = 0xc,
    INA226_ALERT_POWER_OVER_LIMIT       = 0xb,
    INA226_ALERT_CONVERSION_READY       = 0xa,
    INA226_ALERT_FUNCTION_FLAG          = 0x4,
    INA226_ALERT_CONVERSION_READY_FLAG  = 0x3,
    INA226_ALERT_MATH_OVERFLOW_FLAG     = 0x2,
    INA226_ALERT_POLARITY               = 0x1,
    INA226_ALERT_LATCH_ENABLE           = 0x0
} ina226_alert_t;

typedef struct
{
    i2c_port_t i2c_port;
    int i2c_addr;
    int timeout_ms;
    ina226_averages_t averages;
    ina226_bus_conv_time_t bus_conv_time;
    ina226_shunt_conv_time_t shunt_conv_time;
    ina226_mode_t mode;
    float r_shunt; /* ohm */
    float max_current; /* amps */
} ina226_config_t;

typedef struct
{
    i2c_master_dev_handle_t dev_handle;
    int timeout_ms;
    float current_lsb;
    float power_lsb;
} ina226_t;

esp_err_t ina226_get_manufacturer_id(ina226_t *device, uint16_t *manufacturer_id);
esp_err_t ina226_get_die_id(ina226_t *device, uint16_t *die_id);
esp_err_t ina226_get_shunt_voltage(ina226_t *device, float *voltage);
esp_err_t ina226_get_bus_voltage(ina226_t *device, float *voltage);
esp_err_t ina226_get_current(ina226_t *device, float *current);
esp_err_t ina226_get_power(ina226_t *device, float *power);
esp_err_t ina226_get_alert_mask(ina226_t *device, ina226_alert_t *alert_mask);
esp_err_t ina226_set_alert_mask(ina226_t *device, ina226_alert_t alert_mask);
esp_err_t ina226_set_alert_limit(ina226_t *device, float voltage);
esp_err_t ina226_init(ina226_t *device, i2c_master_dev_handle_t dev_handle, const ina226_config_t *config);

#endif