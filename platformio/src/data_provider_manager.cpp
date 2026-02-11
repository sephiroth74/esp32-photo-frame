// ESP32 Photo Frame
// Copyright (C) 2025 Alessandro Crugnola
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "data_provider_manager.h"

namespace photo_frame {

DataProviderManager::DataProviderManager(SdCard &sd_card, const unified_config &config) : sd_card_(sd_card), config_(config) {}

void DataProviderManager::register_provider(DataProvider *provider) {
  if (!provider) {
    return;
  }
  providers_.push_back(provider);
}

DataProvider *DataProviderManager::get_active_provider() const {
  // find the active provider based on config and registered providers
  const char *provider_name = config_.GoogleDrive.enabled ? "gdrive" : (config_.sd_card.enabled ? "sdcard" : "");

  if (provider_name[0] == '\0') {
    return nullptr;
  }

  // use std::find_if to locate the provider
  auto it = std::find_if(providers_.begin(), providers_.end(),
                         [provider_name](DataProvider *provider) { return strcmp(provider->name(), provider_name) == 0; });
  if (it != providers_.end()) {
    return *it;
  }

  return nullptr;
}

ImageLoadResult DataProviderManager::load_next_image(bool is_reset) {
  DataProvider *active_provider = get_active_provider();
  if (!active_provider) {
    return ImageLoadResult(photo_frame::error_type::DataProviderNotConfigured);
  }
  return active_provider->load_next_image(is_reset, sd_card_, config_);
}

} // namespace photo_frame
