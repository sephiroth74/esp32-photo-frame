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

// ============================================================================
// MAIN.CPP - Entry Point Router
// ============================================================================
//
// This file serves as the entry point for the ESP32 Photo Frame firmware.
// It routes to the appropriate main implementation based on compile-time
// configuration flags.
//
// The actual setup() and loop() implementations are in separate files:
// - display_diagnostic_main.cpp (ENABLE_DISPLAY_DIAGNOSTIC)
// - main_ws.cpp                 (ENABLE_WEBSERVER_DATAPROVIDER)
// - main_default.cpp            (normal mode)
//
// This architecture keeps main.cpp clean and minimal while allowing
// each configuration to have its own dedicated implementation file.
//

// ============================================================================
// MODE SELECTION - Based on compile-time configuration
// ============================================================================

#if defined(ENABLE_DISPLAY_DIAGNOSTIC)
// ============================================================================
// DISPLAY DIAGNOSTIC MODE
// ============================================================================
// Used for testing and debugging display hardware
#include "display_diagnostic_main.cpp"

#elif defined(ENABLE_WEBSERVER_DATAPROVIDER)

#include "main_ws.h"

void setup() { main_webserver_setup(); }

void loop() { main_webserver_loop(); }

#else
// ============================================================================
// NORMAL MODE (DEFAULT)
// ============================================================================
// Standard photo frame operation with Google Drive and/or SD Card support
#include "main_default.h"

void setup() { default_main_setup(); }

void loop() { default_main_loop(); }

#endif // Mode selection
