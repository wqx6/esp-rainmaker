/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_matter_controller_credentials_issuer.h>

#include <lib/core/CHIPCallback.h>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>

#include "esp_err.h"

class example_op_creds_issuer : public esp_matter::controller::credentials_issuer {
public:
    example_op_creds_issuer() = default;
    ~example_op_creds_issuer() override {}

    esp_err_t initialize_credentials_issuer(chip::PersistentStorageDelegate & storage) override
    {
        (void)storage;
        return ESP_OK;
    }

    chip::Controller::OperationalCredentialsDelegate *get_delegate() override { return nullptr; }

    esp_err_t generate_controller_noc_chain(chip::NodeId node_id, chip::FabricId fabric,
                                            chip::Crypto::P256Keypair &keypair, chip::MutableByteSpan &rcac,
                                            chip::MutableByteSpan &icac, chip::MutableByteSpan &noc) override
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t generate_controller_noc_chain_with_csr(chip::NodeId node_id, chip::FabricId fabric,
                                                     chip::MutableByteSpan &csr, chip::MutableByteSpan &rcac,
                                                     chip::MutableByteSpan &icac, chip::MutableByteSpan &noc) override;
};
