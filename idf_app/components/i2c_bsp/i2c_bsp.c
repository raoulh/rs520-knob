#include "i2c_bsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "settings.h"

static i2c_master_bus_handle_t s_bus_handle = NULL;
i2c_master_dev_handle_t disp_touch_dev_handle = NULL;
i2c_master_dev_handle_t drv2605_dev_handle = NULL;

static uint32_t s_data_timeout_ticks = 0;
static uint32_t s_done_timeout_ticks = 0;

void i2c_master_Init(void)
{
    s_data_timeout_ticks = pdMS_TO_TICKS(5000);
    s_done_timeout_ticks = pdMS_TO_TICKS(1000);

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = ESP32_SCL_NUM,
        .sda_io_num = ESP32_SDA_NUM,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 300000,
    };

    dev_cfg.device_address = DRV2605_ADDR;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus_handle, &dev_cfg, &drv2605_dev_handle));

    dev_cfg.device_address = TOUCH_ADDR;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus_handle, &dev_cfg, &disp_touch_dev_handle));
}

uint8_t i2c_write_buff(i2c_master_dev_handle_t dev_handle,
                        int reg, uint8_t *buf, uint8_t len)
{
    uint8_t ret;
    ret = i2c_master_bus_wait_all_done(s_bus_handle, s_done_timeout_ticks);
    if (ret != ESP_OK) return ret;

    if (reg == -1)
    {
        ret = i2c_master_transmit(dev_handle, buf, len, s_data_timeout_ticks);
    }
    else
    {
        uint8_t *pbuf = (uint8_t *)malloc(len + 1);
        pbuf[0] = (uint8_t)reg;
        memcpy(pbuf + 1, buf, len);
        ret = i2c_master_transmit(dev_handle, pbuf, len + 1, s_data_timeout_ticks);
        free(pbuf);
    }
    return ret;
}

uint8_t i2c_read_buff(i2c_master_dev_handle_t dev_handle,
                       int reg, uint8_t *buf, uint8_t len)
{
    uint8_t ret;
    ret = i2c_master_bus_wait_all_done(s_bus_handle, s_done_timeout_ticks);
    if (ret != ESP_OK) return ret;

    if (reg == -1)
    {
        ret = i2c_master_receive(dev_handle, buf, len, s_data_timeout_ticks);
    }
    else
    {
        uint8_t addr = (uint8_t)reg;
        ret = i2c_master_transmit_receive(dev_handle, &addr, 1,
                                           buf, len, s_data_timeout_ticks);
    }
    return ret;
}

uint8_t i2c_master_write_read_dev(i2c_master_dev_handle_t dev_handle,
                                   uint8_t *writeBuf, uint8_t writeLen,
                                   uint8_t *readBuf, uint8_t readLen)
{
    uint8_t ret;
    ret = i2c_master_bus_wait_all_done(s_bus_handle, s_done_timeout_ticks);
    if (ret != ESP_OK) return ret;

    ret = i2c_master_transmit_receive(dev_handle, writeBuf, writeLen,
                                       readBuf, readLen, s_data_timeout_ticks);
    return ret;
}
