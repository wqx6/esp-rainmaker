#pragma once

#include <esp_err.h>
#include <esp_rmaker_core.h>

// Matter Commissioning Window Management service
#define ESP_RMAKER_SERVICE_MATTER_COMMISSIONING_WINDOW_MANAGEMENT "esp.service.matter-cwm"

// Matter Commissioning Window Management parameters
#define ESP_RMAKER_DEF_MATTER_SETUP_PIN_NAME            "SetupPIN"
#define ESP_RMAKER_PARAM_MATTER_SETUP_PIN               "esp.param.setup-pin"
#define ESP_RMAKER_DEF_MATTER_DISCRIMINATOR_NAME        "Discriminator"
#define ESP_RMAKER_PARAM_MATTER_DISCRIMINATOR           "esp.param.discriminator"
#define ESP_RMAKER_DEF_MATTER_VENDOR_ID_NAME            "VendorID"
#define ESP_RMAKER_PARAM_MATTER_VENDOR_ID               "esp.param.vendor-id"
#define ESP_RMAKER_DEF_MATTER_PRODUCT_ID_NAME           "ProductID"
#define ESP_RMAKER_PARAM_MATTER_PRODUCT_ID              "esp.param.product-id"
#define ESP_RMAKER_DEF_MATTER_COMMISSIONING_WINDOW_OPEN_NAME     "WindowOpen"
#define ESP_RMAKER_PARAM_MATTER_COMMISSIONING_WINDOW_OPEN        "esp.param.window-open"

esp_rmaker_param_t *matter_commissioning_window_management_service_create(
    const char *serv_name, esp_rmaker_device_write_cb_t write_cb, esp_rmaker_device_read_cb_t read_cb, void *priv_data);
