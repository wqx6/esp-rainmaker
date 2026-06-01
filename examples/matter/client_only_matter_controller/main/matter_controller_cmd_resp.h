/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_rmaker_cmd_resp.h>
#include <stddef.h>

#include <lib/core/TLVReader.h>

esp_err_t matter_controller_cmd_resp_enable(void);

/** Decode TLV value to a string (for attribute report storage). Buffer is null-terminated. */
void matter_controller_decode_tlv_to_string(chip::TLV::TLVReader *data, char *buf, size_t buf_size);