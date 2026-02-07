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

#ifndef ENABLE_WEBSERVER_DATAPROVIDER

#ifndef PHOTO_FRAME_DATA_PROVIDER_GDRIVE_H
#define PHOTO_FRAME_DATA_PROVIDER_GDRIVE_H

#include "data_provider.h"
#include "google_drive.h"
#include "sd_card.h"
#include "unified_config.h"

namespace photo_frame {

class GoogleDriveDataProvider : public DataProvider {
  public:
    GoogleDriveDataProvider(GoogleDrive& drive);

    const char* name() const override { return "gdrive"; }

    ImageLoadResult
    load_next_image(bool is_reset, SdCard& sd_card, const unified_config& config) override;

  private:
    GoogleDrive& drive_;
};

} // namespace photo_frame

#endif // PHOTO_FRAME_DATA_PROVIDER_GDRIVE_H

#endif // ENABLE_WEBSERVER_DATAPROVIDER