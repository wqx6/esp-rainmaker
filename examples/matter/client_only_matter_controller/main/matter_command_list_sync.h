/*
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 */

#pragma once

#include <esp_err.h>
#include <app_rmaker_matter_device_list.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * For devices whose RainMaker metadata does not yet include per-cluster `commands`,
 * perform a wildcard Matter read of AcceptedCommandList (0xFFF9) and PUT merged
 * `metadata.Matter.endpoints` to the cloud.
 */
esp_err_t matter_command_list_sync_for_device_list(matter_device_t *dev_list);

#ifdef __cplusplus
}
#endif
