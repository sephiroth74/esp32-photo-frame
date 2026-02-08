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

#ifndef PHOTO_FRAME_DATA_PROVIDER_MANAGER_H
#define PHOTO_FRAME_DATA_PROVIDER_MANAGER_H

#include "data_provider.h"
#include "sd_card.h"
#include "unified_config.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace photo_frame {

/**
 * @brief Manager for multiple data providers
 *
 * Handles registration and selection of data providers based on configuration.
 */
class DataProviderManager {
  public:
    explicit DataProviderManager(SdCard& sd_card, const unified_config& config);

    /**
     * @brief Register a data provider
     * @param provider Pointer to the DataProvider instance
     */
    void register_provider(DataProvider* provider);

    /**
     * @brief Get the currently active data provider based on configuration
     * @return Pointer to the active DataProvider, or nullptr if none active
     */
    DataProvider* get_active_provider() const;

    /**
     * @brief Load next image using active provider with centralized config/sdcard
     */
    ImageLoadResult load_next_image(bool is_reset);

  private:
    SdCard& sd_card_;
    const unified_config& config_;
    std::vector<DataProvider*> providers_;
};

} // namespace photo_frame

#endif // PHOTO_FRAME_DATA_PROVIDER_MANAGER_H
