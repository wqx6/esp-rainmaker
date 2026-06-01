/* Client-only Matter Controller Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "app_rmaker_matter_device_list.h"
#include "esp_err.h"
#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <esp_rmaker_auth_service.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_utils.h>

#include <esp_rmaker_common_events.h>

#include <app_wifi.h>
#include <app_insights.h>
#include <app_rmaker_matter_controller.h>
#include <app_matter_controller_creds_issuer.h>
#include <app_rmaker_user_api.h>
#include <matter_controller_cmd_resp.h>
#include <matter_command_list_sync.h>
#include <matter_attr_report.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_controller_console.h>
#include <esp_matter_controller_client.h>
#include <esp_matter_controller_credentials_issuer.h>

#include <controller/CHIPDeviceControllerFactory.h>

static const char *TAG = "app_main";
esp_rmaker_device_t *matter_controller_device;
static example_op_creds_issuer s_matter_controller_creds_issuer;

static esp_err_t app_matter_controller_setup_controller(uint8_t *ipk, size_t ipk_len, uint64_t fabric_id)
{
    {
        esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
        esp_matter::controller::matter_controller_client::get_instance().init(0, 0, 5580);
    }
    chip::MutableByteSpan ipk_span(ipk, ipk_len);
    auto &factory = chip::Controller::DeviceControllerFactory::GetInstance();
    chip::FabricIndex controller_fabric_index = chip::kUndefinedFabricIndex;
    if (factory.GetSystemState()) {
        chip::FabricTable *fabric_table = factory.GetSystemState()->Fabrics();
        if (fabric_table) {
            for (const auto & fabric: *fabric_table) {
                if (fabric.GetFabricId() == fabric_id) {
                    controller_fabric_index = fabric.GetFabricIndex();
                }
            }
        }
    }

    if (controller_fabric_index != chip::kUndefinedFabricIndex) {
        ESP_LOGI(TAG, "NOC chain has been installed, setup controller with an empty IPK");
        ipk_span.reduce_size(0);
    }
    esp_err_t err = ESP_OK;
    {
        esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
        err = esp_matter::controller::matter_controller_client::get_instance().setup_controller(ipk_span, controller_fabric_index);
    }
    app_rmaker_update_matter_device_list();
    return err;
}

static esp_err_t app_matter_controller_update_noc_chain(uint64_t fabric_id)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static void app_matter_controller_update_device_list(esp_err_t err)
{
    ESP_LOGI(TAG, "Matter controller device list updated successfully");
    matter_device_t *dev_list = app_rmaker_get_matter_device_list();
    matter_command_list_sync_for_device_list(dev_list);
    app_rmaker_free_matter_device_list(dev_list);
    matter_attr_report_on_device_list_updated();
}

static void matter_ctl_net_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (app_rmaker_matter_controller_handle_update() == ESP_OK) {
            ESP_LOGI(TAG, "Matter controller updated successfully");
            esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, matter_ctl_net_event_handler);
        } else {
            ESP_LOGW(TAG, "Matter controller update failed");
        }
    }
}

static esp_rmaker_param_t *matter_controller_matter_devices_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, "esp.param.matter-devices",
            esp_rmaker_obj("{}"), PROP_FLAG_READ);
    return param;
}

static esp_rmaker_param_t *matter_controller_data_version_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, "esp.param.matter-controller-data-version",
            esp_rmaker_str("1.0.1"), PROP_FLAG_READ);
    return param;
}


namespace esp_matter {
namespace console {

static engine dev_mgr_console;

static esp_err_t print_dev_list_handler(int argc, char *argv[])
{
    // print_device_list();
    matter_device_t *dev_list = app_rmaker_get_matter_device_list();
    matter_device_t *cur_dev = dev_list;
    uint16_t dev_index = 0;
    while (cur_dev) {
        ESP_LOGI(TAG, "device %d : {", dev_index);
        ESP_LOGI(TAG, "    rainmaker_node_id: %s,", cur_dev->rainmaker_node_id);
        ESP_LOGI(TAG, "    matter_node_id: 0x%" PRIx32 "%" PRIx32 ",", (uint32_t)(cur_dev->node_id >> 32),
                 (uint32_t)(cur_dev->node_id & 0xFFFFFFFF));
        if (cur_dev->is_metadata_fetched) {
            ESP_LOGI(TAG, "    is_rainmaker_device: %s,", cur_dev->is_rainmaker_device ? "true" : "false");
            ESP_LOGI(TAG, "    is_online: %s,", matter_attr_report_get_online_state(cur_dev->node_id) ? "true" : "false");
            ESP_LOGI(TAG, "    endpoints : [");
            for (size_t i = 0; i < cur_dev->endpoint_count; ++i) {
                ESP_LOGI(TAG, "        {");
                ESP_LOGI(TAG, "           endpoint_id: %d,", cur_dev->endpoints[i].endpoint_id);
                ESP_LOGI(TAG, "           device_type_id: 0x%" PRIx32 ",", cur_dev->endpoints[i].device_type_id);
                ESP_LOGI(TAG, "           device_name: %s,", cur_dev->endpoints[i].device_name);
                ESP_LOGI(TAG, "        },");
            }
            ESP_LOGI(TAG, "    ]");
        }
        ESP_LOGI(TAG, "}");
        cur_dev = cur_dev->next;
        dev_index++;
    }
    app_rmaker_free_matter_device_list(dev_list);
    return ESP_OK;
}

static esp_err_t reset_handler(int argc, char *argv[])
{
    esp_rmaker_factory_reset(2, 0);
    return ESP_OK;
}

static esp_err_t dev_mgr_dispatch(int argc, char *argv[])
{
    if (argc <= 0) {
        dev_mgr_console.for_each_command(print_description, NULL);
        return ESP_OK;
    }
    return dev_mgr_console.exec_command(argc, argv);
}

esp_err_t ctl_dev_mgr_register_commands()
{
    static const command_t command = {
        .name = "dev_mgr",
        .description = "controller device manager commands. Usage: matter esp dev_mgr <dev_mgr_command>",
        .handler = dev_mgr_dispatch,
    };
    static const command_t dev_mgr_commands[] = {
        {
            .name = "print",
            .description = "print current device list",
            .handler = print_dev_list_handler,
        },
        {
            .name = "reset",
            .description = "reset",
            .handler = reset_handler,
        },
    };
    dev_mgr_console.register_commands(dev_mgr_commands, sizeof(dev_mgr_commands) / sizeof(command_t));
    return add_commands(&command, 1);
}

} // namespace console
} // namespace esp_matter

extern "C" void app_main()
{
    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_node_init()
     */
    app_wifi_init();

    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_wifi_init() but before app_wifi_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Switch");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    app_rmaker_user_api_config_t api_config = {0};
    app_rmaker_user_api_init(&api_config);

    esp_rmaker_system_serv_config_t system_serv_config = {
        .flags = SYSTEM_SERV_FLAGS_ALL,
        .reboot_seconds = 0,
        .reset_seconds = 2,
        .reset_reboot_seconds = 0,
    };
    esp_rmaker_system_service_enable(&system_serv_config);

    /* Create a MatterController device.
     * You can optionally use the helper API esp_rmaker_switch_device_create() to
     * avoid writing code for adding the name and power parameters.
     */
    matter_controller_device = esp_rmaker_device_create("MatterController", "esp.device.matter-controller", NULL);

    /* Add the write callback for the device. We aren't registering any read callback yet as
     * it is for future use.
     */
    esp_rmaker_device_add_cb(matter_controller_device, NULL, NULL);

    /* Add the standard name parameter (type: esp.param.name), which allows setting a persistent,
     * user friendly custom name from the phone apps. All devices are recommended to have this
     * parameter.
     */
    esp_rmaker_device_add_param(matter_controller_device,
                                esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "Matter-Controller"));

    /* Add this switch device to the node */
    esp_rmaker_node_add_device(node, matter_controller_device);

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

    matter_controller_config_t config = {
        .setup_callback = app_matter_controller_setup_controller,
        .update_noc_callback = app_matter_controller_update_noc_chain,
        .device_list_update_callback = app_matter_controller_update_device_list,
    };
    /* Enable Matter Controller service */
    app_rmaker_matter_controller_enable(&config);
    matter_attr_report_init(app_rmaker_matter_controller_get_matter_devices());
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &matter_ctl_net_event_handler, NULL);

    esp_rmaker_auth_service_enable();
    matter_controller_cmd_resp_enable();

    // Start matter
    esp_matter::start(NULL);
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::init();
    esp_matter::console::controller_register_commands();
    esp_matter::console::ctl_dev_mgr_register_commands();

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    esp_matter::controller::set_custom_credentials_issuer(&s_matter_controller_creds_issuer);
    err = app_wifi_set_custom_mfg_data(MFG_DATA_DEVICE_TYPE_MATTER_CONTROLLER,
                                       MFG_DATA_DEVICE_SUBTYPE_MATTER_CONTROLLER);
    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_wifi_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}
