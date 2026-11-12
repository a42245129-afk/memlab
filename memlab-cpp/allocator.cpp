#include "allocator.hpp"

#include <algorithm>

namespace memlab {

struct Arena::Header {
    std::uint64_t size_flags;  // total block size | free bit (bit 0)
    std::uint64_t prev_size;   // total size of the physically previous block, 0 if first

    std::size_t size() const { return static_cast<std::size_t>(size_flags & ~std::uint64_t{1}); }
    bool is_free() const { return (size_flags & 1u) != 0; }
    void set(std::size_t size, bool free) { size_flags = size | (free ? 1u : 0u); }
};

namespace {
constexpr std::size_t align_up(std::size_t n) {
    return (n + Arena::kAlign - 1) & ~(Arena::kAlign - 1);
}
}  // namespace

Arena::Arena(std::size_t bytes, Fit fit) : fit_(fit) {
    static_assert(sizeof(Header) == kHeader, "header must be exactly 16 bytes");
    size_ = bytes & ~(kAlign - 1);
    if (size_ < kMinBlock) throw std::invalid_argument("arena too small");
    base_ = static_cast<std::byte*>(std::aligned_alloc(kAlign, size_));
    if (!base_) throw std::bad_alloc();
    Header* h = first();
    h->set(size_, true);
    h->prev_size = 0;
}

Arena::~Arena() { std::free(base_); }

Arena::Header* Arena::first() const { return reinterpret_cast<Header*>(base_); }

Arena::Header* Arena::next(Header* h) const {
    std::byte* n = reinterpret_cast<std::byte*>(h) + h->size();
    return n < base_ + size_ ? reinterpret_cast<Header*>(n) : nullptr;
}

Arena::Header* Arena::prev(Header* h) const {
    return h->prev_size ? reinterpret_cast<Header*>(reinterpret_cast<std::byte*>(h) - h->prev_size)
                        : nullptr;
}

std::byte* Arena::payload(Header* h) { return reinterpret_cast<std::byte*>(h) + kHeader; }

void* Arena::allocate(std::size_t n) noexcept {
    if (n == 0) n = 1;
    if (n > size_) return nullptr;  // also guards align_up against overflow
    const std::size_t need = align_up(n) + kHeader;

    Header* chosen = nullptr;
    for (Header* h = first(); h; h = next(h)) {
        if (!h->is_free() || h->size() < need) continue;
        if (fit_ == Fit::First) {
            chosen = h;
            break;
        }
        if (!chosen || h->size() < chosen->size()) chosen = h;  // best fit: tightest hole
    }
    if (!chosen) return nullptr;

    const std::size_t rest = chosen->size() - need;
    if (rest >= kMinBlock) {  // split off the unused tail as a new free block
        chosen->set(need, false);
        Header* r = next(chosen);
        r->set(rest, true);
        r->prev_size = need;
        if (Header* after = next(r)) after->prev_size = rest;
    } else {
        chosen->set(chosen->size(), false);  // too small to split; hand out whole block
    }
    return payload(chosen);
}

bool Arena::deallocate(void* p) noexcept {
    if (!p) return true;
    // Walk the list so only genuine payload pointers are accepted.
    for (Header* h = first(); h; h = next(h)) {
        if (static_cast<void*>(payload(h)) != p) continue;
        if (h->is_free()) return false;  // double free
        h->set(h->size(), true);
        coalesce(h);
        return true;
    }
    return false;
}

void Arena::coalesce(Header* h) noexcept {
    if (Header* n = next(h); n && n->is_free()) {
        h->set(h->size() + n->size(), true);
        if (Header* after = next(h)) after->prev_size = h->size();
    }
    if (Header* p = prev(h); p && p->is_free()) {
        p->set(p->size() + h->size(), true);
        if (Header* after = next(p)) after->prev_size = p->size();
    }
}

Stats Arena::stats() const {
    Stats s;
    s.arena = size_;
    for (Header* h = first(); h; h = next(h)) {
        if (h->is_free()) {
            s.free_bytes += h->size();
            s.largest_free = std::max(s.largest_free, h->size());
            ++s.free_blocks;
        } else {
            s.used_bytes += h->size();
            ++s.used_blocks;
        }
    }
    return s;
}

bool Arena::check() const {
    std::size_t total = 0, prev_size = 0;
    bool prev_free = false;
    for (Header* h = first(); h; h = next(h)) {
        const std::size_t sz = h->size();
        if (sz < kMinBlock || sz % kAlign != 0) return false;
        if (h->prev_size != prev_size) return false;
        if (h->is_free() && prev_free) return false;  // missed coalesce
        total += sz;
        prev_size = sz;
        prev_free = h->is_free();
    }
    return total == size_;
}

std::string Arena::render(std::size_t width) const {
    static const char kLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::string bar(width, '.');
    std::size_t offset = 0, index = 0;
    for (Header* h = first(); h; h = next(h)) {
        std::size_t lo = offset * width / size_;
        std::size_t hi = (offset + h->size()) * width / size_;
        if (hi == lo) hi = lo + 1;  // keep tiny blocks visible
        if (!h->is_free()) {
            for (std::size_t i = lo; i < hi && i < width; ++i) bar[i] = kLetters[index % (sizeof(kLetters) - 1)];
        }
        offset += h->size();
        ++index;
    }
    return "[" + bar + "]";
}

}  // namespace memlab
