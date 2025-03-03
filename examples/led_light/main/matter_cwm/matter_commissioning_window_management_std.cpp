#include "matter_commissioning_window_management_std.h"
#include "esp_rmaker_core.h"

static esp_rmaker_param_t *matter_setup_pin_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_SETUP_PIN,
                                                        esp_rmaker_str(""), PROP_FLAG_READ);
    return param;
}

static esp_rmaker_param_t *matter_discriminator_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_DISCRIMINATOR,
                                                        esp_rmaker_int(0), PROP_FLAG_READ);
    return param;
}

static esp_rmaker_param_t *matter_vendor_id_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_VENDOR_ID,
                                                        esp_rmaker_int(0), PROP_FLAG_READ);
    return param;
}

static esp_rmaker_param_t *matter_product_id_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_PRODUCT_ID,
                                                        esp_rmaker_int(0), PROP_FLAG_READ);
    return param;

}
static esp_rmaker_param_t *matter_commissioning_window_open_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_COMMISSIONING_WINDOW_OPEN,
                                                        esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    return param;
}

esp_rmaker_param_t *matter_commissioning_window_management_service_create(
    const char *serv_name, esp_rmaker_device_write_cb_t write_cb, esp_rmaker_device_read_cb_t read_cb, void *priv_data)
{
    esp_rmaker_device_t *service = esp_rmaker_service_create(serv_name,
                                                             ESP_RMAKER_SERVICE_MATTER_COMMISSIONING_WINDOW_MANAGEMENT,
                                                             priv_data);
    if (service) {
        esp_rmaker_device_add_cb(service, write_cb, read_cb);
        esp_rmaker_device_add_param(service, matter_setup_pin_param_create(ESP_RMAKER_DEF_MATTER_SETUP_PIN_NAME));
        esp_rmaker_device_add_param(service, matter_discriminator_param_create(ESP_RMAKER_DEF_MATTER_DISCRIMINATOR_NAME));
        esp_rmaker_device_add_param(service, matter_vendor_id_param_create(ESP_RMAKER_DEF_MATTER_VENDOR_ID_NAME));
        esp_rmaker_device_add_param(service, matter_product_id_param_create(ESP_RMAKER_DEF_MATTER_PRODUCT_ID_NAME));
        esp_rmaker_device_add_param(
            service, matter_commissioning_window_open_param_create(ESP_RMAKER_DEF_MATTER_COMMISSIONING_WINDOW_OPEN_NAME));
    }
    return service;
}
