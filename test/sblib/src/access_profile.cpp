/*
 * Counts memory accesses of the sblib without changing its code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include <access_profile.h>

#include <algorithm>
#include <vector>

namespace
{
struct Region
{
    uintptr_t begin;
    uintptr_t end;
    bool persistent;
    accessProfile::Counters counters;
    std::vector<bool> seen;
};

std::vector<Region> regions;
bool counting = false;

// the last burst over all persistent regions
const Region* burstRegion = nullptr;
uintptr_t burstBegin = 0;
uintptr_t burstEnd = 0;
bool burstIsWrite = false;

void countRegion(Region& region, const uintptr_t begin, const uintptr_t end, const bool isWrite)
{
    accessProfile::Counters& counters = region.counters;
    (isWrite ? counters.writes : counters.reads)++;
    (isWrite ? counters.writeBytes : counters.readBytes) += end - begin;

    for (uintptr_t address = begin; address < end; address++)
    {
        const size_t index = address - region.begin;
        if (!region.seen[index])
        {
            region.seen[index] = true;
            counters.uniqueBytes++;
        }
    }

    if (!region.persistent)
    {
        return;
    }

    // Values are often read back to front, e.g. makeWord(addr[1], addr[0])
    const bool neighbour = (begin == burstEnd) || (end == burstBegin);
    if ((burstRegion == &region) && (burstIsWrite == isWrite) && neighbour)
    {
        burstBegin = std::min(burstBegin, begin);
        burstEnd = std::max(burstEnd, end);
        return;
    }

    (isWrite ? counters.writeBursts : counters.readBursts)++;
    burstRegion = &region;
    burstBegin = begin;
    burstEnd = end;
    burstIsWrite = isWrite;
}

void count(const void* address, const size_t size, const bool isWrite)
{
    if (!counting)
    {
        return;
    }

    const auto begin = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = begin + size;
    for (Region& region : regions)
    {
        const uintptr_t overlapBegin = std::max(begin, region.begin);
        const uintptr_t overlapEnd = std::min(end, region.end);
        if (overlapBegin < overlapEnd)
        {
            countRegion(region, overlapBegin, overlapEnd, isWrite);
        }
    }
}
}

void accessProfile::clearRegions()
{
    counting = false;
    regions.clear();
}

size_t accessProfile::addRegion(const void* start, const size_t length, const bool persistent)
{
    const auto begin = reinterpret_cast<uintptr_t>(start);
    regions.push_back({begin, begin + length, persistent, {}, std::vector<bool>(length, false)});
    return regions.size() - 1;
}

void accessProfile::start()
{
    for (Region& region : regions)
    {
        region.counters = Counters();
        std::fill(region.seen.begin(), region.seen.end(), false);
    }
    burstRegion = nullptr;
    counting = true;
}

void accessProfile::stop()
{
    counting = false;
}

const accessProfile::Counters& accessProfile::region(const size_t index)
{
    return regions.at(index).counters;
}

accessProfile::Counters accessProfile::persistentTotal()
{
    Counters total;
    for (const Region& region : regions)
    {
        if (region.persistent)
        {
            total.reads += region.counters.reads;
            total.readBytes += region.counters.readBytes;
            total.readBursts += region.counters.readBursts;
            total.writes += region.counters.writes;
            total.writeBytes += region.counters.writeBytes;
            total.writeBursts += region.counters.writeBursts;
            total.uniqueBytes += region.counters.uniqueBytes;
        }
    }
    return total;
}

#define ACCESS_PROFILE_HOOKS(size)                                                    \
    void __asan_load##size##_noabort(const void* address) { count(address, size, false); } \
    void __asan_store##size##_noabort(const void* address) { count(address, size, true); }

extern "C"
{
    ACCESS_PROFILE_HOOKS(1)
    ACCESS_PROFILE_HOOKS(2)
    ACCESS_PROFILE_HOOKS(4)
    ACCESS_PROFILE_HOOKS(8)
    ACCESS_PROFILE_HOOKS(16)

    void __asan_loadN_noabort(const void* address, const size_t size) { count(address, size, false); }
    void __asan_storeN_noabort(const void* address, const size_t size) { count(address, size, true); }
    void __asan_handle_no_return() {}
    void __asan_before_dynamic_init(const char*) {}
    void __asan_after_dynamic_init() {}

    void* __real_memcpy(void* destination, const void* source, size_t size);
    void* __real_memmove(void* destination, const void* source, size_t size);
    void* __real_memset(void* destination, int value, size_t size);

    void* __wrap_memcpy(void* destination, const void* source, const size_t size)
    {
        count(source, size, false);
        count(destination, size, true);
        return __real_memcpy(destination, source, size);
    }

    void* __wrap_memmove(void* destination, const void* source, const size_t size)
    {
        count(source, size, false);
        count(destination, size, true);
        return __real_memmove(destination, source, size);
    }

    void* __wrap_memset(void* destination, const int value, const size_t size)
    {
        count(destination, size, true);
        return __real_memset(destination, value, size);
    }
}
