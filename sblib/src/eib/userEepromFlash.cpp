/*
 *  userEepromFlash.cpp - The user EEPROM in the flash with a cache of one page.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/eib/userEepromFlash.h>
#include <sblib/internal/iap.h>
#include <sblib/utils.h>
#include <algorithm>
#include <cstring>

UserEepromFlash::UserEepromFlash(const uint32_t size, FlashPageCache& cache) :
    UserEepromBackend(size),
    cache(cache),
    flashStart(iapFlashSize() - FLASH_SECTOR_SIZE),
    marker(true)
{
    reload();
}

UserEepromFlash::UserEepromFlash(const uint32_t size, FlashPageCache& cache, const uint32_t flashOffset) :
    UserEepromBackend(size),
    cache(cache),
    flashStart(flashOffset),
    marker(false)
{
    if (((flashOffset % FLASH_PAGE_SIZE) != 0) || (flashOffset + size > iapFlashSize()))
    {
        fatalError();
    }
    reload();
}

uint8_t* UserEepromFlash::flashAddress(const uint32_t offset) const
{
    return FLASH_BASE_ADDRESS + flashStart + offset;
}

bool UserEepromFlash::isCached(const uint32_t page) const
{
    return (cache.owner == this) && (cache.page == page);
}

void UserEepromFlash::reload()
{
    if (cache.owner == this)
    {
        cache.owner = nullptr;
        cache.dirty = false;
    }
    valid = !marker || (*flashAddress(size() - 1) != 0xff);
}

void UserEepromFlash::read(uint32_t offset, uint8_t* buffer, uint32_t length) const
{
    while (length > 0)
    {
        const uint32_t page = offset / FLASH_PAGE_SIZE;
        const uint32_t pos = offset % FLASH_PAGE_SIZE;
        const uint32_t count = std::min(length, FLASH_PAGE_SIZE - pos);
        if (isCached(page))
        {
            memcpy(buffer, cache.data + pos, count);
        }
        else if (valid)
        {
            memcpy(buffer, flashAddress(offset), count);
        }
        else
        {
            memset(buffer, 0, count);
        }
        offset += count;
        buffer += count;
        length -= count;
    }
}

void UserEepromFlash::write(uint32_t offset, const uint8_t* buffer, uint32_t length)
{
    while (length > 0)
    {
        const uint32_t page = offset / FLASH_PAGE_SIZE;
        const uint32_t pos = offset % FLASH_PAGE_SIZE;
        const uint32_t count = std::min(length, FLASH_PAGE_SIZE - pos);
        if (!isCached(page))
        {
            if (cache.dirty)
            {
                if (flashGuard)
                {
                    flashGuard->pauseForFlash();
                }
                cache.owner->writeBack(); // maybe the page of another backend
                if (flashGuard)
                {
                    flashGuard->resumeAfterFlash();
                }
            }
            load(page);
        }
        if (memcmp(cache.data + pos, buffer, count) != 0)
        {
            memcpy(cache.data + pos, buffer, count);
            cache.dirty = true;
        }
        offset += count;
        buffer += count;
        length -= count;
    }
}

void UserEepromFlash::flush()
{
    if ((cache.owner == this) && cache.dirty)
    {
        writeBack();
    }
}

void UserEepromFlash::load(const uint32_t page)
{
    cache.owner = nullptr; // the bus interrupt may read meanwhile, from the flash then
    if (valid)
    {
        memcpy(cache.data, flashAddress(page * FLASH_PAGE_SIZE), FLASH_PAGE_SIZE);
    }
    else
    {
        memset(cache.data, 0, FLASH_PAGE_SIZE);
    }
    cache.page = page;
    cache.dirty = false;
    cache.owner = this;
}

void UserEepromFlash::writeBack()
{
    if (!valid)
    {
        format();
        return;
    }

    if (iapErasePage(iapPageOfAddress(flashAddress(cache.page * FLASH_PAGE_SIZE))) != IAP_SUCCESS)
    {
        fatalError();
    }
    program(cache.page);
    cache.dirty = false;
}

void UserEepromFlash::format()
{
    if (iapEraseSector(iapSectorOfAddress(flashAddress(0))) != IAP_SUCCESS)
    {
        fatalError();
    }
    const uint32_t changedPage = cache.page;
    program(changedPage);

    cache.owner = nullptr; // the cache doesn't hold the changed page anymore
    memset(cache.data, 0, FLASH_PAGE_SIZE);
    const uint32_t pages = (size() + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    for (uint32_t page = 0; page < pages; page++)
    {
        if (page != changedPage)
        {
            program(page);
        }
    }

    // The cache holds zeros now, they aren't the content of the changed page anymore
    cache.dirty = false;
    valid = true;
}

void UserEepromFlash::program(const uint32_t page)
{
    const uint32_t lastOffset = size() - 1;
    if (marker && (page == lastOffset / FLASH_PAGE_SIZE))
    {
        cache.data[lastOffset % FLASH_PAGE_SIZE] = 0; // mark the user EEPROM as valid, like the mirror does
    }
    if (iapProgram(flashAddress(page * FLASH_PAGE_SIZE), cache.data, FLASH_PAGE_SIZE) != IAP_SUCCESS)
    {
        fatalError();
    }
    pageWriteCount++;
}
