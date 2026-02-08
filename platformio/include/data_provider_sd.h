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

#ifndef ENABLE_WEBSERVER_DATAPROVIDER

#ifndef PHOTO_FRAME_DATA_PROVIDER_SD_H
#define PHOTO_FRAME_DATA_PROVIDER_SD_H

#include "data_provider.h"
#include "sd_card.h"
#include "unified_config.h"

namespace photo_frame {

class SdCardDataProvider : public DataProvider {
  public:
    SdCardDataProvider();

    const char* name() const override { return "sdcard"; }

    ImageLoadResult
    load_next_image(bool is_reset, SdCard& sd_card, const unified_config& config) override;
};

} // namespace photo_frame

#endif // PHOTO_FRAME_DATA_PROVIDER_SD_H

#endif // ENABLE_WEBSERVER_DATAPROVIDER