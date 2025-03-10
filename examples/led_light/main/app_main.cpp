/* LED Light Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <cstdio>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_rmaker_console.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_scenes.h>

#include <esp_matter_core.h>
#include <esp_matter.h>
#include <esp_matter_providers.h>
#include <platform/ESP32/ThreadStackManagerImpl.h>
#include <dynamic_commissionable_data_provider.h>
#include <matter_commissioning_window_management.h>

#include <app_network.h>
#include <app_insights.h>

#include "app_priv.h"
#include "esp_matter_attribute_utils.h"
#include "platform/CommissionableDataProvider.h"
#include "platform/DeviceInstanceInfoProvider.h"
#include "support/CodeUtils.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "app_main";

esp_rmaker_device_t *light_device;
static dynamic_commissionable_data_provider g_dynamic_passcode_provider;

#ifdef CONFIG_ESP_RMAKER_CMD_RESP_ENABLE

#include <json_parser.h>
#include <esp_rmaker_cmd_resp.h>
#include <esp_rmaker_standard_types.h>

static char resp_data[100];
static uint16_t light_endpoint;
static bool rmaker_started = false;

/* Callback to handle commands received from the RainMaker cloud via the Command - Response Framework
 *
 * Sample payloads:
 *     - {"on":true}
 *     - {"brightness":30}
 */
esp_err_t led_light_cmd_handler(const void *in_data, size_t in_len, void **out_data, size_t *out_len, esp_rmaker_cmd_ctx_t *ctx, void *priv)
{
    if (in_data == NULL ){
        ESP_LOGE(TAG, "No data received");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Got command: %.*s", in_len, (char *)in_data);
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, (char *)in_data, in_len) != 0) {
        snprintf(resp_data, sizeof(resp_data), "{\"status\":\"fail\", \"description\":\"invalid json\"}");
    } else {
        int brightness;
        bool on_state;
        if (json_obj_get_int(&jctx, "brightness", &brightness) == 0) {
            if (brightness < 0 || brightness > 100) {
                snprintf(resp_data, sizeof(resp_data), "{\"status\":\"fail\", \"description\":\"out of bounds\"}");
            } else {
                app_light_set_brightness(brightness);
                esp_rmaker_param_update_and_report(
                        esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_BRIGHTNESS),
                        esp_rmaker_int(brightness));
                snprintf(resp_data, sizeof(resp_data), "{\"status\":\"success\"}");
            }
        } else if (json_obj_get_bool(&jctx, "on", &on_state) == 0) {
            app_light_set_power(on_state);
            esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_POWER),
                    esp_rmaker_bool(on_state));
            snprintf(resp_data, sizeof(resp_data), "{\"status\":\"success\"}");
        } else {
            snprintf(resp_data, sizeof(resp_data), "{\"status\":\"fail\", \"description\":\"invalid param\"}");
        }
    }
    *out_data = resp_data;
    *out_len = strlen(resp_data);
    return ESP_OK;
}

#endif /* CONFIG_ESP_RMAKER_CMD_RESP_ENABLE */

static const char *app_matter_get_rmaker_param_name_from_id(uint32_t cluster_id, uint32_t attribute_id)
{
    if (cluster_id == OnOff::Id) {
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
            return ESP_RMAKER_DEF_POWER_NAME;
        }
    } else if (cluster_id == LevelControl::Id) {
        if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
            return ESP_RMAKER_DEF_BRIGHTNESS_NAME;
        }
    } else if (cluster_id == ColorControl::Id) {
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
            return ESP_RMAKER_DEF_HUE_NAME;
        } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
            return ESP_RMAKER_DEF_SATURATION_NAME;
        } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
            return ESP_RMAKER_DEF_CCT_NAME;
        }
    }
    return NULL;
}

static esp_rmaker_param_val_t app_matter_get_rmaker_val(esp_matter_attr_val_t *val, uint32_t cluster_id,
                                                           uint32_t attribute_id)
{
    /* Attributes which need to be remapped */
    if (cluster_id == LevelControl::Id) {
        if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
            int value = (int)val->val.u8 * 100 / 254;
            return esp_rmaker_int(value);
        }
    } else if (cluster_id == ColorControl::Id) {
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
            int value = (int)val->val.u8 * 360 / 254;
            return esp_rmaker_int(value);
        } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
            int value = (int)val->val.u8 * 100 / 254;
            return esp_rmaker_int(value);
        }
    } else if (cluster_id == OnOff::Id) {
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
            return esp_rmaker_bool(val->val.b);
        }
    }
    return esp_rmaker_int(0);
}
esp_err_t app_matter_report_power(bool val)
{
    esp_matter_attr_val_t value = esp_matter_bool(val);
    return attribute::report(light_endpoint, OnOff::Id, OnOff::Attributes::OnOff::Id, &value);
}

