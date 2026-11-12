// Dependency-free tests. Build with sanitizers via `make test`.
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "allocator.hpp"

using memlab::Arena;
using memlab::Fit;

static int g_failed = 0, g_checks = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                        \
    } while (0)

static void test_alignment_and_basic() {
    Arena a(4096);
    for (std::size_t n : {1, 7, 16, 17, 100, 333}) {
        void* p = a.allocate(n);
        CHECK(p != nullptr);
        CHECK(reinterpret_cast<std::uintptr_t>(p) % 16 == 0);
        std::memset(p, 0xAB, n);  // must be fully writable (ASan would catch overruns of the arena)
    }
    CHECK(a.check());
}

static void test_split_and_coalesce() {
    Arena a(1024);
    void* x = a.allocate(100);
    void* y = a.allocate(100);
    void* z = a.allocate(100);
    CHECK(a.stats().used_blocks == 3);

    CHECK(a.deallocate(y));
    CHECK(a.stats().free_blocks == 2);  // hole + tail
    CHECK(a.deallocate(x));             // merges forward into y's hole
    CHECK(a.stats().free_blocks == 2);
    CHECK(a.deallocate(z));             // merges both sides -> everything is one block
    auto s = a.stats();
    CHECK(s.free_blocks == 1 && s.used_blocks == 0 && s.free_bytes == s.arena);
    CHECK(a.check());
}

static void test_exhaustion_and_recovery() {
    Arena a(512);
    std::vector<void*> ps;
    while (void* p = a.allocate(32)) ps.push_back(p);
    CHECK(!ps.empty());
    CHECK(a.allocate(32) == nullptr);
    CHECK(a.allocate(1u << 30) == nullptr);
    CHECK(a.allocate(static_cast<std::size_t>(-1)) == nullptr);  // overflow guard
    for (void* p : ps) CHECK(a.deallocate(p));
    CHECK(a.stats().free_blocks == 1);
    CHECK(a.allocate(400) != nullptr);  // big request works again after coalescing
}

static void test_invalid_frees() {
    Arena a(1024), other(1024);
    void* p = a.allocate(64);
    CHECK(a.deallocate(nullptr));
    CHECK(a.deallocate(p));
    CHECK(!a.deallocate(p));  // double free rejected
    int local = 0;
    CHECK(!a.deallocate(&local));                                   // foreign pointer
    void* q = other.allocate(64);
    CHECK(!a.deallocate(q));                                        // other arena's pointer
    void* r = a.allocate(64);
    CHECK(!a.deallocate(static_cast<char*>(r) + 16));               // interior pointer
    CHECK(a.check());
}

static void test_best_fit_picks_tightest() {
    for (Fit f : {Fit::First, Fit::Best}) {
        Arena a(1024, f);
        void* big = a.allocate(200);
        a.allocate(16);
        void* small = a.allocate(100);
        a.allocate(16);
        a.allocate(400);
        a.deallocate(big);
        a.deallocate(small);
        void* x = a.allocate(90);
        // first-fit lands in the first (big) hole, best-fit in the tighter one.
        CHECK((f == Fit::First) == (x == big));
        CHECK((f == Fit::Best) == (x == small));
    }
}

static void stress(Fit fit, unsigned seed) {
    Arena a(1 << 16, fit);
    std::mt19937 rng(seed);
    struct Live {
        unsigned char* p;
        std::size_t n;
        unsigned char tag;
    };
    std::vector<Live> live;
    bool corrupted = false;
    for (int i = 0; i < 6000; ++i) {
        if (!live.empty() && rng() % 100 < 45) {
            std::size_t idx = rng() % live.size();
            Live l = live[idx];
            live[idx] = live.back();
            live.pop_back();
            for (std::size_t k = 0; k < l.n; ++k) corrupted |= (l.p[k] != l.tag);  // overlap detector
            CHECK(a.deallocate(l.p));
        } else {
            std::size_t n = 1 + rng() % 300;
            if (auto* p = static_cast<unsigned char*>(a.allocate(n))) {
                unsigned char tag = static_cast<unsigned char>(rng());
                std::memset(p, tag, n);
                live.push_back({p, n, tag});
            }
        }
        if (i % 25 == 0) CHECK(a.check());
    }
    CHECK(!corrupted);
    for (auto& l : live) CHECK(a.deallocate(l.p));
    auto s = a.stats();
    CHECK(a.check() && s.free_blocks == 1 && s.free_bytes == s.arena);
}

int main() {
    struct { const char* name; void (*fn)(); } tests[] = {
        {"alignment_and_basic", test_alignment_and_basic},
        {"split_and_coalesce", test_split_and_coalesce},
        {"exhaustion_and_recovery", test_exhaustion_and_recovery},
        {"invalid_frees", test_invalid_frees},
        {"best_fit_picks_tightest", test_best_fit_picks_tightest},
    };
    for (auto& t : tests) {
        int before = g_failed;
        t.fn();
        std::printf("%-26s %s\n", t.name, g_failed == before ? "ok" : "FAILED");
    }
    for (Fit f : {Fit::First, Fit::Best}) {
        int before = g_failed;
        for (unsigned seed = 1; seed <= 10; ++seed) stress(f, seed);
        std::printf("stress (%-9s x10 seeds) %s\n", f == Fit::First ? "first-fit" : "best-fit",
                    g_failed == before ? "ok" : "FAILED");
    }
    std::printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
