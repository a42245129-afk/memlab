# MemLab (C++) — Free-List Memory Allocator

A custom memory allocator built from scratch in **C++17**, implementing memory allocation, deallocation, block splitting, coalescing, and fragmentation analysis using a fixed-size memory arena.

Unlike a high-level memory simulator, MemLab works with real memory addresses, byte-level operations, and 16-byte aligned pointers.

## Overview

MemLab demonstrates the internal working of dynamic memory allocation through a custom free-list allocator.

It implements two allocation strategies:

- **First-Fit:** Allocates the first available block large enough to satisfy the request.
- **Best-Fit:** Allocates the smallest available block that can satisfy the request.

The allocator also supports block splitting, constant-time adjacent block coalescing, invalid-free detection, and memory consistency verification.

## Features

- Custom memory arena using `std::aligned_alloc`.
- 16-byte aligned memory allocation.
- First-Fit and Best-Fit allocation strategies.
- Automatic block splitting.
- Constant-time block coalescing.
- Double-free and invalid-pointer detection.
- Memory fragmentation statistics.
- Heap visualization using ASCII rendering.
- Internal memory invariant checker.
- Randomized stress testing.
- AddressSanitizer and UndefinedBehaviorSanitizer support.

## Architecture

MemLab manages memory using a contiguous arena divided into multiple blocks.

Each block contains a 16-byte header followed by its payload.

```text
+-------------+----------------+
| Header 16 B | Payload        |
+-------------+----------------+
| Header 16 B | Payload        |
+-------------+----------------+
| Header 16 B | Payload        |
+-------------+----------------+
```

### Block Header

Each block contains two 64-bit fields:

```cpp
struct Arena::Header {
    std::uint64_t size_flags;
    std::uint64_t prev_size;
};
```

- `size_flags`: Stores total block size and the free/allocated status.
- `prev_size`: Stores the size of the previous block.

This design enables constant-time identification of adjacent blocks during coalescing.

## Allocation Workflow

1. Calculate the required memory size and align it to 16 bytes.
2. Search available blocks using the selected allocation strategy.
3. Split the selected block if sufficient space remains.
4. Mark the block as allocated.
5. Return the payload pointer.

## Deallocation Workflow

1. Validate the supplied pointer.
2. Reject foreign, interior, and already-freed pointers.
3. Mark the block as free.
4. Merge adjacent free blocks.
5. Update the previous-block size metadata.

## Complexity Analysis

| Operation | Complexity |
|---|---|
| Allocation | O(n) |
| Deallocation | O(n) |
| Adjacent block coalescing | O(1) |
| Invariant checking | O(n) |

Here, `n` represents the number of blocks in the arena.

## Performance Results

Benchmark performed on a 16,384-byte arena using 20 random seeds and 3,000 operations per run.

| Metric | First-Fit | Best-Fit |
|---|---:|---:|
| Failed allocations | 69.8 | 68.2 |
| Average fragmentation | 47.7% | 45.5% |

**Observation:** Best-Fit performed slightly better than First-Fit under the tested random workload.

## Testing

The project passed **66,520 checks with zero failures**.

Testing was performed using:

- AddressSanitizer (ASan)
- UndefinedBehaviorSanitizer (UBSan)
- Randomized allocation and deallocation operations
- Memory alignment verification
- Block splitting and coalescing tests
- Double-free and invalid-pointer tests
- Memory invariant validation

The randomized stress test used 10 seeds per strategy with 6,000 operations per seed.

## Build and Run

### Requirements

- C++17 compatible compiler
- GNU Make
- AddressSanitizer and UndefinedBehaviorSanitizer support

### Build

```bash
make
```

### Run Tests

```bash
make test
```

## Limitations

- Not thread-safe.
- Fixed-size memory arena.
- Linear-time allocation and pointer validation.
- No `realloc` or `calloc` implementation.
- No support for dynamic arena expansion.

## Future Improvements

- Implement segregated free lists.
- Introduce constant-time pointer validation.
- Add thread safety using mutexes.
- Implement per-thread memory arenas.
- Support dynamic memory expansion using `mmap`.
- Implement `realloc`.
- Integrate custom `operator new` and `operator delete`.
- Benchmark against the system `malloc` implementation.

## Technologies Used

- C++17
- GNU Compiler Collection (GCC)
- Make
- AddressSanitizer
- UndefinedBehaviorSanitizer

## Author

**Arthi Karjee**

BCA (Data Science)  
Chandigarh University

---

*MemLab is an educational systems-programming project designed to explore memory management, allocator architecture, and low-level C++ programming.*

