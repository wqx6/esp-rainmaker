/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <cstddef>
#include <esp_check.h>
#include <esp_err.h>
#include <app_rmaker_matter_controller.h>
#include <stdint.h>

#include <credentials/CHIPCert.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/NodeId.h>
#include <lib/support/PersistentStorageMacros.h>
#include <lib/support/Span.h>

#include <app_matter_controller_creds_issuer.h>

#define TAG "MatterController"

esp_err_t example_op_creds_issuer::generate_controller_noc_chain_with_csr(chip::NodeId node_id, chip::FabricId fabric,
                                                 chip::MutableByteSpan &csr, chip::MutableByteSpan &rcac,
                                                 chip::MutableByteSpan &icac, chip::MutableByteSpan &noc)
{
    (void) node_id;
    (void) fabric;
    // Add new fabric, so we need to query RCAC and IPK for the new fabric

    size_t rcac_der_len = rcac.size();
    ESP_RETURN_ON_ERROR(app_rmaker_matter_controller_fetch_rcac(rcac.data(), &rcac_der_len), TAG, "Failed to fetch RCAC");
    rcac.reduce_size(rcac_der_len);
    icac.reduce_size(0);
    size_t noc_der_len = noc.size();
    ESP_RETURN_ON_ERROR(app_rmaker_matter_controller_issue_controller_noc(csr.data(), csr.size(),
                                                            noc.data(), &noc_der_len, 0, nullptr, 0), TAG, "Failed to issue NOC");
    noc.reduce_size(noc_der_len);
    return ESP_OK;
}
