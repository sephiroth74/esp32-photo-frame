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

#pragma once

#include "RTClib.h"
#include "config.h"
#include "errors.h"
#include "sd_card.h"
#include <WiFi.h>

namespace photo_frame {

class wifi_manager {
  public:
    wifi_manager();
    ~wifi_manager();

    photo_frame_error_t init(const char* config_file, sd_card& sdCard);
    photo_frame_error_t connect();
    DateTime fetch_datetime(photo_frame_error_t* error = nullptr);
    void set_timezone(const char* timezone);
    void disconnect();
    bool is_connected() const;
    String get_ip_address() const;
    String get_ssid() const;
    void end();

  private:
    String _ssid;
    String _password;
    bool _initialized;
    bool _connected;

    static const unsigned long CONNECTION_TIMEOUT_MS;
};

} // namespace photo_frame