esp_err_t app_matter_report_hue(int val)
{
    esp_matter_attr_val_t value = esp_matter_uint8(val * 254 / 360);
    return attribute::report(light_endpoint, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id, &value);
}

esp_err_t app_matter_report_saturation(int val)
{
    esp_matter_attr_val_t value = esp_matter_uint8(val * 254 / 100);
    return attribute::report(light_endpoint, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id, &value);
}

esp_err_t app_matter_report_brightness(int val)
{
    esp_matter_attr_val_t value = esp_matter_nullable_uint8(val * 254 / 100);
    return attribute::report(light_endpoint, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id, &value);
}

/* Callback to handle param updates received from the RainMaker cloud */
static esp_err_t bulk_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_write_req_t write_req[],
        uint8_t count, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    ESP_LOGI(TAG, "Light received %d params in write", count);
    for (int i = 0; i < count; i++) {
        const esp_rmaker_param_t *param = write_req[i].param;
        esp_rmaker_param_val_t val = write_req[i].val;
        const char *device_name = esp_rmaker_device_get_name(device);
        const char *param_name = esp_rmaker_param_get_name(param);
        if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
            ESP_LOGI(TAG, "Received value = %s for %s - %s",
                    val.val.b? "true" : "false", device_name, param_name);
            app_light_set_power(val.val.b);
            app_matter_report_power(val.val.b);
        } else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) {
            ESP_LOGI(TAG, "Received value = %d for %s - %s",
                    val.val.i, device_name, param_name);
            app_light_set_brightness(val.val.i);
            app_matter_report_brightness(val.val.i);
        } else if (strcmp(param_name, ESP_RMAKER_DEF_HUE_NAME) == 0) {
            ESP_LOGI(TAG, "Received value = %d for %s - %s",
                    val.val.i, device_name, param_name);
            app_light_set_hue(val.val.i);
            app_matter_report_hue(val.val.i);
        } else if (strcmp(param_name, ESP_RMAKER_DEF_SATURATION_NAME) == 0) {
            ESP_LOGI(TAG, "Received value = %d for %s - %s",
                    val.val.i, device_name, param_name);
            app_light_set_saturation(val.val.i);
            app_matter_report_saturation(val.val.i);
        } else {
            ESP_LOGI(TAG, "Updating for %s", param_name);
        }
        esp_rmaker_param_update(param, val);
    }
    return ESP_OK;
}


static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    if (type == PRE_UPDATE) {
        if (endpoint_id == light_endpoint) {
            if (cluster_id == chip::app::Clusters::OnOff::Id && attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
                if (val->type == ESP_MATTER_VAL_TYPE_BOOLEAN) {
                    app_light_set_power(val->val.b);
                }
            } else if (cluster_id == chip::app::Clusters::LevelControl::Id &&
                attribute_id == chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id) {
                if (val->type == ESP_MATTER_VAL_TYPE_NULLABLE_UINT8 && val->val.u8 <= 254) {
                    app_light_set_brightness((uint16_t)val->val.u8 * 100 / 254);
                }
            } else if (cluster_id == chip::app::Clusters::ColorControl::Id) {
                if (attribute_id == chip::app::Clusters::ColorControl::Attributes::CurrentHue::Id) {
                    if (val->type == ESP_MATTER_VAL_TYPE_UINT8 && val->val.u8 <= 254) {
                        app_light_set_hue((uint16_t)val->val.u8 * 360 / 254);
                    }
                } else if (attribute_id == chip::app::Clusters::ColorControl::Attributes::CurrentSaturation::Id) {
                    if (val->type == ESP_MATTER_VAL_TYPE_UINT8 && val->val.u8 <= 254) {
                        app_light_set_saturation((uint16_t)val->val.u8 * 100 / 254);
                    }
                }
            }
        }
    } else if (type == POST_UPDATE) {
        if (!rmaker_started) {
            return ESP_OK;
        }
        const char *param_name = app_matter_get_rmaker_param_name_from_id(cluster_id, attribute_id);
        if (!param_name) {
            return ESP_OK;
        }
        esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(light_device, param_name);
        if (!param) {
            return ESP_FAIL;
        }
        esp_rmaker_param_val_t rmaker_val = app_matter_get_rmaker_val(val, cluster_id, attribute_id);
        return esp_rmaker_param_update_and_report(param, rmaker_val);
    }
    return ESP_OK;
}

