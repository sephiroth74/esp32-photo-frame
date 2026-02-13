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

// Display Debug Mode Header
#ifndef __DISPLAY_DEBUG_H__
#define __DISPLAY_DEBUG_H__

#ifdef ENABLE_DISPLAY_DIAGNOSTIC

// Main entry points for debug mode
void display_debug_setup();
void display_debug_loop();

#endif // ENABLE_DISPLAY_DIAGNOSTIC

#endif // __DISPLAY_DEBUG_H__