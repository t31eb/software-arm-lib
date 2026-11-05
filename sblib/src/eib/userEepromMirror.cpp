/*
 *  userEepromMirror.cpp - The user EEPROM as a mirror in RAM, persisted in the flash.
 *
 *  Moved from userEeprom.cpp (created on 19.11.2021 by dridders) without changes of the behavior.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/eib/userEepromMirror.h>
#include <sblib/internal/iap.h>
#include <sblib/interrupt.h>
#include <sblib/platform.h>
#include <sblib/utils.h>
#include <cstring>

UserEepromMirror::UserEepromMirror(const uint32_t size, const uint32_t flashSize) :
    UserEepromBackend(size),
    data(new uint8_t[size]()),
    pageSize(flashSize)
{
    reload();
}

uint32_t UserEepromMirror::flashSize() const
{
    return pageSize;
}

uint32_t UserEepromMirror::numEepromPages() const
{
    return FLASH_SECTOR_SIZE / flashSize();
}

uint8_t* UserEepromMirror::lastEepromPage() const
{
    return flashSectorAddress() + flashSize() * (numEepromPages() - 1);
}

uint8_t* UserEepromMirror::flashSectorAddress() const
{
    return (FLASH_BASE_ADDRESS + iapFlashSize() - FLASH_SECTOR_SIZE);
}

uint8_t* UserEepromMirror::findValidPage() const
{
    const uint8_t* firstPage = FLASH_BASE_ADDRESS + iapFlashSize() - FLASH_SECTOR_SIZE;
    uint8_t* page = lastEepromPage();

    while (page >= firstPage)
    {
        if (page[size() - 1] != 0xff) ///\todo check more then  only one byte for 0xff (empty flash)
            return page;

        page -= pageSize;
    }

    return nullptr; // no valid page found
}

void UserEepromMirror::read(const uint32_t offset, uint8_t* buffer, const uint32_t length) const
{
    memcpy(buffer, data + offset, length);
}

void UserEepromMirror::write(const uint32_t offset, const uint8_t* buffer, const uint32_t length)
{
    memcpy(data + offset, buffer, length);
}

void UserEepromMirror::reload()
{
    uint8_t* page = findValidPage();

    if (page)
        memcpy(data, page, size());
    else
        memset(data, 0, size()); ///\todo should filling with zeros indicate a readError? if yes, then it should be somewhere reported
}

void UserEepromMirror::flush()
{
    noInterrupts();

    uint8_t* page = findValidPage();
    if (page == lastEepromPage())
    {
        page = flashSectorAddress();
    }
    else if (page)
        page += pageSize;
    else
    {
        page = flashSectorAddress();
    }

    if (page == flashSectorAddress())
    {
        // Erase the sector
        const int sectorId = iapSectorOfAddress(page);
        const IAP_Status rc = iapEraseSector(sectorId);
        if (rc != IAP_SUCCESS)
        {
            fatalError(); // erasing failed
        }
    }

    data[size() - 1] = 0; // mark the page as in use

    for (uint32_t i = 0; i < size(); i += 1024)
    {
        int chunk = size() - i;
        if (chunk > 1024)
        {
            chunk = 1024;
        }
        IAP_Status rc = iapProgram(page + i, data + i, chunk);
        if (rc != IAP_SUCCESS)
        {
            fatalError(); // flashing failed
        }
    }

    interrupts();
}
