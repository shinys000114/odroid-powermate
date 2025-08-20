#include "ina226.h"

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <esp_err.h>

#define INA226_REG_CONFIG           (0x00)
#define INA226_REG_SHUNT_VOLTAGE    (0x01)
#define INA226_REG_BUS_VOLTAGE      (0x02)
#define INA226_REG_POWER            (0x03)
#define INA226_REG_CURRENT          (0x04)
#define INA226_REG_CALIBRATION      (0x05)
#define INA226_REG_ALERT_MASK       (0x06)
#define INA226_REG_ALERT_LIMIT      (0x07)
#define INA226_REG_MANUFACTURER_ID  (0xFE)
#define INA226_REG_DIE_ID           (0xFF)

#define INA226_CFG_AVERAGING_OFFSET 9
#define INA226_CFG_BUS_VOLTAGE_OFFSET 6
#define INA226_CFG_SHUNT_VOLTAGE_OFFSET 3

static esp_err_t ina226_read_reg(ina226_t *handle, uint8_t reg_addr, uint16_t *data, size_t len)
{
    return i2c_master_transmit_receive(handle->dev_handle, &reg_addr, 1, (uint8_t *)data, len, handle->timeout_ms);
}

static esp_err_t ina226_write_reg(ina226_t *handle, uint8_t reg_addr, uint16_t value)
{
    uint8_t write_buf[3] = {reg_addr, value >> 8, value & 0xFF};
    return i2c_master_transmit(handle->dev_handle, write_buf, sizeof(write_buf), handle->timeout_ms);
}

esp_err_t ina226_get_manufacturer_id(ina226_t *device, uint16_t *manufacturer_id)
{
    return ina226_read_reg(device, INA226_REG_MANUFACTURER_ID, manufacturer_id, 2);
}

esp_err_t ina226_get_die_id(ina226_t *device, uint16_t *die_id)
{
    return ina226_read_reg(device, INA226_REG_DIE_ID, die_id, 2);
}

esp_err_t ina226_get_shunt_voltage(ina226_t *device, float *voltage)
{
    uint8_t data[2];
    esp_err_t err = ina226_read_reg(device, INA226_REG_SHUNT_VOLTAGE, (uint16_t*)data, 2);
    *voltage = (float) (data[0] << 8 | data[1]) * 2.5e-6f; /* fixed to 2.5 uV */
    return err;
}

esp_err_t ina226_get_bus_voltage(ina226_t *device, float *voltage)
{
    uint8_t data[2];
    esp_err_t err = ina226_read_reg(device, INA226_REG_BUS_VOLTAGE, (uint16_t*)data, 2);
    *voltage = (float) (data[0] << 8 | data[1]) * 0.00125f;
    return err;
}

esp_err_t ina226_get_current(ina226_t *device, float *current)
{
    uint8_t data[2];
    esp_err_t err = ina226_read_reg(device, INA226_REG_CURRENT, (uint16_t*)data, 2);
    *current = ((float) (data[0] << 8 | data[1])) * device->current_lsb;
    return err;
}

esp_err_t ina226_get_power(ina226_t *device, float *power)
{
    uint8_t data[2];
    esp_err_t err = ina226_read_reg(device, INA226_REG_POWER, (uint16_t*)data, 2);
    *power = (float) (data[0] << 8 | data[1]) * device->power_lsb;
    return err;
}

esp_err_t ina226_init(ina226_t *device, i2c_master_dev_handle_t dev_handle, const ina226_config_t *config)
{
    esp_err_t err;
    device->timeout_ms = config->timeout_ms;
    device->dev_handle = dev_handle;
    uint16_t bitmask = 0;
    bitmask |= (config->averages << INA226_CFG_AVERAGING_OFFSET);
    bitmask |= (config->bus_conv_time << INA226_CFG_BUS_VOLTAGE_OFFSET);
    bitmask |= (config->shunt_conv_time << INA226_CFG_SHUNT_VOLTAGE_OFFSET);
    bitmask |= config->mode;
    err = ina226_write_reg(device, INA226_REG_CONFIG, bitmask);
    if(err != ESP_OK) return err;

    /* write calibration*/
    float minimum_lsb = config->max_current / 32767;
    float current_lsb = (uint16_t)(minimum_lsb * 100000000);
    current_lsb /= 100000000;
    current_lsb /= 0.0001;
    current_lsb = ceil(current_lsb);
    current_lsb *= 0.0001;
    device->current_lsb = current_lsb;
    device->power_lsb = current_lsb * 25;
    uint16_t calibration_value = (uint16_t)((0.00512) / (current_lsb * config->r_shunt));
    err = ina226_write_reg(device, INA226_REG_CALIBRATION, calibration_value);
    if(err != ESP_OK) return err;

    return ESP_OK;
}

esp_err_t ina226_get_alert_mask(ina226_t *device, ina226_alert_t *alert_mask)
{
    return ina226_read_reg(device, INA226_REG_ALERT_MASK, (uint16_t *)alert_mask, 2);
}

esp_err_t ina226_set_alert_mask(ina226_t *device, ina226_alert_t alert_mask)
{
    return ina226_write_reg(device, INA226_REG_ALERT_MASK, (uint16_t)alert_mask);
}

esp_err_t ina226_set_alert_limit(ina226_t *device, float voltage)
{
    return ina226_write_reg(device, INA226_REG_ALERT_LIMIT, (uint16_t)(voltage));
}