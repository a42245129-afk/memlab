# MemLab (C++)

A small free-list memory allocator over a fixed arena. It hands out real
16-byte aligned pointers and supports first-fit and best-fit placement,
block splitting and constant-time coalescing.

Needs a C++17 compiler (g++ or clang++) and make.

## Run

```
make test    # build with AddressSanitizer + UBSan and run the tests
make demo    # alloc/free/coalesce walkthrough, first-fit vs best-fit
make bench   # strategy benchmark on a random workload
make clean
```

## Design

Every block starts with a 16-byte header:

```
| size | free-bit (8 bytes) | prev_size (8 bytes) | payload ... |
```

- `size` is the total block size (header + payload), always a multiple of 16
- `prev_size` lets `deallocate` find the previous block in O(1), so it can
  merge with both neighbours without scanning
- A block is split on allocation only if the leftover can hold a header
  plus 16 bytes
- `deallocate` rejects double frees, foreign pointers and interior pointers
- `Arena::check()` verifies the heap invariants (sizes sum to the arena,
  `prev_size` links match, no two adjacent free blocks)

## Files

- `allocator.hpp` / `allocator.cpp`: the `memlab::Arena` class
- `main.cpp`: demo and benchmark
- `tests.cpp`: tests, including a randomized stress test that writes a byte
  pattern into every allocation to detect overlapping blocks
- `Makefile`

## Ideas to extend

- Segregated free lists (size-class bins) so allocation is not an O(n) scan
- Footer-less boundary tags to shave the header down
- A thread-safe version with a mutex, then per-thread arenas
- Compare against `malloc` using the same workload
