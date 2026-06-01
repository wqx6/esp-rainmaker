/*
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 */

#include <cinttypes>
#include <map>
#include <string>
#include <vector>

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <app/ConcreteAttributePath.h>
#include <app/MessageDef/StatusIB.h>
#include <lib/core/TLVReader.h>
#include <lib/support/CHIPMem.h>
#include <esp_matter_controller_read_command.h>
#include <esp_matter_core.h>

#include <matter_command_list_sync.h>
#include <app_rmaker_matter_controller_api.h>

using namespace esp_matter;
using namespace esp_matter::controller;

#define TAG "mt_cmd_list"

namespace {

constexpr uint32_t kGlobalAcceptedCommandListAttrId = 0xFFF9;
const int READ_DONE_BIT = BIT0;

std::map<uint16_t, std::map<uint32_t, std::vector<uint32_t>>> s_by_ep_cluster;
EventGroupHandle_t s_read_ev;

static CHIP_ERROR DecodeCommandListArray(chip::TLV::TLVReader *reader, std::vector<uint32_t> &out)
{
    if (!reader) {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    chip::TLV::TLVType container;
    ReturnErrorOnFailure(reader->EnterContainer(container));
    while (true) {
        CHIP_ERROR err = reader->Next();
        if (err == CHIP_END_OF_TLV) {
            break;
        }
        ReturnErrorOnFailure(err);
        uint32_t v = 0;
        ReturnErrorOnFailure(reader->Get(v));
        out.push_back(v);
    }
    return reader->ExitContainer(container);
}

static void OnReadAttribute(uint64_t remote_node_id, const chip::app::ConcreteDataAttributePath &path,
                            chip::TLV::TLVReader *data, const chip::app::StatusIB &status)
{
    (void)remote_node_id;
    if (status.ToChipError() != CHIP_NO_ERROR || !data) {
        return;
    }
    if (path.mAttributeId != kGlobalAcceptedCommandListAttrId) {
        return;
    }
    auto &vec = s_by_ep_cluster[path.mEndpointId][path.mClusterId];
    vec.clear();
    if (DecodeCommandListArray(data, vec) != CHIP_NO_ERROR) {
        ESP_LOGW(TAG, "CommandList TLV decode failed ep=%u cl=0x%08" PRIx32, path.mEndpointId,
                 path.mClusterId);
    }
}

static void OnReadDone(uint64_t remote_node_id,
                       const chip::Platform::ScopedMemoryBufferWithSize<AttributePathParams> &attr_path,
                       const chip::Platform::ScopedMemoryBufferWithSize<EventPathParams> &event_path)
{
    (void)remote_node_id;
    (void)attr_path;
    (void)event_path;
    if (s_read_ev) {
        xEventGroupSetBits(s_read_ev, READ_DONE_BIT);
    }
}

static void OnReadConnectFail(void *context, const chip::ScopedNodeId &peer_id, CHIP_ERROR error)
{
    (void)context;
    (void)peer_id;
    (void)error;
    ESP_LOGW(TAG, "Read CommandList: connect failure");
    if (s_read_ev) {
        xEventGroupSetBits(s_read_ev, READ_DONE_BIT);
    }
}

static void OnReadError(uint64_t node_id, CHIP_ERROR error)
{
    ESP_LOGW(TAG, "Read CommandList error node_id=%" PRIx64 " err=%s", node_id, error.AsString());
    if (s_read_ev) {
        xEventGroupSetBits(s_read_ev, READ_DONE_BIT);
    }
}

static std::string BuildMetadataPutBody(
    const std::map<uint16_t, std::map<uint32_t, std::vector<uint32_t>>> &data)
{
    std::string o = "{\"metadata\":{\"Matter\":{\"endpoints\":{";
    bool first_ep = true;
    for (const auto &ep_ent : data) {
        if (!first_ep) {
            o += ',';
        }
        first_ep = false;
        char buf[56];
        snprintf(buf, sizeof(buf), "\"0x%x\":{\"clusters\":{\"servers\":{", ep_ent.first);
        o += buf;
        bool first_cl = true;
        for (const auto &cl_ent : ep_ent.second) {
            if (!first_cl) {
                o += ',';
            }
            first_cl = false;
            snprintf(buf, sizeof(buf), "\"0x%lx\":{\"commands\":[", (unsigned long)cl_ent.first);
            o += buf;
            for (size_t i = 0; i < cl_ent.second.size(); ++i) {
                if (i) {
                    o += ',';
                }
                snprintf(buf, sizeof(buf), "\"0x%lx\"", (unsigned long)cl_ent.second[i]);
                o += buf;
            }
            o += "]}";
        }
        o += "}}}";
    }
    o += "}}}}";
    return o;
}

static esp_err_t ReadCommandListWildcard(uint64_t matter_node_id)
{
    s_by_ep_cluster.clear();
    if (!s_read_ev) {
        s_read_ev = xEventGroupCreate();
        if (!s_read_ev) {
            return ESP_ERR_NO_MEM;
        }
    }

    read_command *cmd = chip::Platform::New<read_command>(
        matter_node_id, 0xFFFF, 0xFFFFFFFF, kGlobalAcceptedCommandListAttrId, READ_ATTRIBUTE, OnReadAttribute,
        OnReadDone, nullptr, OnReadConnectFail, OnReadError);
    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }

    xEventGroupClearBits(s_read_ev, READ_DONE_BIT);
    esp_err_t err = ESP_OK;
    {
        lock::ScopedChipStackLock lock(3000);
        err = cmd->send_command();
    }
    if (err != ESP_OK) {
        chip::Platform::Delete(cmd);
        return err;
    }
    xEventGroupWaitBits(s_read_ev, READ_DONE_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(120000));
    return ESP_OK;
}

} // namespace

extern "C" esp_err_t matter_command_list_sync_for_device_list(matter_device_t *dev_list)
{
    for (matter_device_t *dev = dev_list; dev; dev = dev->next) {
        if (dev->metadata_has_command_lists) {
            continue;
        }
        ESP_LOGI(TAG, "Sync CommandList from Matter for node 0x%016" PRIx64, dev->node_id);
        esp_err_t err = ReadCommandListWildcard(dev->node_id);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Matter read CommandList failed for 0x%016" PRIx64 ": %s", dev->node_id,
                     esp_err_to_name(err));
            continue;
        }
        if (s_by_ep_cluster.empty()) {
            ESP_LOGW(TAG, "No CommandList data for node 0x%016" PRIx64, dev->node_id);
            continue;
        }
        std::string body = BuildMetadataPutBody(s_by_ep_cluster);
        err = app_rmaker_api_update_rainmaker_node_metadata(dev->rainmaker_node_id, body.c_str());
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "update_rainmaker_node_metadata failed for %s: %s", dev->rainmaker_node_id,
                     esp_err_to_name(err));
        } else {
            dev->metadata_has_command_lists = true;
        }
    }
    return ESP_OK;
}
