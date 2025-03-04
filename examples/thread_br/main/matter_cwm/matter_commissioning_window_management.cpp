#include "matter_commissioning_window_management.h"
#include "matter_commissioning_window_management_std.h"
#include "platform/PlatformManager.h"

#include <cstring>
#include <esp_check.h>
#include <esp_rmaker_core.h>

#include <app/server/Server.h>

constexpr char *TAG = "MatterCWM";
static esp_rmaker_device_t *s_matter_cwm_service;

esp_err_t matter_commissioning_window_parameters_update(char *setup_pin, uint16_t discriminator, uint16_t vendor_id, uint16_t product_id)
{
    esp_rmaker_param_val_t val;
    val.type = RMAKER_VAL_TYPE_STRING;
    val.val.s = setup_pin;
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_type(s_matter_cwm_service, ESP_RMAKER_PARAM_MATTER_SETUP_PIN);
    ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update setup PIN");
    val.type = RMAKER_VAL_TYPE_INTEGER;
    val.val.i = discriminator;
    param = esp_rmaker_device_get_param_by_type(s_matter_cwm_service, ESP_RMAKER_PARAM_MATTER_DISCRIMINATOR);
    ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update discriminator");
    val.val.i = vendor_id;
    param = esp_rmaker_device_get_param_by_type(s_matter_cwm_service, ESP_RMAKER_PARAM_MATTER_VENDOR_ID);
    ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update vendor id");
    val.val.i = product_id;
    param = esp_rmaker_device_get_param_by_type(s_matter_cwm_service, ESP_RMAKER_PARAM_MATTER_PRODUCT_ID);
    ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update product id");
    return ESP_OK;
}

esp_err_t matter_commissioning_window_status_update(bool open)
{
    esp_rmaker_param_val_t val;
    val.type = RMAKER_VAL_TYPE_BOOLEAN;
    val.val.b = open;
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_type(s_matter_cwm_service,
                                                                    ESP_RMAKER_PARAM_MATTER_COMMISSIONING_WINDOW_OPEN);
    ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update WindowOpen status");
    return ESP_OK;
}

static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (!s_matter_cwm_service) {
        return ESP_ERR_INVALID_STATE;
    }
    if (strcmp(esp_rmaker_param_get_type(param), ESP_RMAKER_PARAM_MATTER_COMMISSIONING_WINDOW_OPEN) == 0 &&
        ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
        if (val.type != RMAKER_VAL_TYPE_BOOLEAN) {
            return ESP_ERR_INVALID_ARG;
        }
        if (val.val.b) {
            chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t arg){
                chip::Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow(); });
        } else {
            chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t arg){
                chip::Server::GetInstance().GetCommissioningWindowManager().CloseCommissioningWindow(); });
        }
    }
    return ESP_OK;
}

esp_err_t matter_commissioning_window_management_enable()
{
    s_matter_cwm_service = matter_commissioning_window_management_service_create("MatterCWM", write_cb, nullptr, nullptr);
    if (!s_matter_cwm_service) {
        ESP_LOGE(TAG, "Failed to create Matter Commissioning Window Management service");
        return ESP_FAIL;
    }
    return esp_rmaker_node_add_device(esp_rmaker_get_node(), s_matter_cwm_service);
}
