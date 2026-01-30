// MIT License
//
// Copyright (c) 2025 Alessandro Crugnola
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "data_provider_manager.h"

namespace photo_frame {

DataProviderManager::DataProviderManager(SdCard& sd_card, const unified_config& config) :
    sd_card_(sd_card),
    config_(config) {}

void DataProviderManager::register_provider(DataProvider* provider) {
    if (!provider) {
        return;
    }
    providers_.push_back(provider);
}

DataProvider* DataProviderManager::get_active_provider() const {
    // find the active provider based on config and registered providers
    const char* provider_name =
        config_.GoogleDrive.enabled ? "gdrive" : (config_.sd_card.enabled ? "sdcard" : "");

    if (provider_name[0] == '\0') {
        return nullptr;
    }

    // use std::find_if to locate the provider
    auto it =
        std::find_if(providers_.begin(), providers_.end(), [provider_name](DataProvider* provider) {
            return strcmp(provider->name(), provider_name) == 0;
        });
    if (it != providers_.end()) {
        return *it;
    }

    return nullptr;
}

ImageLoadResult DataProviderManager::load_next_image(bool is_reset) {
    DataProvider* active_provider = get_active_provider();
    if (!active_provider) {
        return ImageLoadResult(photo_frame::error_type::DataProviderNotConfigured);
    }
    return active_provider->load_next_image(is_reset, sd_card_, config_);
}

} // namespace photo_frame
