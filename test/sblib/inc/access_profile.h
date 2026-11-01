/*
 * Counts memory accesses of the sblib without changing its code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#ifndef TEST_SBLIB_ACCESS_PROFILE_H_
#define TEST_SBLIB_ACCESS_PROFILE_H_

#include <cstddef>
#include <cstdint>

/**
 * Access profile of the memory regions registered with @ref addRegion.
 *
 * @details Most accesses of the sblib dereference raw pointers, a wrapper around class Memory
 *          would never see them. So the sblib is compiled with -fsanitize=kernel-address in
 *          outline mode (CMake option SBLIB_TEST_ACCESS_PROFILE), which makes the compiler call a
 *          __asan_* function for every load and store. These functions are implemented here
 *          instead of a sanitizer runtime. memcpy, memmove and memset are caught with the linker
 *          option --wrap. Code outside of the sblib target isn't instrumented, so assertions and
 *          test helpers don't show up, except for their calls of the wrapped functions.
 *
 *          A burst is a run of accesses to neighbouring addresses in the same direction, which a
 *          serial memory could serve in a single transaction.
 */
namespace accessProfile
{
struct Counters
{
    uint64_t reads = 0;       //!< number of read accesses
    uint64_t readBytes = 0;   //!< bytes read
    uint64_t readBursts = 0;  //!< bursts of reads
    uint64_t writes = 0;      //!< number of write accesses
    uint64_t writeBytes = 0;  //!< bytes written
    uint64_t writeBursts = 0; //!< bursts of writes
    uint64_t uniqueBytes = 0; //!< number of different bytes read or written
};

/** Removes all regions. */
void clearRegions();

/**
 * Adds a region to watch, regions must not overlap.
 *
 * @param start      first byte of the region
 * @param length     size of the region in bytes
 * @param persistent true for non-volatile memory, only these regions take part in bursts
 * @return           index of the region
 */
size_t addRegion(const void* start, size_t length, bool persistent);

/** Clears all counters and starts counting. */
void start();

/** Stops counting. */
void stop();

/** @return The counters of region @p index */
const Counters& region(size_t index);

/** @return The sum of all persistent regions */
Counters persistentTotal();
}

#endif /* TEST_SBLIB_ACCESS_PROFILE_H_ */
