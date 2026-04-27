/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_rmaker_cmd_resp.h>
#include <esp_rmaker_utils.h>
#include <esp_matter_core.h>
#include <esp_matter_controller_utils.h>
#include <esp_matter_controller_cluster_command.h>
#include <esp_matter_controller_write_command.h>
#include <esp_matter_controller_read_command.h>
#include <esp_check.h>
#include <esp_log.h>
#include <matter_controller_cmd_resp.h>

#include <json_generator.h>
#include <json_parser.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <utility>

#include <app/ConcreteAttributePath.h>
#include <app/MessageDef/StatusIB.h>
#include <app/WriteClient.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/Optional.h>
#include <lib/support/ScopedBuffer.h>
#include <lib/support/CHIPMem.h>
#include <lib/core/NodeId.h>
#include <lib/core/TLVReader.h>

using namespace esp_matter;
using namespace chip::app;

#define TAG "MatterController"

#define MATTER_CONTROL_CMD_TYPE_INVOKE_CMD 0x1100
#define MATTER_CONTROL_CMD_TYPE_WRITE_ATTR 0x1101
#define MATTER_CONTROL_CMD_TYPE_READ 0x1102

#define MAX_COMMAND_FIELD_BUFFER_SIZE 120
#define MAX_ATTRIBUTE_VALUE_BUFFER_SIZE MAX_COMMAND_FIELD_BUFFER_SIZE
#define MAX_CMD_RESP_BUFFER_SIZE 5000

