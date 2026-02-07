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
