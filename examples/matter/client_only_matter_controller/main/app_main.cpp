/* Client-only Matter Controller Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_matter_core.h"
#include "portmacro.h"
#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_thread_br.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_user_mapping.h>

#include <esp_rmaker_common_events.h>

#include <app_insights.h>
#include <app_matter_device_manager.h>
#include <app_thread_config.h>
#include <matter_attr_report.h>

#include <matter_controller_std.h>
#include <app_matter_controller.h>
#include <app_matter_controller_callback.h>
#include <app_matter_controller_creds_issuer.h>
#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_controller_console.h>
#include <esp_matter_controller_client.h>
#include <esp_matter_controller_credentials_issuer.h>

static const char *TAG = "app_main";
esp_rmaker_device_t *matter_controller_device;
static bool s_matter_controller_updated;

static void init_matter_controller()
{
    ESP_ERROR_CHECK(esp_matter::start(NULL));
    {
        esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
        esp_matter::controller::matter_controller_client::get_instance().init(0, 0, 5580);
    }
    esp_matter::console::controller_register_commands();
    esp_matter::console::ctl_dev_mgr_register_commands();
    matter_attr_report_init(matter_controller_get_matter_devices_param());
    init_device_manager(matter_attr_report_on_device_list_updated);
}

static void update_matter_controller_task(void *arg)
{
    // Refresh controller state only after Wi-Fi is up so REST and reporting paths can use the network.
    matter_controller_handle_update();
    vTaskDelete(NULL);
}

static void update_matter_controller_once()
{
    if (s_matter_controller_updated) {
        return;
    }
    s_matter_controller_updated = true;
    if (xTaskCreate(update_matter_controller_task, "matter_ctl_update", 8192, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Matter controller update task");
        s_matter_controller_updated = false;
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "Wi-Fi connect skipped: %s", esp_err_to_name(err));
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        update_matter_controller_once();
    }
}

static void app_wifi_init()
{
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
}

static void app_wifi_start()
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

namespace esp_matter {
namespace console {

static engine rmaker_console;

static esp_err_t rmaker_wifi_prov_handler(int argc, char *argv[])
{
    if (argc < 1 || argc > 2) {
        ESP_LOGE(TAG, "Usage: matter esp rmaker wifi-prov <ssid> [passphrase]");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid, argv[0], sizeof(wifi_config.sta.ssid));
    if (argc == 2) {
        strlcpy((char *)wifi_config.sta.password, argv[1], sizeof(wifi_config.sta.password));
    }

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "Failed to stop Wi-Fi: %s", esp_err_to_name(err));
    }
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Failed to set Wi-Fi config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Failed to start Wi-Fi");
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "Failed to connect Wi-Fi");
    return ESP_OK;
}

static esp_err_t rmaker_get_node_id_handler(int argc, char *argv[])
{
    if (argc != 0) {
        ESP_LOGE(TAG, "Usage: matter esp rmaker get-node-id");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Node ID: %s", esp_rmaker_get_node_id());
    return ESP_OK;
}

static esp_err_t rmaker_add_user_handler(int argc, char *argv[])
{
    if (argc != 2) {
        ESP_LOGE(TAG, "Usage: matter esp rmaker add-user <user_id> <secret_key>");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Starting user-node mapping");
    return esp_rmaker_start_user_node_mapping(argv[0], argv[1]);
}

static esp_err_t rmaker_dispatch(int argc, char *argv[])
{
    if (argc <= 0) {
        rmaker_console.for_each_command(print_description, NULL);
        return ESP_OK;
    }
    return rmaker_console.exec_command(argc, argv);
}

static esp_err_t rmaker_register_commands()
{
    static const command_t command = {
        .name = "rmaker",
        .description = "RainMaker setup commands. Usage: matter esp rmaker <command_name>",
        .handler = rmaker_dispatch,
    };
    static const command_t rmaker_commands[] = {
        {
            .name = "wifi-prov",
            .description = "Connect to Wi-Fi. Usage: matter esp rmaker wifi-prov <ssid> [passphrase]",
            .handler = rmaker_wifi_prov_handler,
        },
        {
            .name = "get-node-id",
            .description = "Print the RainMaker node ID. Usage: matter esp rmaker get-node-id",
            .handler = rmaker_get_node_id_handler,
        },
        {
            .name = "add-user",
            .description = "Start user-node association. Usage: matter esp rmaker add-user <user_id> <secret_key>",
            .handler = rmaker_add_user_handler,
        },
    };

    ESP_RETURN_ON_ERROR(rmaker_console.register_commands(rmaker_commands, sizeof(rmaker_commands) / sizeof(command_t)),
                        TAG, "Failed to register RainMaker subcommands");
    return add_commands(&command, 1);
}

} // namespace console
} // namespace esp_matter

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    if (strcmp(esp_rmaker_param_get_name(param), ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", esp_rmaker_device_get_name(device),
                esp_rmaker_param_get_name(param));
        esp_rmaker_param_update_and_report(param, val);
    }
    return ESP_OK;
}

extern "C" void app_main()
{
    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    app_wifi_init();

    esp_matter::console::diagnostics_register_commands();
    ESP_ERROR_CHECK(esp_matter::console::rmaker_register_commands());

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

    /* Create a MatterController device.
     * You can optionally use the helper API esp_rmaker_switch_device_create() to
     * avoid writing code for adding the name and power parameters.
     */
    matter_controller_device = esp_rmaker_device_create("MatterController", ESP_RMAKER_DEVICE_THREAD_BR, NULL);

    /* Add the write callback for the device. We aren't registering any read callback yet as
     * it is for future use.
     */
    esp_rmaker_device_add_cb(matter_controller_device, write_cb, NULL);

    /* Add the standard name parameter (type: esp.param.name), which allows setting a persistent,
     * user friendly custom name from the phone apps. All devices are recommended to have this
     * parameter.
     */
    esp_rmaker_device_add_param(matter_controller_device,
                                esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "MatterController"));

    /* Add this switch device to the node */
    esp_rmaker_node_add_device(node, matter_controller_device);

    esp_openthread_platform_config_t thread_cfg = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()
    };
    esp_rmaker_thread_br_enable(&thread_cfg);

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

    /* Enable Matter Controller service */
    matter_controller_enable(0x131B, app_matter_controller_callback);

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    init_matter_controller();
    ESP_ERROR_CHECK(esp_matter::console::init());
    app_wifi_start();
}
