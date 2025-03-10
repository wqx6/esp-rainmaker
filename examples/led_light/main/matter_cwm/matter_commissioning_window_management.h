#pragma once

#include <esp_rmaker_core.h>
#include <esp_err.h>

esp_err_t matter_commissioning_window_parameters_update();
esp_err_t matter_commissioning_window_status_update(bool open);

esp_err_t matter_commissioning_window_management_enable();
