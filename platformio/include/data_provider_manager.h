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

#ifndef PHOTO_FRAME_DATA_PROVIDER_MANAGER_H
#define PHOTO_FRAME_DATA_PROVIDER_MANAGER_H

#include "data_provider.h"
#include "sd_card.h"
#include "unified_config.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace photo_frame {

class DataProviderManager {
  public:
    explicit DataProviderManager(SdCard& sd_card, const unified_config& config);

    void register_provider(DataProvider* provider);

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
