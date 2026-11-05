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

UserEepromFlash::UserEepromFlash(const uint32_t size) :
    UserEepromBackend(size)
{
    reload();
}

uint8_t* UserEepromFlash::flashAddress(const uint32_t offset) const
{
    return FLASH_BASE_ADDRESS + iapFlashSize() - FLASH_SECTOR_SIZE + offset;
}

void UserEepromFlash::reload()
{
    cachedPage = NO_PAGE;
    dirty = false;
    valid = (*flashAddress(size() - 1) != 0xff);
}

void UserEepromFlash::read(uint32_t offset, uint8_t* buffer, uint32_t length) const
{
    while (length > 0)
    {
        const uint32_t page = offset / FLASH_PAGE_SIZE;
        const uint32_t pos = offset % FLASH_PAGE_SIZE;
        const uint32_t count = std::min(length, FLASH_PAGE_SIZE - pos);
        if (page == cachedPage)
        {
            memcpy(buffer, cache + pos, count);
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
        if (page != cachedPage)
        {
            if (dirty)
            {
                if (flashGuard)
                {
                    flashGuard->pauseForFlash();
                }
                writeBack();
                if (flashGuard)
                {
                    flashGuard->resumeAfterFlash();
                }
            }
            load(page);
        }
        if (memcmp(cache + pos, buffer, count) != 0)
        {
            memcpy(cache + pos, buffer, count);
            dirty = true;
        }
        offset += count;
        buffer += count;
        length -= count;
    }
}

void UserEepromFlash::flush()
{
    if (dirty)
    {
        writeBack();
    }
}

void UserEepromFlash::load(const uint32_t page)
{
    if (valid)
    {
        memcpy(cache, flashAddress(page * FLASH_PAGE_SIZE), FLASH_PAGE_SIZE);
    }
    else
    {
        memset(cache, 0, FLASH_PAGE_SIZE);
    }
    cachedPage = page;
    dirty = false;
}

void UserEepromFlash::writeBack()
{
    if (!valid)
    {
        format();
        return;
    }

    if (iapErasePage(iapPageOfAddress(flashAddress(cachedPage * FLASH_PAGE_SIZE))) != IAP_SUCCESS)
    {
        fatalError();
    }
    program(cachedPage);
    dirty = false;
}

void UserEepromFlash::format()
{
    if (iapEraseSector(iapSectorOfAddress(flashAddress(0))) != IAP_SUCCESS)
    {
        fatalError();
    }
    const uint32_t changedPage = cachedPage;
    program(changedPage);

    memset(cache, 0, FLASH_PAGE_SIZE);
    const uint32_t pages = (size() + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    for (uint32_t page = 0; page < pages; page++)
    {
        if (page != changedPage)
        {
            program(page);
        }
    }

    // The cache holds zeros now, they aren't the content of the changed page anymore
    cachedPage = NO_PAGE;
    dirty = false;
    valid = true;
}

void UserEepromFlash::program(const uint32_t page)
{
    const uint32_t lastOffset = size() - 1;
    if (page == lastOffset / FLASH_PAGE_SIZE)
    {
        cache[lastOffset % FLASH_PAGE_SIZE] = 0; // mark the user EEPROM as valid, like the mirror does
    }
    if (iapProgram(flashAddress(page * FLASH_PAGE_SIZE), cache, FLASH_PAGE_SIZE) != IAP_SUCCESS)
    {
        fatalError();
    }
    pageWriteCount++;
}
