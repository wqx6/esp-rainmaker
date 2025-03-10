/* Thread Border Router Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_rcp_update.h>
#include <esp_spiffs.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_thread_br.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_scenes.h>

#include <esp_matter_core.h>
#include <esp_matter.h>
#include <esp_matter_providers.h>
#include <dynamic_commissionable_data_provider.h>
#include <matter_commissioning_window_management.h>
#include <app-common/zap-generated/ids/Clusters.h>

#include <app_wifi.h>
#include <app_insights.h>
#include <app_thread_config.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;

static const char *TAG = "app_main";

esp_rmaker_device_t *thread_br_device;
static dynamic_commissionable_data_provider g_dynamic_passcode_provider;

#ifdef CONFIG_AUTO_UPDATE_RCP
static esp_err_t init_spiffs()
{
    esp_err_t err = ESP_OK;
    esp_vfs_spiffs_conf_t rcp_fw_conf = {.base_path = "/" CONFIG_RCP_PARTITION_NAME,
                                         .partition_label = CONFIG_RCP_PARTITION_NAME,
                                         .max_files = 10, .format_if_mount_failed = false};
    err = esp_vfs_spiffs_register(&rcp_fw_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount rcp firmware storage");
    }
    return err;
}
#endif // CONFIG_AUTO_UPDATE_RCP

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
    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
#ifdef CONFIG_AUTO_UPDATE_RCP
    ESP_ERROR_CHECK(init_spiffs());
#endif

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_node_init()
     */
    app_wifi_init();

    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_wifi_init() but before app_wifi_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "ThreadBR");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
    thread_br_device = esp_rmaker_device_create("ThreadBR", ESP_RMAKER_DEVICE_THREAD_BR, NULL);
    esp_rmaker_device_add_param(thread_br_device,
                                esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "ESP-ThreadBR"));
    esp_rmaker_node_add_device(node, thread_br_device);

    esp_openthread_platform_config_t thread_cfg = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
#ifdef CONFIG_AUTO_UPDATE_RCP
    esp_rcp_update_config_t rcp_update_cfg = ESP_OPENTHREAD_RCP_UPDATE_CONFIG();
    esp_rmaker_thread_br_enable(&thread_cfg, &rcp_update_cfg);
#else
    esp_rmaker_thread_br_enable(&thread_cfg, NULL);
#endif
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

    /* Create Matter data model */
    node::config_t node_config;

    // node handle can be used to add/modify other endpoints.
    node_t *matter_node = node::create(&node_config, nullptr, nullptr);
    if (!matter_node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return;
    }

    // Add Generic Switch endpoint
    generic_switch::config_t switch_config;
    endpoint_t *endpoint = generic_switch::create(matter_node, &switch_config, ENDPOINT_FLAG_NONE, NULL);
    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create Matter endpoint");
        return;
    }

    /* Add additional features to the node */
    cluster_t *cluster = cluster::get(endpoint, chip::app::Clusters::Switch::Id);

    cluster::switch_cluster::feature::momentary_switch::add(cluster);
    cluster::switch_cluster::feature::action_switch::add(cluster);
    cluster::switch_cluster::feature::momentary_switch_multi_press::config_t msm;
    msm.multi_press_max = 5;
    cluster::switch_cluster::feature::momentary_switch_multi_press::add(cluster, &msm);

    matter_commissioning_window_management_enable();

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    esp_matter::set_custom_commissionable_data_provider(&g_dynamic_passcode_provider);
    err = esp_matter::start(matter_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter, err:%d", err);
    }
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
