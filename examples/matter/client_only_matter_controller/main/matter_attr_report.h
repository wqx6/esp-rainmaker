/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>
#include <esp_rmaker_core.h>

/**
 * Initialize Matter attribute reporting (queue + task).
 */
esp_err_t matter_attr_report_init(esp_rmaker_param_t *params);

/**
 * Callback for init_device_manager(). Syncs subscriptions with the fetched device list and reports removed
 * nodes as JSON null under Matter-Devices.
 */
void matter_attr_report_on_device_list_updated(void);

bool matter_attr_report_get_online_state(uint64_t node_id);
