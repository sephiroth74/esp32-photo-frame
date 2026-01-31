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

#include "data_provider_gdrive.h"
#include "binary_utils.h"
#include "board_util.h"
#include "config.h"
#include "image_buffer.h"
#include "preferences_helper.h"

namespace photo_frame {

GoogleDriveDataProvider::GoogleDriveDataProvider(GoogleDrive& drive) : drive_(drive) {}

ImageLoadResult GoogleDriveDataProvider::load_next_image(bool is_reset,
                                                         SdCard& sd_card,
                                                         const unified_config& config) {
    photo_frame_error_t error    = photo_frame::error_type::None;
    photo_frame_error_t tocError = photo_frame::error_type::JsonParseFailed;
    bool write_toc               = is_reset;
    uint32_t image_index         = 0;
    uint32_t total_files         = 0;
    String original_filename;

    log_i("--------------------------------------");
    log_i(" - Google Drive Mode");
    log_i("--------------------------------------");

    // Ensure display is OFF for SD card operations
    photo_frame::board_utils::display_power_off();

    error = sd_card.begin();

    if (error == photo_frame::error_type::None) {
#if DEBUG_MODE
        sd_card.printStats();
#endif

        log_i("Initializing Google Drive from unified config...");
        error = drive_.initialize_from_unified_config(config.GoogleDrive);

        if (error != photo_frame::error_type::None) {
            log_e("Failed to initialize Google Drive from unified config! Error: %d", error.code);
        } else {
            log_i("Google Drive initialized successfully from unified config");
        }

        if (error == photo_frame::error_type::None) {
            bool shouldCleanup = write_toc;

            if (!shouldCleanup) {
                auto& prefs        = photo_frame::PreferencesHelper::getInstance();
                time_t now         = time(NULL);
                time_t lastCleanup = prefs.getLastCleanup();

                if (now - lastCleanup >= CLEANUP_TEMP_FILES_INTERVAL_SECONDS) {
                    shouldCleanup = true;
                    log_i("Time since last cleanup: %ld seconds", now - lastCleanup);
                } else {
                    log_i("Skipping cleanup, only %ld seconds since last cleanup (need %d seconds)",
                          now - lastCleanup,
                          CLEANUP_TEMP_FILES_INTERVAL_SECONDS);
                }
            }

            if (shouldCleanup) {
                uint32_t cleanedFiles = drive_.cleanup_temporary_files(sd_card, write_toc);
                if (cleanedFiles > 0) {
                    log_i("Cleaned up %u temporary files from previous session", cleanedFiles);
                }

                auto& prefs = photo_frame::PreferencesHelper::getInstance();
                if (prefs.setLastCleanup(time(NULL))) {
                    log_i("Updated last cleanup time");
                } else {
                    log_w("Failed to save cleanup time to preferences");
                }
            }

            if (error == photo_frame::error_type::None) {
                error = drive_.create_directories(sd_card);
            }

            if (error == photo_frame::error_type::None) {
                if (config.GoogleDrive.drive.folder_ids.empty()) {
                    log_e("Google Drive enabled but no folder IDs configured");
                    sd_card.end();
                    return ImageLoadResult(photo_frame::error_type::InvalidConfigNoImageSource);
                }

                bool rebuild_toc = is_reset || !drive_.isMultiDirectoryTocValid(
                                                   sd_card, config.GoogleDrive.drive.folder_ids);

                if (rebuild_toc) {
                    if (is_reset) {
                        log_i("Reset detected - rebuilding all Google Drive TOC files");
                    } else {
                        log_i("TOC mismatch - rebuilding all Google Drive TOC files");
                    }

                    sd_card.cleanupDir(GOOGLE_DRIVE_TOC_BASE_PATH);
                    sd_card.createDirectories(GOOGLE_DRIVE_TOC_BASE_PATH);

                    if (!drive_.buildMultiDirectoryToc(
                            sd_card, config.GoogleDrive.drive.folder_ids, false, &tocError)) {
                        log_e("Failed to build Google Drive TOC cache: %s (code: %u)",
                              tocError.message,
                              tocError.code);
                        sd_card.end();
                        return ImageLoadResult(tocError);
                    }
                } else {
                    log_i("Using existing Google Drive TOC cache");
                }
            } else {
                log_w("Google Drive not initialized - skipping");
            }

            if (error == photo_frame::error_type::None) {
                photo_frame::GoogleDriveFile selectedFile;
                String selected_folder_id;

#ifdef GOOGLE_DRIVE_TEST_FILE
                log_i("Using test file: %s", GOOGLE_DRIVE_TEST_FILE);
                bool found_test = false;
                for (const auto& folder_id : config.GoogleDrive.drive.folder_ids) {
                    String tocPath = drive_.get_toc_file_path_for_folder(folder_id);
                    selectedFile   = drive_.get_toc_file_by_name(
                        sd_card, tocPath, GOOGLE_DRIVE_TEST_FILE, &tocError);
                    if (tocError == photo_frame::error_type::None && selectedFile.id.length() > 0) {
                        selected_folder_id = folder_id;
                        found_test         = true;
                        total_files        = drive_.get_toc_file_count(sd_card, tocPath);
                        image_index        = 0;
                        break;
                    }
                }

                if (!found_test) {
                    log_w("Test file not found in TOC, falling back to random selection. Error: %d",
                          tocError.code);
                    if (!drive_.selectRandomImageFromFolders(sd_card,
                                                             config.GoogleDrive.drive.folder_ids,
                                                             selected_folder_id,
                                                             selectedFile,
                                                             total_files,
                                                             image_index,
                                                             &tocError)) {
                        log_e("No files found in configured Google Drive folders");
                        sd_card.end();
                        return ImageLoadResult(photo_frame::error_type::NoImagesFound);
                    }
                }
#else
                if (!drive_.selectRandomImageFromFolders(sd_card,
                                                         config.GoogleDrive.drive.folder_ids,
                                                         selected_folder_id,
                                                         selectedFile,
                                                         total_files,
                                                         image_index,
                                                         &tocError)) {
                    log_e("No files found in configured Google Drive folders");
                    sd_card.end();
                    return ImageLoadResult(photo_frame::error_type::NoImagesFound);
                }
#endif

                if (tocError == photo_frame::error_type::None && selectedFile.id.length() > 0) {
                    log_i("Selected file: %s", selectedFile.name.c_str());

                    String localFilePath = drive_.get_cached_file_path(selectedFile.name);
                    fs::File file;
                    if (sd_card.fileExists(localFilePath.c_str()) &&
                        sd_card.getFileSize(localFilePath.c_str()) > 0) {
                        log_i("File already exists in SD card, using cached version");
                        file = sd_card.open(localFilePath.c_str(), FILE_READ);
                        drive_.set_last_image_source(photo_frame::IMAGE_SOURCE_LOCAL_CACHE);
                    } else {
                        // Battery check is done by main before calling provider
                        file = drive_.download_file(sd_card, selectedFile, &error);
                    }

                    if (error == photo_frame::error_type::None && file) {
                        const char* filename = file.name();
                        original_filename    = String(filename);

                        log_i("Validating downloaded image file...");
                        auto wrapper = std::make_unique<photo_frame::binary_utils::PFR1BinaryFile>(
                            DISP_WIDTH, DISP_HEIGHT);
                        auto validationError =
                            photo_frame::binary_utils::validatePFR1File(file, *wrapper);
                        file.close();

                        if (validationError != photo_frame::error_type::None) {
                            log_e("Image validation FAILED: %s", validationError.message);

                            String filePath = String(filename);
                            if (sd_card.fileExists(filePath.c_str())) {
                                log_w("Deleting corrupted file from SD card: %s", filePath.c_str());
                                if (sd_card.remove(filePath.c_str())) {
                                    log_i("Corrupted file successfully deleted");
                                } else {
                                    log_e("Failed to delete corrupted file");
                                }
                            }

                            error = validationError;

                            log_i("Closing SD card after validation error");
                            sd_card.end();
                            return ImageLoadResult(error);
                        } else {
                            log_i("Image validation PASSED");

                            // Store last displayed index in preferences
                            auto& prefs = photo_frame::PreferencesHelper::getInstance();
                            prefs.setImageIndex(image_index);

                            sd_card.end();
                            return ImageLoadResult(
                                std::move(wrapper), original_filename, image_index, total_files);
                        }
                    }
                } else {
                    log_e("Failed to get file by index. Error code: %d", tocError.code);
                    error = tocError;
                }

                if (error != photo_frame::error_type::None) {
                    log_e("Failed to process file from Google Drive! Error code: %d", error.code);
                }
            } else {
                photo_frame::photo_frame_error_t lastDriveError = drive_.get_last_error();
                if (lastDriveError != photo_frame::error_type::None) {
                    log_e("Failed to retrieve TOC. Error code: %d", lastDriveError.code);
                    error = lastDriveError;
                } else if (tocError != photo_frame::error_type::None) {
                    log_e("Failed to read TOC file count. Error code: %d", tocError.code);
                    error = tocError;
                } else {
                    log_w("No files found in Google Drive folder!");
                    error = photo_frame::error_type::NoImagesFound;
                }

                log_i("Closing SD card after Google Drive error to prevent SPI conflicts");
                sd_card.end();
            }
        }
    } else {
        log_e("Failed to initialize SD card. Error code: %d", error.code);
    }

    if (error != photo_frame::error_type::None && sd_card.isInitialized()) {
        log_w("Closing SD card due to error (code %d) to prevent SPI conflicts", error.code);
        sd_card.end();
    }

    return ImageLoadResult(error);
}

} // namespace photo_frame
