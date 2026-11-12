// memlab demo + benchmark.
//   ./memlab demo    walk through alloc/free/coalescing and first-fit vs best-fit
//   ./memlab bench   compare strategies on a random workload
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "allocator.hpp"

using memlab::Arena;
using memlab::Fit;

static const char* name(Fit f) { return f == Fit::First ? "first-fit" : "best-fit"; }

static void show(const Arena& a, const char* caption) {
    const auto s = a.stats();
    std::printf("%s\n  %s\n  used=%zu free=%zu largest_free=%zu holes=%zu fragmentation=%.0f%%\n\n", caption,
                a.render().c_str(), s.used_bytes, s.free_bytes, s.largest_free, s.free_blocks,
                s.fragmentation() * 100);
}

static void demo_basics() {
    std::puts("=== 1. alloc, free, coalesce (1 KiB arena) ===\n");
    Arena heap(1024);
    void* p[5];
    const std::size_t sizes[5] = {100, 60, 200, 40, 150};
    for (int i = 0; i < 5; ++i) {
        p[i] = heap.allocate(sizes[i]);
        std::printf("allocate(%3zu) -> offset %4zu  (16-byte aligned: %s)\n", sizes[i], heap.offset_of(p[i]),
                    reinterpret_cast<std::uintptr_t>(p[i]) % 16 == 0 ? "yes" : "NO");
    }
    std::puts("");
    std::strcpy(static_cast<char*>(p[0]), "hello from block A");
    std::printf("block A holds: \"%s\"\n\n", static_cast<char*>(p[0]));

    show(heap, "after 5 allocations:");
    heap.deallocate(p[1]);
    heap.deallocate(p[3]);
    show(heap, "freed B and D (two holes):");
    heap.deallocate(p[2]);
    show(heap, "freed C -> B, C, D merge into one hole:");
    for (int i : {0, 4}) heap.deallocate(p[i]);
    show(heap, "freed everything -> back to a single block:");
}

static void demo_fit() {
    std::puts("=== 2. same request, different strategy ===\n");
    for (Fit f : {Fit::First, Fit::Best}) {
        Arena heap(1024, f);
        void* a = heap.allocate(200);
        heap.allocate(16);  // separators keep the holes from merging
        void* b = heap.allocate(100);
        heap.allocate(16);
        heap.allocate(400);
        heap.deallocate(a);
        heap.deallocate(b);
        std::printf("[%s] holes of 224, 128 and 192 bytes exist; now allocate(90):\n", name(f));
        std::printf("  before: %s\n", heap.render().c_str());
        void* x = heap.allocate(90);
        std::printf("  after : %s  -> offset %zu\n\n", heap.render().c_str(), heap.offset_of(x));
    }
    std::puts("first-fit grabs the first hole that fits; best-fit hunts for the tightest one.\n");
}

static void bench() {
    constexpr int kSeeds = 20, kOps = 3000;
    constexpr std::size_t kArena = 16 * 1024;
    const std::size_t kSizes[] = {8, 24, 64, 100, 256, 512, 900};

    std::printf("Benchmark: %d random alloc/free ops on a %zu-byte arena, averaged over %d seeds\n\n", kOps,
                kArena, kSeeds);
    std::printf("%-10s %16s %22s\n", "strategy", "alloc failures", "avg fragmentation");
    std::puts("-----------------------------------------------------");

    for (Fit f : {Fit::First, Fit::Best}) {
        double fails = 0, frag = 0;
        for (int seed = 0; seed < kSeeds; ++seed) {
            Arena heap(kArena, f);
            std::mt19937 rng(static_cast<unsigned>(seed));
            std::vector<void*> live;
            for (int i = 0; i < kOps; ++i) {
                if (!live.empty() && rng() % 100 < 48) {
                    std::size_t idx = rng() % live.size();
                    heap.deallocate(live[idx]);
                    live[idx] = live.back();
                    live.pop_back();
                } else if (void* p = heap.allocate(kSizes[rng() % (sizeof(kSizes) / sizeof(kSizes[0]))])) {
                    live.push_back(p);
                } else {
                    fails += 1;
                }
                frag += heap.stats().fragmentation();
            }
        }
        std::printf("%-10s %16.1f %21.1f%%\n", name(f), fails / kSeeds, frag / (kSeeds * kOps) * 100);
    }
}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "demo";
    if (mode == "demo") {
        demo_basics();
        demo_fit();
    } else if (mode == "bench") {
        bench();
    } else {
        std::fprintf(stderr, "usage: %s [demo|bench]\n", argv[0]);
        return 1;
    }
}
