#pragma once

#include <esp_rmaker_core.h>
#include <esp_err.h>

esp_err_t matter_commissioning_window_parameters_update(char *setup_pin, uint16_t discriminator, uint16_t vendor_id,
                                                        uint16_t product_id);
esp_err_t matter_commissioning_window_status_update(bool open);

esp_err_t matter_commissioning_window_management_enable();
