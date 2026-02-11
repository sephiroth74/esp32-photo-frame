#ifndef PSRAM_ALLOCATOR_H
#define PSRAM_ALLOCATOR_H

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <memory>

namespace photo_frame {

/**
 * @brief Allocate memory preferring PSRAM if available, otherwise fallback to
 * internal RAM
 *
 * @param size Size in bytes to allocate
 * @return void* Pointer to allocated memory, or nullptr if allocation failed
 */
inline void *psram_malloc(size_t size) {
  // Try to allocate in PSRAM first
  void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (ptr == nullptr) {
    // PSRAM allocation failed or not available, fallback to internal RAM
    ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }

  return ptr;
}

/**
 * @brief Allocate and zero-initialize memory preferring PSRAM if available
 *
 * @param num Number of elements
 * @param size Size of each element in bytes
 * @return void* Pointer to allocated memory, or nullptr if allocation failed
 */
inline void *psram_calloc(size_t num, size_t size) {
  // Try to allocate in PSRAM first
  void *ptr = heap_caps_calloc(num, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (ptr == nullptr) {
    // PSRAM allocation failed or not available, fallback to internal RAM
    ptr = heap_caps_calloc(num, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }

  return ptr;
}

/**
 * @brief Reallocate memory preferring PSRAM if available
 *
 * @param ptr Pointer to previously allocated memory (can be nullptr)
 * @param size New size in bytes
 * @return void* Pointer to reallocated memory, or nullptr if allocation failed
 */
inline void *psram_realloc(void *ptr, size_t size) {
  // Try to reallocate in PSRAM first
  void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (new_ptr == nullptr && size > 0) {
    // PSRAM reallocation failed or not available, fallback to internal RAM
    new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }

  return new_ptr;
}

/**
 * @brief Allocate memory preferring internal RAM if available, otherwise
 * fallback to PSRAM
 *
 * @param size Size in bytes to allocate
 * @return void* Pointer to allocated memory, or nullptr if allocation failed
 */
inline void *heap_malloc(size_t size) {
  // Try to allocate in internal RAM first
  void *ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (ptr == nullptr) {
    // Internal RAM allocation failed, fallback to PSRAM
    ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

  return ptr;
}

/**
 * @brief Allocate and zero-initialize memory preferring internal RAM if
 * available
 *
 * @param num Number of elements
 * @param size Size of each element in bytes
 * @return void* Pointer to allocated memory, or nullptr if allocation failed
 */
inline void *heap_calloc(size_t num, size_t size) {
  // Try to allocate in internal RAM first
  void *ptr = heap_caps_calloc(num, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (ptr == nullptr) {
    // Internal RAM allocation failed, fallback to PSRAM
    ptr = heap_caps_calloc(num, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

  return ptr;
}

/**
 * @brief Reallocate memory preferring internal RAM if available
 *
 * @param ptr Pointer to previously allocated memory (can be nullptr)
 * @param size New size in bytes
 * @return void* Pointer to reallocated memory, or nullptr if allocation failed
 */
inline void *heap_realloc(void *ptr, size_t size) {
  // Try to reallocate in internal RAM first
  void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (new_ptr == nullptr && size > 0) {
    // Internal RAM reallocation failed, fallback to PSRAM
    new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

  return new_ptr;
}

/**
 * @brief Free memory allocated by heap_malloc/calloc/realloc or
 * psram_malloc/calloc/realloc
 *
 * @param ptr Pointer to memory to free (can be nullptr)
 */
inline void heap_free(void *ptr) {
  if (ptr != nullptr) {
    heap_caps_free(ptr);
  }
}

/**
 * @brief Free memory allocated by psram_malloc/calloc/realloc (alias for
 * heap_free)
 *
 * @param ptr Pointer to memory to free (can be nullptr)
 */
inline void psram_free(void *ptr) { heap_free(ptr); }

/**
 * @brief Check if PSRAM is available and get its size
 *
 * @return size_t Total PSRAM size in bytes, or 0 if not available
 */
inline size_t psram_get_size() { return heap_caps_get_total_size(MALLOC_CAP_SPIRAM); }

/**
 * @brief Get free PSRAM size
 *
 * @return size_t Free PSRAM size in bytes, or 0 if not available
 */
inline size_t psram_get_free_size() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }

/**
 * @brief Get largest free PSRAM block size
 *
 * @return size_t Largest free PSRAM block in bytes, or 0 if not available
 */
inline size_t psram_get_largest_free_block() { return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM); }

/**
 * @brief Check if a pointer is in PSRAM
 *
 * @param ptr Pointer to check
 * @return true if pointer is in PSRAM, false otherwise
 */
inline bool psram_is_psram_ptr(const void *ptr) {
  if (ptr == nullptr)
    return false;
  return heap_caps_get_allocated_size(const_cast<void *>(ptr)) > 0 && esp_ptr_external_ram(ptr);
}

/**
 * @brief Print PSRAM memory info to serial
 */
inline void psram_print_info() {
  size_t total = psram_get_size();
  size_t free_size = psram_get_free_size();
  size_t largest_block = psram_get_largest_free_block();

  if (total > 0) {
    log_i("PSRAM: Total=%zu bytes, Free=%zu bytes (%.1f%%), Largest block=%zu "
          "bytes\n",
          total, free_size, (free_size * 100.0) / total, largest_block);
  } else {
    log_w("PSRAM: Not available");
  }
}

/**
 * @brief Custom deleter for PSRAM allocated memory to use with std::unique_ptr
 */
struct PSRAMDeleter {
  void operator()(void *ptr) const { psram_free(ptr); }
};

/**
 * @brief Smart pointer type for PSRAM allocated byte arrays
 *
 * Usage:
 *   PSRAMUniquePtr buffer(static_cast<uint8_t*>(psram_malloc(size)));
 *   buffer.get() - get raw pointer
 *   buffer.reset() - release and deallocate
 */
using PSRAMUniquePtr = std::unique_ptr<uint8_t[], PSRAMDeleter>;

/**
 * @brief Create a smart pointer for PSRAM allocated memory
 *
 * @param size Size in bytes to allocate
 * @return PSRAMUniquePtr Smart pointer to allocated memory, or nullptr if
 * allocation failed
 *
 * Usage:
 *   auto buffer = make_psram_unique(1024); // Allocate 1KB
 *   buffer[0] = 42; // Use like array
 */
inline PSRAMUniquePtr make_psram_unique(size_t size) { return PSRAMUniquePtr(static_cast<uint8_t *>(psram_malloc(size))); }

} // namespace photo_frame

#endif // PSRAM_ALLOCATOR_H