namespace {
char *s_cmd_resp_buffer = nullptr;
json_gen_str_t s_resp_jstr;
const int INVOKE_CMD_HANDLED_EVENT = BIT0;
const int WRITE_ATTR_HANDLED_EVENT = BIT1;
const int READ_HANDLED_EVENT = BIT2;
EventGroupHandle_t s_matter_controller_event_group;
bool s_read_results_array_open = false;

static void close_read_results_array(void)
{
    if (s_read_results_array_open) {
        json_gen_pop_array(&s_resp_jstr);
        s_read_results_array_open = false;
    }
}

static bool appendf(char *buf, size_t buf_size, size_t &pos, const char *fmt, ...)
{
    if (!buf || buf_size == 0 || pos >= buf_size) {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf + pos, buf_size - pos, fmt, args);
    va_end(args);
    if (written < 0) {
        return false;
    }
    if (static_cast<size_t>(written) >= (buf_size - pos)) {
        pos = buf_size - 1;
        buf[pos] = '\0';
        return false;
    }
    pos += static_cast<size_t>(written);
    return true;
}

static void append_json_escaped_charspan(const chip::CharSpan &str, char *buf, size_t buf_size, size_t &pos)
{
    appendf(buf, buf_size, pos, "\"");
    for (size_t i = 0; i < str.size(); ++i) {
        const char c = str.data()[i];
        if (c == '"' || c == '\\') {
            appendf(buf, buf_size, pos, "\\%c", c);
        } else if (c == '\n') {
            appendf(buf, buf_size, pos, "\\n");
        } else if (c == '\r') {
            appendf(buf, buf_size, pos, "\\r");
        } else if (c == '\t') {
            appendf(buf, buf_size, pos, "\\t");
        } else if (static_cast<unsigned char>(c) < 0x20) {
            appendf(buf, buf_size, pos, "\\u%04x", static_cast<unsigned char>(c));
        } else {
            appendf(buf, buf_size, pos, "%c", c);
        }
    }
    appendf(buf, buf_size, pos, "\"");
}

static bool decode_tlv_reader_to_string(chip::TLV::TLVReader &reader, char *buf, size_t buf_size, size_t &pos, int depth)
{
    if (depth > 8) {
        return appendf(buf, buf_size, pos, "\"max_depth\"");
    }

    switch (reader.GetType()) {
    case chip::TLV::kTLVType_Boolean: {
        bool v = false;
        if (reader.Get(v) == CHIP_NO_ERROR) {
            return appendf(buf, buf_size, pos, "%s", v ? "true" : "false");
        }
        return appendf(buf, buf_size, pos, "\"bool_decode_failed\"");
    }
    case chip::TLV::kTLVType_SignedInteger: {
        int64_t v = 0;
        if (reader.Get(v) == CHIP_NO_ERROR) {
            return appendf(buf, buf_size, pos, "%" PRId64, v);
        }
        return appendf(buf, buf_size, pos, "\"int_decode_failed\"");
    }
    case chip::TLV::kTLVType_UnsignedInteger: {
        uint64_t v = 0;
        if (reader.Get(v) == CHIP_NO_ERROR) {
            return appendf(buf, buf_size, pos, "%" PRIu64, v);
        }
        return appendf(buf, buf_size, pos, "\"uint_decode_failed\"");
    }
    case chip::TLV::kTLVType_FloatingPointNumber: {
        double v = 0.0;
        if (reader.Get(v) == CHIP_NO_ERROR) {
            return appendf(buf, buf_size, pos, "%g", v);
        }
        float fv = 0.0f;
        if (reader.Get(fv) == CHIP_NO_ERROR) {
            return appendf(buf, buf_size, pos, "%g", static_cast<double>(fv));
        }
        return appendf(buf, buf_size, pos, "\"float_decode_failed\"");
    }
    case chip::TLV::kTLVType_UTF8String: {
        chip::CharSpan str;
        if (reader.Get(str) == CHIP_NO_ERROR) {
            append_json_escaped_charspan(str, buf, buf_size, pos);
            return true;
        }
        return appendf(buf, buf_size, pos, "\"string_decode_failed\"");
    }
    case chip::TLV::kTLVType_ByteString: {
        chip::ByteSpan bytes;
        if (reader.Get(bytes) == CHIP_NO_ERROR) {
            appendf(buf, buf_size, pos, "\"0x");
            for (size_t i = 0; i < bytes.size(); ++i) {
                if (!appendf(buf, buf_size, pos, "%02x", bytes.data()[i])) {
                    appendf(buf, buf_size, pos, "...");
                    break;
                }
            }
            return appendf(buf, buf_size, pos, "\"");
        }
        return appendf(buf, buf_size, pos, "\"bytes_decode_failed\"");
    }
    case chip::TLV::kTLVType_Null:
        return appendf(buf, buf_size, pos, "null");
    case chip::TLV::kTLVType_Structure:
    case chip::TLV::kTLVType_Array:
    case chip::TLV::kTLVType_List: {
        const bool is_object_like = (reader.GetType() == chip::TLV::kTLVType_Structure);
        appendf(buf, buf_size, pos, "%c", is_object_like ? '{' : '[');

        chip::TLV::TLVType outer_container_type = chip::TLV::kTLVType_NotSpecified;
        if (reader.EnterContainer(outer_container_type) != CHIP_NO_ERROR) {
            return appendf(buf, buf_size, pos, "%c", is_object_like ? '}' : ']');
        }

        bool first = true;
        while (reader.Next() == CHIP_NO_ERROR) {
            if (!first) {
                appendf(buf, buf_size, pos, ",");
            }
            first = false;

            if (is_object_like) {
                if (chip::TLV::IsContextTag(reader.GetTag())) {
                    appendf(buf, buf_size, pos, "\"%" PRIu32 "\":",
                            static_cast<uint32_t>(chip::TLV::TagNumFromTag(reader.GetTag())));
                } else {
                    appendf(buf, buf_size, pos, "\"tag\":");
                }
            }

            if (!decode_tlv_reader_to_string(reader, buf, buf_size, pos, depth + 1)) {
                break;
            }
        }
        reader.ExitContainer(outer_container_type);
        return appendf(buf, buf_size, pos, "%c", is_object_like ? '}' : ']');
    }
    default:
        return appendf(buf, buf_size, pos, "\"unsupported_tlv_type\"");
    }
}

void decode_tlv_value_to_jstrgen(chip::TLV::TLVReader *data, const char *key, json_gen_str_t *jstr)
{
    if (!data || !key || !jstr) {
        return;
    }

    switch (data->GetType()) {
    case chip::TLV::kTLVType_Boolean: {
        bool v = false;
        if (data->Get(v) == CHIP_NO_ERROR) {
            json_gen_obj_set_bool(jstr, key, v);
            return;
        }
        break;
    }
    case chip::TLV::kTLVType_SignedInteger: {
        int64_t v = 0;
        if (data->Get(v) == CHIP_NO_ERROR) {
            if (v >= INT_MIN && v <= INT_MAX) {
                json_gen_obj_set_int(jstr, key, static_cast<int>(v));
            } else {
                // json_generator supports integer as int; keep large values lossless as string.
                char num_buf[32] = {0};
                snprintf(num_buf, sizeof(num_buf), "%" PRId64, v);
                json_gen_obj_set_string(jstr, key, num_buf);
            }
            return;
        }
        break;
    }
    case chip::TLV::kTLVType_UnsignedInteger: {
        uint64_t v = 0;
        if (data->Get(v) == CHIP_NO_ERROR) {
            if (v <= static_cast<uint64_t>(INT_MAX)) {
                json_gen_obj_set_int(jstr, key, static_cast<int>(v));
            } else {
                // json_generator supports integer as int; keep large values lossless as string.
                char num_buf[32] = {0};
                snprintf(num_buf, sizeof(num_buf), "%" PRIu64, v);
                json_gen_obj_set_string(jstr, key, num_buf);
            }
            return;
        }
        break;
    }
    case chip::TLV::kTLVType_FloatingPointNumber: {
        double dv = 0.0;
        if (data->Get(dv) == CHIP_NO_ERROR) {
            json_gen_obj_set_float(jstr, key, static_cast<float>(dv));
            return;
        }
        float fv = 0.0f;
        if (data->Get(fv) == CHIP_NO_ERROR) {
            json_gen_obj_set_float(jstr, key, fv);
            return;
        }
        break;
    }
    case chip::TLV::kTLVType_UTF8String: {
        chip::CharSpan str;
        if (data->Get(str) == CHIP_NO_ERROR) {
            char str_buf[192] = {0};
            size_t copy_len = str.size();
            if (copy_len >= sizeof(str_buf)) {
                copy_len = sizeof(str_buf) - 1;
            }
            memcpy(str_buf, str.data(), copy_len);
            str_buf[copy_len] = '\0';
            json_gen_obj_set_string(jstr, key, str_buf);
            return;
        }
        break;
    }
    case chip::TLV::kTLVType_ByteString: {
        chip::ByteSpan bytes;
        if (data->Get(bytes) == CHIP_NO_ERROR) {
            char hex_buf[192] = {0};
            size_t pos = 0;
            appendf(hex_buf, sizeof(hex_buf), pos, "0x");
            for (size_t i = 0; i < bytes.size(); ++i) {
                if (!appendf(hex_buf, sizeof(hex_buf), pos, "%02x", bytes.data()[i])) {
                    appendf(hex_buf, sizeof(hex_buf), pos, "...");
                    break;
                }
            }
            json_gen_obj_set_string(jstr, key, hex_buf);
            return;
        }
        break;
    }
    case chip::TLV::kTLVType_Null:
        json_gen_obj_set_null(jstr, key);
        return;
    case chip::TLV::kTLVType_Structure:
    case chip::TLV::kTLVType_Array:
    case chip::TLV::kTLVType_List: {
        char json_buf[256] = {0};
        chip::TLV::TLVReader reader_cpy;
        reader_cpy.Init(*data);
        size_t pos = 0;
        if (!decode_tlv_reader_to_string(reader_cpy, json_buf, sizeof(json_buf), pos, 0)) {
            json_gen_obj_set_string(jstr, key, "decode_truncated");
            return;
        }
        if (json_buf[0] == '{') {
            json_gen_push_object_str(jstr, key, json_buf);
        } else if (json_buf[0] == '[') {
            json_gen_push_array_str(jstr, key, json_buf);
        } else {
            json_gen_obj_set_string(jstr, key, json_buf);
        }
        return;
    }
    default:
        break;
    }
    json_gen_obj_set_string(jstr, key, "tlv_decode_failed");
}

void invoke_cmd_success_fcn(void *ctx, const ConcreteCommandPath &command_path, const StatusIB &status,
                            TLVReader *response_data)
{
    json_gen_obj_set_string(&s_resp_jstr, "status", "success");
    if (response_data) {
        decode_tlv_value_to_jstrgen(response_data, "response_data", &s_resp_jstr);
    }
    xEventGroupSetBits(s_matter_controller_event_group, INVOKE_CMD_HANDLED_EVENT);
}

void invoke_cmd_failure_fcn(void *ctx, CHIP_ERROR error)
{
    json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
    json_gen_obj_set_string(&s_resp_jstr, "reason", error.AsString());
    xEventGroupSetBits(s_matter_controller_event_group, INVOKE_CMD_HANDLED_EVENT);
}

static void invoke_cmd_connect_failure_fcn(void *context, const chip::ScopedNodeId &peer_id, CHIP_ERROR error)
{
    (void)peer_id;
    (void)error;
    json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
    json_gen_obj_set_string(&s_resp_jstr, "reason", "device_unreachable");
    xEventGroupSetBits(s_matter_controller_event_group, INVOKE_CMD_HANDLED_EVENT);
}

void write_attr_success_fcn(const ConcreteDataAttributePath & attr_path)
{
    json_gen_obj_set_string(&s_resp_jstr, "status", "success");
    xEventGroupSetBits(s_matter_controller_event_group, WRITE_ATTR_HANDLED_EVENT);
}

void write_attr_failure_fcn(const ConcreteDataAttributePath & attr_path, CHIP_ERROR error)
{
    json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
    json_gen_obj_set_string(&s_resp_jstr, "reason", error.AsString());
    xEventGroupSetBits(s_matter_controller_event_group, WRITE_ATTR_HANDLED_EVENT);
}

void connect_failure_fcn(void *context, const chip::ScopedNodeId & node_id, CHIP_ERROR error)
{
    json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
    json_gen_obj_set_string(&s_resp_jstr, "reason", "device_unreachable");
    xEventGroupSetBits(s_matter_controller_event_group, WRITE_ATTR_HANDLED_EVENT);
}

static void read_attribute_data_cb(uint64_t remote_node_id,
                                    const chip::app::ConcreteDataAttributePath &path,
                                    chip::TLV::TLVReader *data, const chip::app::StatusIB &status)
{
    char endpoint_id_str[8] = {0};
    char cluster_id_str[12] = {0};
    char attribute_id_str[12] = {0};
    snprintf(endpoint_id_str, sizeof(endpoint_id_str), "%u", path.mEndpointId);
    snprintf(cluster_id_str, sizeof(cluster_id_str), "0x%08lx", (unsigned long)path.mClusterId);
    snprintf(attribute_id_str, sizeof(attribute_id_str), "0x%08lx", (unsigned long)path.mAttributeId);

    // Each callback invocation appends one result entry.
    json_gen_start_object(&s_resp_jstr);
    json_gen_obj_set_string(&s_resp_jstr, "endpoint_id", endpoint_id_str);
    json_gen_obj_set_string(&s_resp_jstr, "cluster_id", cluster_id_str);
    json_gen_obj_set_string(&s_resp_jstr, "attribute_id", attribute_id_str);
    if (status.IsFailure()) {
        json_gen_obj_set_string(&s_resp_jstr, "error", status.ToChipError().AsString());
    } else if (data) {
        decode_tlv_value_to_jstrgen(data, "attribute_value", &s_resp_jstr);
    }
    json_gen_end_object(&s_resp_jstr);
}

static void read_event_data_cb(uint64_t remote_node_id,
                               const chip::app::EventHeader &header,
                               chip::TLV::TLVReader *data, const chip::app::StatusIB *status)
{
    char endpoint_id_str[8] = {0};
    char cluster_id_str[12] = {0};
    char event_id_str[12] = {0};
    snprintf(endpoint_id_str, sizeof(endpoint_id_str), "%u", header.mPath.mEndpointId);
    snprintf(cluster_id_str, sizeof(cluster_id_str), "0x%08lx", (unsigned long)header.mPath.mClusterId);
    snprintf(event_id_str, sizeof(event_id_str), "0x%08lx", (unsigned long)header.mPath.mEventId);

    // Each callback invocation appends one result entry.
    json_gen_start_object(&s_resp_jstr);
    json_gen_obj_set_string(&s_resp_jstr, "endpoint_id", endpoint_id_str);
    json_gen_obj_set_string(&s_resp_jstr, "cluster_id", cluster_id_str);
    json_gen_obj_set_string(&s_resp_jstr, "event_id", event_id_str);
    if (status && status->IsFailure()) {
        json_gen_obj_set_string(&s_resp_jstr, "error", status->ToChipError().AsString());
    } else if (data) {
        decode_tlv_value_to_jstrgen(data, "event_data", &s_resp_jstr);
    }
    json_gen_end_object(&s_resp_jstr);
}

static void read_attribute_done_cb(uint64_t remote_node_id,
                                    const chip::Platform::ScopedMemoryBufferWithSize<AttributePathParams> &attr_path,
                                    const chip::Platform::ScopedMemoryBufferWithSize<EventPathParams> &event_path)
{
    (void)remote_node_id;
    (void)attr_path;
    (void)event_path;
    close_read_results_array();
    xEventGroupSetBits(s_matter_controller_event_group, READ_HANDLED_EVENT);
}

static void read_connect_failure_fcn(void *context, const chip::ScopedNodeId &peer_id, CHIP_ERROR error)
{
    (void)peer_id;
    (void)error;
    close_read_results_array();
    json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
    json_gen_obj_set_string(&s_resp_jstr, "reason", "device_unreachable");
    xEventGroupSetBits(s_matter_controller_event_group, READ_HANDLED_EVENT);
}

static void read_error_fcn(uint64_t node_id, CHIP_ERROR error)
{
    (void)node_id;
    close_read_results_array();
    json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
    json_gen_obj_set_string(&s_resp_jstr, "reason", error.AsString());
}

esp_err_t invoke_cluster_command(uint64_t destination_id, uint16_t endpoint_id, uint32_t cluster_id,
                                 uint32_t command_id, const char *command_data_field,
                                 const chip::Optional<uint16_t> timed_interaction_timeout_ms)
{
    ESP_LOGI(TAG, "Send cluster command [cluster 0x%lx, command 0x%lx] to node %llx endpoint %x", cluster_id, command_id,
             destination_id, endpoint_id);
    controller::cluster_command *cluster_command =
        chip::Platform::New<controller::cluster_command>(destination_id, endpoint_id, cluster_id, command_id, command_data_field,
                                                         timed_interaction_timeout_ms, invoke_cmd_success_fcn, invoke_cmd_failure_fcn,
                                                         invoke_cmd_connect_failure_fcn);
    if (!cluster_command) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = ESP_OK;
    {
        lock::ScopedChipStackLock lock(3000);
        xEventGroupClearBits(s_matter_controller_event_group, INVOKE_CMD_HANDLED_EVENT);
        err = cluster_command->send_command();
    }
    if (err == ESP_OK) {
        xEventGroupWaitBits(s_matter_controller_event_group, INVOKE_CMD_HANDLED_EVENT, true, true, 20000 / portTICK_PERIOD_MS);
    } else {
        chip::Platform::Delete(cluster_command);
    }
    return err;
}

esp_err_t esp_rmaker_matter_controller_invoke_cmd_handler(const void *in_data, size_t in_len,
                                                          void **out_data, size_t *out_len,
                                                          esp_rmaker_cmd_ctx_t *ctx, void *priv)
{
    if (in_data == nullptr || in_len == 0) {
        ESP_LOGE(TAG, "No data received for invoking matter command");
        return ESP_FAIL;
    }

    if (s_cmd_resp_buffer == nullptr) {
        ESP_LOGE(TAG, "Command response buffer not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    size_t response_len = 0;
    chip::Optional<uint16_t> timed_interaction_timeout_ms = chip::NullOptional;
    ESP_LOGI(TAG, "Receive invoke-command command: %.*s", (int)in_len, (char *)in_data);
    // Parse the input JSON to build the cluster_command class
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, (char *)in_data, in_len) == 0) {
        uint32_t cluster_id = chip::kInvalidClusterId, command_id = chip::kInvalidCommandId;
        char command_fields_buffer[MAX_COMMAND_FIELD_BUFFER_SIZE];
        char id_buffer[19] = {0};
        if (json_obj_get_object(&jctx, "request_payload") == 0) {
            if (json_obj_get_string(&jctx, "cluster_id", id_buffer, sizeof(id_buffer)) == 0) {
                cluster_id = string_to_uint32(id_buffer);
            }
            if (json_obj_get_string(&jctx, "command_id", id_buffer, sizeof(id_buffer)) == 0) {
                command_id = string_to_uint32(id_buffer);
            }
            if (cluster_id == chip::kInvalidClusterId || command_id == chip::kInvalidCommandId) {
                json_parse_end(&jctx);
                return ESP_FAIL;
            }
            int timed_interaction_timeout = 0;
            if (json_obj_get_int(&jctx, "timed_interaction_timeout", &timed_interaction_timeout) == 0) {
                timed_interaction_timeout_ms.SetValue((uint16_t)timed_interaction_timeout);
            }
            int command_field_len = 0;
            if (json_obj_get_object_strlen(&jctx, "command_fields", &command_field_len) != 0) {
                strncpy(command_fields_buffer, "{}", strlen("{}"));
            } else {
                if (command_field_len >= MAX_COMMAND_FIELD_BUFFER_SIZE ||
                    json_obj_get_object_str(&jctx, "command_fields", command_fields_buffer, command_field_len + 1) != 0) {
                    json_parse_end(&jctx);
                    ESP_LOGE(TAG, "Invalid command_fields");
                    return ESP_FAIL;
                }
            }
            json_obj_leave_object(&jctx);
        } else {
            json_parse_end(&jctx);
            ESP_LOGE(TAG, "No 'request_payload' found in input");
            return ESP_FAIL;
        }
        int obj_num = 0;
        if (json_obj_get_array(&jctx, "objects", &obj_num) == 0) {
            uint64_t node_id = chip::kUndefinedNodeId;
            uint16_t endpoint_id = chip::kInvalidEndpointId;
            memset(s_cmd_resp_buffer, 0, MAX_CMD_RESP_BUFFER_SIZE);
            memset(&s_resp_jstr, 0, sizeof(s_resp_jstr));
            json_gen_str_start(&s_resp_jstr, s_cmd_resp_buffer, MAX_CMD_RESP_BUFFER_SIZE, NULL, NULL);
            json_gen_start_object(&s_resp_jstr);
            json_gen_push_array(&s_resp_jstr, "responses");
            for (int i = 0; i < obj_num; ++i) {
                if (json_arr_get_object(&jctx, i) == 0) {
                    json_gen_start_object(&s_resp_jstr);
                    if (json_obj_get_string(&jctx, "matter_node_id", id_buffer, sizeof(id_buffer)) == 0) {
                        json_gen_obj_set_string(&s_resp_jstr, "matter_node_id", id_buffer);
                        node_id = string_to_uint64(id_buffer);
                    }
                    if (json_obj_get_string(&jctx, "matter_endpoint_id", id_buffer, sizeof(id_buffer)) == 0) {
                        json_gen_obj_set_string(&s_resp_jstr, "matter_endpoint_id", id_buffer);
                        endpoint_id = string_to_uint16(id_buffer);
                    }
                    if (node_id != chip::kUndefinedNodeId && endpoint_id != chip::kInvalidEndpointId) {
                        if (invoke_cluster_command(node_id, endpoint_id, cluster_id, command_id, command_fields_buffer, timed_interaction_timeout_ms) != ESP_OK) {
                            json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
                        }
                    } else {
                        json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
                        json_gen_obj_set_string(&s_resp_jstr, "reason", "invalid object");
                    }
                    json_gen_end_object(&s_resp_jstr);
                    json_arr_leave_object(&jctx);
                }
            }
            json_gen_pop_array(&s_resp_jstr);
            json_gen_end_object(&s_resp_jstr);
            response_len = json_gen_str_end(&s_resp_jstr);
        } else {
            json_parse_end(&jctx);
            ESP_LOGE(TAG, "No 'objects' found in input");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Failed to parse input JSON");
        return ESP_FAIL;
    }
    *out_data = s_cmd_resp_buffer;
    *out_len = response_len - 1;
    ESP_LOGI(TAG, "Returning response: %s", s_cmd_resp_buffer);
    return ESP_OK;
}

esp_err_t write_attr_command(uint64_t node_id, uint16_t endpoint_id, uint32_t cluster_id,
                             uint32_t attribute_id, const char *attr_val)
{
    ESP_LOGI(TAG, "Send write_attr command [cluster 0x%lx, attribute 0x%lx] to node %llx endpoint %x", cluster_id, attribute_id,
             node_id, endpoint_id);
    controller::write_command *write_command =
        chip::Platform::New<controller::write_command>(node_id, endpoint_id, cluster_id, attribute_id, attr_val,
                                                       chip::NullOptional, connect_failure_fcn, write_attr_success_fcn,
                                                       write_attr_failure_fcn, nullptr);
    if (!write_command) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = ESP_OK;
    {
        lock::ScopedChipStackLock lock(3000);
        xEventGroupClearBits(s_matter_controller_event_group, WRITE_ATTR_HANDLED_EVENT);
        err = write_command->send_command();
    }
    if (err == ESP_OK) {
        xEventGroupWaitBits(s_matter_controller_event_group, WRITE_ATTR_HANDLED_EVENT, true, true, 20000 / portTICK_PERIOD_MS);
    } else {
        chip::Platform::Delete(write_command);
    }
    return err;
}

esp_err_t read_attr_or_event_command(uint64_t node_id,
                                     chip::Platform::ScopedMemoryBufferWithSize<AttributePathParams> &&attr_paths,
                                     chip::Platform::ScopedMemoryBufferWithSize<EventPathParams> &&event_paths)
{
    ESP_LOGI(TAG, "Send read attribute/event command to node %llx", node_id);
    controller::read_command *cmd = chip::Platform::New<controller::read_command>(
        node_id, std::move(attr_paths), std::move(event_paths),
        read_attribute_data_cb, read_attribute_done_cb, read_event_data_cb,
        read_connect_failure_fcn, read_error_fcn);
    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = ESP_OK;
    {
        lock::ScopedChipStackLock lock(3000);
        xEventGroupClearBits(s_matter_controller_event_group, READ_HANDLED_EVENT);
        err = cmd->send_command();
    }
    if (err != ESP_OK) {
        json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
        json_gen_obj_set_string(&s_resp_jstr, "reason", "send_command failed");
        chip::Platform::Delete(cmd);
    } else {
        xEventGroupWaitBits(s_matter_controller_event_group, READ_HANDLED_EVENT, true, true, 20000 / portTICK_PERIOD_MS);
    }
    return err;
}

esp_err_t esp_rmaker_matter_controller_write_attr_handler(const void *in_data, size_t in_len,
                                                          void **out_data, size_t *out_len,
                                                          esp_rmaker_cmd_ctx_t *ctx, void *priv)
{
    if (in_data == nullptr || in_len == 0) {
        ESP_LOGE(TAG, "No data received for sending writing attribute command");
        return ESP_FAIL;
    }

    if (s_cmd_resp_buffer == nullptr) {
        ESP_LOGE(TAG, "Command response buffer not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    size_t response_len = 0;
    ESP_LOGI(TAG, "Receive write-attribute command: %.*s", (int)in_len, (char *)in_data);
    // Parse the input JSON to build the write_command class
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, (char *)in_data, in_len) == 0) {
        uint32_t cluster_id = chip::kInvalidClusterId, attribute_id = chip::kInvalidAttributeId;
        char attr_val_buffer[MAX_COMMAND_FIELD_BUFFER_SIZE];
        char id_buffer[19] = {0};
        if (json_obj_get_object(&jctx, "request_payload") == 0) {
            if (json_obj_get_string(&jctx, "cluster_id", id_buffer, sizeof(id_buffer)) == 0) {
                cluster_id = string_to_uint32(id_buffer);
            }
            if (json_obj_get_string(&jctx, "attribute_id", id_buffer, sizeof(id_buffer)) == 0) {
                attribute_id = string_to_uint32(id_buffer);
            }
            if (cluster_id == chip::kInvalidClusterId || attribute_id == chip::kInvalidAttributeId) {
                json_parse_end(&jctx);
                return ESP_FAIL;
            }
            int attr_val_len = 0;
            if (json_obj_get_object_strlen(&jctx, "attribute_value", &attr_val_len) != 0 ||
                attr_val_len >= MAX_ATTRIBUTE_VALUE_BUFFER_SIZE ||
                json_obj_get_object_str(&jctx, "attribute_value", attr_val_buffer, attr_val_len + 1) != 0) {
                json_parse_end(&jctx);
                ESP_LOGE(TAG, "Invalid attribute_value");
                return ESP_FAIL;
            }
            json_obj_leave_object(&jctx);
        } else {
            json_parse_end(&jctx);
            ESP_LOGE(TAG, "No 'request_payload' found in input");
            return ESP_FAIL;
        }
        int obj_num = 0;
        if (json_obj_get_array(&jctx, "objects", &obj_num) == 0) {
            uint64_t node_id = chip::kUndefinedNodeId;
            uint16_t endpoint_id = chip::kInvalidEndpointId;
            memset(s_cmd_resp_buffer, 0, MAX_CMD_RESP_BUFFER_SIZE);
            memset(&s_resp_jstr, 0, sizeof(s_resp_jstr));
            json_gen_str_start(&s_resp_jstr, s_cmd_resp_buffer, MAX_CMD_RESP_BUFFER_SIZE, NULL, NULL);
            json_gen_start_object(&s_resp_jstr);
            json_gen_push_array(&s_resp_jstr, "responses");
            for (int i = 0; i < obj_num; ++i) {
                if (json_arr_get_object(&jctx, i) == 0) {
                    json_gen_start_object(&s_resp_jstr);
                    if (json_obj_get_string(&jctx, "matter_node_id", id_buffer, sizeof(id_buffer)) == 0) {
                        json_gen_obj_set_string(&s_resp_jstr, "matter_node_id", id_buffer);
                        node_id = string_to_uint64(id_buffer);
                    }
                    if (json_obj_get_string(&jctx, "matter_endpoint_id", id_buffer, sizeof(id_buffer)) == 0) {
                        json_gen_obj_set_string(&s_resp_jstr, "matter_endpoint_id", id_buffer);
                        endpoint_id = string_to_uint16(id_buffer);
                    }
                    if (node_id != chip::kUndefinedNodeId && endpoint_id != chip::kInvalidEndpointId) {
                        if (write_attr_command(node_id, endpoint_id, cluster_id, attribute_id, attr_val_buffer) != ESP_OK) {
                            json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
                        }
                    } else {
                        json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
                        json_gen_obj_set_string(&s_resp_jstr, "reason", "invalid object");
                    }
                    json_gen_end_object(&s_resp_jstr);
                    json_arr_leave_object(&jctx);
                }
            }
            json_gen_pop_array(&s_resp_jstr);
            json_gen_end_object(&s_resp_jstr);
            response_len = json_gen_str_end(&s_resp_jstr);
        } else {
            json_parse_end(&jctx);
            ESP_LOGE(TAG, "No 'objects' found in input");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Failed to parse input JSON");
        return ESP_FAIL;
    }
    *out_data = s_cmd_resp_buffer;
    *out_len = response_len - 1;
    ESP_LOGI(TAG, "Returning response: %s", s_cmd_resp_buffer);
    return ESP_OK;
}

esp_err_t esp_rmaker_matter_controller_read_handler(const void *in_data, size_t in_len,
                                                    void **out_data, size_t *out_len,
                                                    esp_rmaker_cmd_ctx_t *ctx, void *priv)
{

    if (in_data == nullptr || in_len == 0) {
        ESP_LOGE(TAG, "No data received for sending reading attribute/event command");
        return ESP_FAIL;
    }

    if (s_cmd_resp_buffer == nullptr) {
        ESP_LOGE(TAG, "Command response buffer not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    size_t response_len = 0;
    ESP_LOGI(TAG, "Receive read attribute/event command: %.*s", (int)in_len, (char *)in_data);
    // Parse the input JSON to build the read_command request
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, (char *)in_data, in_len) == 0) {
        uint64_t node_id = chip::kUndefinedNodeId;
        chip::Platform::ScopedMemoryBufferWithSize<AttributePathParams> attr_paths;
        chip::Platform::ScopedMemoryBufferWithSize<EventPathParams> event_paths;
        
        char id_buffer[19] = {0};
        char node_id_str[19] = {0};
        if (json_obj_get_string(&jctx, "matter_node_id", id_buffer, sizeof(id_buffer)) == 0) {
            node_id = string_to_uint64(id_buffer);
            strncpy(node_id_str, id_buffer, sizeof(node_id_str) - 1);
            node_id_str[sizeof(node_id_str) - 1] = '\0';
        }
        int attr_path_count = 0;
        int event_path_count = 0;
        if (json_obj_get_array(&jctx, "attribute_paths", &attr_path_count) == 0) {
            attr_paths.Alloc(attr_path_count);
            if (!attr_paths.Get()) {
                json_parse_end(&jctx);
                return ESP_ERR_NO_MEM;
            }
            for (int i = 0; i < attr_path_count; ++i) {
                if (json_arr_get_object(&jctx, i) == 0) {
                    uint16_t endpoint_id = chip::kInvalidEndpointId;
                    uint32_t cluster_id = chip::kInvalidClusterId, attribute_id = chip::kInvalidAttributeId;
                    // If we cannot get the endpoint_id/cluster_id/attribute_id from the json, use the wildcard value instead.
                    if (json_obj_get_string(&jctx, "endpoint_id", id_buffer, sizeof(id_buffer)) == 0) {
                        endpoint_id = string_to_uint16(id_buffer);
                    }
                    if (json_obj_get_string(&jctx, "cluster_id", id_buffer, sizeof(id_buffer)) == 0) {
                        cluster_id = string_to_uint32(id_buffer);
                    }
                    if (json_obj_get_string(&jctx, "attribute_id", id_buffer, sizeof(id_buffer)) == 0) {
                        attribute_id = string_to_uint32(id_buffer);
                    }
                    attr_paths[i] = AttributePathParams(endpoint_id, cluster_id, attribute_id);
                    json_arr_leave_object(&jctx);
                }
            }
        }
        if (json_obj_get_array(&jctx, "event_paths", &event_path_count) == 0) {
            event_paths.Alloc(event_path_count);
            if (!event_paths.Get()) {
                json_parse_end(&jctx);
                return ESP_ERR_NO_MEM;
            }
            for (int i = 0; i < event_path_count; ++i) {
                if (json_arr_get_object(&jctx, i) == 0) {
                    uint16_t endpoint_id = chip::kInvalidEndpointId;
                    uint32_t cluster_id = chip::kInvalidClusterId, event_id = chip::kInvalidEventId;
                    // If we cannot get the endpoint_id/cluster_id/event_id from the json, use the wildcard value instead.
                    if (json_obj_get_string(&jctx, "endpoint_id", id_buffer, sizeof(id_buffer)) == 0) {
                        endpoint_id = string_to_uint16(id_buffer);
                    }
                    if (json_obj_get_string(&jctx, "cluster_id", id_buffer, sizeof(id_buffer)) == 0) {
                        cluster_id = string_to_uint32(id_buffer);
                    }
                    if (json_obj_get_string(&jctx, "event_id", id_buffer, sizeof(id_buffer)) == 0) {
                        event_id = string_to_uint32(id_buffer);
                    }
                    event_paths[i] = EventPathParams(endpoint_id, cluster_id, event_id);
                    json_arr_leave_object(&jctx);
                }
            }
        }
        if (node_id == chip::kUndefinedNodeId) {
            json_parse_end(&jctx);
            ESP_LOGE(TAG, "Invalid or missing matter_node_id");
            return ESP_FAIL;
        }
        if (attr_path_count == 0 && event_path_count == 0) {
            json_parse_end(&jctx);
            ESP_LOGE(TAG, "At least one attribute_paths or event_paths entry required");
            return ESP_FAIL;
        }
        memset(s_cmd_resp_buffer, 0, MAX_CMD_RESP_BUFFER_SIZE);
        memset(&s_resp_jstr, 0, sizeof(s_resp_jstr));
        json_gen_str_start(&s_resp_jstr, s_cmd_resp_buffer, MAX_CMD_RESP_BUFFER_SIZE, NULL, NULL);
        json_gen_start_object(&s_resp_jstr);
        json_gen_obj_set_string(&s_resp_jstr, "matter_node_id", node_id_str);
        json_gen_push_array(&s_resp_jstr, "read_results");
        s_read_results_array_open = true;
        if (read_attr_or_event_command(node_id, std::move(attr_paths), std::move(event_paths)) != ESP_OK) {
            close_read_results_array();
            json_gen_obj_set_string(&s_resp_jstr, "status", "failure");
            json_gen_obj_set_string(&s_resp_jstr, "reason", "read_attr_or_event_command failed");
        }
        json_gen_end_object(&s_resp_jstr);
        response_len = json_gen_str_end(&s_resp_jstr);
        json_parse_end(&jctx);
    } else {
        ESP_LOGE(TAG, "Failed to parse input JSON");
        return ESP_FAIL;
    }
    *out_data = s_cmd_resp_buffer;
    *out_len = response_len - 1;
    ESP_LOGI(TAG, "Returning response: %s", s_cmd_resp_buffer);
    return ESP_OK;
}
} // namespace


esp_err_t matter_controller_cmd_resp_enable(void)
{
    if (s_cmd_resp_buffer == nullptr) {
        s_cmd_resp_buffer = (char *)MEM_CALLOC_EXTRAM(1, MAX_CMD_RESP_BUFFER_SIZE);
        if (s_cmd_resp_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate command response buffer");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Allocated %d bytes for command response buffer", MAX_CMD_RESP_BUFFER_SIZE);
    }
    s_matter_controller_event_group = xEventGroupCreate();
    if (!s_matter_controller_event_group) {
        ESP_LOGE(TAG, "Failed to allocate controller event group");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = ESP_OK;
    
    ESP_GOTO_ON_ERROR(esp_rmaker_cmd_register(MATTER_CONTROL_CMD_TYPE_INVOKE_CMD,
                                            ESP_RMAKER_USER_ROLE_SUPER_ADMIN |
                                            ESP_RMAKER_USER_ROLE_PRIMARY_USER |
                                            ESP_RMAKER_USER_ROLE_SECONDARY_USER,
                                            esp_rmaker_matter_controller_invoke_cmd_handler,
                                            false, nullptr),
                      clear, TAG, "Failed to register invoke command handler");

    ESP_GOTO_ON_ERROR(esp_rmaker_cmd_register(MATTER_CONTROL_CMD_TYPE_WRITE_ATTR,
                                            ESP_RMAKER_USER_ROLE_SUPER_ADMIN |
                                            ESP_RMAKER_USER_ROLE_PRIMARY_USER |
                                            ESP_RMAKER_USER_ROLE_SECONDARY_USER,
                                            esp_rmaker_matter_controller_write_attr_handler,
                                            false, nullptr),
                      clear, TAG, "Failed to register write attribute handler");

    ESP_GOTO_ON_ERROR(esp_rmaker_cmd_register(MATTER_CONTROL_CMD_TYPE_READ,
                                            ESP_RMAKER_USER_ROLE_SUPER_ADMIN |
                                            ESP_RMAKER_USER_ROLE_PRIMARY_USER |
                                            ESP_RMAKER_USER_ROLE_SECONDARY_USER,
                                            esp_rmaker_matter_controller_read_handler,
                                            false, nullptr),
                      clear, TAG, "Failed to register read handler");
clear:
    if (ret != ESP_OK) {
        if (s_cmd_resp_buffer) {
            free(s_cmd_resp_buffer);
            s_cmd_resp_buffer = nullptr;
        }
        if (s_matter_controller_event_group) {
            vEventGroupDelete(s_matter_controller_event_group);
            s_matter_controller_event_group = NULL;
        }
    }
    return ret;
}

void matter_controller_decode_tlv_to_string(chip::TLV::TLVReader *data, char *buf, size_t buf_size)
{
    if (!data || !buf || buf_size == 0) {
        if (buf && buf_size > 0) {
            buf[0] = '\0';
        }
        return;
    }
    chip::TLV::TLVReader copy;
    copy.Init(*data);
    size_t pos = 0;
    decode_tlv_reader_to_string(copy, buf, buf_size, pos, 0);
    if (pos < buf_size) {
        buf[pos] = '\0';
    } else if (buf_size > 0) {
        buf[buf_size - 1] = '\0';
    }
}
