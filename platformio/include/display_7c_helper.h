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

/**
 * @file display_7c_helper.h
 * @brief Helper functions for 6-color and 7-color e-paper displays
 *
 * This header is reserved for future helper functions specific to
 * 6-color and 7-color ACeP displays. Currently, all display operations
 * use the standard GxEPD2 API directly with pre-rotated images from
 * the Rust processor.
 */

#ifndef DISPLAY_7C_HELPER_H
#define DISPLAY_7C_HELPER_H

#include <GxEPD2_7C.h>

namespace photo_frame {
namespace display_ext {

// Future helper functions for 6C/7C displays will be added here

} // namespace display_ext
} // namespace photo_frame

#endif // DISPLAY_7C_HELPER_H
