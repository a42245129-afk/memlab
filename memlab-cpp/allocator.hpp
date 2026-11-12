// memlab - a small free-list memory allocator over a fixed arena.
//
// Block layout (all sizes are multiples of 16, payloads are 16-byte aligned):
//
//   +----------------------+----------------------+---------------------+
//   | size | free-bit (8B) | prev_size (8B)       | payload ...         |
//   +----------------------+----------------------+---------------------+
//   ^ header (16 bytes)                           ^ pointer handed to caller
//
// `size` is the TOTAL block size (header + payload). Storing prev_size lets us
// find the previous block in O(1), so freeing can coalesce in both directions
// without scanning the list.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string>

namespace memlab {

enum class Fit { First, Best };

struct Stats {
    std::size_t arena = 0;         // total arena size in bytes
    std::size_t used_bytes = 0;    // bytes in allocated blocks (headers included)
    std::size_t free_bytes = 0;    // bytes in free blocks
    std::size_t largest_free = 0;  // biggest single free block
    std::size_t used_blocks = 0;
    std::size_t free_blocks = 0;

    // 0 = all free memory is one contiguous block, ->1 = badly shattered.
    double fragmentation() const {
        return free_bytes == 0 ? 0.0
                               : 1.0 - static_cast<double>(largest_free) / static_cast<double>(free_bytes);
    }
};

class Arena {
public:
    static constexpr std::size_t kAlign = 16;
    static constexpr std::size_t kHeader = 16;
    static constexpr std::size_t kMinBlock = kHeader + kAlign;  // header + 16B payload

    explicit Arena(std::size_t bytes, Fit fit = Fit::First);
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    // Returns a 16-byte aligned pointer, or nullptr if no block fits.
    void* allocate(std::size_t n) noexcept;

    // Returns false for pointers that are not live allocations from this arena
    // (foreign pointers, interior pointers, double frees). nullptr is a no-op.
    bool deallocate(void* p) noexcept;

    Stats stats() const;
    bool check() const;  // verifies heap invariants; true if consistent
    std::string render(std::size_t width = 64) const;  // ASCII picture of the heap

    Fit fit() const { return fit_; }
    void set_fit(Fit f) { fit_ = f; }
    std::size_t capacity() const { return size_; }

    // Offset of a payload pointer from the arena start (for display).
    std::size_t offset_of(const void* p) const {
        return static_cast<std::size_t>(static_cast<const std::byte*>(p) - base_);
    }

private:
    struct Header;

    Header* first() const;
    Header* next(Header* h) const;
    Header* prev(Header* h) const;
    static std::byte* payload(Header* h);
    void coalesce(Header* h) noexcept;

    std::byte* base_ = nullptr;
    std::size_t size_ = 0;
    Fit fit_;
};

}  // namespace memlab
