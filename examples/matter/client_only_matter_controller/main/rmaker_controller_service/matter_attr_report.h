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
 * @param attributes_param RainMaker object param (`esp.param.matter-attributes`); may be NULL to disable reporting.
 */
esp_err_t matter_attr_report_init(esp_rmaker_param_t *attributes_param);

/** Callback to be passed to init_device_manager(). Calls sync subscriptions. */
void matter_attr_report_on_device_list_updated(void);