static void matter_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    if (event->Type == chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened) {
        matter_commissioning_window_parameters_update();
        matter_commissioning_window_status_update(true);
    } else if (event->Type == chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed) {
        matter_commissioning_window_status_update(false);
    }
}

extern "C" void app_main()
{
    /* Initialize Application specific hardware drivers and
     * set initial state.
     */
    esp_rmaker_console_init();
    app_driver_init();

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi/Thread. Note that, this should be called before esp_rmaker_node_init()
     */
    app_network_init();

    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_network_init() but before app_network_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Lightbulb");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create a device and add the relevant parameters to it */
    light_device = esp_rmaker_lightbulb_device_create("Light", NULL, DEFAULT_POWER);
    esp_rmaker_device_add_bulk_cb(light_device, bulk_write_cb, NULL);

    esp_rmaker_device_add_param(light_device, esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME, DEFAULT_BRIGHTNESS));
    esp_rmaker_device_add_param(light_device, esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME, DEFAULT_HUE));
    esp_rmaker_device_add_param(light_device, esp_rmaker_saturation_param_create(ESP_RMAKER_DEF_SATURATION_NAME, DEFAULT_SATURATION));

    esp_rmaker_node_add_device(node, light_device);

    /* Create Matter data model */
    node::config_t node_config;

    // node handle can be used to add/modify other endpoints.
    node_t *matter_node = node::create(&node_config, app_attribute_update_cb, nullptr);
    if (!matter_node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return;
    }

    extended_color_light::config_t light_config;
    light_config.on_off.on_off = DEFAULT_POWER;
    light_config.on_off.lighting.start_up_on_off = nullptr;
    light_config.level_control.current_level = DEFAULT_BRIGHTNESS;
    light_config.level_control.on_level = DEFAULT_BRIGHTNESS;
    light_config.level_control.lighting.start_up_current_level = DEFAULT_BRIGHTNESS;
    light_config.color_control.color_mode = (uint8_t)ColorControl::ColorMode::kColorTemperature;
    light_config.color_control.enhanced_color_mode = (uint8_t)ColorControl::ColorMode::kColorTemperature;
    light_config.color_control.color_temperature.startup_color_temperature_mireds = nullptr;

    // endpoint handles can be used to add/modify clusters.
    endpoint_t *endpoint = extended_color_light::create(matter_node, &light_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create extended color light endpoint");
        return;
    }
    light_endpoint = endpoint::get_id(endpoint);

    /* Enable OTA */
    esp_rmaker_ota_enable_default();

    /* Enable timezone service which will be require for setting appropriate timezone
     * from the phone apps for scheduling to work correctly.
     * For more information on the various ways of setting timezone, please check
     * https://rainmaker.espressif.com/docs/time-service.html.
     */
    esp_rmaker_timezone_service_enable();

    /* Enable scheduling. */
    esp_rmaker_schedule_enable();

    /* Enable Scenes */
    esp_rmaker_scenes_enable();

    /* Enable Insights. Requires CONFIG_ESP_INSIGHTS_ENABLED=y */
    app_insights_enable();

    matter_commissioning_window_management_enable();

#ifdef CONFIG_ESP_RMAKER_CMD_RESP_ENABLE
    /* Register a command for demonstration */
    esp_rmaker_cmd_register(ESP_RMAKER_CMD_CUSTOM_START, ESP_RMAKER_USER_ROLE_PRIMARY_USER | ESP_RMAKER_USER_ROLE_SECONDARY_USER, led_light_cmd_handler, false, NULL);
#endif

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();
    rmaker_started = true;
    esp_matter::set_custom_commissionable_data_provider(&g_dynamic_passcode_provider);
    err = esp_matter::start(matter_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter, err:%d", err);
    }
    // This will not really initiaize Thread stack as the thread stack has been initialzed in app_network.
    // We call this function to pass the OpenThread instance to GenericThreadStackManagerImpl_OpenThread
    // so that it can be used for SRP service registration.
    chip::DeviceLayer::ThreadStackMgr().InitThreadStack();

    err = app_network_set_custom_mfg_data(MGF_DATA_DEVICE_TYPE_LIGHT, MFG_DATA_DEVICE_SUBTYPE_LIGHT);
    /* Start the Wi-Fi/Thread.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start network. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}
